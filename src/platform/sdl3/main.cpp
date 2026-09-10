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
#else
#include <unistd.h>
#endif

#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/app/runtime_configuration.hpp"
#include "openlegend/audio/legacy_audio.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/compat/runtime_platform.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/input/key_repeat.hpp"
#include "openlegend/input/legacy_keyboard.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/time/legacy_clock.hpp"
#include "sdl_audio_device.hpp"
#include "sdl_runtime_platform.hpp"

namespace {

constexpr openlegend::app::WindowSize kDefaultWindowSize{960, 600};
constexpr std::chrono::milliseconds kDefaultMovementRepeatDelay{500};
constexpr std::chrono::milliseconds kSaveListPageRepeatInterval{100};
constexpr std::chrono::nanoseconds kDefaultFadeFrameDelay{14'268'123};

[[nodiscard]] bool accepts_movement_repeat(
    const openlegend::app::LegacyGameRuntime& game) noexcept {
    return game.view() == openlegend::app::LegacyGameView::world ||
        game.scene_loop_uses_key_states();
}

[[nodiscard]] std::uint64_t current_process_id() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

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

    const auto configuration = app::load_runtime_configuration(
        arguments,
        configuration_path,
        executable_root,
        launch_directory,
        app::RuntimeConfigurationDefaults{
            executable_root / "logs" / "openlegend.log",
            diagnostics::LogLevel::info,
            kDefaultWindowSize,
            kDefaultMovementRepeatDelay,
            kDefaultFadeFrameDelay});
    const auto& data_directory = configuration.paths.data_directory;
    const auto& save_directory_configuration = configuration.paths.save_directory;
    const auto& logging_configuration = configuration.logging;
    const auto& input_configuration = configuration.input;
    const auto& timing_configuration = configuration.timing;
    const auto& window_configuration = configuration.window;

    LoggingLifetime logging_lifetime;
    const auto launch_time = std::chrono::system_clock::now();
    const auto session_log_path = app::make_session_log_path(
        logging_configuration.path, launch_time, current_process_id());
    const auto logging_status = diagnostics::initialize_logging(
        session_log_path, logging_configuration.minimum_level);
    if (logging_status != diagnostics::LoggingInitializationStatus::initialized) {
        report_configuration_error(
            "logging",
            logging_initialization_status_message(logging_status),
            path_utf8(session_log_path));
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
        " config=" + path_utf8(configuration_path) +
        " log=" + path_utf8(session_log_path));

    if (data_directory.status != app::DataDirectoryStatus::ready) {
        report_configuration_error(
            "game data directory",
            app::data_directory_status_message(data_directory.status),
            data_directory.detail);
        return 2;
    }
    if (save_directory_configuration.status !=
        app::SaveDirectoryConfigurationStatus::ready) {
        report_configuration_error(
            "save directory configuration",
            app::save_directory_configuration_status_message(
                save_directory_configuration.status),
            save_directory_configuration.detail);
        return 2;
    }
    if (window_configuration.status != app::WindowConfigurationStatus::ready) {
        report_configuration_error(
            "window configuration",
            app::window_configuration_status_message(window_configuration.status),
            window_configuration.detail);
    }
    if (input_configuration.status != app::InputConfigurationStatus::ready) {
        report_configuration_error(
            "input configuration",
            app::input_configuration_status_message(input_configuration.status),
            input_configuration.detail);
    }
    if (timing_configuration.status != app::TimingConfigurationStatus::ready) {
        report_configuration_error(
            "timing configuration",
            app::timing_configuration_status_message(timing_configuration.status),
            timing_configuration.detail);
    }

