#include "openlegend/app/runtime_configuration.hpp"

#include <toml++/toml.hpp>

#include <cstdint>
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
