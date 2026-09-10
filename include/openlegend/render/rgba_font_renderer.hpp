#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "openlegend/compat/color.hpp"
#include "openlegend/render/legacy_font.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/text/game_text.hpp"

namespace openlegend::render::rgba {

struct TextColors {
    compat::Rgba8 right_shadow{};
    compat::Rgba8 foreground{};
};

[[nodiscard]] bool draw_ascii_glyph(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 16> glyph,
    TextColors colors) noexcept;

[[nodiscard]] bool draw_big5_glyph(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 32> glyph,
    TextColors colors) noexcept;

[[nodiscard]] bool draw_text_big5(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    text::Big5TextView text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors) noexcept;

[[nodiscard]] bool draw_text_utf8(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    std::u8string_view text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors);

[[nodiscard]] bool draw_text_mixed(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    const text::GameText& text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors);

}  // namespace openlegend::render::rgba
