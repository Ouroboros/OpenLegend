#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_pathing.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::battle {

enum class BattleSessionPhase {
    party_selection,
    initial_present,
    initial_fade,
    round_start,
    actor_present,
    player_action,
    player_action_selected,
    player_magic_selection,
    player_attack_direction,
    player_movement_select,
    player_targeting_select,
    player_magic_frame_present,
    player_magic_wait,
    player_damage_frame_present,
    player_damage_wait,
    player_attack_commit_present,
    player_attack_commit_wait,
    player_attack_level_present,
    player_attack_level_wait,
    player_movement_step_present,
    player_movement_wait,
    automatic_present,
    ai_action,
    ai_prelude_present,
    ai_wait,
    ai_action_selected,
    ai_movement_step_present,
    ai_movement_wait,
    round_wait,
    battle_outcome,
};

enum class BattleSessionInputResult {
    ignored,
    changed,
    selection_complete,
    action_changed,
    action_selected,
    cursor_changed,
    cursor_cancelled,
    cursor_selected,
    magic_changed,
    magic_cancelled,
    magic_selected,
    direction_selected,
};

enum class BattleAudioBank : std::int16_t {
    attack,
    effect,
};

struct BattleAudioCommand {
    BattleAudioBank bank{BattleAudioBank::effect};
    std::int16_t sample_id{};

    friend bool operator==(const BattleAudioCommand&, const BattleAudioCommand&) = default;
};

enum class BattlePlayerAction : std::int16_t {
    movement,
    attack,
    use_poison,
    detoxification,
    medicine,
    item,
    wait,
    status,
    rest,
    automatic,
};

struct BattlePlayerActionMenuState {
    std::array<std::int16_t, 10> available{};
    std::size_t available_count{};
    std::size_t cursor{};
    std::int16_t selected_action{-1};
};

class BattleSession {
public:
    BattleSession(
        const resource::DataRoot& data_root,
        model::RangerState& ranger,
        random::LegacyRandom& random,
        std::int16_t battle_id,
        bool grant_experience);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] BattleSessionPhase phase() const noexcept { return phase_; }
    [[nodiscard]] std::int16_t battle_id() const noexcept { return data_.battle_id(); }
    [[nodiscard]] bool grants_experience() const noexcept { return grants_experience_; }
    [[nodiscard]] std::int16_t view_x() const noexcept { return render_state_.view_x; }
    [[nodiscard]] std::int16_t view_y() const noexcept { return render_state_.view_y; }
    [[nodiscard]] std::size_t current_actor_slot() const noexcept { return current_actor_slot_; }
    [[nodiscard]] BattleOutcome outcome() const noexcept { return outcome_; }
    [[nodiscard]] std::size_t fade_frame_count() const noexcept {
        return fade_palettes_.size();
    }
    [[nodiscard]] std::size_t fade_frame() const noexcept { return fade_frame_; }
    [[nodiscard]] const BattleSetup& setup() const noexcept { return setup_; }
    [[nodiscard]] BattleSetup& setup() noexcept { return setup_; }
    [[nodiscard]] const BattleData& data() const noexcept { return data_; }
    [[nodiscard]] const BattlePlayerActionMenuState& player_action_menu() const noexcept {
        return player_action_menu_;
    }
    [[nodiscard]] const std::optional<BattleMagicSelectionState>&
    player_magic_selection() const noexcept {
        return player_magic_selection_;
    }
    [[nodiscard]] std::int16_t selected_magic_slot() const noexcept {
        return selected_magic_slot_;
    }
    [[nodiscard]] std::optional<BattlePathCoord> active_cursor() const noexcept {
        return player_cursor_selection_.has_value()
            ? std::optional<BattlePathCoord>{player_cursor_selection_->cursor}
            : std::nullopt;
    }
    [[nodiscard]] std::optional<BattlePathCoord> selected_player_target() const noexcept {
        return selected_player_target_;
    }

    [[nodiscard]] BattleSessionInputResult handle_key(std::uint8_t translated_key);
    [[nodiscard]] std::vector<BattleAudioCommand> take_audio_commands();
    void advance(std::uint32_t bios_tick = 0U);
    [[nodiscard]] bool render(render::IndexedFramebuffer& framebuffer);
    void finish_presented_tick(std::uint32_t bios_tick = 0U);

