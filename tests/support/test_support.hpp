#pragma once

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace openlegend::test {

inline int failures = 0;

inline std::filesystem::path utf8_path(const std::string_view value) {
#if defined(_WIN32)
    return std::filesystem::u8path(value.begin(), value.end());
#else
    return std::filesystem::path{value};
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
