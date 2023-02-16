#pragma once

/* ==== VMX Includes ======================================================= */
#include <vmx/VolumeMixer.h>

/* ==== Standard Library Includes ========================================== */
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

/* ==== Open Source Includes =============================================== */
#include <ftxui/component/screen_interactive.hpp>

/* ==== Classes ============================================================ */
class FTXUIAudioSessionObserver : public AudioSession::Observer
{
public: /* Methods */
    FTXUIAudioSessionObserver(const std::string &sessionId, std::weak_ptr<AudioSession> pAudioSession, std::function<void(void)> updateScreenFunc);
    ftxui::Component getRenderer() { return m_renderer; };

public: /* Virtual Methods */
    virtual ~FTXUIAudioSessionObserver();
    virtual void onNameChange(std::string name) override;
    virtual void onIconPathChange(std::string iconPath) override;
    virtual void onStateChange(AudioSession::State state) override;
    virtual void onVolumeChange(float volume) override;
    virtual void onMuteChange(bool bMuted) override;
    virtual void onPeakSample(float peak) override;

private:
    std::recursive_mutex m_mutex;
    std::function<void(void)> m_updateScreenFunc;
    ftxui::Component m_component;
    ftxui::Component m_renderer;
    ftxui::Component m_volumeSlider;
    ftxui::Component m_mutedCheckbox;
    std::weak_ptr<AudioSession> m_pAudioSession;
    std::string m_sessionId = "";
    std::string m_name;
    std::string m_iconPath;
    AudioSession::State m_state;
    float m_volume;
    float m_volumeControl;
    bool m_bMuted;
    float m_peak;
};

class FTXUIAudioDeviceObserver : public AudioDevice::Observer
{
public: /* Methods */
    FTXUIAudioDeviceObserver(const std::string &deviceId, std::weak_ptr<AudioDevice> pAudioDevice, std::function<void(void)> updateScreenFunc);
    ftxui::Component getRenderer() { return m_renderer; };
    ftxui::Component getMenuEntry() { return m_menuEntry; };
    bool isDefaultDevice() const { return m_bIsDefaultDevice; };

public: /* Virtual Methods */
    virtual ~FTXUIAudioDeviceObserver();
    virtual void onNameChange(std::string name) override;
    virtual void onIconPathChange(std::string iconPath) override;
    virtual void onStateChange(AudioDevice::State state) override;
    virtual void onDefaultChange(bool bIsDefaultDevice) override;
    virtual void onVolumeChange(float volume) override;
    virtual void onMuteChange(bool bMuted) override;
    virtual void onPeakSample(float peak) override;
    virtual void onAudioSessionAdded(const std::string &audioSessionId, std::weak_ptr<AudioSession> pAudioSession) override;
    virtual void onAudioSessionRemoved(const std::string &audioSessionId) override;

private:
    std::recursive_mutex m_mutex;
    std::function<void(void)> m_updateScreenFunc;
    ftxui::Component m_allComponents;
    ftxui::Component m_renderer;
    ftxui::Component m_volumeSlider;
    ftxui::Component m_mutedCheckbox;
    ftxui::Component m_menuEntry;
    std::weak_ptr<AudioDevice> m_pAudioDevice;
    std::map<std::string, std::shared_ptr<FTXUIAudioSessionObserver>> m_audioSessionObservers;
    std::string m_deviceId = "";
    std::string m_name = "";
    std::string m_iconPath = "";
    AudioDevice::State m_state = AudioDevice::State::Unknown;
    bool m_bIsDefaultDevice = false;
    float m_volume = 0.f;
    float m_volumeControl = 0.f;
    bool m_bMuted = false;
    float m_peak = 0.f;
};

class FTXUIVolumeMixerObserver : public VolumeMixer::Observer
{
public: /* Methods */
    FTXUIVolumeMixerObserver(ftxui::ScreenInteractive &screen);
    ftxui::Component getRenderer() { return m_renderer; };

public: /* Virtual Methods */
    virtual ~FTXUIVolumeMixerObserver();
    virtual void onAudioDeviceAdded(const std::string &audioDeviceId, std::weak_ptr<AudioDevice> pAudioDevice) override;
    virtual void onAudioDeviceRemoved(const std::string &audioDeviceId) override;

private: /* Methods */
    void updateScreen() { if (m_bCanUpdateScreen) { m_screen.PostEvent(ftxui::Event::Custom); }};

private:
    std::atomic<bool> m_bCanUpdateScreen = false;
    ftxui::ScreenInteractive &m_screen;
    int m_menuSelection;
    ftxui::Component m_menu;
    ftxui::Component m_component;
    ftxui::Component m_tabs;
    ftxui::Component m_renderer;
    std::map<std::string, std::shared_ptr<FTXUIAudioDeviceObserver>> m_audioDeviceObservers;
};
