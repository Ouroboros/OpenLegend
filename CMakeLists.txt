cmake_minimum_required(VERSION 3.25)

project(OpenLegend VERSION 0.1.0 LANGUAGES CXX)

option(OPENLEGEND_BUILD_APP "Build the SDL3 OpenLegend executable" ON)
option(OPENLEGEND_ENABLE_SANITIZERS "Enable ASan and UBSan on OpenLegend targets" OFF)
option(OPENLEGEND_FETCH_SDL3 "Fetch SDL3 when no package is installed" ON)
option(
    OPENLEGEND_FETCH_TOMLPLUSPLUS
    "Fetch the pinned toml++ source when no installed package is available"
    ON
)
option(OPENLEGEND_FETCH_ADLMIDI "Fetch the pinned libADLMIDI XMI synthesizer" ON)

find_package(tomlplusplus 3.4 CONFIG QUIET)
if(NOT TARGET tomlplusplus::tomlplusplus)
    if(NOT OPENLEGEND_FETCH_TOMLPLUSPLUS)
        message(FATAL_ERROR "toml++ 3.4 or newer was not found and fetching is disabled")
    endif()
    include(FetchContent)
    set(TOMLPLUSPLUS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(TOMLPLUSPLUS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        tomlplusplus
        URL https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz
        URL_HASH SHA256=8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(tomlplusplus)
endif()

if(NOT OPENLEGEND_FETCH_ADLMIDI)
    message(FATAL_ERROR "The pinned libADLMIDI source is required for deterministic XMI playback")
endif()
include(FetchContent)
set(libADLMIDI_STATIC ON CACHE BOOL "" FORCE)
set(libADLMIDI_SHARED OFF CACHE BOOL "" FORCE)
set(WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_MIDI_SEQUENCER ON CACHE BOOL "" FORCE)
set(WITH_EMBEDDED_BANKS ON CACHE BOOL "" FORCE)
set(WITH_HQ_RESAMPLER OFF CACHE BOOL "" FORCE)
set(WITH_MUS_SUPPORT OFF CACHE BOOL "" FORCE)
set(WITH_XMI_SUPPORT ON CACHE BOOL "" FORCE)
set(USE_DOSBOX_EMULATOR ON CACHE BOOL "" FORCE)
set(USE_NUKED_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_OPAL_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_JAVA_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_ESFMU_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_MAME_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_YMFM_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_NUKED_OPL2_LLE_EMULATOR OFF CACHE BOOL "" FORCE)
set(USE_NUKED_OPL3_LLE_EMULATOR OFF CACHE BOOL "" FORCE)
set(WITH_GENADLDATA OFF CACHE BOOL "" FORCE)
set(WITH_MUS2MID OFF CACHE BOOL "" FORCE)
set(WITH_XMI2MID OFF CACHE BOOL "" FORCE)
set(EXAMPLE_SDL2_AUDIO OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    libADLMIDI
    URL https://github.com/Wohlstand/libADLMIDI/archive/refs/tags/v1.6.1.tar.gz
    URL_HASH SHA256=9f14489ce1e6d020b8b90b6cf902e31d61b2d4e82122505cec6d5d4afebcb3e1
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(libADLMIDI)

# clang-cl defines both _MSC_VER and __clang__, while libADLMIDI 1.6.1's
# public header defines its deprecation macro in both branches. Its sources also
# use MSVC's intentionally deprecated CRT entry points and one signed enum compare.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(
        ADLMIDI_static
        PRIVATE -Wno-macro-redefined -Wno-deprecated-declarations -Wno-sign-compare
    )
    target_compile_options(ADLMIDI_version_check PRIVATE -Wno-macro-redefined)
endif()

add_library(openlegend_project_options INTERFACE)
target_compile_features(openlegend_project_options INTERFACE cxx_std_20)
if(OPENLEGEND_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            openlegend_project_options
            INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(openlegend_project_options INTERFACE -fsanitize=address,undefined)
    else()
        message(FATAL_ERROR "OPENLEGEND_ENABLE_SANITIZERS requires Clang or GCC")
    endif()
endif()

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
add_subdirectory(src/diagnostics)
add_subdirectory(src/input)
add_subdirectory(src/time)
add_subdirectory(src/random)
add_subdirectory(src/resource)
add_subdirectory(src/audio)
add_subdirectory(src/model)
add_subdirectory(src/persistence)
add_subdirectory(src/render)
add_subdirectory(src/ui)
add_subdirectory(src/world)
add_subdirectory(src/scene)
add_subdirectory(src/battle)
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

option(BUILD_TESTING "Build the testing tree." ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
