cmake_minimum_required(VERSION 3.24)

project(Wavely
    VERSION 0.1.0
    DESCRIPTION "Wavely - Universal high-performance desktop audio widget for Windows"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "Wavely targets Windows only (requires WinRT GSMTC and WASAPI).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(Wavely WIN32
    src/main.cpp
    src/core/Handle.hpp
    src/core/ComPtr.hpp
    src/core/WinrtGuard.hpp
    src/settings/AppConfig.hpp
    src/settings/AppConfig.cpp
    src/ui/MainWidget.hpp
    src/ui/MainWidget.cpp
)

target_include_directories(Wavely PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Wavely PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(Wavely PRIVATE
        /W4
        /permissive-
        /Zc:__cplusplus
        /EHsc
    )
endif()

target_link_libraries(Wavely PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ole32
    runtimeobject
)

set_target_properties(Wavely PROPERTIES
    OUTPUT_NAME "Wavely"
)
