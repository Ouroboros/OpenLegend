#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "openlegend/attributes.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/legacy_font.hpp"
#include "openlegend/text/game_text.hpp"

namespace openlegend::render {

NODISCARD bool draw_ascii_glyph(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 16> glyph,
    TextColors colors) noexcept;

NODISCARD bool draw_big5_glyph(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 32> glyph,
    TextColors colors) noexcept;

NODISCARD bool draw_text_big5(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    text::Big5TextView text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors) noexcept;

NODISCARD bool draw_text_utf8(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::u8string_view text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors);

NODISCARD bool draw_text_mixed(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    const text::GameText& text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors);

}  // namespace openlegend::render
