/* ==== Application Includes =============================================== */
#include "FTXUIVolumeMixerObserver.h"

/* ==== Standard Library Includes ========================================== */
#include <chrono>
using namespace std::chrono_literals;

/* ==== Open Source Includes =============================================== */
#include <fmt/core.h>
#include <ftxui/component/component.hpp>

/* ==== Macros ============================================================= */
#define LOCK_GUARD(mutex_var) const std::lock_guard<decltype(mutex_var)> lock(mutex_var)

/* ==== Helper Functions =================================================== */
static unsigned int percent(float value)
{
    return std::max(0U, std::min(100U, (unsigned int)((value + 0.005f) * 100)));
}

static std::string percentStr(float value)
{
    return fmt::format("{:3}%", percent(value));
}

/* ==== FTXUIAudioSessionObserver Methods ================================== */
FTXUIAudioSessionObserver::FTXUIAudioSessionObserver
(
    const std::string &sessionId,
    std::weak_ptr<AudioSession> pAudioSession,
    std::function<void(void)> updateScreenFunc
)
  : m_sessionId(sessionId),
    m_pAudioSession(pAudioSession),
    m_updateScreenFunc(updateScreenFunc)
{
    auto cOption = ftxui::CheckboxOption::Simple();
    cOption.on_change = [this]()
    {
        if (auto ptr = m_pAudioSession.lock())
        {
            ptr->changeMute(m_bMuted);
        }
    };

    m_mutedCheckbox = ftxui::Checkbox("", &m_bMuted, cOption);
    m_volumeControl = 0;
    m_volumeSlider = ftxui::Slider("", &m_volumeControl, 0.0f, 1.0f, 0.01f);
    m_component = ftxui::Container::Vertical({m_volumeSlider, m_mutedCheckbox});

    m_renderer = ftxui::Renderer(m_component, [this]
    {
        LOCK_GUARD(m_mutex);

        // Indicates that TUI changed the volume, we need to update volume mixer state
        if (percent(m_volumeControl) != percent(m_volume))
        {
            if (auto ptr = m_pAudioSession.lock())
            {
                ptr->changeVolume(m_volumeControl);
            }
        }

        return ftxui::window(ftxui::text(m_name) | ftxui::bold | ftxui::color(ftxui::Color::Green),
            ftxui::vbox(
            {
                ftxui::hbox({ftxui::text("Peak:   ["), ftxui::gauge(m_peak), ftxui::text("] " + percentStr(m_peak))}),
                ftxui::hbox({ftxui::text("Volume: "), m_volumeSlider->Render(), ftxui::text(" " + percentStr(m_volumeControl))}),
                ftxui::hbox({ftxui::text("Mute:   "), m_mutedCheckbox->Render()}),
            }) | ftxui::xflex);
    });
}

FTXUIAudioSessionObserver::~FTXUIAudioSessionObserver()
{
    m_renderer->Detach();
    m_renderer->DetachAllChildren();
    m_component->Detach();
    m_component->DetachAllChildren();
}

void
FTXUIAudioSessionObserver::onNameChange
(
    std::string name
)
{
    {
        LOCK_GUARD(m_mutex);
        m_name = name;
    }
    m_updateScreenFunc();
}

void
FTXUIAudioSessionObserver::onIconPathChange
(
    std::string iconPath
)
{
    {
        LOCK_GUARD(m_mutex);
        m_iconPath = iconPath;
    }
    m_updateScreenFunc();
}

void
FTXUIAudioSessionObserver::onStateChange
(
    AudioSession::State state
)
{
    {
        LOCK_GUARD(m_mutex);
        m_state = state;
    }
    m_updateScreenFunc();
}

void
FTXUIAudioSessionObserver::onVolumeChange
(
    float volume
)
{
    bool bUpdateScreen;

    {
        LOCK_GUARD(m_mutex);
        m_volume = volume;
        if (percent(m_volumeControl) != percent(m_volume))
        {
            // Indicates that volume change did not come from TUI so we must update it
            m_volumeControl = m_volume;
            bUpdateScreen = true;
        }
        else
        {
            // TUI state already reflects updated volume, no need to update
            bUpdateScreen = false;
        }
    }
    if (bUpdateScreen)
    {
        m_updateScreenFunc();
    }
}

void
FTXUIAudioSessionObserver::onMuteChange
(
    bool bMuted
)
{
    {
        LOCK_GUARD(m_mutex);
        m_bMuted = bMuted;
    }
    m_updateScreenFunc();
}

