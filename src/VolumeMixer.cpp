/* ==== Application Includes =============================================== */
#include <vmx/VolumeMixer.h>

/* ==== Standard Library Includes ========================================== */
#include <algorithm>
#include <thread>

/* ==== Macros ============================================================= */
// TODO: Investigate *not* using a detached thread here for every update...
//       This hack was originally added to WAR a re-entrant thread from device endpoint volume notification
#define FOR_EACH_OBSERVER_CALL_METHOD(observers, method, ...)       \
    do                                                              \
    {                                                               \
        for (auto it = begin((observers)); it != end((observers));) \
        {                                                           \
            if (auto sptr = it->lock())                             \
            {                                                       \
                std::thread t([=]{sptr->method(__VA_ARGS__);});     \
                t.detach();                                         \
                ++it;                                               \
            }                                                       \
            else                                                    \
            {                                                       \
                it = (observers).erase(it);                         \
            }                                                       \
        }                                                           \
    } while (false)

#define LOCK_GUARD(mutex_var) const std::lock_guard<decltype(mutex_var)> lock(mutex_var)

namespace vmx
{

/* ==== AudioSesssion Methods ============================================== */
void
AudioSession::addObserver
(
    std::shared_ptr<AudioSession::Observer> pObserver,
    bool bNotifyNow
)
{
    LOCK_GUARD(m_mutex);
    std::weak_ptr<AudioSession::Observer> ptr = pObserver;

    auto is_equal =
        [&](const std::weak_ptr<Observer>& wptr)
        {
            return wptr.lock() == pObserver;
        };

    if (std::find_if(m_observers.begin(), m_observers.end(), is_equal) == m_observers.end())
    {
        m_observers.push_back(ptr);
    }

    if (bNotifyNow)
    {
        pObserver->onNameChange(m_name);
        pObserver->onIconPathChange(m_iconPath);
        pObserver->onStateChange(m_state);
        pObserver->onVolumeChange(m_volume);
        pObserver->onMuteChange(m_bMuted);
        pObserver->onPeakSample(m_peak);
    }
}

void
AudioSession::removeObserver
(
    std::shared_ptr<AudioSession::Observer> pObserver
)
{
    LOCK_GUARD(m_mutex);
    m_observers.erase(
        std::remove_if(
            m_observers.begin(),
            m_observers.end(),
            [&](const std::weak_ptr<AudioSession::Observer>& wptr)
            {
                return wptr.expired() || wptr.lock() == pObserver;
            }
        ),
        m_observers.end()
    );
}

void
AudioSession::updateName
(
    std::string name
)
{
    LOCK_GUARD(m_mutex);
    if (m_name == name) return;
    m_name = name;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onNameChange, name);
}

void
AudioSession::updateIconPath
(
    std::string iconPath
)
{
    LOCK_GUARD(m_mutex);
    if (m_iconPath == iconPath) return;
    m_iconPath = iconPath;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onIconPathChange, iconPath);
}

void
AudioSession::updateState
(
    AudioSession::State state
)
{
    LOCK_GUARD(m_mutex);
    if (m_state == state) return;
    m_state = state;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onStateChange, state);
}

void
AudioSession::updateVolume
(
    float volume
)
{
    LOCK_GUARD(m_mutex);
    if (m_volume == volume) return;
    m_volume = volume;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onVolumeChange, volume);
}

void
AudioSession::updateMute
(
    bool bMuted
)
{
    LOCK_GUARD(m_mutex);
    if (m_bMuted == bMuted) return;
    m_bMuted = bMuted;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onMuteChange, bMuted);
}

void
AudioSession::updatePeakSample
(
    float peak
)
{
    LOCK_GUARD(m_mutex);
    if (m_peak == peak) return;
    m_peak = peak;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onPeakSample, peak);
}

/* ==== AudioDevice Methods ================================================ */
void
AudioDevice::addObserver
(
    std::shared_ptr<AudioDevice::Observer> pObserver,
    bool bNotifyNow
)
{
    LOCK_GUARD(m_mutex);
    std::weak_ptr<AudioDevice::Observer> ptr = pObserver;

    auto is_equal =
        [&](const std::weak_ptr<AudioDevice::Observer>& wptr)
        {
            return wptr.lock() == pObserver;
        };

    if (std::find_if(m_observers.begin(), m_observers.end(), is_equal) == m_observers.end())
    {
        m_observers.push_back(ptr);
    }

    if (bNotifyNow)
    {
        pObserver->onNameChange(m_name);
        pObserver->onIconPathChange(m_iconPath);
        pObserver->onStateChange(m_state);
        pObserver->onDefaultChange(m_bIsDefaultDevice);
        pObserver->onVolumeChange(m_volume);
        pObserver->onMuteChange(m_bMuted);
        pObserver->onPeakSample(m_peak);

        for (const auto &entry : m_audioSessions)
        {
            pObserver->onAudioSessionAdded(entry.first, entry.second);
        }
    }
}

