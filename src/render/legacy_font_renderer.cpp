#include "openlegend/render/legacy_font_renderer.hpp"

#include <cstddef>
#include <vector>

#include "openlegend/text/big5.hpp"

namespace openlegend::render {

bool draw_ascii_glyph(
    IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t, 16> glyph,
    const TextColors colors) noexcept {
    if (x < 0 || y < 0 || x + 8 >= framebuffer.coordinate_width() ||
        y + 16 > framebuffer.coordinate_height()) {
        return false;
    }
    for (int row_index = 0; row_index < 16; ++row_index) {
        const auto bits = glyph[static_cast<std::size_t>(row_index)];
        auto mask = std::uint8_t{0x80U};
        for (int column = 0; column < 8; ++column) {
            if ((bits & mask) != 0U) {
                framebuffer.draw_pixel(
                    x + column, y + row_index, colors.foreground);
                framebuffer.draw_pixel(
                    x + column + 1, y + row_index, colors.right_shadow);
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
    const TextColors colors) noexcept {
    if (x < 0 || y < 0 || x + 16 >= framebuffer.coordinate_width() ||
        y + 16 > framebuffer.coordinate_height()) {
        return false;
    }
    for (int row_index = 0; row_index < 16; ++row_index) {
        for (int byte_index = 0; byte_index < 2; ++byte_index) {
            const auto bits = glyph[static_cast<std::size_t>(row_index * 2 + byte_index)];
            auto mask = std::uint8_t{0x80U};
            for (int bit_index = 0; bit_index < 8; ++bit_index) {
                const auto column = byte_index * 8 + bit_index;
                if ((bits & mask) != 0U) {
                    framebuffer.draw_pixel(
                        x + column, y + row_index, colors.foreground);
                    framebuffer.draw_pixel(
                        x + column + 1, y + row_index, colors.right_shadow);
                }
                mask = static_cast<std::uint8_t>(mask >> 1U);
            }
        }
    }
    return true;
}

bool draw_text_big5(
    IndexedFramebuffer& framebuffer,
    int x,
    const int y,
    const text::Big5TextView text,
    const std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    const TextColors colors) noexcept {
    if (ascii_font.size() < 128U * 16U) {
        return false;
    }

    const auto bytes = text.bytes();
    for (std::size_t index = 0U; index < bytes.size();) {
        const auto first = bytes[index++];
        if (first == 0U) {
            return true;
        }
        if (first > 0x7FU) {
            if (index >= bytes.size()) {
                return false;
            }
            const auto second = bytes[index++];
            const auto code = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(first) << 8U | static_cast<std::uint16_t>(second));
            const auto glyph = big5_cache.resolve(code);
            if (!glyph || !draw_big5_glyph(framebuffer, x, y, *glyph, colors)) {
                return false;
            }
            x += 16;
            continue;
        }

        const auto glyph_index = first == static_cast<std::uint8_t>('_') ? 32U : first;
        const auto glyph_offset = static_cast<std::size_t>(glyph_index) * 16U;
        const auto glyph = std::span<const std::uint8_t, 16>{ascii_font.data() + glyph_offset, 16U};
        if (!draw_ascii_glyph(framebuffer, x, y, glyph, colors)) {
            return false;
        }
        x += first == static_cast<std::uint8_t>('_') ? 4 : 8;
    }
    return true;
}

bool draw_text_utf8(
    IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::u8string_view text,
    const std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    const TextColors colors) {
    auto encoded = openlegend::text::encode_big5(text);
    if (!encoded.has_value()) {
        return false;
    }
    return draw_text_big5(
        framebuffer,
        x,
        y,
        text::Big5TextView{*encoded},
        ascii_font,
        big5_cache,
        colors);
}

bool draw_text_mixed(
    IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::GameText& text,
    const std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    const TextColors colors) {
    std::vector<std::uint8_t> encoded;
    if (!text::encode_game_text(text, encoded)) {
        return false;
    }
    return draw_text_big5(
        framebuffer,
        x,
        y,
        text::Big5TextView{encoded},
        ascii_font,
        big5_cache,
        colors);
}

}  // namespace openlegend::render
