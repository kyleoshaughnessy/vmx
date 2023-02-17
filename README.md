<p align="center">
  <img src="./examples/ftxui-windows-observer/capture.gif" alt="Demo image"></img>
</p>

## vmx

<i>Volume Mixer</i>

C++ Volume Mixer Library for Big OSes.

## Features

## Documentation

## Operating systems
- Windows
- macOS (planned)

## How to use in your project

vmx works out of the box with CMake's FetchContent.
```cmake
include(FetchContent)

FetchContent_Declare(vmx
  GIT_REPOSITORY https://github.com/kyleoshaughnessy/vmx
  GIT_TAG some_tag
)

FetchContent_GetProperties(vmx)
if(NOT vmx_POPULATED)
  FetchContent_Populate(vmx)
  add_subdirectory(${vmx_SOURCE_DIR} ${vmx_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()
```

In the future, pre-built binaries may be added to tagged releases.
