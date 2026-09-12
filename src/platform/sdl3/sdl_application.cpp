#include "sdl_application.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

#include <SDL3/SDL.h>

#include "legacy_runtime_loop.hpp"
#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/app/runtime_configuration.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "sdl_runtime_platform.hpp"

namespace openlegend::platform::sdl3 {
namespace {

constexpr app::WindowSize kDefaultWindowSize{960, 600};
constexpr app::GameResolution kDefaultGameResolution{320, 200};
constexpr std::chrono::milliseconds kDefaultMovementRepeatDelay{500};
constexpr std::chrono::milliseconds kDefaultMenuRepeatDelay{500};
constexpr std::chrono::milliseconds kDefaultMenuRepeatInterval{55};
constexpr std::chrono::nanoseconds kDefaultFadeFrameDelay{14'268'123};

[[nodiscard]] std::uint64_t current_process_id() noexcept {
#if defined(_WIN32)
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

class LoggingLifetime {
public:
    ~LoggingLifetime() { diagnostics::shutdown_logging(); }
};

[[nodiscard]] std::string path_utf8(const std::filesystem::path& path) {
    const auto value = path.generic_u8string();
    return {
        reinterpret_cast<const char*>(value.data()),
        reinterpret_cast<const char*>(value.data() + value.size())};
}

[[nodiscard]] std::string_view logging_initialization_status_message(
    const diagnostics::LoggingInitializationStatus status) noexcept {
    using diagnostics::LoggingInitializationStatus;
    switch (status) {
    case LoggingInitializationStatus::initialized: return "initialized";
    case LoggingInitializationStatus::directory_creation_failed:
        return "log directory creation failed";
    case LoggingInitializationStatus::file_open_failed:
        return "log file open failed";
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
    const int argument_count,
    const char* const* argument_values,
    std::vector<std::string>& storage,
    std::vector<std::string_view>& arguments) {
#if defined(_WIN32)
    static_cast<void>(argument_count);
    static_cast<void>(argument_values);
    int wide_count = 0;
    wchar_t** wide_arguments = CommandLineToArgvW(GetCommandLineW(), &wide_count);
    if (wide_arguments == nullptr) {
        return false;
    }
    storage.reserve(
        wide_count > 1 ? static_cast<std::size_t>(wide_count - 1) : 0U);
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
    arguments.reserve(
        argument_count > 1
            ? static_cast<std::size_t>(argument_count - 1)
            : 0U);
    for (int index = 1; index < argument_count; ++index) {
        arguments.emplace_back(argument_values[index]);
    }
#endif
    return true;
}

void report_application_error(
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
    if (diagnostics::logging_to_file()) {
        diagnostics::log_error(record);
    }
}

void initialize_session_logging(
    const app::RuntimeConfiguration& configuration,
    const std::filesystem::path& executable_root,
    const std::filesystem::path& launch_directory,
    const std::filesystem::path& configuration_path) {
    const auto launch_time = std::chrono::system_clock::now();
    const auto session_log_path = app::make_session_log_path(
        configuration.logging.path, launch_time, current_process_id());
    const auto logging_status = diagnostics::initialize_logging(
        session_log_path, configuration.logging.minimum_level);
    if (logging_status !=
        diagnostics::LoggingInitializationStatus::initialized) {
        report_application_error(
            "logging",
            logging_initialization_status_message(logging_status),
            path_utf8(session_log_path));
    }
    if (configuration.logging.status !=
        app::LoggingConfigurationStatus::ready) {
        report_application_error(
            "logging configuration",
            app::logging_configuration_status_message(
                configuration.logging.status),
            configuration.logging.detail);
    }
    diagnostics::log_info(
        "startup executable_root=" + path_utf8(executable_root) +
        " launch_directory=" + path_utf8(launch_directory) +
        " config=" + path_utf8(configuration_path) +
        " log=" + path_utf8(session_log_path));
}

[[nodiscard]] int validate_runtime_configuration(
    const app::RuntimeConfiguration& configuration) {
    if (configuration.paths.data_directory.status !=
        app::DataDirectoryStatus::ready) {
        report_application_error(
            "game data directory",
            app::data_directory_status_message(
                configuration.paths.data_directory.status),
            configuration.paths.data_directory.detail);
        return 2;
    }
    if (configuration.paths.save_directory.status !=
        app::SaveDirectoryConfigurationStatus::ready) {
        report_application_error(
            "save directory configuration",
            app::save_directory_configuration_status_message(
                configuration.paths.save_directory.status),
            configuration.paths.save_directory.detail);
        return 2;
    }
    if (configuration.window.status != app::WindowConfigurationStatus::ready) {
        report_application_error(
            "window configuration",
            app::window_configuration_status_message(configuration.window.status),
            configuration.window.detail);
    }
    if (configuration.display.status != app::DisplayConfigurationStatus::ready) {
        report_application_error(
            "display configuration",
            app::display_configuration_status_message(configuration.display.status),
            configuration.display.detail);
        return 2;
    }
    if (configuration.input.status != app::InputConfigurationStatus::ready) {
        report_application_error(
            "input configuration",
            app::input_configuration_status_message(configuration.input.status),
            configuration.input.detail);
    }
    if (configuration.timing.status != app::TimingConfigurationStatus::ready) {
        report_application_error(
            "timing configuration",
            app::timing_configuration_status_message(configuration.timing.status),
            configuration.timing.detail);
    }
    return 0;
}

[[nodiscard]] std::optional<std::filesystem::path> resolve_save_directory(
    const app::RuntimeConfiguration& configuration) {
    const auto& data_directory = configuration.paths.data_directory;
    const auto& save_configuration = configuration.paths.save_directory;
    auto save_directory = data_directory.directory;
    if (save_configuration.configured) {
        save_directory = save_configuration.directory;
        std::error_code path_error;
        static_cast<void>(
            std::filesystem::create_directories(save_directory, path_error));
        if (!path_error) {
            const auto status =
                std::filesystem::status(save_directory, path_error);
            if (!path_error && !std::filesystem::is_directory(status)) {
                path_error =
                    std::make_error_code(std::errc::not_a_directory);
            }
        }
        if (path_error) {
            report_application_error(
                "save directory",
                path_error.message(),
                path_utf8(save_directory));
            return std::nullopt;
        }
    }
    return save_directory;
}

void log_resolved_configuration(
    const app::RuntimeConfiguration& configuration,
    const std::filesystem::path& save_directory) {
    diagnostics::log_info(
        "resolved data_directory=" +
        path_utf8(configuration.paths.data_directory.directory) +
        " source=" + std::to_string(static_cast<int>(
            configuration.paths.data_directory.source)));
    diagnostics::log_info(
        "resolved save_directory=" + path_utf8(save_directory) +
        " source=" +
        (configuration.paths.save_directory.configured
             ? std::string{"configuration"}
             : std::string{"data_directory"}));
    diagnostics::log_info(
        "display in_game_resolution=" +
        std::to_string(configuration.display.resolution.width) + "x" +
        std::to_string(configuration.display.resolution.height) +
        " input movement_repeat_delay_ms=" +
        std::to_string(configuration.input.movement_repeat_delay.count()) +
        " menu_repeat_delay_ms=" +
        std::to_string(configuration.input.menu_repeat_delay.count()) +
        " menu_repeat_interval_ms=" +
        std::to_string(configuration.input.menu_repeat_interval.count()) +
        " fade_frame_delay_us=" +
        std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
            configuration.timing.fade_frame_delay).count()));
}

void save_window_state(
    SdlRuntimePlatform& platform,
    const app::RuntimeConfiguration& configuration,
    const std::filesystem::path& configuration_path) {
    auto normal_size = configuration.window.size;
    bool maximized = configuration.window.maximized;
    if (!platform.query_window_state(
            normal_size.width, normal_size.height, maximized)) {
        std::cerr <<
            "Unable to query SDL3 window state; window configuration was not saved\n";
        return;
    }

    std::string save_detail;
    const auto save_status = app::save_window_configuration(
        configuration_path, normal_size, maximized, save_detail);
    if (save_status != app::WindowConfigurationStatus::ready) {
        report_application_error(
            "window configuration",
            app::window_configuration_status_message(save_status),
            save_detail);
    }
}

}  // namespace

int run_sdl_application(
    const int argument_count, const char* const* argument_values) {
    std::error_code path_error;
    const auto launch_directory = std::filesystem::current_path(path_error);
    if (path_error) {
        report_application_error("launch directory", path_error.message());
        return 1;
    }

    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        std::cerr << "Unable to resolve executable directory: " <<
            SDL_GetError() << '\n';
        return 1;
    }
    const auto executable_root = app::path_from_utf8(base_path);
    const auto configuration_path =
        executable_root / app::kConfigurationFilename;

