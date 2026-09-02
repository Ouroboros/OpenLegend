#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/legacy_font_renderer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/ui/game_menu.hpp"
#include "openlegend/ui/new_game_name_editor.hpp"
#include "openlegend/ui/title_menu.hpp"

namespace openlegend::ui {

class BasicUiRenderer {
public:
    explicit BasicUiRenderer(const resource::DataRoot& data_root);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

    [[nodiscard]] bool render_name_entry(
        const TitleMenuRenderer& title,
        const NewGameNameEditor& editor,
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_attributes(
        const TitleMenuRenderer& title,
        const model::RoleRecord& protagonist,
        std::span<const std::uint8_t> name,
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_game_menu(
        const GameMenuController& menu,
        const model::RangerState& ranger,
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_game_menu_main(
        const GameMenuController& menu,
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_io_wait(render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_error(
        std::span<const std::uint8_t> legacy_message,
        render::IndexedFramebuffer& framebuffer);

private:
    [[nodiscard]] bool draw_text(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        std::span<const std::uint8_t> text,
        std::uint16_t packed_colors = 0x1715U);
    [[nodiscard]] bool draw_box(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        std::uint16_t width,
        std::uint16_t height);
    [[nodiscard]] bool render_items(
        const model::RangerState& ranger,
        std::uint16_t selection,
        render::IndexedFramebuffer& framebuffer);
    void update_panel_palette(const compat::LegacyPalette& palette) noexcept;
    [[nodiscard]] std::uint8_t blend_panel_pixel(
        std::uint8_t destination) const noexcept;

    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::optional<render::Big5GlyphCache> big5_cache_;
    compat::LegacyPalette panel_palette_{};
    std::array<std::uint8_t, 4'096U> panel_rgb4_lookup_{};
    bool panel_palette_ready_{};
    std::string error_;
};

}  // namespace openlegend::ui
