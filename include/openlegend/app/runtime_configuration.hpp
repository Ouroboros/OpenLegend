#pragma once

#include <cstddef>
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

[[nodiscard]] DataDirectoryResolution resolve_data_directory(
    std::span<const std::string_view> arguments,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& launch_directory);

[[nodiscard]] bool activate_data_directory(
    const std::filesystem::path& directory, std::error_code& error) noexcept;

[[nodiscard]] std::string_view data_directory_status_message(DataDirectoryStatus status) noexcept;

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

enum class LoggingConfigurationStatus {
    ready,
    read_failed,
    parse_failed,
    invalid_logging_table,
    invalid_log_path,
    invalid_log_level,
};

struct LoggingConfigurationLoadResult {
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

[[nodiscard]] std::string_view logging_configuration_status_message(
    LoggingConfigurationStatus status) noexcept;

}  // namespace openlegend::app