    std::vector<std::string> argument_storage;
    std::vector<std::string_view> arguments;
    if (!collect_command_arguments(
            argument_count,
            argument_values,
            argument_storage,
            arguments)) {
        report_application_error(
            "command line", "cannot decode arguments as UTF-8");
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
            kDefaultMenuRepeatDelay,
            kDefaultMenuRepeatInterval,
            kDefaultFadeFrameDelay,
            kDefaultGameResolution});
    LoggingLifetime logging_lifetime;
    initialize_session_logging(
        configuration,
        executable_root,
        launch_directory,
        configuration_path);
    if (const auto validation_status =
            validate_runtime_configuration(configuration);
        validation_status != 0) {
        return validation_status;
    }

    const auto save_directory = resolve_save_directory(configuration);
    if (!save_directory.has_value()) {
        return 3;
    }
    log_resolved_configuration(configuration, *save_directory);
    if (!app::activate_data_directory(
            configuration.paths.data_directory.directory, path_error)) {
        report_application_error(
            "game data directory", path_error.message());
        return 3;
    }

    LegacyRuntimeLoopResult loop_result{};
    {
        SdlRuntimePlatform platform{
            configuration.window.size.width,
            configuration.window.size.height,
            configuration.window.maximized,
            configuration.display.resolution.width,
            configuration.display.resolution.height};
        if (!platform.valid()) {
            report_application_error(
                "SDL3 platform", "initialization failed", SDL_GetError());
            return 4;
        }
        diagnostics::log_info(
            "SDL3 platform ready window=" +
            std::to_string(configuration.window.size.width) + "x" +
            std::to_string(configuration.window.size.height) +
            " maximized=" +
            (configuration.window.maximized
                 ? std::string{"true"}
                 : std::string{"false"}));

        loop_result = run_legacy_runtime_loop(
            platform,
            LegacyRuntimeLoopSettings{
                *save_directory,
                configuration.input.movement_repeat_delay,
                configuration.input.menu_repeat_delay,
                configuration.input.menu_repeat_interval,
                configuration.timing.fade_frame_delay,
                configuration.display.resolution,
                smoke_test});
        if (loop_result.status != 0) {
            return loop_result.status;
        }
        if (!smoke_test) {
            save_window_state(platform, configuration, configuration_path);
            diagnostics::log_info("normal shutdown");
        }
    }

    if (loop_result.ending_completed) {
        const auto message = app::ending_terminal_message();
        static_cast<void>(
            std::fwrite(message.data(), 1U, message.size(), stdout));
    }
    return 0;
}

}  // namespace openlegend::platform::sdl3