private:
    enum class AiMovementContinuation {
        direct,
        escape,
        attack,
        poison,
        item,
        request,
        support,
        throwing_weapon,
    };

    [[nodiscard]] bool begin_initial_battle();
    [[nodiscard]] bool begin_round(std::uint32_t bios_tick);
    [[nodiscard]] bool begin_ai_action();
    [[nodiscard]] bool advance_ai_wait(std::uint32_t bios_tick);
    [[nodiscard]] bool dispatch_selected_ai_action();
    [[nodiscard]] bool begin_ai_movement_to(
        std::int16_t target_slot,
        BattlePathCoord target,
        std::int16_t mode,
        std::int16_t range,
        AiMovementContinuation continuation);
    [[nodiscard]] bool begin_ai_movement(
        BattleAiMovementPlan plan,
        AiMovementContinuation continuation);
    [[nodiscard]] bool advance_ai_movement_step();
    [[nodiscard]] bool advance_ai_movement_wait(std::uint32_t bios_tick);
    [[nodiscard]] bool finish_ai_movement();
    [[nodiscard]] bool finish_ai_handler(BattlePlayerAction action, bool rest_first);
    [[nodiscard]] bool begin_player_action_menu();
    [[nodiscard]] bool begin_player_attack();
    [[nodiscard]] bool begin_player_attack_execution();
    [[nodiscard]] BattleSessionInputResult handle_player_magic_selection_key(
        std::uint8_t translated_key);
    [[nodiscard]] BattleSessionInputResult handle_player_attack_direction_key(
        std::uint8_t translated_key);
    [[nodiscard]] bool begin_player_attack_iteration(
        std::optional<BattlePathCoord> target = std::nullopt);
    [[nodiscard]] bool advance_player_attack_commit_wait(std::uint32_t bios_tick);
    [[nodiscard]] bool commit_player_attack_iteration();
    [[nodiscard]] bool advance_player_attack_level_wait(std::uint32_t bios_tick);
    [[nodiscard]] bool finish_player_attack_iteration();
    [[nodiscard]] bool begin_player_movement();
    [[nodiscard]] bool begin_player_targeting(BattlePlayerAction action);
    [[nodiscard]] BattleSessionInputResult handle_player_movement_key(
        std::uint8_t translated_key);
    [[nodiscard]] BattleSessionInputResult handle_player_targeting_key(
        std::uint8_t translated_key);
    [[nodiscard]] bool begin_player_target_effect(
        BattlePlayerAction action, BattlePathCoord target);
    [[nodiscard]] bool prepare_player_magic_frame();
    [[nodiscard]] bool advance_player_magic_wait(std::uint32_t bios_tick);
    [[nodiscard]] bool begin_player_damage_animation();
    [[nodiscard]] bool prepare_player_damage_frame();
    [[nodiscard]] bool advance_player_damage_wait(std::uint32_t bios_tick);
    [[nodiscard]] bool finish_player_target_effect();
    [[nodiscard]] bool advance_player_movement_step();
    [[nodiscard]] bool advance_player_movement_wait(std::uint32_t bios_tick);
    [[nodiscard]] bool rebuild_player_menu_after_movement();
    [[nodiscard]] std::optional<std::size_t> action_for_ordinal(
        std::size_t ordinal) const noexcept;
    [[nodiscard]] BattleSessionInputResult handle_player_action_key(
        std::uint8_t translated_key);
    [[nodiscard]] bool dispatch_selected_player_action();
    [[nodiscard]] bool finish_current_actor(BattlePlayerAction action);
    [[nodiscard]] bool begin_actor_present();
    [[nodiscard]] bool render_party_selection(
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_battlefield(
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_player_action_menu(
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_player_magic_selection(
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_player_attack_direction(
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_player_attack_level(
        render::IndexedFramebuffer& framebuffer);
    void capture_selection_background(
        const render::IndexedFramebuffer& framebuffer) noexcept;
    void restore_selection_background(
        render::IndexedFramebuffer& framebuffer) const noexcept;

    struct PlayerAttackState {
        BattleAttackProfile profile;
        std::int16_t special_attack_bonus{};
        std::int16_t iteration{};
        std::optional<BattlePathCoord> target;
        std::int16_t direction{-1};
        std::vector<std::uint8_t> level_text;
    };

    struct PlayerTargetEffectState {
        BattlePlayerAction action{};
        BattleMagicAnimationPlan magic_animation;
        std::size_t magic_frame{};
        std::array<BattleDamageAnimationFrame, 10> damage_animation{};
        std::size_t damage_frame{};
        std::uint32_t animation_wait_tick{};
        std::int32_t animation_wait_tick_changes_remaining{};
        std::int16_t effect_id{};
        std::int16_t damage_kind{};
        bool damage_suppress_flash{};
        std::vector<BattleAudioCommand> audio_commands;
    };

    model::RangerState& ranger_;
    random::LegacyRandom& random_;
    BattleData data_;
    BattleSetup setup_;
    BattlePathing pathing_;
    BattleRenderer renderer_;
    BattleRenderState render_state_{};
    BattleSessionPhase phase_{BattleSessionPhase::party_selection};
    std::vector<compat::LegacyPalette> fade_palettes_;
    std::size_t fade_frame_{};
    std::size_t current_actor_slot_{};
    BattleOutcome outcome_{BattleOutcome::ongoing};
    std::optional<BattleAiTurnPrelude> ai_turn_prelude_;
    std::optional<BattleAiTurnDecision> ai_turn_decision_;
    std::uint32_t round_tick_{};
    std::uint32_t ai_wait_tick_{};
    std::int32_t ai_wait_tick_changes_remaining_{};
    std::unique_ptr<BattleAiMovementPlan> ai_movement_plan_;
    std::optional<AiMovementContinuation> ai_movement_continuation_;
    std::optional<BattleAiAttackPlan> ai_attack_plan_;
    std::optional<BattleAiPoisonPlan> ai_poison_plan_;
    std::optional<BattleAiItemPlan> ai_item_plan_;
    std::optional<BattleAiRequestPlan> ai_request_plan_;
    std::optional<BattleAiSupportPlan> ai_support_plan_;
    std::uint32_t ai_movement_wait_tick_{};
    std::int32_t ai_movement_wait_tick_changes_remaining_{};
    BattlePlayerActionMenuState player_action_menu_{};
    std::optional<BattleMagicSelectionState> player_magic_selection_;
    std::optional<BattleCursorSelectionState> player_cursor_selection_;
    std::optional<BattlePathCoord> selected_player_target_;
    std::unique_ptr<PlayerAttackState> player_attack_;
    std::unique_ptr<PlayerTargetEffectState> player_target_effect_;
    std::int16_t selected_magic_slot_{};
    std::optional<BattlePlayerMovementPlan> player_movement_plan_;
    std::uint32_t player_movement_wait_tick_{};
    std::int32_t player_movement_wait_tick_changes_remaining_{};
    std::array<std::uint8_t, compat::kLegacyPixelCount> selection_background_{};
    compat::LegacyPalette selection_palette_{};
    bool selection_background_captured_{};
    bool frame_rendered_{};
    bool grants_experience_{};
    std::string error_;
};

}  // namespace openlegend::battle