void
FTXUIAudioSessionObserver::onPeakSample
(
    float peak
)
{
    {
        LOCK_GUARD(m_mutex);
        m_peak = peak;
    }
    m_updateScreenFunc();
}

/* ==== FTXUIAudioDeviceObserver Methods =================================== */
FTXUIAudioDeviceObserver::FTXUIAudioDeviceObserver
(
    const std::string &deviceId,
    std::weak_ptr<AudioDevice> pAudioDevice,
    std::function<void(void)> updateScreenFunc
)
  : m_deviceId(deviceId),
    m_pAudioDevice(pAudioDevice),
    m_updateScreenFunc(updateScreenFunc)
{
    auto cOption = ftxui::CheckboxOption::Simple();
    cOption.on_change = [this]()
    {
        if (auto ptr = m_pAudioDevice.lock())
        {
            ptr->changeMute(m_bMuted);
        }
    };

    m_mutedCheckbox = ftxui::Checkbox("", &m_bMuted, cOption);
    m_volumeControl = 0;
    m_volumeSlider = ftxui::Slider("", &m_volumeControl, 0.0f, 1.0f, 0.01f);
    auto deviceComponents = ftxui::Container::Vertical({m_volumeSlider, m_mutedCheckbox});

    auto deviceRender = ftxui::Renderer(deviceComponents, [&]()
    {
        return ftxui::vbox(
            {
                ftxui::hbox({ftxui::text("Peak:   ["), ftxui::gauge(m_peak), ftxui::text("]  " + percentStr(m_peak))}),
                ftxui::hbox({ftxui::text("Volume: "), m_volumeSlider->Render(), ftxui::text("  " + percentStr(m_volumeControl))}),
                ftxui::hbox({ftxui::text("Mute:   "), m_mutedCheckbox->Render()}),
            });
    });

    m_allComponents = ftxui::Container::Vertical({deviceRender});

    m_renderer = ftxui::Renderer(m_allComponents, [this]
    {
        LOCK_GUARD(m_mutex);

        // Indicates that TUI changed the volume, we need to update volume mixer state
        if (percent(m_volumeControl) != percent(m_volume))
        {
            if (auto ptr = m_pAudioDevice.lock())
            {
                ptr->changeVolume(m_volumeControl);
            }
        }
        std::string defaultStr = m_bIsDefaultDevice ? " (default)" : "";

        return ftxui::vbox(
            {
                ftxui::separatorDouble(),
                m_allComponents->Render()
            }) | ftxui::xflex;
    });

    ftxui::MenuEntryOption op = {};

    op.transform = [this](const ftxui::EntryState& state)
    {
        bool highlight = state.active || state.state || state.focused;
        std::string defaultStr = m_bIsDefaultDevice ? " (default)" : "";
        std::string label = (highlight ? " [" : "  ") + m_name + defaultStr + (highlight ? "] " : "  ");
        ftxui::Element e = ftxui::text(label);
        if (state.focused)
        {
            e = e | ftxui::inverted;
        }
        if (state.active)
        {
            e = e | ftxui::bold;
        }
        return e;
    };

    m_menuEntry = ftxui::MenuEntry("", op);

}

FTXUIAudioDeviceObserver::~FTXUIAudioDeviceObserver()
{
    m_renderer->Detach();
    m_renderer->DetachAllChildren();
    m_allComponents->Detach();
    m_allComponents->DetachAllChildren();
    m_menuEntry->Detach();
    m_menuEntry->DetachAllChildren();
}

void FTXUIAudioDeviceObserver::onNameChange
(
    std::string name
)
{
    {
        LOCK_GUARD(m_mutex);
        m_name = name;
    }
    m_updateScreenFunc();
}

void FTXUIAudioDeviceObserver::onIconPathChange
(
    std::string iconPath
)
{
    {
        LOCK_GUARD(m_mutex);
        m_iconPath = iconPath;
    }
    m_updateScreenFunc();
}

void FTXUIAudioDeviceObserver::onStateChange
(
    AudioDevice::State state
)
{
    {
        LOCK_GUARD(m_mutex);
        m_state = state;
    }
    m_updateScreenFunc();
}

void FTXUIAudioDeviceObserver::onDefaultChange
(
    bool bIsDefaultDevice
)
{
    {
        LOCK_GUARD(m_mutex);
        m_bIsDefaultDevice = bIsDefaultDevice;
    }
    m_updateScreenFunc();
}

