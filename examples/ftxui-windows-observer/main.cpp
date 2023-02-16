/* ==== Application Includes =============================================== */
#include "FTXUIVolumeMixerObserver.h"

/* ==== VMX Includes ======================================================= */
#include <vmx/WindowsVolumeMixer.h>

/* ==== Standard Library Includes ========================================== */
#include <chrono>
#include <memory>
using namespace std::chrono_literals;

/* ==== Open Source Includes =============================================== */
#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <ftxui/component/screen_interactive.hpp>

/* ==== Functions ========================================================== */
int
throwableMain
(
    int     argc,
    char   *argv[]
)
{
    CLI::App app{"VolumeMixer v1.0"};
    unsigned int peakSamplingPeriodMillis = 125;

    app.add_option("-p,--peakSamplingPeriod", peakSamplingPeriodMillis, "Sampling period for peak meters in milliseconds; 0 indicates no sampling");

    CLI11_PARSE(app, argc, argv);

    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto obs = std::make_shared<FTXUIVolumeMixerObserver>(screen);
    vmx::WindowsVolumeMixer mixer{};
    mixer.addObserver(obs, true);
    mixer.setPeakSamplingPeriod(std::chrono::milliseconds(peakSamplingPeriodMillis));
    screen.Loop(obs->getRenderer());

    return EXIT_SUCCESS;
}

/**
 * @brief      Process entry point
 *
 * @param[in]  argc  The count of CLI arguments
 * @param      argv  The CLI arguments array
 *
 * @return     EXIT_SUCCESS on successful process termination
 *             EXIT_FAILURE if process terminates with error
 */
int
main
(
    int     argc,
    char   *argv[]
)
{
    try
    {
        return throwableMain(argc, argv);
    }
    catch(std::exception e)
    {
        fmt::print("{}\n", e.what());
    }
    catch(...)
    {
        fmt::print("Unknown exception type caught. {}:{}\n", __FILE__, __LINE__);
    }

    return EXIT_FAILURE;
}
