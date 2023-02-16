#pragma once

/* ==== Standard Library Includes ========================================== */
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace vmx
{

/* ==== Classes ============================================================ */
class AudioSession
{
public: /* Enums */
    enum class State
    {
        Active,
        Inactive,
        Expired,
        Unknown,
    };

public: /* Classes */
    class Observer
    {
    public: /* Virtual Methods */
        virtual void onNameChange(std::string name) = 0;
        virtual void onIconPathChange(std::string iconPath) = 0;
        virtual void onStateChange(State state) = 0;
        virtual void onVolumeChange(float volume) = 0;
        virtual void onMuteChange(bool bMuted) = 0;
        virtual void onPeakSample(float peak) = 0;
    };

public: /* Methods */
    AudioSession() = default;
    void addObserver(std::shared_ptr<Observer> pObserver, bool bNotifyNow);
    void removeObserver(std::shared_ptr<Observer> pObserver);

public: /* Virtual Methods */
    virtual ~AudioSession() = default;
    virtual void changeVolume(float volume) = 0;
    virtual void changeMute(bool bMuted) = 0;

protected: /* Methods */
    void updateName(std::string name);
    void updateIconPath(std::string iconPath);
    void updateState(State state);
    void updateVolume(float volume);
    void updateMute(bool bMuted);
    void updatePeakSample(float peak);

private: /* Members */
    std::recursive_mutex m_mutex;
    std::string m_name = "";
    std::string m_iconPath = "";
    State m_state = State::Unknown;
    float m_volume = 0.0f;
    bool m_bMuted = false;
    float m_peak = 0.0f;
    std::vector<std::weak_ptr<Observer>> m_observers;
};

class AudioDevice
{
public: /* Enums */
    enum class State
    {
        Active,
        Disabled,
        NotPresent,
        Unplugged,
        Unknown,
    };

public: /* Classes */
    class Observer
    {
    public: /* Virtual Methods */
        virtual void onNameChange(std::string name) = 0;
        virtual void onIconPathChange(std::string iconPath) = 0;
        virtual void onStateChange(State state) = 0;
        virtual void onDefaultChange(bool bIsDefaultDevice) = 0;
        virtual void onVolumeChange(float volume) = 0;
        virtual void onMuteChange(bool bMuted) = 0;
        virtual void onPeakSample(float peak) = 0;
        virtual void onAudioSessionAdded(const std::string &audioSessionId, std::weak_ptr<AudioSession> pAudioSession) = 0;
        virtual void onAudioSessionRemoved(const std::string &audioSessionId) = 0;
    };

public: /* Methods */
    AudioDevice() = default;
    void addObserver(std::shared_ptr<Observer> pObserver, bool bNotifyNow);
    void removeObserver(std::shared_ptr<Observer> pObserver);

public: /* Virtual Methods */
    virtual ~AudioDevice() = default;
    virtual void changeVolume(float volume) = 0;
    virtual void changeMute(bool bMuted) = 0;

protected: /* Methods */
    void updateName(std::string name);
    void updateIconPath(std::string iconPath);
    void updateState(State state);
    void updateDefault(bool bIsDefaultDevice);
    void updateVolume(float volume);
    void updateMute(bool bMuted);
    void updatePeakSample(float peak);
    void addSession(const std::string &audioSessionId, std::shared_ptr<AudioSession> pAudioSession);
    void removeSession(const std::string &audioSessionId);

private: /* Members */
    std::recursive_mutex m_mutex;
    std::string m_name = "";
    std::string m_iconPath = "";
    State m_state = State::Unknown;
    bool m_bIsDefaultDevice = false;
    float m_volume = 0.0f;
    bool m_bMuted = false;
    float m_peak = 0.0f;
    std::vector<std::weak_ptr<Observer>> m_observers;
    std::map<std::string /*audioSessionId*/, std::shared_ptr<AudioSession>> m_audioSessions;
};

class VolumeMixer
{
public: /* Classes */
    class Observer
    {
    public: /* Virtual Methods */
        virtual void onAudioDeviceAdded(const std::string &audioDeviceId, std::weak_ptr<AudioDevice> pAudioDevice) = 0;
        virtual void onAudioDeviceRemoved(const std::string &audioDeviceId) = 0;
    };

public: /* Methods */
    VolumeMixer() = default;
    void addObserver(std::shared_ptr<Observer> pObserver, bool bNotifyNow);
    void removeObserver(std::shared_ptr<Observer> pObserver);

public: /* Virtual Methods */
    virtual ~VolumeMixer() = default;
    virtual void setPeakSamplingPeriod(std::chrono::milliseconds period) = 0;

protected: /* Methods */
    void addDevice(const std::string &audioDeviceId, std::shared_ptr<AudioDevice> pAudioDevice);
    void removeDevice(const std::string &audioDeviceId);

private: /* Members */
    std::mutex m_mutex;
    std::vector<std::weak_ptr<Observer>> m_observers;
    std::map<std::string /*audioDeviceId*/, std::shared_ptr<AudioDevice>> m_audioDevices;
};

} // namespace vmx