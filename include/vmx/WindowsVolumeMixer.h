#pragma once

/* ==== Application Includes =============================================== */
#include <vmx/VolumeMixer.h>

/* ==== Standard Library Includes ========================================== */
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stop_token>
#include <string>
#include <thread>


/* ==== Operating System Includes ========================================== */
#define NOMINMAX
#include <windows.h>
#include <winerror.h>
#include <combaseapi.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>

/* ==== Helper Classes ===================================================== */
template <class T>
class QueuedWorkThread
{
public:
    QueuedWorkThread(std::function<void(const T&)> queuedWorkFunction)
      : m_queuedWorkFunction(queuedWorkFunction),
        m_thread(std::bind_front(&QueuedWorkThread::workThreadFunc, this))
    {
    }

    void queue(T &&workItem)
    {
        {
            std::lock_guard guard(m_mutex);
            m_queue.push(workItem);
        }
        m_condition.notify_one();
    }

private:
    void workThreadFunc(std::stop_token stopToken)
    {
        while (true)
        {
            std::unique_lock lock(m_mutex);
            if (!m_condition.wait(lock, stopToken, [this]{return !(this->m_queue.empty());}))
            {
                lock.unlock();
                return;
            }
            T workItem = std::move(m_queue.front());
            m_queue.pop();
            lock.unlock();
            m_queuedWorkFunction(workItem);
        }
    }
private:
    std::function<void(const T&)> m_queuedWorkFunction;
    std::queue<T> m_queue;
    std::jthread m_thread;
    std::mutex m_mutex;
    std::condition_variable_any m_condition;
};

class PeriodicWorkThread
{
public:
    // A period of ZERO is special, means the threaded loop will block until destruction or period is changed
    PeriodicWorkThread(std::function<void(void)> periodicFunction, std::chrono::milliseconds period)
      : m_periodicFunction(periodicFunction),
        m_period(period),
        m_thread(std::bind_front(&PeriodicWorkThread::periodicThreadFunc, this))
    {
    }

    void changePeriod(std::chrono::milliseconds period)
    {
        {
            std::lock_guard guard(m_mutex);
            m_period = period;
            m_bPeriodUpdated = true;
        }

        m_condition.notify_all();
    }

private:
    void periodicThreadFunc(std::stop_token stopToken)
    {
        while (true)
        {
            auto now = std::chrono::system_clock::now();
            std::unique_lock lock(m_mutex);

            if (m_period == std::chrono::milliseconds(0))
            {
               m_condition.wait(lock, stopToken,
                    [this](){return m_bPeriodUpdated;});
            }
            else
            {
               m_condition.wait_until(lock, stopToken, now + m_period,
                    [this](){return m_bPeriodUpdated;});
            }
            m_bPeriodUpdated = false;
            if (stopToken.stop_requested())
            {
                lock.unlock();
                return;
            }
            if (m_period == std::chrono::milliseconds(0))
            {
                lock.unlock();
                continue;
            }
            lock.unlock();

            m_periodicFunction();
        }
    }

private:
    std::function<void(void)> m_periodicFunction;
    std::chrono::milliseconds m_period;
    bool m_bPeriodUpdated = false;
    std::jthread m_thread;
    std::mutex m_mutex;
    std::condition_variable_any m_condition;
};

class CoInitializer
{
public:
   CoInitializer()
   {
      // Initialize the COM library on the current thread.
      HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
      if (SUCCEEDED(hr))
         m_bCoInitialized = true;
   }

   ~CoInitializer()
   {
      // Free the COM library.
      if (m_bCoInitialized)
         CoUninitialize();
   }

   CoInitializer(const CoInitializer&) = delete;
   CoInitializer& operator=(const CoInitializer&) = delete;

private:
   bool m_bCoInitialized = false;
};

template <class ComClass>
class SmartComPtr
{
public:
    SmartComPtr(ComClass *pObject, bool bAddRef)
    {
        m_pObject = pObject;
        if (bAddRef)
        {
            m_pObject->AddRef();
        }
    };

