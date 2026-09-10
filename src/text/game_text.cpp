#include "openlegend/text/game_text.hpp"

#include <type_traits>

namespace openlegend::text {
namespace {

[[nodiscard]] std::optional<std::size_t> utf8_width_units(
    const std::u8string_view text) noexcept {
    std::size_t width = 0U;
    for (std::size_t index = 0U; index < text.size();) {
        const auto first = static_cast<std::uint8_t>(text[index]);
        if (first <= 0x7FU) {
            ++width;
            ++index;
            continue;
        }
        std::size_t sequence_size{};
        if (first >= 0xC2U && first <= 0xDFU) {
            sequence_size = 2U;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            sequence_size = 3U;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            sequence_size = 4U;
        } else {
            return std::nullopt;
        }
        if (index + sequence_size > text.size()) {
            return std::nullopt;
        }
        for (std::size_t continuation = 1U; continuation < sequence_size; ++continuation) {
            const auto value = static_cast<std::uint8_t>(text[index + continuation]);
            if ((value & 0xC0U) != 0x80U) {
                return std::nullopt;
            }
        }
        const auto second = static_cast<std::uint8_t>(text[index + 1U]);
        if ((first == 0xE0U && second < 0xA0U) ||
            (first == 0xEDU && second >= 0xA0U) ||
            (first == 0xF0U && second < 0x90U) ||
            (first == 0xF4U && second > 0x8FU)) {
            return std::nullopt;
        }
        width += 2U;
        index += sequence_size;
    }
    return width;
}

}  // namespace

std::u8string utf8_from_ascii(const std::string_view text) {
    std::u8string utf8;
    utf8.reserve(text.size());
    for (const char value : text) {
        utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(value)));
    }
    return utf8;
}

void GameText::append_utf8(const std::u8string_view text) {
    if (text.empty()) {
        return;
    }
    if (!segments_.empty() && std::holds_alternative<std::u8string>(segments_.back())) {
        std::get<std::u8string>(segments_.back()).append(text);
        return;
    }
    segments_.emplace_back(std::u8string{text});
}

void GameText::append_ascii(const std::string_view text) {
    if (text.empty()) {
        return;
    }
    append_utf8(utf8_from_ascii(text));
}

std::optional<std::size_t> GameText::legacy_width_units() const noexcept {
    std::size_t width = 0U;
    for (const auto& segment : segments_) {
        const auto segment_width = std::visit(
            [](const auto& value) -> std::optional<std::size_t> {
                using Value = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::u8string>) {
                    return utf8_width_units(value);
                } else {
                    return value.size();
                }
            },
            segment);
        if (!segment_width.has_value()) {
            return std::nullopt;
        }
        width += *segment_width;
    }
    return width;
}

std::size_t GameText::trailing_ascii_digit_count() const noexcept {
    std::size_t count = 0U;
    for (auto segment = segments_.rbegin(); segment != segments_.rend(); ++segment) {
        const auto segment_count = std::visit(
            [](const auto& value) {
                std::size_t digits = 0U;
                for (auto cursor = value.rbegin(); cursor != value.rend(); ++cursor) {
                    const auto code_unit = static_cast<std::uint8_t>(*cursor);
                    if (code_unit < static_cast<std::uint8_t>('0') ||
                        code_unit > static_cast<std::uint8_t>('9')) {
                        break;
                    }
                    ++digits;
                }
                return digits;
            },
            *segment);
        count += segment_count;
        const auto segment_size = std::visit(
            [](const auto& value) { return value.size(); }, *segment);
        if (segment_count != segment_size) {
            break;
        }
    }
    return count;
}

void GameText::append_legacy(const Big5TextView text) {
    const auto bytes = text.bytes();
    if (bytes.empty()) {
        return;
    }
    if (!segments_.empty() &&
        std::holds_alternative<std::vector<std::uint8_t>>(segments_.back())) {
        auto& existing = std::get<std::vector<std::uint8_t>>(segments_.back());
        existing.insert(existing.end(), bytes.begin(), bytes.end());
        return;
    }
    segments_.emplace_back(std::vector<std::uint8_t>{bytes.begin(), bytes.end()});
}

bool encode_game_text(
    const GameText& text,
    std::vector<std::uint8_t>& encoded) {
    const auto original_size = encoded.size();
    for (const auto& segment : text.segments_) {
        const auto appended = std::visit(
            [&encoded](const auto& value) {
                using Value = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::u8string>) {
                    return append_big5(encoded, value);
                } else {
                    encoded.insert(encoded.end(), value.begin(), value.end());
                    return true;
                }
            },
            segment);
        if (!appended) {
            encoded.resize(original_size);
            return false;
        }
    }
    return true;
}

}  // namespace openlegend::text
