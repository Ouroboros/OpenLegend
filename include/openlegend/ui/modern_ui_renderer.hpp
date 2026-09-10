#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/legacy_font.hpp"
#include "openlegend/render/rgba_font_renderer.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/text/game_text.hpp"
#include "openlegend/ui/save_list.hpp"

namespace openlegend::ui {

class ModernUiRenderer {
public:
    explicit ModernUiRenderer(const resource::DataRoot& data_root);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    [[nodiscard]] bool render_location_status(
        std::span<const std::uint8_t> legacy_name,
        int location_x,
        int location_y,
        const compat::LegacyPalette& palette,
        render::RgbaFramebuffer& framebuffer);

    [[nodiscard]] bool render_save_list(
        SaveListMode mode,
        std::uint16_t selection,
        std::span<const SaveListEntry> entries,
        const compat::LegacyPalette& palette,
        render::RgbaFramebuffer& framebuffer);

    [[nodiscard]] bool render_save_delete_confirmation(
        std::uint16_t selection,
        const compat::LegacyPalette& palette,
        render::RgbaFramebuffer& framebuffer);

    [[nodiscard]] bool render_io_wait(
        const compat::LegacyPalette& palette,
        render::RgbaFramebuffer& framebuffer);

private:
    [[nodiscard]] bool draw_text_utf8(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        std::u8string_view text,
        render::TextColors colors,
        const compat::LegacyPalette& palette);

    [[nodiscard]] bool draw_text_mixed(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        const text::GameText& text,
        render::TextColors colors,
        const compat::LegacyPalette& palette);

    [[nodiscard]] bool draw_text_big5(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        text::Big5TextView text,
        render::TextColors colors,
        const compat::LegacyPalette& palette);

    [[nodiscard]] bool draw_box(
        render::RgbaFramebuffer& framebuffer,
        int x,
        int y,
        std::uint16_t width,
        std::uint16_t height,
        const compat::LegacyPalette& palette);

    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::optional<render::Big5GlyphCache> big5_cache_;
    std::string error_;
};

}  // namespace openlegend::ui
