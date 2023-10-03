cmake_minimum_required(VERSION 3.16)

project(
    vmx
    VERSION 0.0.1
    DESCRIPTION "Volume Mixing Library"
    LANGUAGES CXX
)

add_subdirectory(src)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    option(VMX_BUILD_EXAMPLES    "whether or not examples should be build" ON)
    option(VMX_BUILD_SRC_PACKAGE "whether or not the source package should be built" ON)

    if(VMX_BUILD_SRC_PACKAGE)
        set(package_files include/ src/ CMakeLists.txt LICENSE)
        add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vmx-src.zip
            COMMAND ${CMAKE_COMMAND} -E tar c ${CMAKE_CURRENT_BINARY_DIR}/vmx-src.zip --format=zip -- ${package_files}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${package_files})
        add_custom_target(vmx_src_package DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/vmx-src.zip)
    endif()

    if(VMX_BUILD_EXAMPLES)
        add_subdirectory(examples)
    endif()
endif()