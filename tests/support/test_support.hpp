#pragma once

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace openlegend::test {

inline int failures = 0;

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
    const wchar_t* value = ::_wgetenv(L"OPENLEGEND_GAME_DATA_ROOT");
    if (value == nullptr || *value == L'\0') {
        throw std::runtime_error("OPENLEGEND_GAME_DATA_ROOT is not set");
    }
    return std::filesystem::path{value};
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
