#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "openlegend/diagnostics/log.hpp"

namespace openlegend::app {

inline constexpr std::string_view kConfigurationFilename = "openlegend.toml";

[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view value);

enum class DataDirectorySource {
    launch_directory,
    configuration_file,
    command_line,
};

enum class DataDirectoryStatus {
    ready,
    missing_command_line_value,
    empty_command_line_value,
    configuration_read_failed,
    configuration_parse_failed,
    configuration_paths_not_table,
    configuration_value_not_string,
    empty_configuration_value,
    directory_query_failed,
    directory_not_found_or_not_directory,
};

struct DataDirectoryResolution {
    DataDirectoryStatus status{DataDirectoryStatus::ready};
    DataDirectorySource source{DataDirectorySource::launch_directory};
    std::filesystem::path directory;
    std::string detail;
};

enum class SaveDirectoryConfigurationStatus {
    ready,
    read_failed,
    parse_failed,
    invalid_paths_table,
    invalid_save_directory,
    directory_query_failed,
};

struct SaveDirectoryConfigurationLoadResult {
    SaveDirectoryConfigurationStatus status{SaveDirectoryConfigurationStatus::ready};
    std::filesystem::path directory;
    bool configured{};
    std::string detail;
};

struct PathsConfigurationLoadResult {
    static constexpr std::string_view toml_table_name = "paths";
    static constexpr std::string_view data_directory_toml_key = "data_dir";
    static constexpr std::string_view save_directory_toml_key = "save_dir";
    static constexpr std::array<std::string_view, 2U> toml_field_order{
        data_directory_toml_key,
        save_directory_toml_key,
    };

    DataDirectoryResolution data_directory;
    SaveDirectoryConfigurationLoadResult save_directory;
};

[[nodiscard]] DataDirectoryResolution resolve_data_directory(
    std::span<const std::string_view> arguments,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& launch_directory);

[[nodiscard]] bool activate_data_directory(
    const std::filesystem::path& directory, std::error_code& error) noexcept;

[[nodiscard]] std::string_view data_directory_status_message(DataDirectoryStatus status) noexcept;

[[nodiscard]] std::string_view save_directory_configuration_status_message(
    SaveDirectoryConfigurationStatus status) noexcept;

struct WindowSize {
    int width{};
    int height{};

    [[nodiscard]] bool operator==(const WindowSize&) const = default;
};

enum class WindowConfigurationStatus {
    ready,
    read_failed,
    parse_failed,
    invalid_window_table,
    invalid_window_size,
    invalid_window_state,
    write_failed,
};

struct WindowConfigurationLoadResult {
    static constexpr std::string_view toml_table_name = "window";
    static constexpr std::string_view width_toml_key = "width";
    static constexpr std::string_view height_toml_key = "height";
    static constexpr std::string_view maximized_toml_key = "maximized";
    static constexpr std::array<std::string_view, 3U> toml_field_order{
        width_toml_key,
        height_toml_key,
        maximized_toml_key,
    };

    WindowConfigurationStatus status{WindowConfigurationStatus::ready};
    WindowSize size;
    bool maximized{};
    bool loaded_from_file{};
    std::string detail;
};

[[nodiscard]] WindowConfigurationLoadResult load_window_configuration(
    const std::filesystem::path& configuration_path, WindowSize fallback);

[[nodiscard]] WindowConfigurationStatus save_window_configuration(
    const std::filesystem::path& configuration_path,
    WindowSize size,
    bool maximized,
    std::string& detail);

[[nodiscard]] std::string_view window_configuration_status_message(
    WindowConfigurationStatus status) noexcept;

inline constexpr int kMinimumGameWidth = 320;
inline constexpr int kMinimumGameHeight = 200;
inline constexpr int kMaximumGameWidth = 1280;
inline constexpr int kMaximumGameHeight = 800;

struct GameResolution {
    int width{kMinimumGameWidth};
    int height{kMinimumGameHeight};

    [[nodiscard]] bool operator==(const GameResolution&) const = default;
};

enum class DisplayConfigurationStatus {
    ready,
    read_failed,
    parse_failed,
    invalid_display_table,
    invalid_game_resolution,
};

struct DisplayConfigurationLoadResult {
    static constexpr std::string_view toml_table_name = "display";
    static constexpr std::string_view width_toml_key = "width";
    static constexpr std::string_view height_toml_key = "height";
    static constexpr std::array<std::string_view, 2U> toml_field_order{
        width_toml_key,
        height_toml_key,
    };

