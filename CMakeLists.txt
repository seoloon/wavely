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

# --- C++/WinRT projection headers (GSMTC) -----------------------------------
# The Windows SDK ships cppwinrt.exe but not the per-namespace projected headers
# (winrt/Windows.Media.Control.h, etc.) - they must be generated from the SDK's .winmd
# metadata. Generation is scoped to the namespaces we actually use to keep it fast
# (~15s) and is cached in the build tree, regenerated only if missing.
file(GLOB _wavely_cppwinrt_candidates
    "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/cppwinrt.exe"
)
if(NOT _wavely_cppwinrt_candidates)
    message(FATAL_ERROR "cppwinrt.exe not found under a Windows Kits 10 SDK install. Install the Windows SDK (Desktop development with C++ workload).")
endif()
list(SORT _wavely_cppwinrt_candidates COMPARE NATURAL ORDER DESCENDING)
list(GET _wavely_cppwinrt_candidates 0 CPPWINRT_EXE)

set(WAVELY_WINRT_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
set(WAVELY_WINRT_GEN_STAMP "${WAVELY_WINRT_GEN_DIR}/winrt.stamp")

add_custom_command(
    OUTPUT "${WAVELY_WINRT_GEN_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${WAVELY_WINRT_GEN_DIR}"
    COMMAND "${CPPWINRT_EXE}" -input sdk -reference sdk
            -include Windows.Media.Control
            -include Windows.Storage.Streams
            -output "${WAVELY_WINRT_GEN_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${WAVELY_WINRT_GEN_STAMP}"
    COMMENT "Generating C++/WinRT projection headers (GSMTC, Storage.Streams)"
    VERBATIM
)
add_custom_target(WavelyWinrtHeaders DEPENDS "${WAVELY_WINRT_GEN_STAMP}")

add_executable(Wavely WIN32
    src/main.cpp
    src/core/Handle.hpp
    src/core/ComPtr.hpp
    src/core/WinrtGuard.hpp
    src/core/AutoStart.hpp
    src/core/AutoStart.cpp
    src/core/MediaMetadata.hpp
    src/core/MediaSessionManager.hpp
    src/core/MediaSessionManager.cpp
    src/settings/AppConfig.hpp
    src/settings/AppConfig.cpp
    src/ui/MainWidget.hpp
    src/ui/MainWidget.cpp
    src/ui/TrayIcon.hpp
    src/ui/TrayIcon.cpp
    src/ui/VisibilityController.hpp
    src/ui/VisibilityController.cpp
)

add_dependencies(Wavely WavelyWinrtHeaders)

target_include_directories(Wavely PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${WAVELY_WINRT_GEN_DIR}
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
    windowsapp
)

set_target_properties(Wavely PROPERTIES
    OUTPUT_NAME "Wavely"
)
