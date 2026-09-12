#include "openlegend/render/rgba_font_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

[[nodiscard]] double sinc(const double value) noexcept {
    constexpr double kPi = 3.14159265358979323846;
    if (std::abs(value) < 1.0e-12) {
        return 1.0;
    }
    const auto radians = kPi * value;
    return std::sin(radians) / radians;
}

[[nodiscard]] double lanczos3(
    const double distance, const double filter_scale) noexcept {
    const auto value = distance * filter_scale;
    if (std::abs(value) >= 3.0) {
        return 0.0;
    }
    return sinc(value) * sinc(value / 3.0);
}

[[nodiscard]] compat::Rgba8 with_coverage(
    compat::Rgba8 color, const std::uint8_t coverage) noexcept {
    color.alpha = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(color.alpha) * coverage + 127U) / 255U);
    return color;
}

[[nodiscard]] bool glyph_fits(
    const RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::uint16_t width,
    const FontMetrics metrics) noexcept {
    return x >= 0 && y >= 0 &&
        x + static_cast<int>(width) + metrics.shadow_offset_x <=
            framebuffer.logical_width() &&
        y + metrics.pixel_height <= framebuffer.logical_height();
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
    const int physical_x,
    const int physical_y,
    const GlyphMaskView mask,
    const compat::Rgba8 color) {
    for (std::uint16_t row = 0U; row < mask.height; ++row) {
        for (std::uint16_t column = 0U; column < mask.width; ++column) {
            const auto coverage = mask.alpha[
                static_cast<std::size_t>(row) * mask.width + column];
            if (coverage == 0U) {
                continue;
            }
            static_cast<void>(framebuffer.blend_physical_pixel(
                physical_x + column,
                physical_y + row,
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
    if (!metrics.valid() ||
        !glyph_fits(framebuffer, x, y, width, metrics)) {
        return false;
    }
    if (size == kBaseFontSize) {
        draw_source_glyph(
            framebuffer, x, y, glyph, source_width(kind), colors);
        return true;
    }

    const auto render_scale = framebuffer.scale();
    const auto mask = glyph_mask_cache.resolve(
        kind, glyph_code, glyph, size, render_scale);
    if (!mask.has_value() || !mask->valid()) {
        return false;
    }
    const auto physical_x = x * render_scale;
    const auto physical_y = y * render_scale;
    const auto physical_shadow_offset = std::max(
        1,
        static_cast<int>((
            static_cast<std::uint64_t>(size.pixel_height) *
                static_cast<std::uint64_t>(render_scale) +
            kBaseFontPixelHeight / 2U) /
            kBaseFontPixelHeight));
    draw_mask_layer(
        framebuffer,
        physical_x + physical_shadow_offset,
        physical_y,
        *mask,
        colors.right_shadow);
    draw_mask_layer(
        framebuffer, physical_x, physical_y, *mask, colors.foreground);
    return true;
}

}  // namespace

bool GlyphMaskCache::generate_mask(
    Entry& entry,
    const GlyphKind kind,
    const std::span<const std::uint8_t> packed_glyph,
    const FontSize size,
    const int render_scale) {
    const auto width = source_width(kind);
    const auto row_bytes = static_cast<std::size_t>((width + 7U) / 8U);
    const auto metrics = font_metrics(size);
    const auto destination_width = target_width(kind, metrics);
    const auto maximum_dimension = std::max({
        width,
        kSourceHeight,
        destination_width,
        metrics.pixel_height});
    if (!metrics.valid() || render_scale <= 0 ||
        render_scale >
            static_cast<int>(std::numeric_limits<std::uint16_t>::max() /
                             maximum_dimension) ||
        packed_glyph.size() < row_bytes * kSourceHeight) {
        return false;
    }

    const auto source_pixel_width =
        static_cast<std::uint16_t>(width * render_scale);
    const auto source_pixel_height =
        static_cast<std::uint16_t>(kSourceHeight * render_scale);
    entry.width = static_cast<std::uint16_t>(
        destination_width * render_scale);
    entry.height = static_cast<std::uint16_t>(
        metrics.pixel_height * render_scale);
    entry.alpha.assign(
        static_cast<std::size_t>(entry.width) * entry.height, 0U);

    const auto horizontal_scale = static_cast<double>(entry.width) /
        static_cast<double>(source_pixel_width);
    const auto horizontal_filter_scale = std::min(1.0, horizontal_scale);
    const auto horizontal_radius = 3.0 / horizontal_filter_scale;
    std::vector<double> horizontal(
        static_cast<std::size_t>(entry.width) * source_pixel_height, 0.0);
    for (std::uint16_t source_y = 0U;
         source_y < source_pixel_height;
         ++source_y) {
        const auto glyph_y = static_cast<std::uint16_t>(
            source_y / render_scale);
        for (std::uint16_t destination_x = 0U;
             destination_x < entry.width;
             ++destination_x) {
            const auto source_position =
                (static_cast<double>(destination_x) + 0.5) /
                    horizontal_scale -
                0.5;
            const auto first_source_x = std::max(
                0,
                static_cast<int>(std::floor(
                    source_position - horizontal_radius)));
            const auto last_source_x = std::min(
                static_cast<int>(source_pixel_width) - 1,
                static_cast<int>(std::ceil(
                    source_position + horizontal_radius)));
            double weighted_value = 0.0;
            double weight_sum = 0.0;
            for (int source_x = first_source_x;
                 source_x <= last_source_x;
                 ++source_x) {
                const auto weight = lanczos3(
                    source_position - static_cast<double>(source_x),
                    horizontal_filter_scale);
                weight_sum += weight;
                if (packed_pixel(
                        packed_glyph,
                        width,
                        static_cast<std::uint16_t>(source_x / render_scale),
                        glyph_y)) {
                    weighted_value += weight;
                }
            }
            if (std::abs(weight_sum) < 1.0e-12) {
                return false;
            }
            horizontal[static_cast<std::size_t>(source_y) * entry.width +
                       destination_x] = weighted_value / weight_sum;
        }
    }

    const auto vertical_scale = static_cast<double>(entry.height) /
        static_cast<double>(source_pixel_height);
    const auto vertical_filter_scale = std::min(1.0, vertical_scale);
    const auto vertical_radius = 3.0 / vertical_filter_scale;
    for (std::uint16_t destination_y = 0U;
         destination_y < entry.height;
         ++destination_y) {
        const auto source_position =
            (static_cast<double>(destination_y) + 0.5) / vertical_scale - 0.5;
        const auto first_source_y = std::max(
            0,
            static_cast<int>(std::floor(source_position - vertical_radius)));
        const auto last_source_y = std::min(
            static_cast<int>(source_pixel_height) - 1,
            static_cast<int>(std::ceil(source_position + vertical_radius)));
        for (std::uint16_t destination_x = 0U;
             destination_x < entry.width;
             ++destination_x) {
            double weighted_value = 0.0;
            double weight_sum = 0.0;
            for (int source_y = first_source_y;
                 source_y <= last_source_y;
                 ++source_y) {
                const auto weight = lanczos3(
                    source_position - static_cast<double>(source_y),
                    vertical_filter_scale);
                weight_sum += weight;
                weighted_value += horizontal[
                    static_cast<std::size_t>(source_y) * entry.width +
                    destination_x] * weight;
            }
            if (std::abs(weight_sum) < 1.0e-12) {
                return false;
            }
            entry.alpha[static_cast<std::size_t>(destination_y) * entry.width +
                        destination_x] = static_cast<std::uint8_t>(std::lround(
                std::clamp(weighted_value / weight_sum, 0.0, 1.0) * 255.0));
        }
    }
    return true;
}

std::optional<GlyphMaskView> GlyphMaskCache::resolve(
    const GlyphKind kind,
    const std::uint16_t glyph_code,
    const std::span<const std::uint8_t> packed_glyph,
    const FontSize size,
    const int render_scale) {
    for (const auto& entry : entries_) {
        if (entry.valid && entry.kind == kind &&
            entry.glyph_code == glyph_code && entry.size == size &&
            entry.render_scale == render_scale) {
            return GlyphMaskView{entry.alpha, entry.width, entry.height};
        }
    }

    auto& entry = entries_[next_slot_];
    if (!generate_mask(entry, kind, packed_glyph, size, render_scale)) {
        return std::nullopt;
    }
    entry.kind = kind;
    entry.glyph_code = glyph_code;
    entry.size = size;
    entry.render_scale = render_scale;
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
