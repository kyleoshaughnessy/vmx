get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include ABSOLUTE)

add_library(vmx-core
    VolumeMixer.cpp
    ${include_dir}/vmx/VolumeMixer.h
    $<$<PLATFORM_ID:Windows>:
       WindowsVolumeMixer.cpp
       ${include_dir}/vmx/WindowsVolumeMixer.h
    >
)

target_compile_features(vmx-core PUBLIC cxx_std_20) # required for jthread in WindowsVolumeMixer.h

target_include_directories(vmx-core PRIVATE $<BUILD_INTERFACE:${include_dir}> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>)
target_include_directories(vmx-core PUBLIC $<BUILD_INTERFACE:${include_dir}>)
target_include_directories(vmx-core SYSTEM INTERFACE $<INSTALL_INTERFACE:include>)

if (MSVC)
    target_compile_options(vmx-core PRIVATE /W4 /WX)
else()
    target_compile_options(vmx-core PRIVATE -Wall -Wextra -Wpedantic -Werror -Wmissing-declarations -Wdeprecated -Wshadow)
endif()
