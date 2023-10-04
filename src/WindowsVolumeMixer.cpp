/* ==== Application Includes =============================================== */
#include <vmx/WindowsVolumeMixer.h>

/* ==== Standard Library Includes ========================================== */
#include <stdexcept>
#include <algorithm>
#include <format>

/* ==== Operating System Includes ========================================== */
// todo do this in cmake instead
#pragma comment(lib, "Version.lib")
#include <strsafe.h>

/* ==== Macros ============================================================= */
#define CHECK_HRESULT(hr)                                                                                   \
    do                                                                                                      \
    {                                                                                                       \
        if (FAILED((hr)))                                                                                   \
        {                                                                                                   \
            auto s = std::format("Error encountered at {}:{} hr:{:x}", __FILE__, __LINE__, (unsigned)hr);   \
            throw std::runtime_error(s);                                                                    \
        }                                                                                                   \
    }                                                                                                       \
    while (false)

#define LOCK_GUARD(mutex_var) const std::lock_guard<decltype(mutex_var)> lock(mutex_var)

/* ==== Forward Declarations =============================================== */
static std::string utf8_encode(const std::wstring &wstr);
static std::string windowName(unsigned int processId, bool bSystemsSoundSession);
static vmx::AudioSession::State from(AudioSessionState state);
static vmx::AudioDevice::State from(DWORD state);


