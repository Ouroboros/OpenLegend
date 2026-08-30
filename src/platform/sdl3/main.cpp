#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/app/runtime_configuration.hpp"
#include "openlegend/audio/legacy_audio.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/compat/runtime_platform.hpp"
#include "openlegend/input/legacy_keyboard.hpp"
#include "openlegend/time/legacy_clock.hpp"
#include "sdl_audio_device.hpp"
#include "sdl_runtime_platform.hpp"

namespace {

constexpr openlegend::app::WindowSize kDefaultWindowSize{960, 600};

#if defined(_WIN32)
[[nodiscard]] std::optional<std::string> utf8_from_wide(const wchar_t* value) {
    if (value == nullptr) {
        return std::nullopt;
    }
    const int byte_count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, nullptr, 0, nullptr, nullptr);
    if (byte_count <= 0) {
        return std::nullopt;
    }
    std::string result(static_cast<std::size_t>(byte_count), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value,
            -1,
            result.data(),
            byte_count,
            nullptr,
            nullptr) != byte_count) {
        return std::nullopt;
    }
    result.pop_back();
    return result;
}
#endif

[[nodiscard]] bool collect_command_arguments(
    const int argc,
    const char* const* argv,
    std::vector<std::string>& storage,
    std::vector<std::string_view>& arguments) {
#if defined(_WIN32)
    static_cast<void>(argc);
    static_cast<void>(argv);
    int wide_count = 0;
    wchar_t** wide_arguments = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (wide_arguments == nullptr) {
        return false;
    }
    storage.reserve(wide_count > 1 ? static_cast<std::size_t>(wide_count - 1) : 0U);
    for (int index = 1; index < wide_count; ++index) {
        auto converted = utf8_from_wide(wide_arguments[index]);
        if (!converted.has_value()) {
            static_cast<void>(LocalFree(wide_arguments));
            return false;
        }
        storage.push_back(std::move(*converted));
    }
    static_cast<void>(LocalFree(wide_arguments));
    arguments.reserve(storage.size());
    for (const auto& argument : storage) {
        arguments.emplace_back(argument);
    }
#else
    static_cast<void>(storage);
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);
    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index]);
    }
#endif
    return true;
}

void report_configuration_error(
    const std::string_view category,
    const std::string_view message,
    const std::string_view detail = {}) {
    std::cerr << category << ": " << message;
    if (!detail.empty()) {
        std::cerr << ": " << detail;
    }
    std::cerr << '\n';
}

}  // namespace

int main(const int argc, const char* const* argv) {
    using namespace openlegend;

    std::error_code path_error;
    const auto launch_directory = std::filesystem::current_path(path_error);
    if (path_error) {
        report_configuration_error("launch directory", path_error.message());
        return 1;
    }
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        std::cerr << "Unable to resolve executable directory: " << SDL_GetError() << '\n';
        return 1;
    }
    const auto executable_root = app::path_from_utf8(base_path);
    const auto configuration_path = executable_root / app::kConfigurationFilename;

    std::vector<std::string> argument_storage;
    std::vector<std::string_view> arguments;
    if (!collect_command_arguments(argc, argv, argument_storage, arguments)) {
        std::cerr << "Unable to decode the process command line as UTF-8\n";
        return 1;
    }
    bool smoke_test = false;
    for (const std::string_view argument : arguments) {
        smoke_test = smoke_test || argument == "--smoke-test";
    }

    const auto data_directory =
        app::resolve_data_directory(arguments, executable_root, launch_directory);
    if (data_directory.status != app::DataDirectoryStatus::ready) {
        report_configuration_error(
            "game data directory",
            app::data_directory_status_message(data_directory.status),
            data_directory.detail);
        return 2;
    }

    const auto window_configuration =
        app::load_window_configuration(configuration_path, kDefaultWindowSize);
    if (window_configuration.status != app::WindowConfigurationStatus::ready) {
        report_configuration_error(
            "window configuration",
            app::window_configuration_status_message(window_configuration.status),
            window_configuration.detail);
    }

    if (!app::activate_data_directory(data_directory.directory, path_error)) {
        report_configuration_error("game data directory", path_error.message());
        return 3;
    }

    platform::sdl3::SdlRuntimePlatform platform{
        window_configuration.size.width,
        window_configuration.size.height,
        window_configuration.maximized};
    if (!platform.valid()) {
        std::cerr << "Unable to initialize SDL3 platform\n";
        return 4;
    }

    audio::AudioMixer audio_mixer;
    platform::sdl3::SdlAudioDevice audio_device{audio_mixer};
    if (!audio_mixer.valid()) {
        std::cerr << "XMI synthesizer unavailable; audio is disabled: " << audio_mixer.error()
                  << '\n';
    } else if (!audio_device.valid()) {
        std::cerr << "SDL3 audio device unavailable; audio is disabled: " << SDL_GetError() << '\n';
    }

    const auto wall_time = std::chrono::system_clock::now().time_since_epoch();
    const auto whole_seconds = std::chrono::duration_cast<std::chrono::seconds>(wall_time);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(wall_time - whole_seconds);
    const auto second = static_cast<std::uint8_t>(whole_seconds.count() % 60);
    const auto hundredth = static_cast<std::uint8_t>(milliseconds.count() / 10);
    app::LegacyGameRuntime game{
        std::filesystem::current_path(), random::LegacyRandom::dos_time_seed(second, hundredth)};
    if (!game.valid()) {
        report_configuration_error("game runtime", game.error());
        return 5;
    }

    input::LegacyKeyboard keyboard;
    timing::SteadyBiosTickSource tick_source;
    bool running = true;
    while (running) {
        const auto frame_tick = tick_source.tick();
        game.advance();
        compat::HostEvent event{};
        while (platform.poll_event(event)) {
            if (event.type == compat::HostEventType::quit) {
                running = false;
            } else if (event.type == compat::HostEventType::key_down) {
                keyboard.handle_host_key(event.key, true);
                if (!event.repeat) {
                    game.handle_key(
                        keyboard.last_key(),
                        keyboard.down(0x82U),
                        keyboard.down(0x83U) || keyboard.down(0x84U));
                    keyboard.clear_last_key();
                }
            } else if (event.type == compat::HostEventType::key_up) {
                keyboard.handle_host_key(event.key, false);
            }
        }
        running = running && game.running();
        if (running) {
            if (!game.render()) {
                std::cerr << "Unable to render legacy game state\n";
                return 6;
            }
            const auto& framebuffer = game.framebuffer();
            const compat::IndexedFrameView frame{framebuffer.pixels(), framebuffer.palette()};
            if (!platform.present(frame)) {
                std::cerr << "Unable to present indexed framebuffer\n";
                return 7;
            }
        }
        if (smoke_test) {
            running = false;
        } else if (running) {
            static_cast<void>(timing::wait_for_tick_change(tick_source, frame_tick));
        }
    }

    if (smoke_test) {
        return 0;
    }

    auto normal_size = window_configuration.size;
    bool maximized = window_configuration.maximized;
    if (!platform.query_window_state(normal_size.width, normal_size.height, maximized)) {
        std::cerr << "Unable to query SDL3 window state; window configuration was not saved\n";
        return 0;
    }
    std::string save_detail;
    const auto save_status =
        app::save_window_configuration(configuration_path, normal_size, maximized, save_detail);
    if (save_status != app::WindowConfigurationStatus::ready) {
        report_configuration_error(
            "window configuration",
            app::window_configuration_status_message(save_status),
            save_detail);
    }

    return 0;
}
