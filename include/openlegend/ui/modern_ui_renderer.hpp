#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/legacy_font.hpp"
#include "openlegend/render/rgba_font_renderer.hpp"
#include "openlegend/render/reference_layout.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/text/game_text.hpp"

namespace openlegend::ui {

// 文字实际笔画的外接矩形，坐标相对绘制原点（逻辑像素，右、下边界不含）。
struct TextInkBounds {
    int left{};
    int top{};
    int right{};
    int bottom{};
};

class ModernUiRenderer {
public:
    explicit ModernUiRenderer(const resource::DataRoot& data_root);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD static constexpr render::rgba::FontMetrics font_metrics(
        const render::rgba::FontSize size = {}) noexcept {
        return render::rgba::font_metrics(size);
    }

    NODISCARD static constexpr render::rgba::FontSize scaled_font_size(
        const render::RgbaFramebuffer& framebuffer,
        const render::rgba::FontSize size = {}) noexcept {
        return render::rgba::FontSize{static_cast<std::uint16_t>(
            render::scale_legacy_reference_length(
                size.pixel_height,
                framebuffer.logical_width(),
                framebuffer.logical_height()))};
    }

    NODISCARD static constexpr render::rgba::FontMetrics font_metrics(
        const render::RgbaFramebuffer& framebuffer,
        const render::rgba::FontSize size = {}) noexcept {
        return render::rgba::font_metrics(scaled_font_size(framebuffer, size));
    }

    NODISCARD bool draw_text_utf8(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        std::u8string_view text,
        render::TextColors colors,
        const compat::LegacyPalette& palette,
        render::rgba::FontSize size = {});

    NODISCARD bool draw_text_mixed(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        const text::GameText& text,
        render::TextColors colors,
        const compat::LegacyPalette& palette,
        render::rgba::FontSize size = {});

    NODISCARD bool draw_text_big5(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        text::Big5TextView text,
        render::TextColors colors,
        const compat::LegacyPalette& palette,
        render::rgba::FontSize size = {});

    // 直接指定RGBA颜色，不经调色板；字号已是画面逻辑像素高度，可连续调整。
    // physical_offset_x/y为在逻辑坐标之上再偏移的物理像素数，用于逐物理像素定位。
    NODISCARD bool draw_text_big5_pixels(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        text::Big5TextView text,
        render::rgba::TextColors colors,
        render::rgba::FontSize pixel_size,
        int physical_offset_x = 0,
        int physical_offset_y = 0);

    // 直接指定RGBA颜色，不经调色板；字号已是画面逻辑像素高度。
    NODISCARD bool draw_text_utf8_pixels(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        std::u8string_view text,
        render::rgba::TextColors colors,
        render::rgba::FontSize pixel_size);

    // 直接指定RGBA颜色，不经调色板；字号按320×200参考尺寸换算。
    NODISCARD bool draw_text_utf8_rgba(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        std::u8string_view text,
        render::rgba::TextColors colors,
        render::rgba::FontSize size = {});

    NODISCARD bool draw_box(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        std::uint16_t width,
        std::uint16_t height,
        const compat::LegacyPalette& palette);

    NODISCARD std::optional<TextInkBounds> text_ink_bounds_utf8(
        const render::RgbaFramebuffer& framebuffer,
        std::u8string_view text,
        render::rgba::FontSize size = {});

private:
    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::optional<render::Big5GlyphCache> big5_cache_;
    render::rgba::GlyphMaskCache glyph_mask_cache_;
    std::string error_;
};

}  // namespace openlegend::ui
