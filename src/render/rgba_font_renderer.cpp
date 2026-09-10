#include "openlegend/render/rgba_font_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "openlegend/text/big5.hpp"

namespace openlegend::render::rgba {
namespace {

constexpr std::uint16_t kAsciiSourceWidth = 8U;
constexpr std::uint16_t kBig5SourceWidth = 16U;
constexpr std::uint16_t kSourceHeight = kBaseFontPixelHeight;

[[nodiscard]] std::uint16_t source_width(const GlyphKind kind) noexcept {
    return kind == GlyphKind::big5 ? kBig5SourceWidth : kAsciiSourceWidth;
}

[[nodiscard]] std::uint16_t target_width(
    const GlyphKind kind, const FontMetrics metrics) noexcept {
    return kind == GlyphKind::big5 ? metrics.big5_width : metrics.ascii_width;
}

[[nodiscard]] bool packed_pixel(
    const std::span<const std::uint8_t> glyph,
    const std::uint16_t width,
    const std::uint16_t x,
    const std::uint16_t y) noexcept {
    const auto row_bytes = static_cast<std::size_t>((width + 7U) / 8U);
    const auto byte = glyph[static_cast<std::size_t>(y) * row_bytes + x / 8U];
    const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7U));
    return (byte & mask) != 0U;
}

[[nodiscard]] compat::Rgba8 with_coverage(
    compat::Rgba8 color, const std::uint8_t coverage) noexcept {
    color.alpha = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(color.alpha) * coverage + 127U) / 255U);
    return color;
}

[[nodiscard]] bool glyph_fits(
    const int x,
    const int y,
    const std::uint16_t width,
    const FontMetrics metrics) noexcept {
    return x >= 0 && y >= 0 &&
        x + static_cast<int>(width) + metrics.shadow_offset_x <=
            RgbaFramebuffer::width &&
        y + metrics.pixel_height <= RgbaFramebuffer::height;
}

void draw_source_glyph(
    RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t> glyph,
    const std::uint16_t width,
    const TextColors colors) {
    for (std::uint16_t row = 0U; row < kSourceHeight; ++row) {
        for (std::uint16_t column = 0U; column < width; ++column) {
            if (!packed_pixel(glyph, width, column, row)) {
                continue;
            }
            static_cast<void>(framebuffer.blend_pixel(
                x + column, y + row, colors.foreground));
            static_cast<void>(framebuffer.blend_pixel(
                x + column + 1, y + row, colors.right_shadow));
        }
    }
}

void draw_mask_layer(
    RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const GlyphMaskView mask,
    const compat::Rgba8 color) {
    for (std::uint16_t row = 0U; row < mask.height; ++row) {
        for (std::uint16_t column = 0U; column < mask.width; ++column) {
            const auto coverage = mask.alpha[
                static_cast<std::size_t>(row) * mask.width + column];
            if (coverage == 0U) {
                continue;
            }
            static_cast<void>(framebuffer.blend_pixel(
                x + column,
                y + row,
                with_coverage(color, coverage)));
        }
    }
}

[[nodiscard]] bool draw_glyph(
    RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t> glyph,
    const GlyphKind kind,
    const std::uint16_t glyph_code,
    GlyphMaskCache& glyph_mask_cache,
    const FontSize size,
    const TextColors colors) {
    const auto metrics = font_metrics(size);
    const auto width = target_width(kind, metrics);
    if (!metrics.valid() || !glyph_fits(x, y, width, metrics)) {
        return false;
    }
    if (size == kBaseFontSize) {
        draw_source_glyph(
            framebuffer, x, y, glyph, source_width(kind), colors);
        return true;
    }

    const auto mask = glyph_mask_cache.resolve(kind, glyph_code, glyph, size);
    if (!mask.has_value() || !mask->valid()) {
        return false;
    }
    draw_mask_layer(
        framebuffer,
        x + metrics.shadow_offset_x,
        y,
        *mask,
        colors.right_shadow);
    draw_mask_layer(framebuffer, x, y, *mask, colors.foreground);
    return true;
}

}  // namespace

