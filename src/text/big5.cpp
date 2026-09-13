#include "openlegend/text/big5.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace openlegend::text {
namespace {

constexpr std::array<std::uint32_t, 13'493U> kUnicodeToBig5{
#include "big5_mapping.inc"
};

[[nodiscard]] constexpr bool continuation(const std::uint8_t value) noexcept {
    return value >= 0x80U && value <= 0xBFU;
}

}  // namespace

std::optional<char32_t> decode_next_utf8(
    const std::u8string_view value,
    std::size_t& offset) noexcept {
    if (offset >= value.size()) {
        return std::nullopt;
    }

    const auto byte = [&value](const std::size_t index) {
        return static_cast<std::uint8_t>(value[index]);
    };
    const auto first = byte(offset);
    if (first <= 0x7FU) {
        ++offset;
        return static_cast<char32_t>(first);
    }

    std::size_t length{};
    char32_t code_point{};
    if (first >= 0xC2U && first <= 0xDFU) {
        length = 2U;
        code_point = static_cast<char32_t>(first & 0x1FU);
    } else if (first >= 0xE0U && first <= 0xEFU) {
        length = 3U;
        code_point = static_cast<char32_t>(first & 0x0FU);
    } else if (first >= 0xF0U && first <= 0xF4U) {
        length = 4U;
        code_point = static_cast<char32_t>(first & 0x07U);
    } else {
        return std::nullopt;
    }
    if (value.size() - offset < length) {
        return std::nullopt;
    }
    for (std::size_t index = 1U; index < length; ++index) {
        const auto next = byte(offset + index);
        if (!continuation(next)) {
            return std::nullopt;
        }
        code_point = static_cast<char32_t>((code_point << 6U) | (next & 0x3FU));
    }
    if ((length == 3U && code_point < 0x800U) ||
        (length == 4U && code_point < 0x10000U) ||
        (code_point >= 0xD800U && code_point <= 0xDFFFU) ||
        code_point > 0x10FFFFU) {
        return std::nullopt;
    }
    offset += length;
    return code_point;
}

std::optional<std::uint16_t> big5_code_for_unicode(const char32_t code_point) noexcept {
    if (code_point <= 0x7FU) {
        return static_cast<std::uint16_t>(code_point);
    }
    if (code_point > 0xFFFFU) {
        return std::nullopt;
    }

    const auto key = static_cast<std::uint32_t>(code_point) << 16U;
    const auto found = std::lower_bound(
        kUnicodeToBig5.begin(),
        kUnicodeToBig5.end(),
        key,
        [](const std::uint32_t mapping, const std::uint32_t value) {
            return (mapping & 0xFFFF0000U) < value;
        });
    if (found == kUnicodeToBig5.end() || (*found >> 16U) != code_point) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(*found & 0xFFFFU);
}

bool append_big5(
    std::vector<std::uint8_t>& destination,
    const std::u8string_view utf8_text) {
    const auto original_size = destination.size();
    destination.reserve(destination.size() + utf8_text.size());
    std::size_t offset = 0U;
    while (offset < utf8_text.size()) {
        const auto code_point = decode_next_utf8(utf8_text, offset);
        if (!code_point.has_value()) {
            destination.resize(original_size);
            return false;
        }
        const auto code = big5_code_for_unicode(*code_point);
        if (!code.has_value() || *code == 0U) {
            destination.resize(original_size);
            return false;
        }
        if (*code <= 0x7FU) {
            destination.push_back(static_cast<std::uint8_t>(*code));
        } else {
            destination.push_back(static_cast<std::uint8_t>(*code >> 8U));
            destination.push_back(static_cast<std::uint8_t>(*code));
        }
    }
    return true;
}

std::optional<std::vector<std::uint8_t>> encode_big5(
    const std::u8string_view utf8_text) {
    std::vector<std::uint8_t> result;
    if (!append_big5(result, utf8_text)) {
        return std::nullopt;
    }
    return result;
}

}  // namespace openlegend::text
