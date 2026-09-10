#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/text/game_text.hpp"

namespace openlegend::render {

class Big5GlyphCache {
public:
    explicit Big5GlyphCache(std::span<const std::uint8_t> font_bytes) noexcept;

    [[nodiscard]] std::optional<std::span<const std::uint8_t, 32>> resolve(std::uint16_t big5_code) noexcept;
    [[nodiscard]] std::size_t next_replacement_slot() const noexcept { return next_slot_; }

private:
    std::span<const std::uint8_t> font_bytes_;
    std::array<std::uint16_t, 64> codes_{};
    std::array<bool, 64> valid_{};
    std::array<std::array<std::uint8_t, 32>, 64> glyphs_{};
    std::size_t next_slot_{0U};
};

[[nodiscard]] bool draw_ascii_glyph(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 16> glyph,
    TextColors colors) noexcept;

[[nodiscard]] bool draw_big5_glyph(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 32> glyph,
    TextColors colors) noexcept;

[[nodiscard]] bool draw_text_big5(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    text::Big5TextView text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors) noexcept;

[[nodiscard]] bool draw_text_utf8(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::u8string_view text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors);

[[nodiscard]] bool draw_text_mixed(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    const text::GameText& text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    TextColors colors);

}  // namespace openlegend::render
