#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace openlegend::text {

class Big5TextView {
public:
    explicit Big5TextView(const std::span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    explicit Big5TextView(const std::vector<std::uint8_t>& bytes) noexcept
        : bytes_(bytes) {}

    template <std::size_t Size>
    explicit Big5TextView(const std::array<std::uint8_t, Size>& bytes) noexcept
        : bytes_(bytes) {}

    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept {
        return bytes_;
    }

private:
    std::span<const std::uint8_t> bytes_;
};

[[nodiscard]] std::optional<char32_t> decode_next_utf8(
    std::u8string_view value,
    std::size_t& offset) noexcept;

[[nodiscard]] std::optional<std::uint16_t> big5_code_for_unicode(
    char32_t code_point) noexcept;

[[nodiscard]] bool append_big5(
    std::vector<std::uint8_t>& destination,
    std::u8string_view utf8_text);

[[nodiscard]] std::optional<std::vector<std::uint8_t>> encode_big5(
    std::u8string_view utf8_text);

}  // namespace openlegend::text