void FTXUIAudioDeviceObserver::onVolumeChange
(
    float volume
)
{
    bool bUpdateScreen;
    {
        LOCK_GUARD(m_mutex);
        m_volume = volume;
        if (percent(m_volumeControl) != percent(m_volume))
        {
            // Indicates that volume change did not come from TUI so we must update it
            m_volumeControl = m_volume;
            bUpdateScreen = true;
        }
        else
        {
            // TUI state already reflects updated volume, no need to update
            bUpdateScreen = false;
        }
    }
    if (bUpdateScreen)
    {
        m_updateScreenFunc();
    }
}

void FTXUIAudioDeviceObserver::onMuteChange
(
    bool bMuted
)
{
    {
        LOCK_GUARD(m_mutex);
        m_bMuted = bMuted;
    }
    m_updateScreenFunc();
}

void FTXUIAudioDeviceObserver::onPeakSample
(
    float peak
)
{
    {
        LOCK_GUARD(m_mutex);
        m_peak = peak;
    }
    m_updateScreenFunc();
}

void FTXUIAudioDeviceObserver::onAudioSessionAdded
(
    const std::string &audioSessionId, std::weak_ptr<AudioSession> pAudioSession
)
{
    if (auto ptr = pAudioSession.lock())
    {
        auto pObserver = std::make_shared<FTXUIAudioSessionObserver>(audioSessionId, pAudioSession, m_updateScreenFunc);
        ptr->addObserver(pObserver, true);
        m_audioSessionObservers[audioSessionId] = pObserver;
        m_allComponents->Add(pObserver->getRenderer());
    }
}

void FTXUIAudioDeviceObserver::onAudioSessionRemoved
(
    const std::string &audioSessionId
)
{
    m_audioSessionObservers.erase(audioSessionId);
}

/* ==== FTXUIVolumeMixerObserver Methods =================================== */
FTXUIVolumeMixerObserver::FTXUIVolumeMixerObserver(ftxui::ScreenInteractive &screen)
  : m_bCanUpdateScreen(false),
    m_screen(screen)
{
    std::vector<std::string> items = {};
    // m_menu = ftxui::Menu(&items, &m_menuSelection, ftxui::MenuOption::HorizontalAnimated());
    m_menu = ftxui::Container::Horizontal({}, &m_menuSelection);
    m_tabs = ftxui::Container::Tab({}, &m_menuSelection);
    m_component = ftxui::Container::Vertical({m_menu, m_tabs});
    m_renderer = ftxui::Renderer(m_component, [this]
    {
        // On first render, this will get set to make it known that we can now update the screen
        m_bCanUpdateScreen = true;
        return ftxui::yframe(ftxui::vbox(
            {
                ftxui::text("VolumeMixer") | ftxui::hcenter | ftxui::bold | ftxui::color(ftxui::Color::Blue),
                ftxui::separatorHeavy(),
                m_component->Render()
            }));
    });
}

FTXUIVolumeMixerObserver::~FTXUIVolumeMixerObserver()
{
    // TODO: Check if these are really needed...
    m_renderer->Detach();
    m_renderer->DetachAllChildren();
    m_component->Detach();
    m_component->DetachAllChildren();
    m_tabs->Detach();
    m_tabs->DetachAllChildren();
    m_menu->Detach();
    m_menu->DetachAllChildren();
}

void
FTXUIVolumeMixerObserver::onAudioDeviceAdded
(
    const std::string &audioDeviceId,
    std::weak_ptr<AudioDevice> pAudioDevice
)
{
    if (auto ptr = pAudioDevice.lock())
    {
        auto pObserver = std::make_shared<FTXUIAudioDeviceObserver>(audioDeviceId, pAudioDevice, std::bind(&FTXUIVolumeMixerObserver::updateScreen, this));
        ptr->addObserver(pObserver, true);
        m_audioDeviceObservers[audioDeviceId] = pObserver;
        m_tabs->Add(pObserver->getRenderer());
        m_menu->Add(pObserver->getMenuEntry());
        if (pObserver->isDefaultDevice())
        {
            m_menuSelection = (int)(m_menu->ChildCount() - 1);
        }
        updateScreen();
    }
}

void
FTXUIVolumeMixerObserver::onAudioDeviceRemoved
(
    const std::string &audioDeviceId
)
{
    m_audioDeviceObservers.erase(audioDeviceId);
    updateScreen();
}
