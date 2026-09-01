#include "openlegend/app/runtime_configuration.hpp"

#include <toml++/toml.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <limits>
#include <optional>
#include <utility>

namespace openlegend::app {

namespace {

constexpr std::string_view kDataDirectoryOption = "--data-dir";
constexpr std::string_view kDataDirectoryOptionPrefix = "--data-dir=";

struct DirectoryCandidate {
    DataDirectoryStatus status{DataDirectoryStatus::ready};
    DataDirectorySource source{DataDirectorySource::launch_directory};
    std::filesystem::path path;
    std::filesystem::path relative_base;
    std::string detail;
};

[[nodiscard]] DirectoryCandidate candidate_for_path(
    const DataDirectorySource source,
    std::filesystem::path path,
    std::filesystem::path relative_base) {
    DirectoryCandidate candidate;
    candidate.source = source;
    candidate.path = std::move(path);
    candidate.relative_base = std::move(relative_base);
    return candidate;
}

[[nodiscard]] DirectoryCandidate candidate_error(
    const DataDirectoryStatus status,
    const DataDirectorySource source,
    std::string detail = {}) {
    DirectoryCandidate candidate;
    candidate.status = status;
    candidate.source = source;
    candidate.detail = std::move(detail);
    return candidate;
}

[[nodiscard]] DirectoryCandidate command_line_candidate(
    const std::span<const std::string_view> arguments,
    const std::filesystem::path& launch_directory) {
    for (std::size_t index = 0U; index < arguments.size(); ++index) {
        const std::string_view argument = arguments[index];
        if (argument == kDataDirectoryOption) {
            if (index + 1U >= arguments.size()) {
                return candidate_error(
                    DataDirectoryStatus::missing_command_line_value,
                    DataDirectorySource::command_line);
            }
            if (arguments[index + 1U].empty()) {
                return candidate_error(
                    DataDirectoryStatus::empty_command_line_value,
                    DataDirectorySource::command_line);
            }
            return candidate_for_path(
                DataDirectorySource::command_line,
                path_from_utf8(arguments[index + 1U]),
                launch_directory);
        }
        if (argument.starts_with(kDataDirectoryOptionPrefix)) {
            const std::string_view value = argument.substr(kDataDirectoryOptionPrefix.size());
            if (value.empty()) {
                return candidate_error(
                    DataDirectoryStatus::empty_command_line_value,
                    DataDirectorySource::command_line);
            }
            return candidate_for_path(
                DataDirectorySource::command_line, path_from_utf8(value), launch_directory);
        }
    }
    return candidate_for_path(
        DataDirectorySource::launch_directory, launch_directory, launch_directory);
}

[[nodiscard]] DirectoryCandidate configuration_candidate(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& launch_directory) {
    const auto configuration_path = executable_directory / kConfigurationFilename;
    std::error_code error;
    const bool exists = std::filesystem::exists(configuration_path, error);
    if (error) {
        return candidate_error(
            DataDirectoryStatus::configuration_read_failed,
            DataDirectorySource::configuration_file,
            error.message());
    }
    if (!exists) {
        return candidate_for_path(
            DataDirectorySource::launch_directory, launch_directory, launch_directory);
    }

    std::ifstream input{configuration_path, std::ios::binary};
    if (!input) {
        return candidate_error(
            DataDirectoryStatus::configuration_read_failed,
            DataDirectorySource::configuration_file,
            configuration_path.string());
    }

    toml::table document;
    try {
        document = toml::parse(input, configuration_path.string());
    } catch (const toml::parse_error& parse_error) {
        return candidate_error(
            DataDirectoryStatus::configuration_parse_failed,
            DataDirectorySource::configuration_file,
            std::string{parse_error.description()});
    }

    const toml::node* paths_node = document.get("paths");
    if (paths_node == nullptr) {
        return candidate_for_path(
            DataDirectorySource::launch_directory, launch_directory, launch_directory);
    }
    const toml::table* paths = paths_node->as_table();
    if (paths == nullptr) {
        return candidate_error(
            DataDirectoryStatus::configuration_paths_not_table,
            DataDirectorySource::configuration_file);
    }
    const toml::node* data_directory_node = paths->get("data_dir");
    if (data_directory_node == nullptr) {
        return candidate_for_path(
            DataDirectorySource::launch_directory, launch_directory, launch_directory);
    }
    const toml::value<std::string>* data_directory_value = data_directory_node->as_string();
    if (data_directory_value == nullptr) {
        return candidate_error(
            DataDirectoryStatus::configuration_value_not_string,
            DataDirectorySource::configuration_file);
    }
    const std::string& value = data_directory_value->get();
    if (value.empty()) {
        return candidate_error(
            DataDirectoryStatus::empty_configuration_value,
            DataDirectorySource::configuration_file);
    }
    return candidate_for_path(
        DataDirectorySource::configuration_file,
        path_from_utf8(value),
        executable_directory);
}

[[nodiscard]] DataDirectoryResolution validate_candidate(DirectoryCandidate candidate) {
    DataDirectoryResolution resolution;
    resolution.status = candidate.status;
    resolution.source = candidate.source;
    resolution.detail = std::move(candidate.detail);
    if (candidate.status != DataDirectoryStatus::ready) {
        return resolution;
    }

    auto directory = candidate.path;
    if (directory.is_relative()) {
        directory = candidate.relative_base / directory;
    }
    std::error_code error;
    directory = std::filesystem::absolute(directory, error).lexically_normal();
    if (error) {
        resolution.status = DataDirectoryStatus::directory_query_failed;
        resolution.directory = std::move(directory);
        resolution.detail = error.message();
        return resolution;
    }
    const auto file_status = std::filesystem::status(directory, error);
    if (error) {
        resolution.status = error == std::errc::no_such_file_or_directory
            ? DataDirectoryStatus::directory_not_found_or_not_directory
            : DataDirectoryStatus::directory_query_failed;
        resolution.directory = std::move(directory);
        resolution.detail = error.message();
        return resolution;
    }
    if (!std::filesystem::is_directory(file_status)) {
        resolution.status = DataDirectoryStatus::directory_not_found_or_not_directory;
        resolution.directory = std::move(directory);
        return resolution;
    }
    resolution.directory = std::move(directory);
    return resolution;
}

template <typename Status>
[[nodiscard]] bool read_existing_document(
    const std::filesystem::path& configuration_path,
    toml::table& document,
    Status& status,
    std::string& detail) {
    std::error_code error;
    const bool exists = std::filesystem::exists(configuration_path, error);
    if (error) {
        status = Status::read_failed;
        detail = error.message();
        return false;
    }
    if (!exists) {
        return true;
    }
    std::ifstream input{configuration_path, std::ios::binary};
    if (!input) {
        status = Status::read_failed;
        detail = configuration_path.string();
        return false;
    }
    try {
        document = toml::parse(input, configuration_path.string());
    } catch (const toml::parse_error& parse_error) {
        status = Status::parse_failed;
        detail = std::string{parse_error.description()};
        return false;
    }
    return true;
}

[[nodiscard]] bool valid_dimension(const std::int64_t value) noexcept {
    return value > 0 && value <= static_cast<std::int64_t>(std::numeric_limits<int>::max());
}

[[nodiscard]] std::optional<diagnostics::LogLevel> parse_log_level(
    const std::string_view value) noexcept {
    if (value == "trace") {
        return diagnostics::LogLevel::trace;
    }
    if (value == "debug") {
        return diagnostics::LogLevel::debug;
    }
    if (value == "info") {
        return diagnostics::LogLevel::info;
    }
    if (value == "warning") {
        return diagnostics::LogLevel::warning;
    }
    if (value == "error") {
        return diagnostics::LogLevel::error;
    }
    if (value == "critical") {
        return diagnostics::LogLevel::critical;
    }
    return std::nullopt;
}

[[nodiscard]] WindowConfigurationLoadResult window_load_error(
    const WindowConfigurationStatus status,
    const WindowSize fallback,
    std::string detail = {}) {
    WindowConfigurationLoadResult result;
    result.status = status;
    result.size = fallback;
    result.detail = std::move(detail);
    return result;
}

}  // namespace

std::filesystem::path path_from_utf8(const std::string_view value) {
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{utf8};
}

DataDirectoryResolution resolve_data_directory(
    const std::span<const std::string_view> arguments,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& launch_directory) {
    auto candidate = command_line_candidate(arguments, launch_directory);
    if (candidate.status != DataDirectoryStatus::ready ||
        candidate.source == DataDirectorySource::command_line) {
        return validate_candidate(std::move(candidate));
    }
    candidate = configuration_candidate(executable_directory, launch_directory);
    return validate_candidate(std::move(candidate));
}

bool activate_data_directory(
    const std::filesystem::path& directory, std::error_code& error) noexcept {
    std::filesystem::current_path(directory, error);
    return !error;
}

std::string_view data_directory_status_message(const DataDirectoryStatus status) noexcept {
    switch (status) {
    case DataDirectoryStatus::ready:
        return "ready";
    case DataDirectoryStatus::missing_command_line_value:
        return "--data-dir requires a directory";
    case DataDirectoryStatus::empty_command_line_value:
        return "--data-dir cannot be empty";
    case DataDirectoryStatus::configuration_read_failed:
        return "cannot read openlegend.toml";
    case DataDirectoryStatus::configuration_parse_failed:
        return "invalid openlegend.toml";
    case DataDirectoryStatus::configuration_paths_not_table:
        return "openlegend.toml paths must be a table";
    case DataDirectoryStatus::configuration_value_not_string:
        return "openlegend.toml paths.data_dir must be a string";
    case DataDirectoryStatus::empty_configuration_value:
        return "openlegend.toml paths.data_dir cannot be empty";
    case DataDirectoryStatus::directory_query_failed:
        return "cannot inspect the game data directory";
    case DataDirectoryStatus::directory_not_found_or_not_directory:
        return "game data directory does not exist or is not a directory";
    }
    return "unknown data directory error";
}

WindowConfigurationLoadResult load_window_configuration(
    const std::filesystem::path& configuration_path, const WindowSize fallback) {
    toml::table document;
    WindowConfigurationStatus status = WindowConfigurationStatus::ready;
    std::string detail;
    if (!read_existing_document(configuration_path, document, status, detail)) {
        return window_load_error(status, fallback, std::move(detail));
    }
    const toml::node* window_node = document.get("window");
    if (window_node == nullptr) {
        return WindowConfigurationLoadResult{
            WindowConfigurationStatus::ready, fallback, false, false, {}};
    }
    const toml::table* window = window_node->as_table();
    if (window == nullptr) {
        return window_load_error(WindowConfigurationStatus::invalid_window_table, fallback);
    }
    const auto width = (*window)["width"].value<std::int64_t>();
    const auto height = (*window)["height"].value<std::int64_t>();
    if (!width.has_value() || !height.has_value() || !valid_dimension(*width) ||
        !valid_dimension(*height)) {
        return window_load_error(WindowConfigurationStatus::invalid_window_size, fallback);
    }
    bool maximized = false;
    if (const toml::node* maximized_node = window->get("maximized");
        maximized_node != nullptr) {
        const std::optional<bool> value = maximized_node->value<bool>();
        if (!value.has_value()) {
            return window_load_error(WindowConfigurationStatus::invalid_window_state, fallback);
        }
        maximized = *value;
    }
    return WindowConfigurationLoadResult{
        WindowConfigurationStatus::ready,
        WindowSize{static_cast<int>(*width), static_cast<int>(*height)},
        maximized,
        true,
        {}};
}

WindowConfigurationStatus save_window_configuration(
    const std::filesystem::path& configuration_path,
    const WindowSize size,
    const bool maximized,
    std::string& detail) {
    detail.clear();
    if (size.width <= 0 || size.height <= 0) {
        return WindowConfigurationStatus::invalid_window_size;
    }
    toml::table document;
    WindowConfigurationStatus status = WindowConfigurationStatus::ready;
    if (!read_existing_document(configuration_path, document, status, detail)) {
        return status;
    }
    toml::table* window = document["window"].as_table();
    if (window == nullptr) {
        document.insert_or_assign("window", toml::table{});
        window = document["window"].as_table();
    }
    window->insert_or_assign("width", size.width);
    window->insert_or_assign("height", size.height);
    window->insert_or_assign("maximized", maximized);

    std::ofstream output{configuration_path, std::ios::binary | std::ios::trunc};
    if (!output) {
        detail = configuration_path.string();
        return WindowConfigurationStatus::write_failed;
    }
    output << document << '\n';
    if (!output) {
        detail = configuration_path.string();
        return WindowConfigurationStatus::write_failed;
    }
    return WindowConfigurationStatus::ready;
}

LoggingConfigurationLoadResult load_logging_configuration(
    const std::filesystem::path& configuration_path,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& fallback_path,
    const diagnostics::LogLevel fallback_level) {
    LoggingConfigurationLoadResult result;
    result.path = fallback_path;
    result.minimum_level = fallback_level;

    toml::table document;
    if (!read_existing_document(
            configuration_path, document, result.status, result.detail)) {
        return result;
    }
    const toml::node* logging_node = document.get("logging");
    if (logging_node == nullptr) {
        return result;
    }
    const toml::table* logging = logging_node->as_table();
    if (logging == nullptr) {
        result.status = LoggingConfigurationStatus::invalid_logging_table;
        return result;
    }
    if (const toml::node* path_node = logging->get("path"); path_node != nullptr) {
        const auto value = path_node->value<std::string>();
        if (!value.has_value() || value->empty()) {
            result.status = LoggingConfigurationStatus::invalid_log_path;
            return result;
        }
        auto configured_path = path_from_utf8(*value);
        if (configured_path.is_relative()) {
            configured_path = executable_directory / configured_path;
        }
        result.path = configured_path.lexically_normal();
    }
    if (const toml::node* level_node = logging->get("level"); level_node != nullptr) {
        const auto value = level_node->value<std::string>();
        if (!value.has_value()) {
            result.status = LoggingConfigurationStatus::invalid_log_level;
            return result;
        }
        const auto parsed = parse_log_level(*value);
        if (!parsed.has_value()) {
            result.status = LoggingConfigurationStatus::invalid_log_level;
            result.detail = *value;
            return result;
        }
        result.minimum_level = *parsed;
    }
    result.loaded_from_file = true;
    return result;
}

std::filesystem::path make_session_log_path(
    const std::filesystem::path& configured_path,
    const std::chrono::system_clock::time_point launch_time,
    const std::uint64_t process_id) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(launch_time);
    std::tm utc{};
#if defined(_WIN32)
    const bool converted = gmtime_s(&utc, &seconds) == 0;
#else
    const bool converted = gmtime_r(&seconds, &utc) != nullptr;
#endif

