#include "openlegend/ui/modern_ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>

#include "openlegend/text/big5.hpp"

namespace openlegend::ui {
namespace {

namespace palette_colors = render::legacy_color;
namespace text_colors = render::legacy_color::text;

constexpr std::uint8_t kPanelAlpha = 96U;

[[nodiscard]] compat::Rgba8 palette_color(
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

[[nodiscard]] render::rgba::TextColors rgba_text_colors(
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

bool ModernUiRenderer::render_location_status(
    const std::span<const std::uint8_t> legacy_name,
    const int location_x,
    const int location_y,
    const compat::LegacyPalette& palette,
    render::RgbaFramebuffer& framebuffer) {
    if (!valid()) {
        return false;
    }
    std::array<std::uint8_t, 64> status{};
    if (legacy_name.size() >= status.size()) {
        return false;
    }
    auto length = legacy_name.size();
    std::copy(legacy_name.begin(), legacy_name.end(), status.begin());
    if (!legacy_name.empty()) {
        status[length++] = static_cast<std::uint8_t>(' ');
    }
    const auto append_coordinate = [&status, &length](const int value) {
        std::array<char, 16> digits{};
        const auto converted = std::to_chars(
            digits.data(), digits.data() + digits.size(), value);
        if (converted.ec != std::errc{} ||
            length + static_cast<std::size_t>(converted.ptr - digits.data()) >
                status.size()) {
            return false;
        }
        for (const auto* cursor = digits.data(); cursor != converted.ptr; ++cursor) {
            status[length++] = static_cast<std::uint8_t>(*cursor);
        }
        return true;
    };
    if (!append_coordinate(location_x) || length >= status.size()) {
        return false;
    }
    status[length++] = static_cast<std::uint8_t>(',');
    if (!append_coordinate(location_y)) {
        return false;
    }
    const auto metrics = font_metrics();
    return draw_text_big5(
        framebuffer,
        4,
        render::RgbaFramebuffer::height - metrics.line_height - 4,
        text::Big5TextView{std::span<const std::uint8_t>{status}.first(length)},
        text_colors::location_status,
        palette);
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
        size,
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
        size,
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
        size,
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
        x + static_cast<int>(width) > render::RgbaFramebuffer::width ||
        y + static_cast<int>(height) > render::RgbaFramebuffer::height) {
        return false;
    }
    const auto w = static_cast<int>(width);
    const auto h = static_cast<int>(height);
    const auto panel = palette_color(palette, palette_colors::black, kPanelAlpha);
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
