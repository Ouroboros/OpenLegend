#include "openlegend/attributes.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

#include <utility>

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
    if (width <= 10U || height <= 10U || x < 0 || y < 0 ||
        x + static_cast<int>(width) > framebuffer.logical_width() ||
        y + static_cast<int>(height) > framebuffer.logical_height()) {
        return false;
    }
    const auto w = static_cast<int>(width);
    const auto h = static_cast<int>(height);
    const auto panel = palette_color(palette, palette_colors::black, kPanelAlpha);
    if (framebuffer.logical_width() != render::RgbaFramebuffer::width ||
        framebuffer.logical_height() != render::RgbaFramebuffer::height) {
        const auto border = render::scale_legacy_reference_length(
            1, framebuffer.logical_width(), framebuffer.logical_height());
        if (w <= 2 * border || h <= 2 * border ||
            !framebuffer.blend_rectangle(x, y, width, height, panel)) {
            return false;
        }
        const auto outline = palette_color(
            palette, palette_colors::panel_outline);
        return framebuffer.fill_rectangle(
                   x,
                   y,
                   width,
                   static_cast<std::uint16_t>(border),
                   outline) &&
            framebuffer.fill_rectangle(
                x,
                y + h - border,
                width,
                static_cast<std::uint16_t>(border),
                outline) &&
            framebuffer.fill_rectangle(
                x,
                y,
                static_cast<std::uint16_t>(border),
                height,
                outline) &&
            framebuffer.fill_rectangle(
                x + w - border,
                y,
                static_cast<std::uint16_t>(border),
                height,
                outline);
    }
    const auto blend = [&framebuffer, panel](
                           const int left,
                           const int top,
                           const int rectangle_width,
                           const int rectangle_height) {
        return framebuffer.blend_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width),
            static_cast<std::uint16_t>(rectangle_height),
            panel);
    };
    if (!blend(x + 5, y, w - 10, 1) ||
        !blend(x + 4, y + 1, w - 8, 1) ||
        !blend(x + 3, y + 2, w - 6, 1) ||
        !blend(x + 2, y + 3, w - 4, 1) ||
        !blend(x + 1, y + 4, w - 2, 1) ||
        !blend(x, y + 5, w, h - 10) ||
        !blend(x + 1, y + h - 5, w - 2, 1) ||
        !blend(x + 2, y + h - 4, w - 4, 1) ||
        !blend(x + 3, y + h - 3, w - 6, 1) ||
        !blend(x + 4, y + h - 2, w - 8, 1) ||
        !blend(x + 5, y + h - 1, w - 10, 1)) {
        return false;
    }

    const auto outline = palette_color(palette, palette_colors::panel_outline);
    const auto fill = [&framebuffer, outline](
                          const int left,
                          const int top,
                          const int rectangle_width,
                          const int rectangle_height) {
        return framebuffer.fill_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width),
            static_cast<std::uint16_t>(rectangle_height),
            outline);
    };
    return fill(x + 5, y + 1, w - 10, 1) &&
        fill(x + 4, y + 2, 1, 2) && fill(x + w - 5, y + 2, 1, 2) &&
        fill(x + 2, y + 4, 2, 1) && fill(x + w - 4, y + 4, 2, 1) &&
        fill(x + 1, y + 5, 1, h - 10) &&
        fill(x + w - 2, y + 5, 1, h - 10) &&
        fill(x + 2, y + h - 5, 2, 1) &&
        fill(x + w - 4, y + h - 5, 2, 1) &&
        fill(x + 4, y + h - 4, 1, 2) &&
        fill(x + w - 5, y + h - 4, 1, 2) &&
        fill(x + 5, y + h - 2, w - 10, 1);
}

}  // namespace openlegend::ui