    DisplayConfigurationStatus status{DisplayConfigurationStatus::ready};
    GameResolution resolution;
    bool loaded_from_file{};
    std::string detail;
};

[[nodiscard]] DisplayConfigurationLoadResult load_display_configuration(
    const std::filesystem::path& configuration_path,
    GameResolution fallback);

[[nodiscard]] std::string_view display_configuration_status_message(
    DisplayConfigurationStatus status) noexcept;

enum class InputConfigurationStatus {
    ready,
    read_failed,
    parse_failed,
    invalid_input_table,
    invalid_movement_repeat_delay,
    invalid_menu_repeat_delay,
    invalid_menu_repeat_interval,
};

struct InputConfigurationLoadResult {
    static constexpr std::string_view toml_table_name = "input";
    static constexpr std::string_view movement_repeat_delay_toml_key =
        "movement_repeat_delay_ms";
    static constexpr std::string_view menu_repeat_delay_toml_key =
        "menu_repeat_delay_ms";
    static constexpr std::string_view menu_repeat_interval_toml_key =
        "menu_repeat_interval_ms";
    static constexpr std::array<std::string_view, 3U> toml_field_order{
        movement_repeat_delay_toml_key,
        menu_repeat_delay_toml_key,
        menu_repeat_interval_toml_key,
    };

    InputConfigurationStatus status{InputConfigurationStatus::ready};
    std::chrono::milliseconds movement_repeat_delay{};
    std::chrono::milliseconds menu_repeat_delay{};
    std::chrono::milliseconds menu_repeat_interval{};
    bool loaded_from_file{};
    std::string detail;
};

[[nodiscard]] InputConfigurationLoadResult load_input_configuration(
    const std::filesystem::path& configuration_path,
    std::chrono::milliseconds fallback_movement_repeat_delay,
    std::chrono::milliseconds fallback_menu_repeat_delay,
    std::chrono::milliseconds fallback_menu_repeat_interval);

[[nodiscard]] std::string_view input_configuration_status_message(
    InputConfigurationStatus status) noexcept;

enum class TimingConfigurationStatus {
    ready,
    read_failed,
    parse_failed,
    invalid_timing_table,
    invalid_fade_frame_delay,
};

struct TimingConfigurationLoadResult {
    static constexpr std::string_view toml_table_name = "timing";
    static constexpr std::string_view fade_frame_delay_toml_key =
        "fade_frame_delay_ms";
    static constexpr std::array<std::string_view, 1U> toml_field_order{
        fade_frame_delay_toml_key,
    };

    TimingConfigurationStatus status{TimingConfigurationStatus::ready};
    std::chrono::nanoseconds fade_frame_delay{};
    bool loaded_from_file{};
    std::string detail;
};

[[nodiscard]] TimingConfigurationLoadResult load_timing_configuration(
    const std::filesystem::path& configuration_path,
    std::chrono::nanoseconds fallback_fade_frame_delay);

[[nodiscard]] std::string_view timing_configuration_status_message(
    TimingConfigurationStatus status) noexcept;

enum class LoggingConfigurationStatus {
    ready,
    read_failed,
    parse_failed,
    invalid_logging_table,
    invalid_log_path,
    invalid_log_level,
};

struct LoggingConfigurationLoadResult {
    static constexpr std::string_view toml_table_name = "logging";
    static constexpr std::string_view path_toml_key = "path";
    static constexpr std::string_view level_toml_key = "level";
    static constexpr std::array<std::string_view, 2U> toml_field_order{
        path_toml_key,
        level_toml_key,
    };

    LoggingConfigurationStatus status{LoggingConfigurationStatus::ready};
    std::filesystem::path path;
    diagnostics::LogLevel minimum_level{diagnostics::LogLevel::info};
    bool loaded_from_file{};
    std::string detail;
};

[[nodiscard]] LoggingConfigurationLoadResult load_logging_configuration(
    const std::filesystem::path& configuration_path,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& fallback_path,
    diagnostics::LogLevel fallback_level);

struct RuntimeConfigurationDefaults {
    std::filesystem::path logging_path;
    diagnostics::LogLevel logging_level{diagnostics::LogLevel::info};
    WindowSize window_size;
    std::chrono::milliseconds movement_repeat_delay{};
    std::chrono::milliseconds menu_repeat_delay{};
    std::chrono::milliseconds menu_repeat_interval{};
    std::chrono::nanoseconds fade_frame_delay{};
    GameResolution game_resolution;
};

struct RuntimeConfiguration {
    PathsConfigurationLoadResult paths;
    LoggingConfigurationLoadResult logging;
    InputConfigurationLoadResult input;
    TimingConfigurationLoadResult timing;
    WindowConfigurationLoadResult window;
    DisplayConfigurationLoadResult display;

    static constexpr std::array<std::string_view, 6U> toml_table_order{
        PathsConfigurationLoadResult::toml_table_name,
        LoggingConfigurationLoadResult::toml_table_name,
        InputConfigurationLoadResult::toml_table_name,
        TimingConfigurationLoadResult::toml_table_name,
        WindowConfigurationLoadResult::toml_table_name,
        DisplayConfigurationLoadResult::toml_table_name,
    };
};

[[nodiscard]] RuntimeConfiguration load_runtime_configuration(
    std::span<const std::string_view> arguments,
    const std::filesystem::path& configuration_path,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& launch_directory,
    const RuntimeConfigurationDefaults& defaults);

[[nodiscard]] std::filesystem::path make_session_log_path(
    const std::filesystem::path& configured_path,
    std::chrono::system_clock::time_point launch_time,
    std::uint64_t process_id);

[[nodiscard]] std::string_view logging_configuration_status_message(
    LoggingConfigurationStatus status) noexcept;

}  // namespace openlegend::app
