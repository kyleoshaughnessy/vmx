if($<NOT:$<PLATFORM_ID:Windows>>)
  return()
endif()

include(FetchContent)

# --- Fetch FTXUI --------------------------------------------------------------
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG        v3.0.0
)
FetchContent_GetProperties(ftxui)
if(NOT ftxui_POPULATED)
  FetchContent_Populate(ftxui)
  add_subdirectory(${ftxui_SOURCE_DIR} ${ftxui_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
# ------------------------------------------------------------------------------

# --- Fetch CLI11 --------------------------------------------------------------
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11
  GIT_TAG        v2.3.2
)
set(CLI11_PRECOMPILED ON)
FetchContent_MakeAvailable(cli11)
# ------------------------------------------------------------------------------

# --- Fetch fmt ----------------------------------------------------------------
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt
  GIT_TAG        9.1.0
)
FetchContent_MakeAvailable(fmt)
# ------------------------------------------------------------------------------

add_executable(vmx_example_ftxui_windows_observer
	main.cpp
	FTXUIVolumeMixerObserver.cpp
	FTXUIVolumeMixerObserver.h
)

target_link_libraries(vmx_example_ftxui_windows_observer
  PRIVATE CLI11::CLI11
  PRIVATE fmt::fmt
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
  PRIVATE vmx-core
)

set_target_properties(vmx_example_ftxui_windows_observer PROPERTIES CXX_STANDARD 20)
