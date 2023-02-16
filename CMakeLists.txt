cmake_minimum_required(VERSION 3.11)

project(
    vmx
    VERSION 0.0.1
    DESCRIPTION "Volume Mixing Library"
    LANGUAGES CXX
)

option(VMX_BUILD_EXAMPLES "Set to ON to build examples" ON)

add_library(vmx-core
    src/VolumeMixer.cpp
    include/vmx/VolumeMixer.h
    $<$<PLATFORM_ID:Windows>:
       src/WindowsVolumeMixer.cpp
       include/vmx/WindowsVolumeMixer.h
    >
)

set_target_properties(vmx-core PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(vmx-core PROPERTIES CXX_STANDARD 20 CXX_EXTENSIONS OFF) # required for jthread in WindowsVolumeMixer

target_include_directories(vmx-core PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_include_directories(vmx-core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_include_directories(vmx-core SYSTEM INTERFACE $<INSTALL_INTERFACE:include>)

if (MSVC)
    target_compile_options(vmx-core PRIVATE /W4 /WX)
else()
    target_compile_options(vmx-core PRIVATE -Wall -Wextra -Wpedantic -Werror -Wmissing-declarations -Wdeprecated -Wshadow)
endif()

add_subdirectory(examples)
