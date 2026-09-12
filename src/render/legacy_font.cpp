#include "openlegend/attributes.hpp"
#include "openlegend/render/legacy_font.hpp"

#include <algorithm>
#include <cstddef>

namespace openlegend::render {
namespace {

NODISCARD std::optional<std::size_t> big5_glyph_index(
    const std::uint16_t code) noexcept {
    const auto lead = static_cast<std::uint8_t>(code >> 8U);
    const auto trail = static_cast<std::uint8_t>(code);
    if (lead < 0xA1U) {
        return std::nullopt;
    }

    std::size_t trail_index{};
    if (trail >= 0x40U && trail <= 0x7EU) {
        trail_index = static_cast<std::size_t>(trail - 0x40U);
    } else if (trail >= 0xA1U && trail <= 0xFEU) {
        trail_index = static_cast<std::size_t>(trail - 0x62U);
    } else {
        return std::nullopt;
    }
    return static_cast<std::size_t>(lead - 0xA1U) * 157U + trail_index;
}

}  // namespace

Big5GlyphCache::Big5GlyphCache(
    const std::span<const std::uint8_t> font_bytes) noexcept
    : font_bytes_(font_bytes) {}

std::optional<std::span<const std::uint8_t, 32>> Big5GlyphCache::resolve(
    const std::uint16_t big5_code) noexcept {
    for (std::size_t index = 0U; index < codes_.size(); ++index) {
        if (valid_[index] && codes_[index] == big5_code) {
            return std::span<const std::uint8_t, 32>{glyphs_[index]};
        }
    }

    const auto glyph_index = big5_glyph_index(big5_code);
    if (!glyph_index.has_value()) {
        return std::nullopt;
    }
    const auto byte_offset = *glyph_index * 32U;
    if (byte_offset > font_bytes_.size() || font_bytes_.size() - byte_offset < 32U) {
        return std::nullopt;
    }

    auto& destination = glyphs_[next_slot_];
    std::copy_n(
        font_bytes_.begin() + static_cast<std::ptrdiff_t>(byte_offset),
        32U,
        destination.begin());
    codes_[next_slot_] = big5_code;
    valid_[next_slot_] = true;
    const auto result_slot = next_slot_;
    next_slot_ = (next_slot_ + 1U) & 63U;
    return std::span<const std::uint8_t, 32>{glyphs_[result_slot]};
}

}  // namespace openlegend::render
