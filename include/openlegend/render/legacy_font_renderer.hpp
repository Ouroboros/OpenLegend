#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "openlegend/render/indexed_framebuffer.hpp"

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
    std::uint8_t right_shadow,
    std::uint8_t foreground) noexcept;

[[nodiscard]] bool draw_big5_glyph(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 32> glyph,
    std::uint8_t right_shadow,
    std::uint8_t foreground) noexcept;

[[nodiscard]] bool draw_legacy_text(
    IndexedFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t> zero_terminated_text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    std::uint8_t right_shadow,
    std::uint8_t foreground) noexcept;

}  // namespace openlegend::render
