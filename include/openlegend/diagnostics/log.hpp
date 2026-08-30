#pragma once

#include <filesystem>
#include <source_location>
#include <string_view>

namespace openlegend::diagnostics {

enum class LogLevel {
    trace,
    debug,
    info,
    warning,
    error,
    critical,
};

enum class LoggingInitializationStatus {
    initialized,
    directory_creation_failed,
    file_open_failed,
};

[[nodiscard]] LoggingInitializationStatus initialize_logging(
    const std::filesystem::path& file_path,
    LogLevel minimum_level = LogLevel::debug,
    std::source_location location = std::source_location::current()) noexcept;

void shutdown_logging() noexcept;
void set_minimum_log_level(LogLevel level) noexcept;
[[nodiscard]] LogLevel minimum_log_level() noexcept;
[[nodiscard]] bool logging_to_file() noexcept;
[[nodiscard]] std::string_view log_level_name(LogLevel level) noexcept;

void log_message(
    LogLevel level,
    std::string_view message,
    std::source_location location = std::source_location::current()) noexcept;

inline void log_trace(
    const std::string_view message,
    const std::source_location location = std::source_location::current()) noexcept {
    log_message(LogLevel::trace, message, location);
}

inline void log_debug(
    const std::string_view message,
    const std::source_location location = std::source_location::current()) noexcept {
    log_message(LogLevel::debug, message, location);
}

inline void log_info(
    const std::string_view message,
    const std::source_location location = std::source_location::current()) noexcept {
    log_message(LogLevel::info, message, location);
}

inline void log_warning(
    const std::string_view message,
    const std::source_location location = std::source_location::current()) noexcept {
    log_message(LogLevel::warning, message, location);
}

inline void log_error(
    const std::string_view message,
    const std::source_location location = std::source_location::current()) noexcept {
    log_message(LogLevel::error, message, location);
}

inline void log_critical(
    const std::string_view message,
    const std::source_location location = std::source_location::current()) noexcept {
    log_message(LogLevel::critical, message, location);
}

}  // namespace openlegend::diagnostics
