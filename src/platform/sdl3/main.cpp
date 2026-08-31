#include <chrono>
#include <cstdint>
#include <cstdio>
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
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/input/legacy_keyboard.hpp"
#include "openlegend/time/legacy_clock.hpp"
#include "sdl_audio_device.hpp"
#include "sdl_runtime_platform.hpp"

namespace {

constexpr openlegend::app::WindowSize kDefaultWindowSize{960, 600};

class LoggingLifetime {
public:
    ~LoggingLifetime() { openlegend::diagnostics::shutdown_logging(); }
};

[[nodiscard]] std::string path_utf8(const std::filesystem::path& path) {
    const auto value = path.generic_u8string();
    return {
        reinterpret_cast<const char*>(value.data()),
        reinterpret_cast<const char*>(value.data() + value.size())};
}

[[nodiscard]] std::string_view logging_initialization_status_message(
    const openlegend::diagnostics::LoggingInitializationStatus status) noexcept {
    using openlegend::diagnostics::LoggingInitializationStatus;
    switch (status) {
    case LoggingInitializationStatus::initialized: return "initialized";
    case LoggingInitializationStatus::directory_creation_failed:
        return "log directory creation failed";
    case LoggingInitializationStatus::file_open_failed: return "log file open failed";
    }
    return "unknown logging initialization status";
}

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
    std::string record{category};
    record += ": ";
    record += message;
    if (!detail.empty()) {
        record += ": ";
        record += detail;
    }
    std::cerr << record << '\n';
    if (openlegend::diagnostics::logging_to_file()) {
        openlegend::diagnostics::log_error(record);
    }
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

    LoggingLifetime logging_lifetime;
    const auto logging_configuration = app::load_logging_configuration(
        configuration_path,
        executable_root,
        executable_root / "logs" / "openlegend.log",
        diagnostics::LogLevel::info);
    const auto logging_status = diagnostics::initialize_logging(
        logging_configuration.path, logging_configuration.minimum_level);
    if (logging_status != diagnostics::LoggingInitializationStatus::initialized) {
        report_configuration_error(
            "logging",
            logging_initialization_status_message(logging_status),
            path_utf8(logging_configuration.path));
    }
    if (logging_configuration.status != app::LoggingConfigurationStatus::ready) {
        report_configuration_error(
            "logging configuration",
            app::logging_configuration_status_message(logging_configuration.status),
            logging_configuration.detail);
    }
    diagnostics::log_info(
        "startup executable_root=" + path_utf8(executable_root) +
        " launch_directory=" + path_utf8(launch_directory) +
        " config=" + path_utf8(configuration_path));