bool GlyphMaskCache::generate_mask(
    Entry& entry,
    const GlyphKind kind,
    const std::span<const std::uint8_t> packed_glyph,
    const FontSize size) {
    const auto width = source_width(kind);
    const auto row_bytes = static_cast<std::size_t>((width + 7U) / 8U);
    const auto metrics = font_metrics(size);
    if (!metrics.valid() || packed_glyph.size() < row_bytes * kSourceHeight) {
        return false;
    }

    entry.width = target_width(kind, metrics);
    entry.height = metrics.pixel_height;
    entry.alpha.assign(
        static_cast<std::size_t>(entry.width) * entry.height, 0U);
    const auto target_pixel_area =
        static_cast<std::uint32_t>(width) * kSourceHeight;

    for (std::uint16_t destination_y = 0U;
         destination_y < entry.height;
         ++destination_y) {
        const auto destination_top =
            static_cast<std::uint32_t>(destination_y) * kSourceHeight;
        const auto destination_bottom = destination_top + kSourceHeight;
        for (std::uint16_t destination_x = 0U;
             destination_x < entry.width;
             ++destination_x) {
            const auto destination_left =
                static_cast<std::uint32_t>(destination_x) * width;
            const auto destination_right = destination_left + width;
            std::uint32_t covered_area = 0U;

            for (std::uint16_t source_y = 0U;
                 source_y < kSourceHeight;
                 ++source_y) {
                const auto source_top =
                    static_cast<std::uint32_t>(source_y) * entry.height;
                const auto source_bottom = source_top + entry.height;
                const auto overlap_top = std::max(destination_top, source_top);
                const auto overlap_bottom = std::min(destination_bottom, source_bottom);
                if (overlap_bottom <= overlap_top) {
                    continue;
                }
                const auto overlap_y = overlap_bottom - overlap_top;

                for (std::uint16_t source_x = 0U;
                     source_x < width;
                     ++source_x) {
                    if (!packed_pixel(packed_glyph, width, source_x, source_y)) {
                        continue;
                    }
                    const auto source_left =
                        static_cast<std::uint32_t>(source_x) * entry.width;
                    const auto source_right = source_left + entry.width;
                    const auto overlap_left = std::max(destination_left, source_left);
                    const auto overlap_right = std::min(destination_right, source_right);
                    if (overlap_right > overlap_left) {
                        covered_area += (overlap_right - overlap_left) * overlap_y;
                    }
                }
            }

            entry.alpha[static_cast<std::size_t>(destination_y) * entry.width +
                        destination_x] = static_cast<std::uint8_t>(
                (covered_area * 255U + target_pixel_area / 2U) /
                target_pixel_area);
        }
    }
    return true;
}

std::optional<GlyphMaskView> GlyphMaskCache::resolve(
    const GlyphKind kind,
    const std::uint16_t glyph_code,
    const std::span<const std::uint8_t> packed_glyph,
    const FontSize size) {
    for (const auto& entry : entries_) {
        if (entry.valid && entry.kind == kind &&
            entry.glyph_code == glyph_code && entry.size == size) {
            return GlyphMaskView{entry.alpha, entry.width, entry.height};
        }
    }

    auto& entry = entries_[next_slot_];
    if (!generate_mask(entry, kind, packed_glyph, size)) {
        return std::nullopt;
    }
    entry.kind = kind;
    entry.glyph_code = glyph_code;
    entry.size = size;
    entry.valid = true;
    const auto result = GlyphMaskView{entry.alpha, entry.width, entry.height};
    next_slot_ = (next_slot_ + 1U) % entries_.size();
    return result;
}

bool draw_ascii_glyph(
    RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t, 16> glyph,
    const std::uint8_t glyph_code,
    GlyphMaskCache& glyph_mask_cache,
    const FontSize size,
    const TextColors colors) {
    return draw_glyph(
        framebuffer,
        x,
        y,
        glyph,
        GlyphKind::ascii,
        glyph_code,
        glyph_mask_cache,
        size,
        colors);
}

bool draw_big5_glyph(
    RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t, 32> glyph,
    const std::uint16_t glyph_code,
    GlyphMaskCache& glyph_mask_cache,
    const FontSize size,
    const TextColors colors) {
    return draw_glyph(
        framebuffer,
        x,
        y,
        glyph,
        GlyphKind::big5,
        glyph_code,
        glyph_mask_cache,
        size,
        colors);
}

bool draw_text_big5(
    RgbaFramebuffer& framebuffer,
    int x,
    const int y,
    const text::Big5TextView text,
    const std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    GlyphMaskCache& glyph_mask_cache,
    const FontSize size,
    const TextColors colors) {
    const auto metrics = font_metrics(size);
    if (!metrics.valid() || ascii_font.size() < 128U * 16U) {
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
                static_cast<std::uint16_t>(first) << 8U |
                static_cast<std::uint16_t>(second));
            const auto glyph = big5_cache.resolve(code);
            if (!glyph.has_value() ||
                !draw_big5_glyph(
                    framebuffer,
                    x,
                    y,
                    *glyph,
                    code,
                    glyph_mask_cache,
                    size,
                    colors)) {
                return false;
            }
            x += metrics.big5_width;
            continue;
        }

        const auto glyph_index = first == static_cast<std::uint8_t>('_')
            ? std::uint8_t{32U}
            : first;
        const auto glyph_offset = static_cast<std::size_t>(glyph_index) * 16U;
        const auto glyph = std::span<const std::uint8_t, 16>{
            ascii_font.data() + glyph_offset, 16U};
        if (!draw_ascii_glyph(
                framebuffer,
                x,
                y,
                glyph,
                glyph_index,
                glyph_mask_cache,
                size,
                colors)) {
            return false;
        }
        x += first == static_cast<std::uint8_t>('_')
            ? metrics.underscore_advance
            : metrics.ascii_width;
    }
    return true;
}

bool draw_text_utf8(
    RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::u8string_view text,
    const std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    GlyphMaskCache& glyph_mask_cache,
    const FontSize size,
    const TextColors colors) {
    auto encoded = text::encode_big5(text);
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
        glyph_mask_cache,
        size,
        colors);
}

bool draw_text_mixed(
    RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::GameText& text,
    const std::span<const std::uint8_t> ascii_font,
    Big5GlyphCache& big5_cache,
    GlyphMaskCache& glyph_mask_cache,
    const FontSize size,
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
        glyph_mask_cache,
        size,
        colors);
}

}  // namespace openlegend::render::rgba
