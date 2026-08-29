cmake_minimum_required(VERSION 3.25)

project(OpenLegend VERSION 0.1.0 LANGUAGES CXX)

option(OPENLEGEND_BUILD_APP "Build the SDL3 OpenLegend executable" ON)
option(OPENLEGEND_FETCH_SDL3 "Fetch SDL3 when no package is installed" ON)

add_library(openlegend_project_options INTERFACE)
target_compile_features(openlegend_project_options INTERFACE cxx_std_20)

add_library(openlegend_project_warnings INTERFACE)
if(MSVC)
    target_compile_options(openlegend_project_warnings INTERFACE /W4 /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(
        openlegend_project_warnings
        INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    )
else()
    message(FATAL_ERROR "OpenLegend requires MSVC, Clang, or GCC")
endif()

add_subdirectory(src/compat)
add_subdirectory(src/resource)
add_subdirectory(src/app)

if(OPENLEGEND_BUILD_APP)
    find_package(SDL3 3.2 CONFIG QUIET)
    if(NOT TARGET SDL3::SDL3)
        if(NOT OPENLEGEND_FETCH_SDL3)
            message(FATAL_ERROR "SDL3 was not found and OPENLEGEND_FETCH_SDL3 is OFF")
        endif()
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        # Keep the vendored Linux build self-contained on minimal CI/WSL hosts.
        # Windows keeps its native WASAPI/window backends; Linux uses the X11
        # core and runtime-loaded libraries without optional development SDKs.
        if(UNIX AND NOT APPLE)
            set(SDL_ALSA OFF CACHE BOOL "" FORCE)
            set(SDL_JACK OFF CACHE BOOL "" FORCE)
            set(SDL_PIPEWIRE OFF CACHE BOOL "" FORCE)
            set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
            set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
            set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XCURSOR OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XFIXES OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XINPUT OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XRANDR OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XSHAPE OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XSYNC OFF CACHE BOOL "" FORCE)
            set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
        endif()
        FetchContent_Declare(
            SDL3
            URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.12.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(SDL3)
    endif()
    add_subdirectory(src/platform/sdl3)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