    ~SmartComPtr()
    {
        if (m_pObject)
        {
            m_pObject->Release();
            m_pObject = nullptr;
        }
    };

    SmartComPtr(SmartComPtr<ComClass>&& other)
    {
        *this = std::move(other);
    }

    SmartComPtr<ComClass>& operator=(SmartComPtr<ComClass>&& other)
    {
        if (this != &other)
        {
            if (m_pObject)
            {
                m_pObject->Release();
            }
            m_pObject = other.m_pObject;
            other.m_pObject = nullptr;
        }

        return *this;
    }

    SmartComPtr<ComClass>() = delete;
    SmartComPtr<ComClass>(const SmartComPtr<ComClass>&) = delete;
    SmartComPtr<ComClass>& operator=(const SmartComPtr<ComClass>&) = delete;

    ComClass* operator->() const
    {
        return m_pObject;
    }

    ComClass* get() const
    {
        return m_pObject;
    }

private:
    ComClass *m_pObject = nullptr;
};

/* ==== Volume Mixer Classes =============================================== */
class WindowsAudioSession : public AudioSession
{
public: /* Methods */
    WindowsAudioSession(IAudioSessionControl *pAudioSessionControl);
    std::string getId() const { return m_id; };

public: /* Virtual Methods */
    virtual ~WindowsAudioSession();
    virtual void changeVolume(float volume) override;
    virtual void changeMute(bool bMute) override;

private: /* Methods */
    void peakSample();

private: /* Members */
    std::recursive_mutex m_mutex;
    SmartComPtr<IAudioSessionControl> m_pAudioSessionControl = { nullptr, false };
    SmartComPtr<IAudioSessionEvents> m_pAudioSessionEvents = { nullptr, false };
    bool m_bAudioSessionEventsRegistered = false;
    SmartComPtr<IAudioSessionControl2> m_pAudioSessionControl2 = { nullptr, false };
    SmartComPtr<ISimpleAudioVolume> m_pSimpleAudioVolume = { nullptr, false };
    SmartComPtr<IAudioMeterInformation> m_pAudioMeterInformation = { nullptr, false };
    std::string m_id = "";
    unsigned int m_pid = 0;
    bool m_bSystemsSoundSession = false;

public: /* Friends */
    friend class WindowsAudioDevice;
    friend class CAudioSessionEvents;
};

class CAudioSessionEvents : public IAudioSessionEvents
{
public: /* Methods */
    CAudioSessionEvents(WindowsAudioSession &parent);
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) override;
    virtual HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD ChannelCount, float NewChannelVolumeArray[], DWORD ChangedChannel, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState) override;
    virtual HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) override;

private: /* Members */
    LONG m_ref;
    WindowsAudioSession &m_parent;
};
class CAudioSessionLifetimeObserver;

class WindowsAudioDevice : public AudioDevice
{
public: /* Friends */
    friend class WindowsVolumeMixer;

public: /* Methods */
    WindowsAudioDevice(IMMDevice *pMMDevice, bool bDefaultDevice);
    std::string getId() const { return m_id; };

public: /* Virtual Methods */
    virtual ~WindowsAudioDevice();
    virtual void changeVolume(float volume) override;
    virtual void changeMute(bool bMute) override;

private: /* Methods */
    void markSessionForDeletion(const std::string &audioSessionId);
    void killSession(const std::string &sessionId);
    void peakSample();

private: /* Members */
    std::recursive_mutex m_mutex;
    SmartComPtr<IMMDevice> m_pMMDevice = { nullptr, false };
    SmartComPtr<IPropertyStore> m_pPropertyStore = { nullptr, false };
    SmartComPtr<IAudioSessionManager2> m_pAudioSessionManager2 = { nullptr, false };
    SmartComPtr<IAudioSessionNotification> m_pAudioSessionNotification = { nullptr, false };
    bool m_bAudioSessionNotificationRegistered = false;
    SmartComPtr<IAudioSessionEnumerator> m_pAudioSessionEnumerator = { nullptr, false };
    SmartComPtr<IAudioEndpointVolume> m_pAudioEndpointVolume = { nullptr, false };
    SmartComPtr<IAudioMeterInformation> m_pAudioMeterInformation = { nullptr, false };
    SmartComPtr<IAudioEndpointVolumeCallback> m_pAudioEndpointVolumeCallback = { nullptr, false };
    bool m_bAudioEndpointVolumeCallbackRegistered = false;
    std::map<std::string /* AudioSessionId */, SmartComPtr<CAudioSessionLifetimeObserver>> m_audioSessionLifetimeObservers;
    std::map<std::string /* AudioSessionId */, std::shared_ptr<WindowsAudioSession>> m_audioSessionsMirror;
    std::string m_id = "";
    QueuedWorkThread<std::string> m_queuedWorkThread;

public: /* Friends */
    friend class WindowsVolumeMixer;
    friend class CMMNotificationClient;
    friend class CAudioSessionNotification;
    friend class CAudioEndpointVolumeCallback;
    friend class CAudioSessionLifetimeObserver;
};

