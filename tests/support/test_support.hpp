#pragma once

#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace openlegend::test {

inline int failures = 0;

class ScopedTimeZone {
public:
    explicit ScopedTimeZone(const std::string_view time_zone)
        : previous_(read()) {
        apply(std::string{time_zone}.c_str());
    }

    ~ScopedTimeZone() {
        apply(previous_.has_value() ? previous_->c_str() : nullptr, false);
    }

    ScopedTimeZone(const ScopedTimeZone&) = delete;
    ScopedTimeZone& operator=(const ScopedTimeZone&) = delete;

private:
    [[nodiscard]] static std::optional<std::string> read() {
#if defined(_WIN32)
        char* raw_value = nullptr;
        std::size_t value_size = 0U;
        if (::_dupenv_s(&raw_value, &value_size, "TZ") != 0) {
            throw std::runtime_error("TZ could not be read");
        }
        const auto value = std::unique_ptr<char, decltype(&std::free)>{raw_value, &std::free};
        if (value == nullptr || value_size <= 1U || *value == '\0') {
            return std::nullopt;
        }
        return std::string{value.get()};
#else
        const char* value = std::getenv("TZ");
        return value == nullptr ? std::nullopt : std::optional<std::string>{value};
#endif
    }

    static void apply(const char* value, const bool throw_on_error = true) {
#if defined(_WIN32)
        const auto result = ::_putenv_s("TZ", value == nullptr ? "" : value);
        ::_tzset();
#else
        const auto result = value == nullptr ? ::unsetenv("TZ") : ::setenv("TZ", value, 1);
        ::tzset();
#endif
        if (result != 0 && throw_on_error) {
            throw std::runtime_error("TZ could not be changed");
        }
    }

    std::optional<std::string> previous_;
};

struct TestShard {
    std::size_t index = 0U;
    std::size_t count = 1U;

    [[nodiscard]] bool includes(const std::size_t check_index) const noexcept {
        return check_index % count == index;
    }
};

inline std::size_t parse_test_shard_argument(const char* value) {
    const std::string_view text{value == nullptr ? "" : value};
    if (text.empty()) {
        throw std::invalid_argument("test shard arguments must be decimal integers");
    }

    std::size_t result = 0U;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            throw std::invalid_argument("test shard arguments must be decimal integers");
        }
        const auto digit = static_cast<std::size_t>(character - '0');
        if (result > (std::numeric_limits<std::size_t>::max() - digit) / 10U) {
            throw std::invalid_argument("test shard argument is too large");
        }
        result = result * 10U + digit;
    }
    return result;
}

inline TestShard test_shard(const int argc, char* argv[]) {
    if (argc == 1) {
        return {};
    }
    if (argc != 3) {
        throw std::invalid_argument(
            "expected optional test shard arguments: SHARD_INDEX SHARD_COUNT");
    }

    const auto index = parse_test_shard_argument(argv[1]);
    const auto count = parse_test_shard_argument(argv[2]);
    if (count == 0U || index >= count) {
        throw std::invalid_argument("test shard index must be smaller than shard count");
    }
    return TestShard{index, count};
}

inline std::filesystem::path utf8_path(const std::string_view value) {
#if defined(_WIN32)
    std::u8string utf8;
    utf8.reserve(value.size());
    for (const char character : value) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));
    }
    return std::filesystem::path{utf8};
#else
    return std::filesystem::path{value};
#endif
}

inline std::filesystem::path game_data_root() {
#if defined(_WIN32)
    wchar_t* raw_value = nullptr;
    std::size_t value_size = 0U;
    const auto result = ::_wdupenv_s(
        &raw_value, &value_size, L"OPENLEGEND_GAME_DATA_ROOT");
    const auto value =
        std::unique_ptr<wchar_t, decltype(&std::free)>{raw_value, &std::free};
    if (result != 0) {
        throw std::runtime_error("OPENLEGEND_GAME_DATA_ROOT could not be read");
    }
    if (value == nullptr || value_size <= 1U || *value == L'\0') {
        throw std::runtime_error("OPENLEGEND_GAME_DATA_ROOT is not set");
    }
    return std::filesystem::path{value.get()};
#else
    const char* value = std::getenv("OPENLEGEND_GAME_DATA_ROOT");
    if (value == nullptr || *value == '\0') {
        throw std::runtime_error("OPENLEGEND_GAME_DATA_ROOT is not set");
    }
    return utf8_path(value);
#endif
}

inline void check(const bool condition, const std::string_view expression, const char* file, const int line) {
    if (!condition) {
        ++failures;
        std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
    }
}

}  // namespace openlegend::test

#define OL_CHECK(expression) \
    ::openlegend::test::check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
