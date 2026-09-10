#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "openlegend/compat/color.hpp"
#include "openlegend/render/legacy_font.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/text/game_text.hpp"

namespace openlegend::render::rgba {

inline constexpr std::uint16_t kBaseFontPixelHeight = 16U;
inline constexpr std::uint16_t kMaximumFontPixelHeight = 64U;

struct FontSize {
    std::uint16_t pixel_height{kBaseFontPixelHeight};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return pixel_height > 0U && pixel_height <= kMaximumFontPixelHeight;
    }

    [[nodiscard]] friend constexpr bool operator==(
        FontSize, FontSize) noexcept = default;
};

struct FontMetrics {
    std::uint16_t pixel_height{};
    std::uint16_t line_height{};
    std::uint16_t ascii_width{};
    std::uint16_t big5_width{};
    std::uint16_t underscore_advance{};
    std::uint16_t shadow_offset_x{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return pixel_height > 0U && line_height >= pixel_height &&
            ascii_width > 0U && big5_width > 0U &&
            underscore_advance > 0U && shadow_offset_x > 0U;
    }
};

[[nodiscard]] constexpr FontMetrics font_metrics(
    const FontSize size = {}) noexcept {
    if (!size.valid()) {
        return {};
    }
    const auto scaled = [size](const std::uint16_t base_value) {
        const auto value = static_cast<std::uint32_t>(base_value) *
            size.pixel_height + kBaseFontPixelHeight / 2U;
        const auto result = static_cast<std::uint16_t>(
            value / kBaseFontPixelHeight);
        return result == 0U ? std::uint16_t{1U} : result;
    };
    return {
        size.pixel_height,
        size.pixel_height,
        scaled(8U),
        scaled(16U),
        scaled(4U),
        scaled(1U),
    };
}

inline constexpr FontSize kBaseFontSize{};
inline constexpr FontMetrics kBaseFontMetrics = font_metrics(kBaseFontSize);

struct TextColors {
    compat::Rgba8 right_shadow{};
    compat::Rgba8 foreground{};
};

enum class GlyphKind : std::uint8_t {
    ascii,
    big5,
};

struct GlyphMaskView {
    std::span<const std::uint8_t> alpha;
    std::uint16_t width{};
    std::uint16_t height{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return width > 0U && height > 0U &&
            alpha.size() == static_cast<std::size_t>(width) * height;
    }
};

class GlyphMaskCache {
public:
    [[nodiscard]] std::optional<GlyphMaskView> resolve(
        GlyphKind kind,
        std::uint16_t glyph_code,
        std::span<const std::uint8_t> packed_glyph,
        FontSize size);

    [[nodiscard]] std::size_t next_replacement_slot() const noexcept {
        return next_slot_;
    }

private:
    struct Entry {
        GlyphKind kind{GlyphKind::ascii};
        std::uint16_t glyph_code{};
        FontSize size{};
        std::uint16_t width{};
        std::uint16_t height{};
        std::vector<std::uint8_t> alpha;
        bool valid{};
    };

    [[nodiscard]] bool generate_mask(
        Entry& entry,
        GlyphKind kind,
        std::span<const std::uint8_t> packed_glyph,
        FontSize size);

    static constexpr std::size_t capacity = 64U;

    std::array<Entry, capacity> entries_{};
    std::size_t next_slot_{};
};

[[nodiscard]] bool draw_ascii_glyph(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 16> glyph,
    std::uint8_t glyph_code,
    GlyphMaskCache& glyph_mask_cache,
    FontSize size,
    TextColors colors);

[[nodiscard]] bool draw_big5_glyph(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    std::span<const std::uint8_t, 32> glyph,
    std::uint16_t glyph_code,
    GlyphMaskCache& glyph_mask_cache,
    FontSize size,
    TextColors colors);

[[nodiscard]] bool draw_text_big5(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    text::Big5TextView text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    GlyphMaskCache& glyph_mask_cache,
    FontSize size,
    TextColors colors);

[[nodiscard]] bool draw_text_utf8(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    std::u8string_view text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    GlyphMaskCache& glyph_mask_cache,
    FontSize size,
    TextColors colors);

[[nodiscard]] bool draw_text_mixed(
    RgbaFramebuffer& framebuffer,
    int x,
    int y,
    const text::GameText& text,
    std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    GlyphMaskCache& glyph_mask_cache,
    FontSize size,
    TextColors colors);

}  // namespace openlegend::render::rgba
