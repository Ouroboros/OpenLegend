#include "openlegend/render/legacy_font_renderer.hpp"

#include <algorithm>
#include <cstddef>

namespace openlegend::render {
namespace {

[[nodiscard]] std::optional<std::size_t> big5_glyph_index(const std::uint16_t code) noexcept {
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

Big5GlyphCache::Big5GlyphCache(const std::span<const std::uint8_t> font_bytes) noexcept
    : font_bytes_(font_bytes) {}

std::optional<std::span<const std::uint8_t, 32>> Big5GlyphCache::resolve(
    const std::uint16_t big5_code) noexcept {
    for (std::size_t index = 0U; index < codes_.size(); ++index) {
        if (valid_[index] && codes_[index] == big5_code) {
            return std::span<const std::uint8_t, 32>{glyphs_[index]};
        }
    }

    const auto glyph_index = big5_glyph_index(big5_code);
    if (!glyph_index) {
        return std::nullopt;
    }
    const auto byte_offset = *glyph_index * 32U;
    if (byte_offset > font_bytes_.size() || font_bytes_.size() - byte_offset < 32U) {
        return std::nullopt;
    }

    auto& destination = glyphs_[next_slot_];
    std::copy_n(font_bytes_.begin() + static_cast<std::ptrdiff_t>(byte_offset), 32U, destination.begin());
    codes_[next_slot_] = big5_code;
    valid_[next_slot_] = true;
    const auto result_slot = next_slot_;
    next_slot_ = (next_slot_ + 1U) & 63U;
    return std::span<const std::uint8_t, 32>{glyphs_[result_slot]};
}

bool draw_ascii_glyph(
    IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t, 16> glyph,
    const std::uint8_t right_shadow,
    const std::uint8_t foreground) noexcept {
    if (x < 0 || y < 0 || x + 8 >= IndexedFramebuffer::width || y + 16 > IndexedFramebuffer::height) {
        return false;
    }
    for (int row_index = 0; row_index < 16; ++row_index) {
        const auto bits = glyph[static_cast<std::size_t>(row_index)];
        auto mask = std::uint8_t{0x80U};
        auto* destination = framebuffer.row(y + row_index) + x;
        for (int column = 0; column < 8; ++column) {
            if ((bits & mask) != 0U) {
                destination[column] = foreground;
                destination[column + 1] = right_shadow;
            }
            mask = static_cast<std::uint8_t>(mask >> 1U);
        }
    }
    return true;
}

bool draw_big5_glyph(
    IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t, 32> glyph,
    const std::uint8_t right_shadow,
    const std::uint8_t foreground) noexcept {
    if (x < 0 || y < 0 || x + 16 >= IndexedFramebuffer::width || y + 16 > IndexedFramebuffer::height) {
        return false;
    }
    for (int row_index = 0; row_index < 16; ++row_index) {
        auto* destination = framebuffer.row(y + row_index) + x;
        for (int byte_index = 0; byte_index < 2; ++byte_index) {
            const auto bits = glyph[static_cast<std::size_t>(row_index * 2 + byte_index)];
            auto mask = std::uint8_t{0x80U};
            for (int bit_index = 0; bit_index < 8; ++bit_index) {
                const auto column = byte_index * 8 + bit_index;
                if ((bits & mask) != 0U) {
                    destination[column] = foreground;
                    destination[column + 1] = right_shadow;
                }
                mask = static_cast<std::uint8_t>(mask >> 1U);
            }
        }
    }
    return true;
}

bool draw_legacy_text(
    IndexedFramebuffer& framebuffer,
    int x,
    const int y,
    const std::span<const std::uint8_t> zero_terminated_text,
    const std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    const std::uint8_t right_shadow,
    const std::uint8_t foreground) noexcept {
    if (ascii_font.size() < 128U * 16U) {
        return false;
    }

    for (std::size_t index = 0U; index < zero_terminated_text.size();) {
        const auto first = zero_terminated_text[index++];
        if (first == 0U) {
            return true;
        }
        if (first > 0x7FU) {
            if (index >= zero_terminated_text.size()) {
                return false;
            }
            const auto second = zero_terminated_text[index++];
            const auto code = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(first) << 8U | static_cast<std::uint16_t>(second));
            const auto glyph = big5_cache.resolve(code);
            if (!glyph || !draw_big5_glyph(framebuffer, x, y, *glyph, right_shadow, foreground)) {
                return false;
            }
            x += 16;
            continue;
        }

        const auto glyph_index = first == static_cast<std::uint8_t>('_') ? 32U : first;
        const auto glyph_offset = static_cast<std::size_t>(glyph_index) * 16U;
        const auto glyph = std::span<const std::uint8_t, 16>{ascii_font.data() + glyph_offset, 16U};
        if (!draw_ascii_glyph(framebuffer, x, y, glyph, right_shadow, foreground)) {
            return false;
        }
        x += first == static_cast<std::uint8_t>('_') ? 4 : 8;
    }
    return false;
}

}  // namespace openlegend::render
