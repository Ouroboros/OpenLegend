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

    NODISCARD bool draw_box(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        std::uint16_t width,
        std::uint16_t height,
        const compat::LegacyPalette& palette);

private:
    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::optional<render::Big5GlyphCache> big5_cache_;
    render::rgba::GlyphMaskCache glyph_mask_cache_;
    std::string error_;
};

}  // namespace openlegend::ui