    char suffix[64]{};
    if (converted) {
        static_cast<void>(std::snprintf(
            suffix,
            sizeof(suffix),
            "-%04d-%02d-%02d_%02d-%02d-%02d-%llu",
            utc.tm_year + 1900,
            utc.tm_mon + 1,
            utc.tm_mday,
            utc.tm_hour,
            utc.tm_min,
            utc.tm_sec,
            static_cast<unsigned long long>(process_id)));
    } else {
        static_cast<void>(std::snprintf(
            suffix,
            sizeof(suffix),
            "-0000-00-00_00-00-00-%llu",
            static_cast<unsigned long long>(process_id)));
    }

    auto filename = configured_path.stem();
    filename += path_from_utf8(suffix);
    filename += configured_path.extension();
    return configured_path.parent_path() / filename;
}

std::string_view logging_configuration_status_message(
    const LoggingConfigurationStatus status) noexcept {
    switch (status) {
    case LoggingConfigurationStatus::ready:
        return "ready";
    case LoggingConfigurationStatus::read_failed:
        return "cannot read openlegend.toml";
    case LoggingConfigurationStatus::parse_failed:
        return "cannot parse openlegend.toml";
    case LoggingConfigurationStatus::invalid_logging_table:
        return "[logging] must be a TOML table";
    case LoggingConfigurationStatus::invalid_log_path:
        return "[logging] path must be a non-empty string";
    case LoggingConfigurationStatus::invalid_log_level:
        return "[logging] level must be trace, debug, info, warning, error, or critical";
    }
    return "unknown logging configuration status";
}

std::string_view window_configuration_status_message(
    const WindowConfigurationStatus status) noexcept {
    switch (status) {
    case WindowConfigurationStatus::ready:
        return "ready";
    case WindowConfigurationStatus::read_failed:
        return "cannot read openlegend.toml";
    case WindowConfigurationStatus::parse_failed:
        return "cannot parse openlegend.toml";
    case WindowConfigurationStatus::invalid_window_table:
        return "[window] must be a TOML table";
    case WindowConfigurationStatus::invalid_window_size:
        return "[window] width and height must be positive integers";
    case WindowConfigurationStatus::invalid_window_state:
        return "[window] maximized must be a boolean";
    case WindowConfigurationStatus::write_failed:
        return "cannot write openlegend.toml";
    }
    return "unknown window configuration status";
}

}  // namespace openlegend::app