namespace vmx
{

/* ==== WindowsAudioSession Class ========================================== */
WindowsAudioSession::WindowsAudioSession
(
    IAudioSessionControl *pAudioSessionControl
)
  : m_pAudioSessionControl(pAudioSessionControl, true),
    m_pAudioSessionEvents(new CAudioSessionEvents(*this), false)
{
    HRESULT             hr;
    AudioSessionState   sessionState;
    DWORD               processId;
    float               sessionVolume;
    BOOL                bSessionMuted;
    float               peak;
    void*               ptr = nullptr;
    LPWSTR              wstring = nullptr;

    hr = m_pAudioSessionControl->QueryInterface(
        __uuidof(IAudioSessionControl2),
        &ptr);
    m_pAudioSessionControl2 = { (IAudioSessionControl2*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pAudioSessionControl->RegisterAudioSessionNotification(m_pAudioSessionEvents.get());
    CHECK_HRESULT(hr);
    m_bAudioSessionEventsRegistered = true;

    hr = m_pAudioSessionControl->QueryInterface(
        __uuidof(ISimpleAudioVolume),
        &ptr);
    m_pSimpleAudioVolume = { (ISimpleAudioVolume*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pAudioSessionControl->QueryInterface(
        __uuidof(IAudioMeterInformation),
        &ptr);
    m_pAudioMeterInformation = { (IAudioMeterInformation*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pAudioSessionControl2->GetProcessId(&processId);
    CHECK_HRESULT(hr);
    m_pid = processId;

    hr = m_pAudioSessionControl2->GetSessionInstanceIdentifier(&wstring);
    CHECK_HRESULT(hr);
    if (wstring)
    {
        m_id = utf8_encode(wstring);
        CoTaskMemFree(wstring);
        wstring = nullptr;
    }

    m_bSystemsSoundSession = (m_pAudioSessionControl2->IsSystemSoundsSession() == S_OK);

    hr = m_pAudioSessionControl->GetDisplayName(&wstring);
    CHECK_HRESULT(hr);
    if (wstring)
    {
        std::string name = utf8_encode(wstring);
        if (name.empty() || m_bSystemsSoundSession)
        {
            name = windowName(m_pid, m_bSystemsSoundSession);
        }
        updateName(name.empty() ? m_id : name);
        CoTaskMemFree(wstring);
        wstring = nullptr;
    }

    hr = m_pAudioSessionControl->GetIconPath(&wstring);
    CHECK_HRESULT(hr);
    if (wstring)
    {
        // todo: proper iconpath
        updateIconPath(utf8_encode(wstring));
        CoTaskMemFree(wstring);
        wstring = nullptr;
    }

    hr = m_pAudioSessionControl->GetState(&sessionState);
    CHECK_HRESULT(hr);
    updateState(from(sessionState));

    hr = m_pSimpleAudioVolume->GetMasterVolume(&sessionVolume);
    CHECK_HRESULT(hr);
    updateVolume(sessionVolume);

    hr = m_pSimpleAudioVolume->GetMute(&bSessionMuted);
    CHECK_HRESULT(hr);
    updateMute(bSessionMuted);

    hr = m_pAudioMeterInformation->GetPeakValue(&peak);
    CHECK_HRESULT(hr);
    updatePeakSample(peak);
}

WindowsAudioSession::~WindowsAudioSession()
{
    if (m_bAudioSessionEventsRegistered)
    {
        (void)m_pAudioSessionControl->UnregisterAudioSessionNotification(m_pAudioSessionEvents.get());
    }
}

void
WindowsAudioSession::changeVolume
(
    float volume
)
{
    LOCK_GUARD(m_mutex);
    CoInitializer com{};
    volume = std::min(1.0f, volume);
    volume = std::max(0.0f, volume);
    HRESULT hr = m_pSimpleAudioVolume->SetMasterVolume(volume, nullptr);
    CHECK_HRESULT(hr);
    updateVolume(volume);
}

void
WindowsAudioSession::changeMute
(
    bool bMute
)
{
    LOCK_GUARD(m_mutex);
    CoInitializer com{};
    HRESULT hr = m_pSimpleAudioVolume->SetMute(bMute, nullptr);
    CHECK_HRESULT(hr);
    updateMute(bMute);
}

void
WindowsAudioSession::peakSample()
{
    LOCK_GUARD(m_mutex);
    CoInitializer com{};
    float peak;
    HRESULT hr = m_pAudioMeterInformation->GetPeakValue(&peak);
    // CHECK_HRESULT(hr);
    if (FAILED(hr))
    {
        peak = 0.0f;
    }
    updatePeakSample(peak);
}

/* ==== CAudioSessionEvents Class ========================================== */
CAudioSessionEvents::CAudioSessionEvents
(
    WindowsAudioSession &parent
)
  : m_ref(1),
    m_parent(parent)
{
}
ULONG STDMETHODCALLTYPE
CAudioSessionEvents::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE
CAudioSessionEvents::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_ref);
    if (0 == ulRef)
    {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE CAudioSessionEvents::QueryInterface
(
    REFIID riid,
    VOID **ppvInterface
)
{
    if (IID_IUnknown == riid)
    {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if (__uuidof(IAudioSessionEvents) == riid)
    {
        AddRef();
        *ppvInterface = (IAudioSessionEvents*)this;
    }
    else
    {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionEvents::OnDisplayNameChanged
(
    LPCWSTR NewDisplayName,
    LPCGUID EventContext
)
{
    (void)EventContext;
    std::string name = utf8_encode(NewDisplayName);
    if (name.empty() || m_parent.m_bSystemsSoundSession)
    {
        name = windowName(m_parent.m_pid, m_parent.m_bSystemsSoundSession);
    }
    m_parent.updateName(name.empty() ? m_parent.m_id : name);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionEvents::OnIconPathChanged
(
    LPCWSTR NewIconPath,
    LPCGUID EventContext
)
{
    (void)EventContext;
    // todo: re-lookup actual icon path
    m_parent.updateIconPath(utf8_encode(NewIconPath));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionEvents::OnSimpleVolumeChanged
(
    float NewVolume,
    BOOL NewMute,
    LPCGUID EventContext
)
{
    (void)EventContext;
    m_parent.updateVolume(NewVolume);
    m_parent.updateMute(NewMute);
    return S_OK;

}

HRESULT STDMETHODCALLTYPE
CAudioSessionEvents::OnChannelVolumeChanged
(
    DWORD ChannelCount,
    float NewChannelVolumeArray[],
    DWORD ChangedChannel,
    LPCGUID EventContext
)
{
    // Ignoring for now. per-channel support not enabled yet.
    (void)ChannelCount;
    (void)NewChannelVolumeArray;
    (void)ChangedChannel;
    (void)EventContext;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionEvents::OnGroupingParamChanged
(
    LPCGUID NewGroupingParam,
    LPCGUID EventContext
)
{
    // Going against the grain here and ignoring Microsoft's heavy-handed
    // advice to control groups of sessions based on grouping parameter.
    // There is probably a better way to group sessions together (common
    // image/binary name perhaps) if the AudioDevice observer wants to.
    // https://learn.microsoft.com/en-us/windows/win32/coreaudio/grouping-parameters
    (void)NewGroupingParam;
    (void)EventContext;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionEvents::OnStateChanged
(
    AudioSessionState NewState
)
{
    m_parent.updateState(from(NewState));
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionEvents::OnSessionDisconnected
(
    AudioSessionDisconnectReason DisconnectReason
)
{
    // Ignoring for now. Letting the device's individual lifetime observers use
    // this for session cleanup.
    (void)DisconnectReason;
    return S_OK;
}

/* ==== WindowsAudioDevice Class =========================================== */
WindowsAudioDevice::WindowsAudioDevice
(
    IMMDevice *pMMDevice,
    bool bDefaultDevice
)
  : m_pMMDevice(pMMDevice, true),
    m_pAudioEndpointVolumeCallback(new CAudioEndpointVolumeCallback(*this), false),
    m_pAudioSessionNotification(new CAudioSessionNotification(*this), false),
    m_queuedWorkThread([this](const std::string &str){ killSession(str); })
{
    HRESULT     hr;
    PROPVARIANT varString;
    float       volume;
    BOOL        bMuted;
    float       peak;
    int         sessionCount;
    DWORD       state;
    LPWSTR      deviceId = nullptr;
    void*       ptr = nullptr;

    hr = m_pMMDevice->Activate(
        __uuidof(IAudioSessionManager2),
        CLSCTX_ALL,
        nullptr,
        &ptr);
    m_pAudioSessionManager2 = { (IAudioSessionManager2*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pAudioSessionManager2->GetSessionEnumerator((IAudioSessionEnumerator**)&ptr);
    m_pAudioSessionEnumerator = { (IAudioSessionEnumerator*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pAudioSessionManager2->RegisterSessionNotification(m_pAudioSessionNotification.get());
    CHECK_HRESULT(hr);
    m_bAudioSessionNotificationRegistered = true;

    hr = m_pAudioSessionEnumerator->GetCount(&sessionCount);
    CHECK_HRESULT(hr);

    hr = m_pMMDevice->Activate(
        __uuidof(IAudioEndpointVolume),
        CLSCTX_ALL,
        nullptr,
        &ptr);
    m_pAudioEndpointVolume = { (IAudioEndpointVolume*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pMMDevice->Activate(
        __uuidof(IAudioMeterInformation),
        CLSCTX_ALL,
        nullptr,
        &ptr);
    m_pAudioMeterInformation = { (IAudioMeterInformation*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pMMDevice->OpenPropertyStore(STGM_READ, (IPropertyStore**)&ptr);
    m_pPropertyStore = { (IPropertyStore*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pMMDevice->GetId(&deviceId);
    CHECK_HRESULT(hr);
    m_id = utf8_encode(deviceId);
    CoTaskMemFree(deviceId);
    deviceId = nullptr;

    hr = m_pPropertyStore->GetValue(PKEY_Device_DeviceDesc, &varString);
    CHECK_HRESULT(hr);
    updateName(utf8_encode(varString.pwszVal));

    hr = m_pAudioEndpointVolume->GetMasterVolumeLevelScalar(&volume);
    CHECK_HRESULT(hr);
    updateVolume(volume);

    hr = m_pAudioEndpointVolume->GetMute(&bMuted);
    CHECK_HRESULT(hr);
    updateMute(bMuted);

    hr = m_pAudioEndpointVolume->RegisterControlChangeNotify(m_pAudioEndpointVolumeCallback.get());
    CHECK_HRESULT(hr);
    m_bAudioEndpointVolumeCallbackRegistered = true;

    hr = m_pAudioMeterInformation->GetPeakValue(&peak);
    CHECK_HRESULT(hr);
    updatePeakSample(peak);

    // TODO: updateIconPath() maybe from EndpointFormFactor property?

    // TODO: Defer all session, volume, and peak information until device is in an active state
    hr = m_pMMDevice->GetState(&state);
    CHECK_HRESULT(hr);
    updateState(from(state));

    updateDefault(bDefaultDevice);

    for (int i = 0; i < sessionCount; i++)
    {
        IAudioSessionControl *pSessionControl = nullptr;

        m_pAudioSessionEnumerator->GetSession(i, &pSessionControl);
        SmartComPtr<IAudioSessionControl> cleanup{ pSessionControl, false };
        CHECK_HRESULT(hr);

        auto pWindowsAudioSession = std::make_shared<WindowsAudioSession>(pSessionControl);
        m_audioSessionsMirror[pWindowsAudioSession->getId()] = pWindowsAudioSession;
        addSession(pWindowsAudioSession->getId(), pWindowsAudioSession);

        auto pListener = SmartComPtr<CAudioSessionLifetimeObserver>(new CAudioSessionLifetimeObserver(*this, pWindowsAudioSession->getId(), pSessionControl), false);
        m_audioSessionLifetimeObservers.insert({pWindowsAudioSession->getId(), std::move(pListener)});
    }

    // m_sessionKillerThread = std::thread(&WindowsAudioDevice::sessionKiller, this);
}

WindowsAudioDevice::~WindowsAudioDevice()
{
    if (m_bAudioEndpointVolumeCallbackRegistered)
    {
        (void)m_pAudioEndpointVolume->UnregisterControlChangeNotify(m_pAudioEndpointVolumeCallback.get());
        m_bAudioEndpointVolumeCallbackRegistered = false;
    }

    if (m_bAudioSessionNotificationRegistered)
    {
        (void)m_pAudioSessionManager2->UnregisterSessionNotification(m_pAudioSessionNotification.get());
        m_bAudioSessionNotificationRegistered = false;
    }
}

void
WindowsAudioDevice::changeVolume
(
    float volume
)
{
    LOCK_GUARD(m_mutex);
    CoInitializer com{};
    volume = std::min(1.0f, volume);
    volume = std::max(0.0f, volume);
    HRESULT hr = m_pAudioEndpointVolume->SetMasterVolumeLevelScalar(volume, nullptr);
    CHECK_HRESULT(hr);
    updateVolume(volume);
}

void
WindowsAudioDevice::changeMute
(
    bool bMute
)
{
    LOCK_GUARD(m_mutex);
    CoInitializer com{};
    HRESULT hr = m_pAudioEndpointVolume->SetMute(bMute, nullptr);
    CHECK_HRESULT(hr);
    updateMute(bMute);
}

void
WindowsAudioDevice::markSessionForDeletion
(
    const std::string &audioSessionId
)
{
    std::string session = audioSessionId;
    m_queuedWorkThread.queue(std::move(session));
}

void
WindowsAudioDevice::killSession
(
    const std::string &sessionId
)
{
    LOCK_GUARD(m_mutex);

    try
    {
        // Delete any CAudioSessionLifetimeObserver that is already marked for deletion
        std::erase_if(m_audioSessionLifetimeObservers, [](const auto& item) {
            auto const& [key, value] = item;
            return value->isReadyForDeletion();
        });

        // Delete the session associated with the id
        m_audioSessionsMirror.erase(sessionId);
        removeSession(sessionId);

        //
        // Mark the CAudioSessionLifetimeObserver for deletion.
        // This is not deleted above in order to allow the observer to go dormant
        // (race condition between CAudioSessionLifetimeObserver::OnStateChanged()
        // and WindowsAudioDevice::sessionKiller())
        //
        m_audioSessionLifetimeObservers.at(sessionId)->markForDeletion();
    }
    catch(std::exception e)
    {
        // TODO Something with this in nice way
        // printf("Exception caught: %s\n", e.what());
        std::terminate();
    }
    catch(...)
    {
        // TODO something with this in a nice way
        // printf("Unknown exception type caught. %s:%d\n", __FILE__, __LINE__);
        std::terminate();
    }
}

void
WindowsAudioDevice::peakSample()
{
    LOCK_GUARD(m_mutex);
    CoInitializer com{};
    float peak;
    HRESULT hr = m_pAudioMeterInformation->GetPeakValue(&peak);
    CHECK_HRESULT(hr);
    updatePeakSample(peak);

    for (auto &entry : m_audioSessionsMirror)
    {
        entry.second->peakSample();
    }
}

/* ==== CAudioSessionLifetimeObserver Class ================================ */
CAudioSessionLifetimeObserver::CAudioSessionLifetimeObserver
(
    WindowsAudioDevice &parent,
    const std::string &audioSessionId,
    IAudioSessionControl *pAudioSessionControl
)
  : m_ref(1),
    m_parent(parent),
    m_id(audioSessionId),
    m_bReadyForDeletion(false),
    m_pAudioSessionControl{ pAudioSessionControl, true }
{
    HRESULT hr = m_pAudioSessionControl->RegisterAudioSessionNotification(this);
    CHECK_HRESULT(hr);
}

CAudioSessionLifetimeObserver::~CAudioSessionLifetimeObserver()
{
    (void)m_pAudioSessionControl->UnregisterAudioSessionNotification(this);
}

ULONG STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_ref);
    if (0 == ulRef)
    {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::QueryInterface
(
    REFIID riid,
    VOID **ppvInterface
)
{
    if (IID_IUnknown == riid)
    {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if (__uuidof(IAudioSessionEvents) == riid)
    {
        AddRef();
        *ppvInterface = (IAudioSessionEvents*)this;
    }
    else
    {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::OnDisplayNameChanged
(
    LPCWSTR NewDisplayName,
    LPCGUID EventContext
)
{
    // Ignoring, has nothing to do with AudioSession's lifetime
    (void)NewDisplayName;
    (void)EventContext;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::OnIconPathChanged
(
    LPCWSTR NewIconPath,
    LPCGUID EventContext
)
{
    // Ignoring, has nothing to do with AudioSession's lifetime
    (void)NewIconPath;
    (void)EventContext;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::OnSimpleVolumeChanged
(
    float NewVolume,
    BOOL NewMute,
    LPCGUID EventContext
)
{
    // Ignoring, has nothing to do with AudioSession's lifetime
    (void)NewVolume;
    (void)NewMute;
    (void)EventContext;
    return S_OK;

}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::OnChannelVolumeChanged
(
    // Ignoring, has nothing to do with AudioSession's lifetime
    DWORD ChannelCount,
    float NewChannelVolumeArray[],
    DWORD ChangedChannel,
    LPCGUID EventContext
)
{
    (void)ChannelCount;
    (void)NewChannelVolumeArray;
    (void)ChangedChannel;
    (void)EventContext;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::OnGroupingParamChanged
(
    // Ignoring, has nothing to do with AudioSession's lifetime
    LPCGUID NewGroupingParam,
    LPCGUID EventContext
)
{
    (void)NewGroupingParam;
    (void)EventContext;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::OnStateChanged
(
    AudioSessionState NewState
)
{
    if (from(NewState) == AudioSession::State::Expired)
    {
        m_parent.markSessionForDeletion(m_id);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionLifetimeObserver::OnSessionDisconnected
(
    AudioSessionDisconnectReason DisconnectReason
)
{
    (void)DisconnectReason;
    return S_OK;
}

void
CAudioSessionLifetimeObserver::markForDeletion()
{
    m_bReadyForDeletion = true;
}

bool
CAudioSessionLifetimeObserver::isReadyForDeletion() const
{
    return m_bReadyForDeletion;
}

/* ==== CAudioSessionNotification Class ==================================== */
CAudioSessionNotification::CAudioSessionNotification
(
    WindowsAudioDevice &parent
)
  : m_ref(1),
    m_parent(parent)
{
}

ULONG STDMETHODCALLTYPE
CAudioSessionNotification::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE
CAudioSessionNotification::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_ref);
    if (0 == ulRef)
    {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionNotification::QueryInterface
(
    REFIID riid,
    VOID **ppvInterface
)
{
    if (IID_IUnknown == riid)
    {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if (__uuidof(IAudioSessionNotification) == riid)
    {
        AddRef();
        *ppvInterface = (IAudioSessionNotification*)this;
    }
    else
    {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioSessionNotification::OnSessionCreated
(
    IAudioSessionControl *NewSession
)
{
    LOCK_GUARD(m_parent.m_mutex);
    CoInitializer com{};

    auto pWindowsAudioSession = std::make_shared<WindowsAudioSession>(NewSession);
    m_parent.m_audioSessionsMirror[pWindowsAudioSession->getId()] = pWindowsAudioSession;
    m_parent.addSession(pWindowsAudioSession->getId(), pWindowsAudioSession);


    auto pListener = SmartComPtr<CAudioSessionLifetimeObserver>(new CAudioSessionLifetimeObserver(m_parent, pWindowsAudioSession->getId(), NewSession), false);
    m_parent.m_audioSessionLifetimeObservers.insert({pWindowsAudioSession->getId(), std::move(pListener)});

    return S_OK;
}

/* ==== CAudioEndpointVolumeCallback Class ================================= */
CAudioEndpointVolumeCallback::CAudioEndpointVolumeCallback
(
    WindowsAudioDevice &parent
)
  : m_ref(1),
    m_parent(parent)
{
}

ULONG STDMETHODCALLTYPE
CAudioEndpointVolumeCallback::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE
CAudioEndpointVolumeCallback::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_ref);
    if (0 == ulRef)
    {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE
CAudioEndpointVolumeCallback::QueryInterface
(
    REFIID riid,
    VOID **ppvInterface
)
{
    if (IID_IUnknown == riid)
    {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if (__uuidof(IAudioEndpointVolumeCallback) == riid)
    {
        AddRef();
        *ppvInterface = (IAudioEndpointVolumeCallback*)this;
    }
    else
    {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CAudioEndpointVolumeCallback::OnNotify
(
    PAUDIO_VOLUME_NOTIFICATION_DATA pNotify
)
{
    if (pNotify)
    {
        m_parent.updateVolume(pNotify->fMasterVolume);
        m_parent.updateMute(pNotify->bMuted);
    }

    return S_OK;
}

/* ==== WindowsVolumeMixer Class =========================================== */
WindowsVolumeMixer::WindowsVolumeMixer()
  : m_pMMNotificationClient{new CMMNotificationClient(*this), false },
    m_peakSamplingThread([this](){peakSample();}, std::chrono::milliseconds(0))
{
    HRESULT hr;
    UINT    deviceCount;
    void*   ptr = nullptr;
    LPWSTR  wstring = nullptr;
    std::string defaultDeviceID;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        &ptr);
    m_pMMDeviceEnumerator = { (IMMDeviceEnumerator*)ptr, false };
    CHECK_HRESULT(hr);

    // TODO : re-evaluate including ALL devices, not just active ones. I think for proper onDeviceAdded callbacks, we'll want to track ALL devices...
    hr = m_pMMDeviceEnumerator->EnumAudioEndpoints(
        eRender,
        DEVICE_STATE_ACTIVE,
        (IMMDeviceCollection**)&ptr);
    m_pMMDeviceCollection = { (IMMDeviceCollection*)ptr, false };
    CHECK_HRESULT(hr);

    hr = m_pMMDeviceCollection->GetCount(&deviceCount);
    CHECK_HRESULT(hr);

    m_pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, (IMMDevice**)&ptr);
    SmartComPtr<IMMDevice> pDefaultMMDevice((IMMDevice*)ptr, false);
    CHECK_HRESULT(hr);

    hr = pDefaultMMDevice->GetId(&wstring);
    CHECK_HRESULT(hr);
    defaultDeviceID = utf8_encode(wstring);
    CoTaskMemFree(wstring);
    wstring = nullptr;

    for (UINT i = 0; i < deviceCount; i++)
    {
        bool        bDefaultDevice;
        IMMDevice  *pMMDevice = nullptr;

        hr = m_pMMDeviceCollection->Item(i, &pMMDevice);
        SmartComPtr<IMMDevice> cleanup(pMMDevice, false);
        CHECK_HRESULT(hr);

        hr = pMMDevice->GetId(&wstring);
        CHECK_HRESULT(hr);
        bDefaultDevice = (utf8_encode(wstring) == defaultDeviceID);
        CoTaskMemFree(wstring);
        wstring = nullptr;

        auto pWindowsAudioDevice = std::make_shared<WindowsAudioDevice>(pMMDevice, bDefaultDevice);
        addDevice(pWindowsAudioDevice->getId(), pWindowsAudioDevice);
        m_audioDevicesMirror[pWindowsAudioDevice->getId()] = pWindowsAudioDevice;
    }

    hr = m_pMMDeviceEnumerator->RegisterEndpointNotificationCallback(m_pMMNotificationClient.get());
    CHECK_HRESULT(hr);
    m_bNotificationClientRegistered = true;
}

WindowsVolumeMixer::~WindowsVolumeMixer()
{
    if (m_bNotificationClientRegistered)
    {
        (void)m_pMMDeviceEnumerator->UnregisterEndpointNotificationCallback(m_pMMNotificationClient.get());
        m_bNotificationClientRegistered = false;
    }
}

void
WindowsVolumeMixer::setPeakSamplingPeriod
(
    std::chrono::milliseconds period
)
{
    m_peakSamplingThread.changePeriod(period);
}

void
WindowsVolumeMixer::peakSample()
{
    try
    {
        LOCK_GUARD(m_mutex);
        for (auto &entry : m_audioDevicesMirror)
        {
            entry.second->peakSample();
        }
    }
    catch(std::exception e)
    {
        // TODO Something with this in nice way
        // printf("Exception caught: %s\n", e.what());
        std::terminate();
    }
    catch(...)
    {
        // TODO something with this in a nice way
        // printf("Unknown exception type caught. %s:%d\n", __FILE__, __LINE__);
        std::terminate();
    }
}

/* ==== CMMNotificationClient Class ======================================== */
CMMNotificationClient::CMMNotificationClient(WindowsVolumeMixer &parent)
  : m_ref(1),
    m_parent(parent)
{
}

ULONG STDMETHODCALLTYPE
CMMNotificationClient::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG STDMETHODCALLTYPE
CMMNotificationClient::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_ref);
    if (0 == ulRef)
    {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::QueryInterface
(
    REFIID riid,
    VOID **ppvInterface
)
{
    if (IID_IUnknown == riid)
    {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    }
    else if (__uuidof(IMMNotificationClient) == riid)
    {
        AddRef();
        *ppvInterface = (IMMNotificationClient*)this;
    }
    else
    {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::OnDefaultDeviceChanged
(
    EDataFlow flow,
    ERole role,
    LPCWSTR pwstrDeviceId
)
{
    if ((flow == eRender) &&
        (role == eConsole))
    {
        LOCK_GUARD(m_parent.m_mutex);
        std::string deviceId = utf8_encode(pwstrDeviceId);
        for (auto &entry : m_parent.m_audioDevicesMirror)
        {
            entry.second->updateDefault(deviceId == entry.first);
        }
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::OnDeviceAdded
(
    LPCWSTR pwstrDeviceId
)
{
    (void)pwstrDeviceId;

    // todo: revisit this when it can be tested.
    // Check that the device's endpoint flow is eRender before adding (IMMEndpoint::GetDataFlow)

    // HRESULT hr;
    // void* ptr = nullptr;
    // LPWSTR wstring = nullptr;
    // std::string defaultDeviceID;
    // bool bDefaultDevice;
    // IMMDevice *pMMDevice = nullptr;

    // fmt::print("{} bing", utf8_encode(pwstrDeviceId));

    // hr = m_parent.m_pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, (IMMDevice**)&ptr);
    // SmartComPtr<IMMDevice> pDefaultMMDevice((IMMDevice*)ptr, false);
    // CHECK_HRESULT(hr);

    // hr = pDefaultMMDevice->GetId(&wstring);
    // CHECK_HRESULT(hr);
    // defaultDeviceID = utf8_encode(wstring);
    // CoTaskMemFree(wstring);
    // wstring = nullptr;

    // hr = m_parent.m_pMMDeviceEnumerator->GetDevice(pwstrDeviceId, &pMMDevice);
    // SmartComPtr<IMMDevice> cleanup(pMMDevice, false);
    // CHECK_HRESULT(hr);

    // hr = pMMDevice->GetId(&wstring);
    // CHECK_HRESULT(hr);
    // bDefaultDevice = (utf8_encode(wstring) == defaultDeviceID);
    // CoTaskMemFree(wstring);
    // wstring = nullptr;


    // auto pWindowsAudioDevice = std::make_shared<WindowsAudioDevice>(pMMDevice, bDefaultDevice);
    // m_parent.addDevice(pWindowsAudioDevice->getId(), pWindowsAudioDevice);
    // m_parent.m_audioDevicesMirror[pWindowsAudioDevice->getId()] = pWindowsAudioDevice;

    // fmt::print("  -->Added device\n");
    return S_OK;
};

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::OnDeviceRemoved
(
    LPCWSTR pwstrDeviceId
)
{
    LOCK_GUARD(m_parent.m_mutex);
    std::string deviceId = utf8_encode(pwstrDeviceId);

    if (m_parent.m_audioDevicesMirror.contains(deviceId))
    {
        m_parent.m_audioDevicesMirror.erase(deviceId);
    }
    m_parent.removeDevice(deviceId);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::OnDeviceStateChanged
(
    LPCWSTR pwstrDeviceId,
    DWORD dwNewState
)
{
    LOCK_GUARD(m_parent.m_mutex);
    std::string deviceId = utf8_encode(pwstrDeviceId);

    if (m_parent.m_audioDevicesMirror.contains(deviceId))
    {
        m_parent.m_audioDevicesMirror[deviceId]->updateState(from(dwNewState));
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE
CMMNotificationClient::OnPropertyValueChanged
(
    LPCWSTR pwstrDeviceId,
    const PROPERTYKEY key
)
{
    LOCK_GUARD(m_parent.m_mutex);
    std::string deviceId = utf8_encode(pwstrDeviceId);

    // Note: this should be compared against PKEY_Device_DeviceDesc, but that isn't as trivial as I'd like :(
    (void)key;

    if (m_parent.m_audioDevicesMirror.contains(deviceId))
    {
        CoInitializer com{};
        PROPVARIANT varString;
        HRESULT hr = m_parent.m_audioDevicesMirror[deviceId]->m_pPropertyStore->GetValue(PKEY_Device_DeviceDesc, &varString);
        CHECK_HRESULT(hr);
        std::string name = utf8_encode(varString.pwszVal);
        m_parent.m_audioDevicesMirror[deviceId]->updateName(name);
    }
    return S_OK;
}

} // namespace vmx

/* ==== Static Helper Functions ============================================ */
static std::string
utf8_encode
(
    const std::wstring &wstr
)
{
    if( wstr.empty() ) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo( size_needed, 0 );
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

static std::string
windowName
(
    unsigned int processId,
    bool bSystemsSoundSession
)
{
    std::string ret = "";

    struct LANGANDCODEPAGE
    {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;

    if (bSystemsSoundSession)
    {
        return "System Sounds";
    }

    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION , FALSE, processId);
    if (!handle)
    {
        return "";
    }
    char pszFile[MAX_PATH] = "";
    DWORD len = MAX_PATH;
    QueryFullProcessImageNameA(handle, 0, pszFile, &len);
    ret = std::string(pszFile);
    UINT dwBytes, cbTranslate;
    DWORD dwSize = GetFileVersionInfoSizeA(pszFile, (DWORD*)&dwBytes);
    if (dwSize == 0)
    {
        return ret;
    }
    LPVOID lpData = (LPVOID)malloc(dwSize);
    ZeroMemory(lpData, dwSize);
    if (GetFileVersionInfoA(pszFile, 0, dwSize, lpData))
    {
        VerQueryValueA(lpData,
            "\\VarFileInfo\\Translation",
            (LPVOID*)&lpTranslate,
            &cbTranslate);
        char strSubBlock[MAX_PATH] = { 0 };
        char* lpBuffer;

        for (int i = 0; i < (cbTranslate / sizeof(struct LANGANDCODEPAGE)); i++)
        {
            StringCchPrintfA(strSubBlock,50,
                "\\StringFileInfo\\%04x%04x\\FileDescription",
                lpTranslate[i].wLanguage,
                lpTranslate[i].wCodePage);
            VerQueryValueA(lpData,
                strSubBlock,
                (void**)&lpBuffer,
                &dwBytes);
            ret = std::string(lpBuffer);
        }
    }
    if (lpData) free(lpData);
    if (handle) CloseHandle(handle);
    return ret;
}


static vmx::AudioSession::State
from
(
    AudioSessionState state
)
{
    switch (state)
    {
        case AudioSessionStateInactive: return vmx::AudioSession::State::Inactive;
        case AudioSessionStateActive:   return vmx::AudioSession::State::Active;
        case AudioSessionStateExpired:  return vmx::AudioSession::State::Expired;
        default:                        return vmx::AudioSession::State::Unknown;
    }
}

static vmx::AudioDevice::State
from
(
    DWORD state
)
{
    switch (state)
    {
        case DEVICE_STATE_ACTIVE:       return vmx::AudioDevice::State::Active;
        case DEVICE_STATE_DISABLED:     return vmx::AudioDevice::State::Disabled;
        case DEVICE_STATE_NOTPRESENT:   return vmx::AudioDevice::State::NotPresent;
        case DEVICE_STATE_UNPLUGGED:    return vmx::AudioDevice::State::Unplugged;
        default:                        return vmx::AudioDevice::State::Unknown;
    }
}
