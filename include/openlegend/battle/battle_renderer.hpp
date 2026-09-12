#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/legacy_font_renderer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/resource/packed_archive.hpp"

namespace openlegend::battle {

enum class PartySelectionKind {
    medicine_target,
    detoxification_target,
    status,
    equipment_target,
    practice_target,
    item_target,
    leave_party,
};

enum class PartyAbilityKind {
    medicine,
    detoxification,
};

class BattleRenderer {
public:
    BattleRenderer(const resource::DataRoot& data_root, std::int16_t battlefield_id);

    NODISCARD bool load_battlefield_assets();

    NODISCARD bool load_effect_assets();

    NODISCARD bool load_battle_assets();

    NODISCARD bool load_fight_package(std::int16_t fight_head_id);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD bool render(
        const BattleRenderPlan& plan,
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_status_panel(
        const BattleStatusPanelPlan& plan,
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_character_selection(
        const model::RangerState& ranger,
        std::size_t cursor,
        PartySelectionKind kind,
        render::IndexedFramebuffer& framebuffer,
        std::optional<std::int16_t> item_id = std::nullopt);

    NODISCARD bool render_character_status_selection(
        const model::RangerState& ranger,
        std::size_t cursor,
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_party_ability_selection(
        const model::RangerState& ranger,
        std::span<const std::uint8_t> party_slots,
        std::size_t cursor,
        PartyAbilityKind kind,
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_party_action_notice(
        PartyAbilityKind kind,
        std::optional<std::int32_t> amount,
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_character_status(
        const model::RangerState& ranger,
        std::int16_t role_id,
        std::uint8_t page,
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_item_effect(
        const model::RangerState& ranger,
        std::int16_t item_id,
        const BattleItemEffectResult& effect,
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool draw_box(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        std::uint16_t width,
        std::uint16_t height) const noexcept;

    NODISCARD bool draw_text_utf8(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        std::u8string_view text,
        render::TextColors colors = render::legacy_color::text::notice);

    NODISCARD bool draw_text_mixed(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        const text::GameText& text,
        render::TextColors colors = render::legacy_color::text::notice);

    NODISCARD bool draw_text_big5(
        render::IndexedFramebuffer& framebuffer,
        int x,
        int y,
        text::Big5TextView text,
        render::TextColors colors = render::legacy_color::text::notice);

    NODISCARD bool draw_portrait(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t portrait_id,
        int x,
        int y) const;

    NODISCARD bool draw_item_icon(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t item_id,
        int x,
        int y) const;

private:
    NODISCARD std::span<const std::uint8_t> fight_entry(
        std::int32_t legacy_id) const;

    NODISCARD const resource::SpriteFrameView* fight_frame(
        std::int32_t legacy_id) const;

    NODISCARD const resource::SpriteFrameView* archive_frame(
        const resource::PackedArchive& archive,
        std::vector<std::optional<resource::SpriteFrameView>>& cache,
        std::size_t index) const;

    NODISCARD bool draw_fight_sprite(
        render::IndexedFramebuffer& framebuffer,
        std::int32_t legacy_id,
        int anchor_x,
        int anchor_y) const;

    NODISCARD bool draw_tinted_fight_sprite(
        render::IndexedFramebuffer& framebuffer,
        std::int32_t legacy_id,
        int anchor_x,
        int anchor_y,
        render::PaletteIndex color) const;

    NODISCARD bool draw_cursor_overlay(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t variant,
        int anchor_x,
        int anchor_y,
        std::int16_t source_weight) const;

    NODISCARD bool draw_damage_text(
        render::IndexedFramebuffer& framebuffer,
        const BattleRenderCommand& command);

    NODISCARD std::uint8_t blend_pixel(
        std::uint8_t source,
        std::uint8_t destination,
        std::int16_t source_weight) const noexcept;

    void build_rgb4_lookup() noexcept;

    resource::DataRoot data_root_;
    std::int16_t battlefield_id_{};
    bool battlefield_assets_loaded_{};
    bool effect_assets_loaded_{};
    std::vector<std::uint32_t> battlefield_offsets_;
    std::vector<std::uint8_t> battlefield_group_;
    resource::PackedArchive effect_sprites_;
    std::optional<resource::PackedArchive> fight_sprites_;
    resource::PackedArchive cloud_sprites_;
    resource::PackedArchive portraits_;
    resource::PackedArchive item_sprites_;
    mutable std::unordered_map<std::int32_t, resource::SpriteFrameView> fight_frame_cache_;
    mutable std::vector<std::optional<resource::SpriteFrameView>> cloud_frame_cache_;
    mutable std::vector<std::optional<resource::SpriteFrameView>> portrait_frame_cache_;
    mutable std::vector<std::optional<resource::SpriteFrameView>> item_frame_cache_;
    compat::LegacyPalette palette_{};
    std::array<std::uint8_t, 4'096U> rgb4_lookup_{};
    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::optional<render::Big5GlyphCache> big5_cache_;
    std::string error_;
};

}  // namespace openlegend::battle
