get_filename_component(include_dir ${CMAKE_CURRENT_SOURCE_DIR}/../include ABSOLUTE)

add_library(vmx_core
    VolumeMixer.cpp
    ${include_dir}/vmx/VolumeMixer.h
    $<$<PLATFORM_ID:Windows>:
       WindowsVolumeMixer.cpp
       ${include_dir}/vmx/WindowsVolumeMixer.h
    >
)

function(add_alias name target)
    add_library(${name} ALIAS ${target})
    set_target_properties(${target} PROPERTIES EXPORT_NAME ${name})
endfunction()

add_alias(vmx::core vmx_core)

target_compile_features(vmx_core PUBLIC cxx_std_20) # required for jthread in WindowsVolumeMixer.h

target_include_directories(vmx_core PRIVATE $<BUILD_INTERFACE:${include_dir}> $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>)
target_include_directories(vmx_core PUBLIC $<BUILD_INTERFACE:${include_dir}>)
target_include_directories(vmx_core SYSTEM INTERFACE $<INSTALL_INTERFACE:include>)

if (MSVC)
    target_compile_options(vmx_core PRIVATE /W4 /WX)
else()
    target_compile_options(vmx_core PRIVATE -Wall -Wextra -Wpedantic -Werror -Wmissing-declarations -Wdeprecated -Wshadow)
endif()
