#include "openlegend/attributes.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

#include <algorithm>
#include <utility>

#include "openlegend/text/big5.hpp"

namespace openlegend::ui {
namespace {

namespace palette_colors = render::legacy_color;

constexpr std::uint8_t kPanelAlpha = 96U;

NODISCARD compat::Rgba8 palette_color(
    const compat::LegacyPalette& palette,
    const render::PaletteIndex index,
    const std::uint8_t alpha = compat::kOpaqueAlpha) noexcept {
    const auto source = palette[index];
    return {
        compat::expand_rgb6(source.red),
        compat::expand_rgb6(source.green),
        compat::expand_rgb6(source.blue),
        alpha,
    };
}

NODISCARD render::rgba::TextColors rgba_text_colors(
    const compat::LegacyPalette& palette,
    const render::TextColors colors) noexcept {
    return {
        palette_color(palette, colors.right_shadow),
        palette_color(palette, colors.foreground),
    };
}

}  // namespace

ModernUiRenderer::ModernUiRenderer(const resource::DataRoot& data_root) {
    auto ascii = data_root.read("FONT3.E16");
    if (!ascii) {
        error_ = ascii.error;
        return;
    }
    auto big5 = data_root.read("FONT3.C16");
    if (!big5) {
        error_ = big5.error;
        return;
    }
    if (ascii.bytes.size() != 128U * 16U || big5.bytes.size() % 32U != 0U) {
        error_ = "legacy UI fonts have unexpected sizes";
        return;
    }
    ascii_font_ = std::move(ascii.bytes);
    big5_font_ = std::move(big5.bytes);
    big5_cache_.emplace(big5_font_);
}

bool ModernUiRenderer::draw_text_utf8(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::u8string_view text,
    const render::TextColors colors,
    const compat::LegacyPalette& palette,
    const render::rgba::FontSize size) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::rgba::draw_text_utf8(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        glyph_mask_cache_,
        scaled_font_size(framebuffer, size),
        rgba_text_colors(palette, colors));
}

bool ModernUiRenderer::draw_text_mixed(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::GameText& text,
    const render::TextColors colors,
    const compat::LegacyPalette& palette,
    const render::rgba::FontSize size) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::rgba::draw_text_mixed(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        glyph_mask_cache_,
        scaled_font_size(framebuffer, size),
        rgba_text_colors(palette, colors));
}

std::optional<TextInkBounds> ModernUiRenderer::text_ink_bounds_utf8(
    const render::RgbaFramebuffer& framebuffer,
    const std::u8string_view text,
    const render::rgba::FontSize size) {
    const auto encoded = text::encode_big5(text);
    if (!encoded.has_value() || !big5_cache_.has_value()) {
        return std::nullopt;
    }
    const auto metrics = font_metrics(framebuffer, size);
    constexpr int kSourceHeight = 16;
    std::optional<TextInkBounds> bounds;
    int pen = 0;
    const auto include = [&](const int column, const int row, const int source_width,
                             const int advance) {
        const auto left = pen + column * advance / source_width;
        const auto right = pen + (column + 1) * advance / source_width;
        const auto top = row * static_cast<int>(metrics.pixel_height) / kSourceHeight;
        const auto bottom = (row + 1) * static_cast<int>(metrics.pixel_height) / kSourceHeight;
        if (!bounds.has_value()) {
            bounds = TextInkBounds{left, top, right, bottom};
            return;
        }
        bounds->left = std::min(bounds->left, left);
        bounds->top = std::min(bounds->top, top);
        bounds->right = std::max(bounds->right, right);
        bounds->bottom = std::max(bounds->bottom, bottom);
    };
    const auto& bytes = *encoded;
    for (std::size_t index = 0U; index < bytes.size();) {
        const auto first = bytes[index++];
        if (first > 0x7FU && index < bytes.size()) {
            const auto code = static_cast<std::uint16_t>((first << 8U) | bytes[index++]);
            const auto glyph = big5_cache_->resolve(code);
            const auto advance = static_cast<int>(metrics.big5_width);
            if (glyph.has_value()) {
                for (int row = 0; row < kSourceHeight; ++row) {
                    const auto bits = static_cast<unsigned>(
                        ((*glyph)[static_cast<std::size_t>(2 * row)] << 8U) |
                        (*glyph)[static_cast<std::size_t>(2 * row + 1)]);
                    for (int column = 0; column < 16; ++column) {
                        if ((bits & (0x8000U >> static_cast<unsigned>(column))) != 0U) {
                            include(column, row, 16, advance);
                        }
                    }
                }
            }
            pen += advance;
            continue;
        }
        const auto advance = static_cast<int>(metrics.ascii_width);
        const auto offset = static_cast<std::size_t>(first) * 16U;
        if (offset + 16U <= ascii_font_.size()) {
            for (int row = 0; row < kSourceHeight; ++row) {
                const auto bits = ascii_font_[offset + static_cast<std::size_t>(row)];
                for (int column = 0; column < 8; ++column) {
                    if ((bits & (0x80U >> static_cast<unsigned>(column))) != 0U) {
                        include(column, row, 8, advance);
                    }
                }
            }
        }
        pen += advance;
    }
    return bounds;
}

