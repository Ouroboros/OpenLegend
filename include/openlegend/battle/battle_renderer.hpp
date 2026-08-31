#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/legacy_font_renderer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/packed_archive.hpp"

namespace openlegend::battle {

class BattleRenderer {
public:
    BattleRenderer(const resource::DataRoot& data_root, std::int16_t battlefield_id);

    [[nodiscard]] bool load_fight_package(std::int16_t fight_head_id);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] const compat::LegacyPalette& palette() const noexcept { return palette_; }

    [[nodiscard]] bool render(
        const BattleRenderPlan& plan,
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_status_panel(
        const BattleStatusPanelPlan& plan,
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool draw_box(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        std::uint16_t width,
        std::uint16_t height) const noexcept;
    [[nodiscard]] bool draw_text(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        std::span<const std::uint8_t> text,
        std::uint16_t packed_colors = 0x0705U);
    [[nodiscard]] bool draw_portrait(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t portrait_id,
        int x,
        int y) const;

private:
    [[nodiscard]] std::span<const std::uint8_t> fight_entry(
        std::int32_t legacy_id) const;
    [[nodiscard]] bool draw_fight_sprite(
        render::IndexedFramebuffer& framebuffer,
        std::int32_t legacy_id,
        int anchor_x,
        int anchor_y) const;
    [[nodiscard]] bool draw_tinted_fight_sprite(
        render::IndexedFramebuffer& framebuffer,
        std::int32_t legacy_id,
        int anchor_x,
        int anchor_y,
        std::uint8_t color) const;
    [[nodiscard]] bool draw_cursor_overlay(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t variant,
        int anchor_x,
        int anchor_y,
        std::int16_t source_weight) const;
    [[nodiscard]] bool draw_damage_text(
        render::IndexedFramebuffer& framebuffer,
        const BattleRenderCommand& command);
    [[nodiscard]] std::uint8_t blend_pixel(
        std::uint8_t source,
        std::uint8_t destination,
        std::int16_t source_weight) const noexcept;
    void build_rgb4_lookup() noexcept;

    resource::DataRoot data_root_;
    std::vector<std::uint32_t> battlefield_offsets_;
    std::vector<std::uint8_t> battlefield_group_;
    resource::PackedArchive effect_sprites_;
    std::optional<resource::PackedArchive> fight_sprites_;
    resource::PackedArchive cloud_sprites_;
    resource::PackedArchive portraits_;
    compat::LegacyPalette palette_{};
    std::array<std::uint8_t, 4'096U> rgb4_lookup_{};
    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::optional<render::Big5GlyphCache> big5_cache_;
    std::string error_;
};

}  // namespace openlegend::battle