class CAudioSessionLifetimeObserver : public IAudioSessionEvents
{
public: /* Methods */
    CAudioSessionLifetimeObserver(WindowsAudioDevice &parent, const std::string &audioSessionId, IAudioSessionControl *pAudioSessionControl);
    virtual ~CAudioSessionLifetimeObserver();
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) override;
    virtual HRESULT STDMETHODCALLTYPE OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnChannelVolumeChanged(DWORD ChannelCount, float NewChannelVolumeArray[], DWORD ChangedChannel, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext) override;
    virtual HRESULT STDMETHODCALLTYPE OnStateChanged(AudioSessionState NewState) override;
    virtual HRESULT STDMETHODCALLTYPE OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason) override;
    void markForDeletion();
    bool isReadyForDeletion() const;

private: /* Members */
    LONG m_ref;
    WindowsAudioDevice &m_parent;
    std::string m_id;
    SmartComPtr<IAudioSessionControl> m_pAudioSessionControl = { nullptr, false };
    bool m_bReadyForDeletion;
};

class CAudioSessionNotification : public IAudioSessionNotification
{
public: /* Methods */
    CAudioSessionNotification(WindowsAudioDevice &parent);
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) override;
    virtual HRESULT STDMETHODCALLTYPE OnSessionCreated(IAudioSessionControl *NewSession) override;

private: /* Members */
    LONG m_ref;
    WindowsAudioDevice &m_parent;
};

class CAudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback
{
public: /* Methods */
    CAudioEndpointVolumeCallback(WindowsAudioDevice &parent);
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) override;
    virtual HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override;

private: /* Members */
    LONG m_ref;
    WindowsAudioDevice &m_parent;
};

class WindowsVolumeMixer : public VolumeMixer
{
public: /* Methods */
    WindowsVolumeMixer();

public: /* Virtual Methods */
    virtual ~WindowsVolumeMixer();
    virtual void setPeakSamplingPeriod(std::chrono::milliseconds period) override;

private: /* Methods */
    void peakSample();

private: /* Members */
    CoInitializer m_coInitializer{};
    std::mutex m_mutex;
    SmartComPtr<IMMDeviceEnumerator> m_pMMDeviceEnumerator = { nullptr, false };
    SmartComPtr<IMMDeviceCollection> m_pMMDeviceCollection = { nullptr, false }; // todo : we don't need to hold onto this past the constructor
    SmartComPtr<IMMNotificationClient> m_pMMNotificationClient = { nullptr, false };
    bool m_bNotificationClientRegistered = false;
    std::map<std::string /*audioDeviceId*/, std::shared_ptr<WindowsAudioDevice>> m_audioDevicesMirror;
    PeriodicWorkThread m_peakSamplingThread;

public: /* Friends */
    friend class CMMNotificationClient;
};

class CMMNotificationClient : public IMMNotificationClient
{
public: /* Methods */
    CMMNotificationClient(WindowsVolumeMixer &parent);
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) override;
    virtual HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) override;
    virtual HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) override;
    virtual HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) override;
    virtual HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) override;
    virtual HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) override;

private: /* Members */
    LONG m_ref;
    WindowsVolumeMixer &m_parent;
};
