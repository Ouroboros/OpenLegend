#include "openlegend/app/runtime_configuration.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

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

enum class ConfigurationDocumentStatus {
    ready,
    read_failed,
    parse_failed,
};

struct ConfigurationDocument {
    ConfigurationDocumentStatus status{ConfigurationDocumentStatus::ready};
    toml::table values;
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
    const ConfigurationDocument& document,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& launch_directory) {
    if (document.status == ConfigurationDocumentStatus::read_failed) {
        return candidate_error(
            DataDirectoryStatus::configuration_read_failed,
            DataDirectorySource::configuration_file,
            document.detail);
    }
    if (document.status == ConfigurationDocumentStatus::parse_failed) {
        return candidate_error(
            DataDirectoryStatus::configuration_parse_failed,
            DataDirectorySource::configuration_file,
            document.detail);
    }

    const toml::node* paths_node =
        document.values.get(PathsConfigurationLoadResult::toml_table_name);
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
    const toml::node* data_directory_node =
        paths->get(PathsConfigurationLoadResult::data_directory_toml_key);
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

[[nodiscard]] SaveDirectoryConfigurationLoadResult save_directory_configuration_from_document(
    const toml::table& document,
    const std::filesystem::path& executable_directory) {
    SaveDirectoryConfigurationLoadResult result;
    const toml::node* paths_node =
        document.get(PathsConfigurationLoadResult::toml_table_name);
    if (paths_node == nullptr) {
        return result;
    }
    const toml::table* paths = paths_node->as_table();
    if (paths == nullptr) {
        result.status = SaveDirectoryConfigurationStatus::invalid_paths_table;
        return result;
    }
    const toml::node* save_directory_node =
        paths->get(PathsConfigurationLoadResult::save_directory_toml_key);
    if (save_directory_node == nullptr) {
        return result;
    }
    const auto value = save_directory_node->value<std::string>();
    if (!value.has_value() || value->empty()) {
        result.status = SaveDirectoryConfigurationStatus::invalid_save_directory;
        return result;
    }
    auto directory = path_from_utf8(*value);
    if (directory.is_relative()) {
        directory = executable_directory / directory;
    }
    std::error_code error;
    directory = std::filesystem::absolute(directory, error).lexically_normal();
    if (error) {
        result.status = SaveDirectoryConfigurationStatus::directory_query_failed;
        result.detail = error.message();
        return result;
    }
    result.directory = std::move(directory);
    result.configured = true;
    return result;
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

[[nodiscard]] ConfigurationDocument read_configuration_document(
    const std::filesystem::path& configuration_path) {
    ConfigurationDocument document;
    std::error_code error;
    const bool exists = std::filesystem::exists(configuration_path, error);
    if (error) {
        document.status = ConfigurationDocumentStatus::read_failed;
        document.detail = error.message();
        return document;
    }
    if (!exists) {
        return document;
    }
    std::ifstream input{configuration_path, std::ios::binary};
    if (!input) {
        document.status = ConfigurationDocumentStatus::read_failed;
        document.detail = configuration_path.string();
        return document;
    }
    try {
        document.values = toml::parse(input, configuration_path.string());
    } catch (const toml::parse_error& parse_error) {
        document.status = ConfigurationDocumentStatus::parse_failed;
        document.detail = std::string{parse_error.description()};
    }
    return document;
}

template <typename Status>
[[nodiscard]] Status configuration_load_status(
    const ConfigurationDocumentStatus status) noexcept {
    switch (status) {
    case ConfigurationDocumentStatus::ready:
        return Status::ready;
    case ConfigurationDocumentStatus::read_failed:
        return Status::read_failed;
    case ConfigurationDocumentStatus::parse_failed:
        return Status::parse_failed;
    }
    return Status::parse_failed;
}

[[nodiscard]] bool valid_dimension(const std::int64_t value) noexcept {
    return value > 0 && value <= static_cast<std::int64_t>(std::numeric_limits<int>::max());
}

[[nodiscard]] bool valid_game_resolution_dimension(
    const std::int64_t value,
    const int minimum,
    const int maximum) noexcept {
    return value >= static_cast<std::int64_t>(minimum) &&
        value <= static_cast<std::int64_t>(maximum);
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

using ConfigurationKeyPath = std::vector<std::string>;

[[nodiscard]] std::span<const std::string_view> configuration_key_order(
    const ConfigurationKeyPath& table_path) noexcept {
    if (table_path.empty()) {
        return RuntimeConfiguration::toml_table_order;
    }
    if (table_path.size() != 1U) {
        return {};
    }
    if (table_path.front() == PathsConfigurationLoadResult::toml_table_name) {
        return PathsConfigurationLoadResult::toml_field_order;
    }
    if (table_path.front() == LoggingConfigurationLoadResult::toml_table_name) {
        return LoggingConfigurationLoadResult::toml_field_order;
    }
    if (table_path.front() == InputConfigurationLoadResult::toml_table_name) {
        return InputConfigurationLoadResult::toml_field_order;
    }
    if (table_path.front() == TimingConfigurationLoadResult::toml_table_name) {
        return TimingConfigurationLoadResult::toml_field_order;
    }
    if (table_path.front() == WindowConfigurationLoadResult::toml_table_name) {
        return WindowConfigurationLoadResult::toml_field_order;
    }
    if (table_path.front() == DisplayConfigurationLoadResult::toml_table_name) {
        return DisplayConfigurationLoadResult::toml_field_order;
    }
    return {};
}

[[nodiscard]] std::vector<std::string_view> ordered_keys(
    const toml::table& table, const ConfigurationKeyPath& table_path) {
    const auto key_order = configuration_key_order(table_path);
    std::vector<std::string_view> keys;
    keys.reserve(key_order.size());
    for (const auto key : key_order) {
        if (table.get(key) != nullptr) {
            keys.push_back(key);
        }
    }
    return keys;
}

[[nodiscard]] bool is_bare_key(const std::string_view key) noexcept {
    if (key.empty()) {
        return false;
    }
    return std::ranges::all_of(key, [](const unsigned char character) {
        return (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9') || character == '_' || character == '-';
    });
}

void write_key(std::ostream& output, const std::string_view key) {
    if (is_bare_key(key)) {
        output << key;
        return;
    }

    static constexpr std::string_view hexadecimal = "0123456789ABCDEF";
    output.put('"');
    for (const char byte : key) {
        const auto character = static_cast<unsigned char>(byte);
        switch (character) {
        case '\b':
            output << "\\b";
            break;
        case '\t':
            output << "\\t";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\r':
            output << "\\r";
            break;
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        default:
            if (character < 0x20U || character == 0x7FU) {
                output << "\\u00" << hexadecimal[character >> 4U]
                       << hexadecimal[character & 0x0FU];
            } else {
                output.put(static_cast<char>(character));
            }
            break;
        }
    }
    output.put('"');
}

void write_key_path(std::ostream& output, const ConfigurationKeyPath& key_path) {
    for (std::size_t index = 0U; index < key_path.size(); ++index) {
        if (index != 0U) {
            output.put('.');
        }
        write_key(output, key_path[index]);
    }
}

[[nodiscard]] bool is_child_table(const toml::node& node) noexcept {
    const auto* table = node.as_table();
    return table != nullptr && !table->is_inline();
}

[[nodiscard]] bool is_table_array(const toml::node& node) noexcept {
    const auto* array = node.as_array();
    if (array == nullptr || !array->is_array_of_tables() || array->empty()) {
        return false;
    }
    const auto* first_table = (*array)[0U].as_table();
    return first_table != nullptr && !first_table->is_inline();
}

void write_table_contents(
    std::ostream& output,
    const toml::table& table,
    const ConfigurationKeyPath& table_path,
    bool& has_output) {
    const auto keys = ordered_keys(table, table_path);
    for (const auto key : keys) {
        const toml::node* value = table.get(key);
        if (value == nullptr || is_child_table(*value) || is_table_array(*value)) {
            continue;
        }
        write_key(output, key);
        output << " = " << toml::toml_formatter{*value} << '\n';
        has_output = true;
    }

    for (const auto key : keys) {
        const toml::node* value = table.get(key);
        if (value == nullptr || !is_child_table(*value)) {
            continue;
        }
        auto child_path = table_path;
        child_path.emplace_back(key);
        if (has_output) {
            output.put('\n');
        }
        output.put('[');
        write_key_path(output, child_path);
        output << "]\n";
        has_output = true;
        write_table_contents(output, *value->as_table(), child_path, has_output);
    }

    for (const auto key : keys) {
        const toml::node* value = table.get(key);
        if (value == nullptr || !is_table_array(*value)) {
            continue;
        }
        auto child_path = table_path;
        child_path.emplace_back(key);
        for (const auto& element : *value->as_array()) {
            if (has_output) {
                output.put('\n');
            }
            output << "[[";
            write_key_path(output, child_path);
            output << "]]\n";
            has_output = true;
            write_table_contents(output, *element.as_table(), child_path, has_output);
        }
    }
}

void write_configuration_document(std::ostream& output, const toml::table& document) {
    bool has_output = false;
    write_table_contents(output, document, {}, has_output);
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

[[nodiscard]] WindowConfigurationLoadResult window_configuration_from_document(
    const toml::table& document, const WindowSize fallback) {
    const toml::node* window_node =
        document.get(WindowConfigurationLoadResult::toml_table_name);
    if (window_node == nullptr) {
        return WindowConfigurationLoadResult{
            WindowConfigurationStatus::ready, fallback, false, false, {}};
    }
    const toml::table* window = window_node->as_table();
    if (window == nullptr) {
        return window_load_error(WindowConfigurationStatus::invalid_window_table, fallback);
    }
    const auto width =
        (*window)[WindowConfigurationLoadResult::width_toml_key].value<std::int64_t>();
    const auto height =
        (*window)[WindowConfigurationLoadResult::height_toml_key].value<std::int64_t>();
    if (!width.has_value() || !height.has_value() || !valid_dimension(*width) ||
        !valid_dimension(*height)) {
        return window_load_error(WindowConfigurationStatus::invalid_window_size, fallback);
    }
    bool maximized = false;
    if (const toml::node* maximized_node =
            window->get(WindowConfigurationLoadResult::maximized_toml_key);
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

[[nodiscard]] DisplayConfigurationLoadResult display_load_error(
    const DisplayConfigurationStatus status,
    const GameResolution fallback,
    std::string detail = {}) {
    DisplayConfigurationLoadResult result;
    result.status = status;
    result.resolution = fallback;
    result.detail = std::move(detail);
    return result;
}

[[nodiscard]] DisplayConfigurationLoadResult display_configuration_from_document(
    const toml::table& document,
    const GameResolution fallback) {
    const toml::node* display_node =
        document.get(DisplayConfigurationLoadResult::toml_table_name);
    if (display_node == nullptr) {
        return DisplayConfigurationLoadResult{
            DisplayConfigurationStatus::ready, fallback, false, {}};
    }
    const toml::table* display = display_node->as_table();
    if (display == nullptr) {
        return display_load_error(
            DisplayConfigurationStatus::invalid_display_table, fallback);
    }
    const auto* width_node =
        display->get(DisplayConfigurationLoadResult::width_toml_key);
    const auto* height_node =
        display->get(DisplayConfigurationLoadResult::height_toml_key);
    if (width_node == nullptr && height_node == nullptr) {
        return DisplayConfigurationLoadResult{
            DisplayConfigurationStatus::ready, fallback, false, {}};
    }
    if (width_node == nullptr || height_node == nullptr) {
        return display_load_error(
            DisplayConfigurationStatus::invalid_game_resolution, fallback);
    }
    const auto width = width_node->value<std::int64_t>();
    const auto height = height_node->value<std::int64_t>();
    if (!width.has_value() || !height.has_value() ||
        !valid_game_resolution_dimension(
            *width, kMinimumGameWidth, kMaximumGameWidth) ||
        !valid_game_resolution_dimension(
            *height, kMinimumGameHeight, kMaximumGameHeight)) {
        return display_load_error(
            DisplayConfigurationStatus::invalid_game_resolution, fallback);
    }
    return DisplayConfigurationLoadResult{
        DisplayConfigurationStatus::ready,
        GameResolution{static_cast<int>(*width), static_cast<int>(*height)},
        true,
        {}};
}

[[nodiscard]] InputConfigurationLoadResult input_configuration_from_document(
    const toml::table& document,
    const std::chrono::milliseconds fallback_movement_repeat_delay,
    const std::chrono::milliseconds fallback_menu_repeat_delay,
    const std::chrono::milliseconds fallback_menu_repeat_interval) {
    InputConfigurationLoadResult result;
    result.movement_repeat_delay = fallback_movement_repeat_delay;
    result.menu_repeat_delay = fallback_menu_repeat_delay;
    result.menu_repeat_interval = fallback_menu_repeat_interval;
    const toml::node* input_node =
        document.get(InputConfigurationLoadResult::toml_table_name);
    if (input_node == nullptr) {
        return result;
    }
    const toml::table* input = input_node->as_table();
    if (input == nullptr) {
        result.status = InputConfigurationStatus::invalid_input_table;
        return result;
    }
    if (const toml::node* delay_node =
            input->get(InputConfigurationLoadResult::movement_repeat_delay_toml_key);
        delay_node != nullptr) {
        const auto delay = delay_node->value<std::int64_t>();
        if (!delay.has_value() || *delay < 0) {
            result.status = InputConfigurationStatus::invalid_movement_repeat_delay;
            return result;
        }
        result.movement_repeat_delay = std::chrono::milliseconds{*delay};
    }
    if (const toml::node* delay_node =
            input->get(InputConfigurationLoadResult::menu_repeat_delay_toml_key);
        delay_node != nullptr) {
        const auto delay = delay_node->value<std::int64_t>();
        if (!delay.has_value() || *delay < 0) {
            result.status = InputConfigurationStatus::invalid_menu_repeat_delay;
            return result;
        }
        result.menu_repeat_delay = std::chrono::milliseconds{*delay};
    }
    if (const toml::node* interval_node =
            input->get(InputConfigurationLoadResult::menu_repeat_interval_toml_key);
        interval_node != nullptr) {
        const auto interval = interval_node->value<std::int64_t>();
        if (!interval.has_value() || *interval <= 0) {
            result.status = InputConfigurationStatus::invalid_menu_repeat_interval;
            return result;
        }
        result.menu_repeat_interval = std::chrono::milliseconds{*interval};
    }
    result.loaded_from_file = true;
    return result;
}

[[nodiscard]] TimingConfigurationLoadResult timing_configuration_from_document(
    const toml::table& document,
    const std::chrono::nanoseconds fallback_fade_frame_delay) {
    TimingConfigurationLoadResult result;
    result.fade_frame_delay = fallback_fade_frame_delay;
    const toml::node* timing_node =
        document.get(TimingConfigurationLoadResult::toml_table_name);
    if (timing_node == nullptr) {
        return result;
    }
    const toml::table* timing = timing_node->as_table();
    if (timing == nullptr) {
        result.status = TimingConfigurationStatus::invalid_timing_table;
        return result;
    }
    if (const toml::node* delay_node =
            timing->get(TimingConfigurationLoadResult::fade_frame_delay_toml_key);
        delay_node != nullptr) {
        std::optional<long double> delay_ms;
        if (const auto* integer = delay_node->as_integer(); integer != nullptr) {
            delay_ms = static_cast<long double>(integer->get());
        } else if (const auto* floating = delay_node->as_floating_point();
                   floating != nullptr) {
            delay_ms = static_cast<long double>(floating->get());
        }
        const auto maximum_ms = std::chrono::duration<long double, std::milli>{
            std::chrono::nanoseconds::max()}.count();
        if (!delay_ms.has_value() || !std::isfinite(*delay_ms) || *delay_ms < 0.0L ||
            *delay_ms > maximum_ms) {
            result.status = TimingConfigurationStatus::invalid_fade_frame_delay;
            return result;
        }
        result.fade_frame_delay = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::duration<long double, std::milli>{*delay_ms});
    }
    result.loaded_from_file = true;
    return result;
}

[[nodiscard]] LoggingConfigurationLoadResult logging_configuration_from_document(
    const toml::table& document,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& fallback_path,
    const diagnostics::LogLevel fallback_level) {
    LoggingConfigurationLoadResult result;
    result.path = fallback_path;
    result.minimum_level = fallback_level;
    const toml::node* logging_node =
        document.get(LoggingConfigurationLoadResult::toml_table_name);
    if (logging_node == nullptr) {
        return result;
    }
    const toml::table* logging = logging_node->as_table();
    if (logging == nullptr) {
        result.status = LoggingConfigurationStatus::invalid_logging_table;
        return result;
    }
    if (const toml::node* path_node =
            logging->get(LoggingConfigurationLoadResult::path_toml_key);
        path_node != nullptr) {
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
    if (const toml::node* level_node =
            logging->get(LoggingConfigurationLoadResult::level_toml_key);
        level_node != nullptr) {
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
    const auto document =
        read_configuration_document(executable_directory / kConfigurationFilename);
    candidate = configuration_candidate(document, executable_directory, launch_directory);
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

std::string_view save_directory_configuration_status_message(
    const SaveDirectoryConfigurationStatus status) noexcept {
    switch (status) {
    case SaveDirectoryConfigurationStatus::ready:
        return "ready";
    case SaveDirectoryConfigurationStatus::read_failed:
        return "cannot read openlegend.toml";
    case SaveDirectoryConfigurationStatus::parse_failed:
        return "cannot parse openlegend.toml";
    case SaveDirectoryConfigurationStatus::invalid_paths_table:
        return "[paths] must be a TOML table";
    case SaveDirectoryConfigurationStatus::invalid_save_directory:
        return "[paths] save_dir must be a non-empty string";
    case SaveDirectoryConfigurationStatus::directory_query_failed:
        return "cannot resolve [paths] save_dir";
    }
    return "unknown save directory configuration status";
}

WindowConfigurationLoadResult load_window_configuration(
    const std::filesystem::path& configuration_path, const WindowSize fallback) {
    const auto document = read_configuration_document(configuration_path);
    if (document.status != ConfigurationDocumentStatus::ready) {
        return window_load_error(
            configuration_load_status<WindowConfigurationStatus>(document.status),
            fallback,
            document.detail);
    }
    return window_configuration_from_document(document.values, fallback);
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
    auto loaded_document = read_configuration_document(configuration_path);
    if (loaded_document.status != ConfigurationDocumentStatus::ready) {
        detail = std::move(loaded_document.detail);
        return configuration_load_status<WindowConfigurationStatus>(loaded_document.status);
    }
    auto document = std::move(loaded_document.values);
    toml::table* window =
        document[WindowConfigurationLoadResult::toml_table_name].as_table();
    if (window == nullptr) {
        document.insert_or_assign(
            WindowConfigurationLoadResult::toml_table_name, toml::table{});
        window = document[WindowConfigurationLoadResult::toml_table_name].as_table();
    }
    window->insert_or_assign(WindowConfigurationLoadResult::width_toml_key, size.width);
    window->insert_or_assign(WindowConfigurationLoadResult::height_toml_key, size.height);
    window->insert_or_assign(
        WindowConfigurationLoadResult::maximized_toml_key, maximized);

    std::ofstream output{configuration_path, std::ios::binary | std::ios::trunc};
    if (!output) {
        detail = configuration_path.string();
        return WindowConfigurationStatus::write_failed;
    }
    write_configuration_document(output, document);
    if (!output) {
        detail = configuration_path.string();
        return WindowConfigurationStatus::write_failed;
    }
    return WindowConfigurationStatus::ready;
}

DisplayConfigurationLoadResult load_display_configuration(
    const std::filesystem::path& configuration_path,
    const GameResolution fallback) {
    const auto document = read_configuration_document(configuration_path);
    if (document.status != ConfigurationDocumentStatus::ready) {
        return display_load_error(
            configuration_load_status<DisplayConfigurationStatus>(document.status),
            fallback,
            document.detail);
    }
    return display_configuration_from_document(document.values, fallback);
}

InputConfigurationLoadResult load_input_configuration(
    const std::filesystem::path& configuration_path,
    const std::chrono::milliseconds fallback_movement_repeat_delay,
    const std::chrono::milliseconds fallback_menu_repeat_delay,
    const std::chrono::milliseconds fallback_menu_repeat_interval) {
    const auto document = read_configuration_document(configuration_path);
    if (document.status != ConfigurationDocumentStatus::ready) {
        InputConfigurationLoadResult result;
        result.status = configuration_load_status<InputConfigurationStatus>(document.status);
        result.movement_repeat_delay = fallback_movement_repeat_delay;
        result.menu_repeat_delay = fallback_menu_repeat_delay;
        result.menu_repeat_interval = fallback_menu_repeat_interval;
        result.detail = document.detail;
        return result;
    }
    return input_configuration_from_document(
        document.values,
        fallback_movement_repeat_delay,
        fallback_menu_repeat_delay,
        fallback_menu_repeat_interval);
}

TimingConfigurationLoadResult load_timing_configuration(
    const std::filesystem::path& configuration_path,
    const std::chrono::nanoseconds fallback_fade_frame_delay) {
    const auto document = read_configuration_document(configuration_path);
    if (document.status != ConfigurationDocumentStatus::ready) {
        TimingConfigurationLoadResult result;
        result.status = configuration_load_status<TimingConfigurationStatus>(document.status);
        result.fade_frame_delay = fallback_fade_frame_delay;
        result.detail = document.detail;
        return result;
    }
    return timing_configuration_from_document(document.values, fallback_fade_frame_delay);
}

LoggingConfigurationLoadResult load_logging_configuration(
    const std::filesystem::path& configuration_path,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& fallback_path,
    const diagnostics::LogLevel fallback_level) {
    const auto document = read_configuration_document(configuration_path);
    if (document.status != ConfigurationDocumentStatus::ready) {
        LoggingConfigurationLoadResult result;
        result.status = configuration_load_status<LoggingConfigurationStatus>(document.status);
        result.path = fallback_path;
        result.minimum_level = fallback_level;
        result.detail = document.detail;
        return result;
    }
    return logging_configuration_from_document(
        document.values, executable_directory, fallback_path, fallback_level);
}

RuntimeConfiguration load_runtime_configuration(
    const std::span<const std::string_view> arguments,
    const std::filesystem::path& configuration_path,
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& launch_directory,
    const RuntimeConfigurationDefaults& defaults) {
    const auto document = read_configuration_document(configuration_path);

    RuntimeConfiguration configuration;
    auto data_directory_candidate = command_line_candidate(arguments, launch_directory);
    if (data_directory_candidate.status == DataDirectoryStatus::ready &&
        data_directory_candidate.source != DataDirectorySource::command_line) {
        data_directory_candidate =
            configuration_candidate(document, executable_directory, launch_directory);
    }
    configuration.paths.data_directory =
        validate_candidate(std::move(data_directory_candidate));

    if (document.status == ConfigurationDocumentStatus::ready) {
        configuration.paths.save_directory = save_directory_configuration_from_document(
            document.values, executable_directory);
        configuration.logging = logging_configuration_from_document(
            document.values,
            executable_directory,
            defaults.logging_path,
            defaults.logging_level);
        configuration.display = display_configuration_from_document(
            document.values, defaults.game_resolution);
        configuration.input = input_configuration_from_document(
            document.values,
            defaults.movement_repeat_delay,
            defaults.menu_repeat_delay,
            defaults.menu_repeat_interval);
        configuration.timing = timing_configuration_from_document(
            document.values, defaults.fade_frame_delay);
        configuration.window =
            window_configuration_from_document(document.values, defaults.window_size);
        return configuration;
    }

    configuration.paths.save_directory.status =
        configuration_load_status<SaveDirectoryConfigurationStatus>(document.status);
    configuration.paths.save_directory.detail = document.detail;

    configuration.logging.status =
        configuration_load_status<LoggingConfigurationStatus>(document.status);
    configuration.logging.path = defaults.logging_path;
    configuration.logging.minimum_level = defaults.logging_level;
    configuration.logging.detail = document.detail;

    configuration.display = display_load_error(
        configuration_load_status<DisplayConfigurationStatus>(document.status),
        defaults.game_resolution,
        document.detail);

    configuration.input.status =
        configuration_load_status<InputConfigurationStatus>(document.status);
    configuration.input.movement_repeat_delay = defaults.movement_repeat_delay;
    configuration.input.menu_repeat_delay = defaults.menu_repeat_delay;
    configuration.input.menu_repeat_interval = defaults.menu_repeat_interval;
    configuration.input.detail = document.detail;

    configuration.timing.status =
        configuration_load_status<TimingConfigurationStatus>(document.status);
    configuration.timing.fade_frame_delay = defaults.fade_frame_delay;
    configuration.timing.detail = document.detail;

    configuration.window = window_load_error(
        configuration_load_status<WindowConfigurationStatus>(document.status),
        defaults.window_size,
        document.detail);
    return configuration;
}

std::filesystem::path make_session_log_path(
    const std::filesystem::path& configured_path,
    const std::chrono::system_clock::time_point launch_time,
    const std::uint64_t process_id) {
    const std::time_t seconds = std::chrono::system_clock::to_time_t(launch_time);
    std::tm local{};
#if defined(_WIN32)
    const bool converted = localtime_s(&local, &seconds) == 0;
#else
    const bool converted = localtime_r(&seconds, &local) != nullptr;
#endif

    char suffix[64]{};
    if (converted) {
        static_cast<void>(std::snprintf(
            suffix,
            sizeof(suffix),
            "-%04d-%02d-%02d_%02d-%02d-%02d-%llu",
            local.tm_year + 1900,
            local.tm_mon + 1,
            local.tm_mday,
            local.tm_hour,
            local.tm_min,
            local.tm_sec,
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

std::string_view input_configuration_status_message(
    const InputConfigurationStatus status) noexcept {
    switch (status) {
    case InputConfigurationStatus::ready:
        return "ready";
    case InputConfigurationStatus::read_failed:
        return "cannot read openlegend.toml";
    case InputConfigurationStatus::parse_failed:
        return "cannot parse openlegend.toml";
    case InputConfigurationStatus::invalid_input_table:
        return "[input] must be a TOML table";
    case InputConfigurationStatus::invalid_movement_repeat_delay:
        return "[input] movement_repeat_delay_ms must be a non-negative integer";
    case InputConfigurationStatus::invalid_menu_repeat_delay:
        return "[input] menu_repeat_delay_ms must be a non-negative integer";
    case InputConfigurationStatus::invalid_menu_repeat_interval:
        return "[input] menu_repeat_interval_ms must be a positive integer";
    }
    return "unknown input configuration status";
}

std::string_view timing_configuration_status_message(
    const TimingConfigurationStatus status) noexcept {
    switch (status) {
    case TimingConfigurationStatus::ready:
        return "ready";
    case TimingConfigurationStatus::read_failed:
        return "cannot read openlegend.toml";
    case TimingConfigurationStatus::parse_failed:
        return "cannot parse openlegend.toml";
    case TimingConfigurationStatus::invalid_timing_table:
        return "[timing] must be a TOML table";
    case TimingConfigurationStatus::invalid_fade_frame_delay:
        return "[timing] fade_frame_delay_ms must be a non-negative number";
    }
    return "unknown timing configuration status";
}

std::string_view display_configuration_status_message(
    const DisplayConfigurationStatus status) noexcept {
    switch (status) {
    case DisplayConfigurationStatus::ready:
        return "ready";
    case DisplayConfigurationStatus::read_failed:
        return "cannot read openlegend.toml";
    case DisplayConfigurationStatus::parse_failed:
        return "cannot parse openlegend.toml";
    case DisplayConfigurationStatus::invalid_display_table:
        return "[display] must be a TOML table";
    case DisplayConfigurationStatus::invalid_game_resolution:
        return "[display] width must be 320..1280 and height must be 200..800";
    }
    return "unknown display configuration status";
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