bool ModernUiRenderer::draw_text_big5(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::Big5TextView text,
    const render::TextColors colors,
    const compat::LegacyPalette& palette,
    const render::rgba::FontSize size) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::rgba::draw_text_big5(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        glyph_mask_cache_,
        scaled_font_size(framebuffer, size),
        rgba_text_colors(palette, colors));
}

bool ModernUiRenderer::draw_box(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height,
    const compat::LegacyPalette& palette) {
    // 原版圆角框：按画面相对320×200的比例放大圆角与描边，任何分辨率都保持原版形状。
    const auto unit = render::scale_legacy_reference_length(
        1, framebuffer.logical_width(), framebuffer.logical_height());
    const auto w = static_cast<int>(width);
    const auto h = static_cast<int>(height);
    if (w <= 10 * unit || h <= 10 * unit || x < 0 || y < 0 ||
        x + w > framebuffer.logical_width() || y + h > framebuffer.logical_height()) {
        return false;
    }
    const auto panel = palette_color(palette, palette_colors::black, kPanelAlpha);
    const auto blend = [&framebuffer, panel, unit](
                           const int left,
                           const int top,
                           const int rectangle_width,
                           const int rectangle_height) {
        return framebuffer.blend_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width),
            static_cast<std::uint16_t>(rectangle_height * unit),
            panel);
    };
    const auto u = unit;
    if (!blend(x + 5 * u, y, w - 10 * u, 1) ||
        !blend(x + 4 * u, y + 1 * u, w - 8 * u, 1) ||
        !blend(x + 3 * u, y + 2 * u, w - 6 * u, 1) ||
        !blend(x + 2 * u, y + 3 * u, w - 4 * u, 1) ||
        !blend(x + 1 * u, y + 4 * u, w - 2 * u, 1) ||
        !framebuffer.blend_rectangle(x, y + 5 * u, width,
            static_cast<std::uint16_t>(h - 10 * u), panel) ||
        !blend(x + 1 * u, y + h - 5 * u, w - 2 * u, 1) ||
        !blend(x + 2 * u, y + h - 4 * u, w - 4 * u, 1) ||
        !blend(x + 3 * u, y + h - 3 * u, w - 6 * u, 1) ||
        !blend(x + 4 * u, y + h - 2 * u, w - 8 * u, 1) ||
        !blend(x + 5 * u, y + h - 1 * u, w - 10 * u, 1)) {
        return false;
    }

    const auto outline = palette_color(palette, palette_colors::panel_outline);
    const auto fill = [&framebuffer, outline, u](
                          const int left,
                          const int top,
                          const int rectangle_width,
                          const int rectangle_height) {
        return framebuffer.fill_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width * u),
            static_cast<std::uint16_t>(rectangle_height * u),
            outline);
    };
    const auto right = [x, w, u](const int offset) { return x + w - offset * u; };
    const auto bottom = [y, h, u](const int offset) { return y + h - offset * u; };
    return framebuffer.fill_rectangle(x + 5 * u, y + 1 * u,
               static_cast<std::uint16_t>(w - 10 * u), static_cast<std::uint16_t>(u), outline) &&
        fill(x + 4 * u, y + 2 * u, 1, 2) && fill(right(5), y + 2 * u, 1, 2) &&
        fill(x + 2 * u, y + 4 * u, 2, 1) && fill(right(4), y + 4 * u, 2, 1) &&
        framebuffer.fill_rectangle(x + 1 * u, y + 5 * u, static_cast<std::uint16_t>(u),
            static_cast<std::uint16_t>(h - 10 * u), outline) &&
        framebuffer.fill_rectangle(right(2), y + 5 * u, static_cast<std::uint16_t>(u),
            static_cast<std::uint16_t>(h - 10 * u), outline) &&
        fill(x + 2 * u, bottom(5), 2, 1) && fill(right(4), bottom(5), 2, 1) &&
        fill(x + 4 * u, bottom(4), 1, 2) && fill(right(5), bottom(4), 1, 2) &&
        framebuffer.fill_rectangle(x + 5 * u, bottom(2),
            static_cast<std::uint16_t>(w - 10 * u), static_cast<std::uint16_t>(u), outline);
}

}  // namespace openlegend::ui