void
AudioDevice::removeObserver
(
    std::shared_ptr<AudioDevice::Observer> pObserver
)
{
    LOCK_GUARD(m_mutex);
    m_observers.erase(
        std::remove_if(
            m_observers.begin(),
            m_observers.end(),
            [&](const std::weak_ptr<AudioDevice::Observer>& wptr)
            {
                return wptr.expired() || wptr.lock() == pObserver;
            }
        ),
        m_observers.end()
    );
}

void
AudioDevice::updateName
(
    std::string name
)
{
    LOCK_GUARD(m_mutex);
    if (m_name == name) return;
    m_name = name;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onNameChange, name);
}

void
AudioDevice::updateIconPath
(
    std::string iconPath
)
{
    LOCK_GUARD(m_mutex);
    if (m_iconPath == iconPath) return;
    m_iconPath = iconPath;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onIconPathChange, iconPath);

}

void
AudioDevice::updateState
(
    State state
)
{
    LOCK_GUARD(m_mutex);
    if (m_state == state) return;
    m_state = state;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onStateChange, state);
}

void
AudioDevice::updateDefault
(
    bool bIsDefaultDevice
)
{
    LOCK_GUARD(m_mutex);
    if (m_bIsDefaultDevice == bIsDefaultDevice) return;
    m_bIsDefaultDevice = bIsDefaultDevice;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onDefaultChange, bIsDefaultDevice);
}

void
AudioDevice::updateVolume
(
    float volume
)
{
    LOCK_GUARD(m_mutex);
    if (m_volume == volume) return;
    m_volume = volume;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onVolumeChange, volume);
}

void
AudioDevice::updateMute
(
    bool bMuted
)
{
    LOCK_GUARD(m_mutex);
    if (m_bMuted == bMuted) return;
    m_bMuted = bMuted;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onMuteChange, bMuted);
}

void
AudioDevice::updatePeakSample
(
    float peak
)
{
    LOCK_GUARD(m_mutex);
    if (m_peak == peak) return;
    m_peak = peak;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onPeakSample, peak);
}

void
AudioDevice::addSession
(
    const std::string &audioSessionId,
    std::shared_ptr<AudioSession> pAudioSession
)
{
    LOCK_GUARD(m_mutex);
    m_audioSessions[audioSessionId] = pAudioSession;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onAudioSessionAdded, audioSessionId, pAudioSession);
}

void
AudioDevice::removeSession
(
    const std::string &audioSessionId
)
{
    LOCK_GUARD(m_mutex);
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onAudioSessionRemoved, audioSessionId);
    m_audioSessions.erase(audioSessionId);
}

/* ==== VolumeMixer Methods ================================================ */
void
VolumeMixer::addObserver
(
    std::shared_ptr<VolumeMixer::Observer> pObserver,
    bool bNotifyNow
)
{
    LOCK_GUARD(m_mutex);
    std::weak_ptr<VolumeMixer::Observer> ptr = pObserver;

    auto is_equal =
        [&](const std::weak_ptr<VolumeMixer::Observer>& wptr)
        {
            return wptr.lock() == pObserver;
        };

    if (std::find_if(m_observers.begin(), m_observers.end(), is_equal) == m_observers.end())
    {
        m_observers.push_back(ptr);
    }

    if (bNotifyNow)
    {
        for (const auto &entry : m_audioDevices)
        {
            pObserver->onAudioDeviceAdded(entry.first, entry.second);
        }
    }
}

void
VolumeMixer::removeObserver
(
    std::shared_ptr<VolumeMixer::Observer> pObserver
)
{
    LOCK_GUARD(m_mutex);
    m_observers.erase(
        std::remove_if(
            m_observers.begin(),
            m_observers.end(),
            [&](const std::weak_ptr<VolumeMixer::Observer>& wptr)
            {
                return wptr.expired() || wptr.lock() == pObserver;
            }
        ),
        m_observers.end()
    );
}

void
VolumeMixer::addDevice
(
    const std::string &audioDeviceId,
    std::shared_ptr<AudioDevice> pAudioDevice
)
{
    LOCK_GUARD(m_mutex);
    m_audioDevices[audioDeviceId] = pAudioDevice;
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onAudioDeviceAdded, audioDeviceId, pAudioDevice);
}

void
VolumeMixer::removeDevice
(
    const std::string &audioDeviceId
)
{
    LOCK_GUARD(m_mutex);
    FOR_EACH_OBSERVER_CALL_METHOD(m_observers, onAudioDeviceRemoved, audioDeviceId);
    m_audioDevices.erase(audioDeviceId);
}

} // namespace vmx