    diagnostics::log_info(
        "resolved data_directory=" + path_utf8(data_directory.directory) +
        " source=" + std::to_string(static_cast<int>(data_directory.source)));
    auto save_directory = data_directory.directory;
    if (save_directory_configuration.configured) {
        save_directory = save_directory_configuration.directory;
        path_error.clear();
        static_cast<void>(std::filesystem::create_directories(save_directory, path_error));
        if (!path_error) {
            const auto status = std::filesystem::status(save_directory, path_error);
            if (!path_error && !std::filesystem::is_directory(status)) {
                path_error = std::make_error_code(std::errc::not_a_directory);
            }
        }
        if (path_error) {
            report_configuration_error(
                "save directory", path_error.message(), path_utf8(save_directory));
            return 3;
        }
    }
    diagnostics::log_info(
        "resolved save_directory=" + path_utf8(save_directory) +
        " source=" +
        (save_directory_configuration.configured ? std::string{"configuration"}
                                                 : std::string{"data_directory"}));
    diagnostics::log_info(
        "input movement_repeat_delay_ms=" +
        std::to_string(input_configuration.movement_repeat_delay.count()) +
        " fade_frame_delay_us=" +
        std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
            timing_configuration.fade_frame_delay).count()));
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
    bool fade_music_on_exit = false;
    {
    app::LegacyGameRuntime game{
        std::filesystem::current_path(), save_directory, random_seed};
    diagnostics::log_info("runtime random_seed=" + std::to_string(random_seed));
    if (!game.valid()) {
        diagnostics::log_critical("game runtime initialization failed: " + game.error());
        report_configuration_error("game runtime", game.error());
        return 5;
    }
    if (!legacy_audio.play_music(16U)) {
        diagnostics::log_warning("title music unavailable: " + legacy_audio.error());
    }

    input::LegacyKeyboard keyboard;
    const auto sync_battle_confirmation = [&game, &keyboard]() {
        if (game.take_clear_battle_confirmation_states_request()) {
            keyboard.clear_confirmation_states();
        }
        const auto direction = game.take_clear_battle_menu_direction_request();
        if (direction != 0U) {
            keyboard.clear_state(direction);
        }
        const auto cursor_key = game.take_clear_battle_cursor_key_request();
        if (cursor_key == input::legacy_key::down) {
            keyboard.consume_world_direction(input::LegacyWorldDirectionInput::down);
        } else if (cursor_key == input::legacy_key::right) {
            keyboard.consume_world_direction(input::LegacyWorldDirectionInput::right);
        } else if (cursor_key == input::legacy_key::left) {
            keyboard.consume_world_direction(input::LegacyWorldDirectionInput::left);
        } else if (cursor_key == input::legacy_key::up) {
            keyboard.consume_world_direction(input::LegacyWorldDirectionInput::up);
        } else if (cursor_key != 0U) {
            keyboard.clear_state(cursor_key);
        }
        const auto any_down = [&keyboard](const auto& keys) {
            return keyboard.down(keys[0]) || keyboard.down(keys[1]);
        };
        game.set_battle_confirmation_state(
            keyboard.down(input::legacy_key::enter) ||
            keyboard.down(input::legacy_key::space) ||
            keyboard.down(input::legacy_key::keypad_insert));
        game.set_battle_menu_direction_states(
            keyboard.down(input::legacy_key::down),
            keyboard.down(input::legacy_key::up));
        game.set_battle_cursor_input_states(
            any_down(input::legacy_key::world_down),
            any_down(input::legacy_key::world_right),
            any_down(input::legacy_key::world_left),
            any_down(input::legacy_key::world_up),
            keyboard.down(input::legacy_key::escape));
    };
    const auto sync_scene_input_reset = [&game, &keyboard]() {
        switch (game.take_scene_input_reset_request()) {
        case scene::SceneInputReset::confirmation_group:
            keyboard.clear_confirmation_states();
            break;
        case scene::SceneInputReset::main_ui_edge:
            keyboard.consume_edge(input::legacy_key::escape);
            break;
        case scene::SceneInputReset::weather_disable_edge:
            keyboard.consume_edge(input::legacy_key::weather_toggle);
            break;
        case scene::SceneInputReset::none:
            break;
        }
    };
    const auto dispatch_key_down =
        [&game, &keyboard, &sync_battle_confirmation](
            const compat::HostKey key,
            const bool repeat,
            const std::uint32_t frame_tick) {
            keyboard.handle_host_key(key, true);
            sync_battle_confirmation();
            const auto translated_key = keyboard.last_key();
            diagnostics::log_debug(
                "host key_down key=" + std::to_string(static_cast<int>(key)) +
                " repeat=" + (repeat ? std::string{"true"} : std::string{"false"}) +
                " translated=" + std::to_string(translated_key));
            if (translated_key == 0U) {
                return;
            }
            const bool defer_world_menu = translated_key == input::legacy_key::escape &&
                game.view() == app::LegacyGameView::world;
            const bool defer_scene_input = game.scene_loop_uses_key_states() &&
                (translated_key == input::legacy_key::escape ||
                 translated_key == input::legacy_key::weather_toggle ||
                 translated_key == input::legacy_key::enter ||
                 translated_key == input::legacy_key::space ||
                 translated_key == input::legacy_key::keypad_insert);
            if (!defer_world_menu && !defer_scene_input &&
                !game.battle_menu_uses_key_states()) {
                const auto key_state_reset = game.handle_key(
                    translated_key,
                    keyboard.down(input::legacy_key::left_control),
                    keyboard.down(input::legacy_key::left_shift) ||
                        keyboard.down(input::legacy_key::right_shift),
                    frame_tick);
                if (key_state_reset == app::LegacyKeyStateReset::edge) {
                    keyboard.consume_edge(translated_key);
                } else if (key_state_reset == app::LegacyKeyStateReset::translated) {
                    keyboard.clear_state(translated_key);
                } else if (key_state_reset == app::LegacyKeyStateReset::down_translated) {
                    keyboard.clear_state(input::legacy_key::down);
                } else if (key_state_reset == app::LegacyKeyStateReset::confirmation_group) {
                    keyboard.clear_confirmation_states();
                } else if (translated_key == input::legacy_key::escape) {
                    keyboard.consume_edge(input::legacy_key::escape);
                }
            }
            keyboard.clear_last_key();
        };
    input::KeyRepeatController key_repeat{
        input_configuration.movement_repeat_delay,
        kSaveListPageRepeatInterval};
    render::RgbaFramebuffer rgba_framebuffer;
    render::RgbaFramebuffer modern_ui_framebuffer;
    timing::SteadyBiosTickSource tick_source;
    timing::SteadyVgaRetraceSource retrace_source{
        timing_configuration.fade_frame_delay};
    bool running = true;
    while (running) {
        const auto frame_tick = tick_source.tick();
        const auto frame_retrace = retrace_source.tick();
        const auto input_now = std::chrono::steady_clock::now();
        key_repeat.begin_frame();
        compat::HostEvent event{};
        while (platform.poll_event(event)) {
            if (event.type == compat::HostEventType::quit) {
                diagnostics::log_info("host quit event");
                running = false;
            } else if (event.type == compat::HostEventType::key_down) {
                if (key_repeat.handle_key_down(
                        event.key,
                        event.repeat,
                        accepts_movement_repeat(game),
                        game.save_list_active(),
                        input_now)) {
                    dispatch_key_down(event.key, event.repeat, frame_tick);
                }
            } else if (event.type == compat::HostEventType::key_up) {
                key_repeat.handle_key_up(event.key);
                keyboard.handle_host_key(event.key, false);
                diagnostics::log_debug(
                    "host key_up key=" + std::to_string(static_cast<int>(event.key)));
            }
            sync_scene_input_reset();
            sync_battle_confirmation();
        }
        if (const auto repeated_key = key_repeat.take_movement_repeat(
                accepts_movement_repeat(game), std::chrono::steady_clock::now())) {
            keyboard.handle_host_key(*repeated_key, false);
            dispatch_key_down(*repeated_key, true, frame_tick);
        }
        if (const auto repeated_key = key_repeat.take_save_list_page_repeat(
                game.save_list_active(), std::chrono::steady_clock::now())) {
            keyboard.handle_host_key(*repeated_key, false);
            dispatch_key_down(*repeated_key, true, frame_tick);
        }
        sync_scene_input_reset();
        sync_battle_confirmation();
        const auto world_direction = keyboard.world_direction();
        using input::LegacyWorldDirectionInput;
        const bool directional_input_consumed = game.handle_world_input(
            world_direction == LegacyWorldDirectionInput::left,
            world_direction == LegacyWorldDirectionInput::up,
            world_direction == LegacyWorldDirectionInput::down,
            world_direction == LegacyWorldDirectionInput::right,
            keyboard.edge(input::legacy_key::escape));
        if (directional_input_consumed &&
            world_direction != LegacyWorldDirectionInput::none) {
            keyboard.consume_world_direction(world_direction);
        } else if (directional_input_consumed &&
                   keyboard.edge(input::legacy_key::escape)) {
            keyboard.consume_edge(input::legacy_key::escape);
        }
        game.set_scene_input_states(
            keyboard.down(input::legacy_key::enter) ||
                keyboard.down(input::legacy_key::space) ||
                keyboard.down(input::legacy_key::keypad_insert),
            keyboard.edge(input::legacy_key::escape),
            keyboard.edge(input::legacy_key::weather_toggle));
        game.advance(frame_tick);
        const bool vga_frame = game.uses_vga_retrace();
        sync_scene_input_reset();
        sync_battle_confirmation();
        if (game.take_clear_scene_exit_key_states_request()) {
            keyboard.clear_scene_exit_key_states();
            key_repeat.defer_movement_repeat(std::chrono::steady_clock::now());
        }
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
        for (const auto& command : game.take_battle_audio_commands()) {
            if (command.sample_id < 0) {
                continue;
            }
            const auto bank = command.bank == battle::BattleAudioBank::attack
                ? audio::SampleBank::attack
                : audio::SampleBank::effect;
            const auto sample_id = static_cast<std::size_t>(command.sample_id);
            if (command.action == battle::BattleAudioAction::load) {
                static_cast<void>(legacy_audio.load_sample(bank, sample_id));
            } else if (command.action == battle::BattleAudioAction::start_loaded) {
                static_cast<void>(legacy_audio.start_loaded_sample(bank, sample_id));
            } else {
                static_cast<void>(legacy_audio.play_sample(bank, sample_id));
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
            const compat::IndexedFrameView indexed_frame{
                framebuffer.pixels(), framebuffer.palette()};
            if (!compat::convert_indexed_frame_to_rgba(
                    indexed_frame, rgba_framebuffer.pixels())) {
                diagnostics::log_critical("indexed framebuffer conversion failed");
                report_configuration_error(
                    "render", "unable to convert indexed framebuffer to RGBA");
                return 7;
            }

            const auto presentation_scale = platform.presentation_scale();
            if (!modern_ui_framebuffer.set_scale(presentation_scale)) {
                diagnostics::log_critical(
                    "modern RGBA UI framebuffer resize failed scale=" +
                    std::to_string(presentation_scale));
                report_configuration_error(
                    "render", "unable to resize modern RGBA UI framebuffer");
                return 7;
            }
            modern_ui_framebuffer.clear({0U, 0U, 0U, 0U});
            if (!game.render_modern_ui(modern_ui_framebuffer)) {
                diagnostics::log_critical("modern RGBA UI render failed");
                report_configuration_error(
                    "render", "unable to render modern RGBA UI");
                return 7;
            }
            const compat::RgbaFrameView frame{rgba_framebuffer.pixels()};
            const compat::RgbaFrameView modern_ui{
                modern_ui_framebuffer.pixels(),
                modern_ui_framebuffer.pixel_width(),
                modern_ui_framebuffer.pixel_height()};
            if (!platform.present(frame, modern_ui)) {
                diagnostics::log_critical(
                    std::string{"RGBA framebuffer present failed: "} + SDL_GetError());
                report_configuration_error(
                    "present", "unable to present RGBA framebuffer", SDL_GetError());
                return 7;
            }
            game.finish_presented_tick(tick_source.tick());
            sync_scene_input_reset();
            if (game.take_clear_scene_exit_key_states_request()) {
                keyboard.clear_scene_exit_key_states();
            }
            sync_battle_confirmation();
            diagnostics::log_trace(
                "frame presented tick=" + std::to_string(frame_tick) +
                " view=" + std::to_string(static_cast<int>(game.view())));
        }
        if (smoke_test) {
            running = false;
        } else if (running && !game.needs_immediate_frame(tick_source.tick())) {
            if (vga_frame &&
                timing_configuration.fade_frame_delay > std::chrono::nanoseconds::zero()) {
                static_cast<void>(
                    timing::wait_for_tick_change(retrace_source, frame_retrace));
            } else if (!vga_frame) {
                static_cast<void>(timing::wait_for_tick_change(tick_source, frame_tick));
            }
        }
    }

    ending_completed = game.ending_complete();
    fade_music_on_exit = game.fade_music_on_exit();
    if (smoke_test) {
        diagnostics::log_info("smoke test completed");
        return 0;
    }
    }
    if (fade_music_on_exit) {
        legacy_audio.fade_out_music();
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