    std::vector<std::string> argument_storage;
    std::vector<std::string_view> arguments;
    if (!collect_command_arguments(argc, argv, argument_storage, arguments)) {
        report_configuration_error("command line", "cannot decode arguments as UTF-8");
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

    diagnostics::log_info(
        "resolved data_directory=" + path_utf8(data_directory.directory) +
        " source=" + std::to_string(static_cast<int>(data_directory.source)));
    if (!app::activate_data_directory(data_directory.directory, path_error)) {
        report_configuration_error("game data directory", path_error.message());
        return 3;
    }

    bool ending_completed{};
    const auto run_status = [&]() -> int {
    platform::sdl3::SdlRuntimePlatform platform{
        window_configuration.size.width,
        window_configuration.size.height,
        window_configuration.maximized};
    if (!platform.valid()) {
        report_configuration_error("SDL3 platform", "initialization failed", SDL_GetError());
        return 4;
    }
    diagnostics::log_info(
        "SDL3 platform ready window=" + std::to_string(window_configuration.size.width) +
        "x" + std::to_string(window_configuration.size.height) +
        " maximized=" + (window_configuration.maximized ? std::string{"true"}
                                                          : std::string{"false"}));

    audio::AudioMixer audio_mixer;
    audio::SystemAudioDelay audio_delay;
    audio::LegacyAudioController legacy_audio{
        resource::DataRoot{std::filesystem::current_path()}, audio_mixer, audio_delay};
    platform::sdl3::SdlAudioDevice audio_device{audio_mixer};
    if (!audio_mixer.valid()) {
        diagnostics::log_warning(
            "XMI synthesizer unavailable; audio disabled: " + audio_mixer.error());
    } else if (!audio_device.valid()) {
        diagnostics::log_warning(
            std::string{"SDL3 audio device unavailable; audio disabled: "} + SDL_GetError());
    } else {
        diagnostics::log_info("audio backend ready");
    }

    const auto wall_time = std::chrono::system_clock::now().time_since_epoch();
    const auto whole_seconds = std::chrono::duration_cast<std::chrono::seconds>(wall_time);
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(wall_time - whole_seconds);
    const auto second = static_cast<std::uint8_t>(whole_seconds.count() % 60);
    const auto hundredth = static_cast<std::uint8_t>(milliseconds.count() / 10);
    const auto random_seed = random::LegacyRandom::dos_time_seed(second, hundredth);
    app::LegacyGameRuntime game{std::filesystem::current_path(), random_seed};
    diagnostics::log_info("runtime random_seed=" + std::to_string(random_seed));
    if (!game.valid()) {
        diagnostics::log_critical("game runtime initialization failed: " + game.error());
        report_configuration_error("game runtime", game.error());
        return 5;
    }

    input::LegacyKeyboard keyboard;
    timing::SteadyBiosTickSource tick_source;
    bool running = true;
    while (running) {
        const auto frame_tick = tick_source.tick();
        compat::HostEvent event{};
        while (platform.poll_event(event)) {
            if (event.type == compat::HostEventType::quit) {
                diagnostics::log_info("host quit event");
                running = false;
            } else if (event.type == compat::HostEventType::key_down) {
                keyboard.handle_host_key(event.key, true);
                const auto translated_key = keyboard.last_key();
                diagnostics::log_debug(
                    "host key_down key=" + std::to_string(static_cast<int>(event.key)) +
                    " repeat=" + (event.repeat ? std::string{"true"} : std::string{"false"}) +
                    " translated=" + std::to_string(translated_key));
                if (!event.repeat) {
                    const bool defer_world_menu =
                        translated_key == 0x1BU && game.view() == app::LegacyGameView::world;
                    if (!defer_world_menu) {
                        game.handle_key(
                            translated_key,
                            keyboard.down(0x82U),
                            keyboard.down(0x83U) || keyboard.down(0x84U));
                        if (translated_key == 0x1BU) {
                            keyboard.consume_edge(0x1BU);
                        }
                    }
                    keyboard.clear_last_key();
                }
            } else if (event.type == compat::HostEventType::key_up) {
                keyboard.handle_host_key(event.key, false);
                diagnostics::log_debug(
                    "host key_up key=" + std::to_string(static_cast<int>(event.key)));
            }
        }
        const bool world_menu_consumed = game.handle_world_input(
            keyboard.down(input::kLegacyLeftKey),
            keyboard.down(input::kLegacyUpKey),
            keyboard.down(input::kLegacyDownKey),
            keyboard.down(input::kLegacyRightKey),
            keyboard.edge(0x1BU));
        if (world_menu_consumed) {
            keyboard.consume_edge(0x1BU);
        }
        game.advance(frame_tick);
        for (const auto& command : game.take_scene_audio_commands()) {
            if (command.id < 0) {
                continue;
            }
            if (command.kind == scene::SceneAudioCommand::Kind::music) {
                const auto music_id = static_cast<std::size_t>(command.id);
                if (command.force || legacy_audio.current_music() != music_id) {
                    static_cast<void>(legacy_audio.play_music(music_id));
                }
            } else {
                static_cast<void>(legacy_audio.play_sample(
                    audio::SampleBank::effect, static_cast<std::size_t>(command.id)));
            }
        }
        running = running && game.running();
        if (running) {
            if (!game.render()) {
                diagnostics::log_critical(
                    "render failed view=" + std::to_string(static_cast<int>(game.view())));
                report_configuration_error("render", "unable to render legacy game state");
                return 6;
            }
            const auto& framebuffer = game.framebuffer();
            const compat::IndexedFrameView frame{framebuffer.pixels(), framebuffer.palette()};
            if (!platform.present(frame)) {
                diagnostics::log_critical(std::string{"indexed framebuffer present failed: "} + SDL_GetError());
                report_configuration_error("present", "unable to present indexed framebuffer", SDL_GetError());
                return 7;
            }
            game.finish_presented_tick(tick_source.tick());
            diagnostics::log_trace(
                "frame presented tick=" + std::to_string(frame_tick) +
                " view=" + std::to_string(static_cast<int>(game.view())));
        }
        if (smoke_test) {
            running = false;
        } else if (running) {
            static_cast<void>(timing::wait_for_tick_change(tick_source, frame_tick));
        }
    }

    ending_completed = game.ending_complete();
    if (smoke_test) {
        diagnostics::log_info("smoke test completed");
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

    diagnostics::log_info("normal shutdown");
    return 0;
    }();

    if (run_status == 0 && ending_completed) {
        const auto message = app::ending_terminal_message();
        static_cast<void>(std::fwrite(message.data(), 1U, message.size(), stdout));
    }
    return run_status;
}
