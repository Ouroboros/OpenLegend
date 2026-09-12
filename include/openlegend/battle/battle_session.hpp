#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_pathing.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/rgba_fade.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::battle {

enum class BattleSessionPhase {
    party_selection,
    initial_fade_to_black,
    initial_present,
    initial_fade,
    round_start,
    actor_present,
    player_action_initial_present,
    player_action_return_present,
    player_action,
    player_action_selected,
    player_magic_selection,
    player_attack_direction,
    player_item_selection,
    player_item_context_present,
    player_item_effect_present,
    player_item_effect_wait,
    player_status_selection,
    player_status_page_present,
    player_status_page_wait,
    player_movement_select,
    player_targeting_select,
    player_effect_prelude_present,
    player_effect_prelude_wait,
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
    ai_item_effect_present,
    ai_item_post_effect_wait,
    ai_effect_prelude_present,
    ai_effect_prelude_wait,
    ai_magic_frame_present,
    ai_magic_wait,
    ai_damage_frame_present,
    ai_damage_wait,
    ai_attack_commit_present,
    ai_attack_commit_wait,
    ai_attack_level_present,
    ai_attack_level_wait,
    ai_movement_step_present,
    ai_movement_wait,
    round_wait,
    battle_outcome,
    battle_outcome_wait,
    post_battle_message_present,
    post_battle_message_wait,
    complete,
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
    item_changed,
    item_cancelled,
    item_selected,
    item_effect_acknowledged,
    status_changed,
    status_cancelled,
    status_selected,
    status_page_advanced,
    status_closed,
    outcome_acknowledged,
    post_battle_message_acknowledged,
};

enum class BattleStepResult {
    stay,
    victory,
    defeat,
};

enum class BattleAudioBank : std::int16_t {
    attack,
    effect,
};

enum class BattleAudioAction : std::uint8_t {
    play,
    load,
    start_loaded,
};

struct BattleAudioCommand {
    BattleAudioBank bank{BattleAudioBank::effect};
    std::int16_t sample_id{};
    BattleAudioAction action{BattleAudioAction::play};

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
        bool grant_experience,
        BattleRenderState initial_render_state = {},
        std::int16_t* legacy_player_item_slot = nullptr,
        std::int16_t* legacy_hp_cost_scale = nullptr,
        std::int16_t* legacy_magic_slot = nullptr);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD BattleSessionPhase phase() const noexcept { return phase_; }

    NODISCARD std::int16_t battle_id() const noexcept { return data_.battle_id(); }

    NODISCARD bool grants_experience() const noexcept { return grants_experience_; }

    NODISCARD std::int16_t view_x() const noexcept { return render_state_.view_x; }

    NODISCARD std::int16_t view_y() const noexcept { return render_state_.view_y; }

    NODISCARD std::size_t current_actor_slot() const noexcept { return current_actor_slot_; }

    NODISCARD BattleOutcome outcome() const noexcept { return outcome_; }

    NODISCARD BattleStepResult result() const noexcept { return result_; }

    NODISCARD bool finished() const noexcept { return result_ != BattleStepResult::stay; }

    NODISCARD const std::optional<BattlePostBattleResult>& post_battle_result() const noexcept {
        return post_battle_result_;
    }

    NODISCARD std::size_t post_battle_message_index() const noexcept {
        return post_battle_message_index_;
    }

    NODISCARD std::size_t post_battle_message_count() const noexcept {
        return post_battle_messages_.size();
    }

    NODISCARD std::size_t fade_frame_count() const noexcept {
        return render::kFadeFromBlackFrameCount + 1U;
    }

    NODISCARD std::size_t fade_frame() const noexcept { return fade_frame_; }

    NODISCARD std::optional<std::uint8_t> rgba_fade_alpha() const noexcept;

    NODISCARD bool frame_rendered() const noexcept { return frame_rendered_; }

    NODISCARD bool needs_immediate_frame(std::uint32_t bios_tick) const noexcept;

    NODISCARD const BattleRenderState& render_state() const noexcept {
        return render_state_;
    }

    NODISCARD const BattleSetup& setup() const noexcept { return setup_; }

    NODISCARD BattleSetup& setup() noexcept { return setup_; }

    NODISCARD const BattleData& data() const noexcept { return data_; }

    void set_confirmation_state(bool active) noexcept { confirmation_state_ = active; }

    void set_player_menu_direction_states(bool down, bool up) noexcept {
        player_menu_down_state_ = down;
        player_menu_up_state_ = up;
    }

    void set_cursor_selection_input_states(
        bool down, bool right, bool left, bool up, bool escape) noexcept {
        cursor_down_state_ = down;
        cursor_right_state_ = right;
        cursor_left_state_ = left;
        cursor_up_state_ = up;
        cursor_escape_state_ = escape;
    }

    NODISCARD bool player_menu_uses_key_states() const noexcept {
        return phase_ == BattleSessionPhase::party_selection ||
            phase_ == BattleSessionPhase::player_action_initial_present ||
            phase_ == BattleSessionPhase::player_action_return_present ||
            phase_ == BattleSessionPhase::player_action ||
            phase_ == BattleSessionPhase::player_magic_selection;
    }

    NODISCARD bool cursor_selection_uses_key_states() const noexcept {
        return phase_ == BattleSessionPhase::player_attack_direction ||
            phase_ == BattleSessionPhase::player_movement_select ||
            phase_ == BattleSessionPhase::player_targeting_select;
    }

    std::uint8_t take_clear_player_menu_direction_request() noexcept {
        const auto key = clear_player_menu_direction_requested_;
        clear_player_menu_direction_requested_ = 0U;
        return key;
    }

    std::uint8_t take_clear_cursor_selection_key_request() noexcept {
        const auto key = clear_cursor_selection_key_requested_;
        clear_cursor_selection_key_requested_ = 0U;
        return key;
    }

    bool take_clear_confirmation_states_request() noexcept {
        const auto requested = clear_confirmation_states_requested_;
        clear_confirmation_states_requested_ = false;
        return requested;
    }

    NODISCARD const BattlePlayerActionMenuState& player_action_menu() const noexcept {
        return player_action_menu_;
    }

    NODISCARD const std::optional<BattleMagicSelectionState>&
    player_magic_selection() const noexcept {
        return player_magic_selection_;
    }

    NODISCARD std::int16_t selected_magic_slot() const noexcept {
        return selected_magic_slot_;
    }

    NODISCARD const BattleItemSelectionState* player_item_selection() const noexcept;

    NODISCARD std::int16_t player_item_page() const noexcept;

    NODISCARD std::int16_t player_item_row() const noexcept;

    NODISCARD std::int16_t player_item_column() const noexcept;

    NODISCARD std::uint8_t player_item_presentations_before_input() const noexcept {
        return player_item_ != nullptr ? player_item_presentations_before_input_ : 0U;
    }

    NODISCARD std::size_t player_status_count() const noexcept;

    NODISCARD std::size_t player_status_cursor() const noexcept;

    NODISCARD std::int16_t player_status_role_id() const noexcept;

    NODISCARD std::uint8_t player_status_page() const noexcept;

    NODISCARD std::optional<BattlePathCoord> active_cursor() const noexcept {
        return player_cursor_selection_.has_value()
            ? std::optional<BattlePathCoord>{player_cursor_selection_->cursor}
            : std::nullopt;
    }

    NODISCARD std::uint8_t cursor_presentations_before_input() const noexcept {
        return player_cursor_selection_.has_value()
            ? cursor_presentations_before_input_
            : 0U;
    }

    NODISCARD std::optional<BattlePathCoord> selected_player_target() const noexcept {
        return selected_player_target_;
    }

    NODISCARD BattleSessionInputResult handle_key(
        std::uint8_t translated_key,
        std::optional<std::uint32_t> bios_tick = std::nullopt);

    NODISCARD bool finish_initial_fade_to_black();

    NODISCARD std::vector<BattleAudioCommand> take_audio_commands();

    void advance(std::uint32_t bios_tick = 0U);

    NODISCARD bool render(
        render::IndexedFramebuffer& framebuffer,
        bool party_selection_background_redrawn = false);

    void finish_presented_tick(std::uint32_t bios_tick = 0U);

private:
    enum class PostBattleMessageKind {
        experience,
        level_up,
        practice,
        magic_level,
        craft,
    };

    struct PostBattleMessage {
        PostBattleMessageKind kind{PostBattleMessageKind::experience};
        std::size_t role_result_index{};
        std::size_t magic_increase_index{};
    };

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

    NODISCARD bool prepare_initial_fade_to_black();

    NODISCARD bool begin_initial_battle();

    NODISCARD bool begin_round(std::uint32_t bios_tick);

    NODISCARD bool begin_ai_action();

    NODISCARD bool advance_ai_wait(std::uint32_t bios_tick);

    NODISCARD bool dispatch_selected_ai_action();

    NODISCARD bool begin_ai_attack_action();

    NODISCARD bool begin_ai_attack_execution();

    NODISCARD bool continue_ai_poison_plan();

    NODISCARD bool begin_ai_poison_execution();

    NODISCARD bool continue_ai_request_plan();

    NODISCARD bool continue_ai_support_plan();

    NODISCARD bool begin_ai_support_execution();

    NODISCARD bool continue_ai_item_plan();

    NODISCARD bool begin_ai_item_execution();

    NODISCARD bool begin_ai_throwing_weapon_execution();

    NODISCARD bool commit_ai_throwing_weapon_effect();

    NODISCARD bool begin_ai_item_post_effect_wait();

    NODISCARD bool advance_ai_item_post_effect_wait(std::uint32_t bios_tick);

    NODISCARD bool finish_ai_item_action();

    NODISCARD bool begin_ai_movement_to(
        std::int16_t target_slot,
        BattlePathCoord target,
        std::int16_t mode,
        std::int16_t range,
        AiMovementContinuation continuation);

    NODISCARD bool begin_ai_movement(
        BattleAiMovementPlan plan,
        AiMovementContinuation continuation);

    NODISCARD bool advance_ai_movement_step();

    NODISCARD bool advance_ai_movement_wait(std::uint32_t bios_tick);

    NODISCARD bool finish_ai_movement();

    NODISCARD bool finish_ai_handler(BattlePlayerAction action, bool rest_first);

    NODISCARD bool begin_player_action_menu();

    NODISCARD bool begin_player_attack();

    NODISCARD bool begin_player_attack_execution();

    NODISCARD BattleSessionInputResult handle_player_magic_selection_key(
        std::uint8_t translated_key);

    NODISCARD BattleSessionInputResult handle_player_attack_direction_key(
        std::uint8_t translated_key);

    void poll_player_attack_direction_states();

    NODISCARD bool begin_player_attack_iteration(
        std::optional<BattlePathCoord> target = std::nullopt);

    NODISCARD bool advance_player_attack_commit_wait(std::uint32_t bios_tick);

    NODISCARD bool commit_player_attack_iteration();

    NODISCARD bool advance_player_attack_level_wait(std::uint32_t bios_tick);

    NODISCARD bool finish_player_attack_iteration();

    NODISCARD bool begin_player_movement();

    NODISCARD bool begin_player_item_selection();

    NODISCARD BattleSessionInputResult handle_player_item_key(
        std::uint8_t translated_key);

    NODISCARD bool continue_player_item_after_context_present();

    NODISCARD bool begin_player_status_selection();

    NODISCARD BattleSessionInputResult handle_player_status_selection_key(
        std::uint8_t translated_key);

    NODISCARD BattleSessionInputResult handle_player_status_page_key(
        std::uint8_t translated_key);

    NODISCARD bool begin_player_targeting(BattlePlayerAction action);

    NODISCARD BattleSessionInputResult handle_player_movement_key(
        std::uint8_t translated_key);

    NODISCARD BattleSessionInputResult handle_player_targeting_key(
        std::uint8_t translated_key);

    NODISCARD bool begin_player_target_effect(
        BattlePlayerAction action, BattlePathCoord target);

    NODISCARD bool commit_player_throwing_weapon_effect();

    NODISCARD bool prepare_player_magic_frame();

    NODISCARD bool advance_player_effect_prelude_wait(std::uint32_t bios_tick);

    NODISCARD bool prepare_player_effect_frame();

    NODISCARD bool advance_player_magic_wait(std::uint32_t bios_tick);

    NODISCARD bool begin_player_damage_animation();

    NODISCARD bool prepare_player_damage_frame();

    NODISCARD bool advance_player_damage_wait(std::uint32_t bios_tick);

    NODISCARD bool finish_player_target_effect();

    NODISCARD bool advance_player_movement_step();

    NODISCARD bool advance_player_movement_wait(std::uint32_t bios_tick);

    NODISCARD bool rebuild_player_menu_after_movement();

    NODISCARD std::optional<std::size_t> action_for_ordinal(
        std::size_t ordinal) const noexcept;

    NODISCARD BattleSessionInputResult handle_player_action_key(
        std::uint8_t translated_key);

    NODISCARD bool dispatch_selected_player_action();

    NODISCARD bool finish_player_action_call(bool redraw_completed = false);

    NODISCARD bool finish_current_actor(BattlePlayerAction action);

    NODISCARD bool begin_battle_outcome(BattleOutcome outcome);

    NODISCARD bool finish_outcome_round();

    void consume_actor_confirmation_state() noexcept;

    NODISCARD bool begin_actor_present();

    NODISCARD bool begin_post_battle_settlement();

    NODISCARD bool begin_post_battle_role();

    NODISCARD bool continue_post_battle_level();

    NODISCARD bool continue_post_battle_practice();

    NODISCARD bool continue_post_battle_crafting();

    NODISCARD bool finish_post_battle_role();

    NODISCARD bool schedule_post_battle_message(PostBattleMessage message);

    NODISCARD std::optional<BattleLevelUpResult> preview_post_battle_level(
        std::size_t role_id);

    NODISCARD std::optional<BattlePracticeResult> preview_post_battle_practice(
        std::size_t role_id);

    NODISCARD bool advance_post_battle_message();

    NODISCARD bool render_party_selection(
        render::IndexedFramebuffer& framebuffer,
        bool background_redrawn);

    NODISCARD bool render_battlefield(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_action_menu(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_magic_selection(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_item_selection(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_item_effect(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_status_selection(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_status_page(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_attack_direction(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_player_attack_level(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_battle_outcome(
        render::IndexedFramebuffer& framebuffer);

    NODISCARD bool render_post_battle_message(
        render::IndexedFramebuffer& framebuffer);

    void capture_selection_background(
        const render::IndexedFramebuffer& framebuffer) noexcept;

    void restore_selection_background(
        render::IndexedFramebuffer& framebuffer) const noexcept;

    struct PlayerAttackState {
        BattleAttackProfile profile;
        std::int16_t special_attack_bonus{};
        std::int16_t iteration{};
        bool ai_controlled{};
        std::optional<BattlePathCoord> target;
        std::int16_t direction{-1};
        text::GameText level_text;
    };

    struct PlayerItemState {
        BattleItemSelectionState selection;
        std::int16_t page{};
        std::int16_t row{};
        std::int16_t column{};
        std::optional<std::size_t> selected_inventory_slot;
        std::int16_t selected_item_id{-1};
        std::optional<BattleItemEffectResult> effect_result;
        bool inventory_consumed{};
    };

    struct PlayerStatusState {
        std::size_t party_count{};
        std::size_t cursor{};
        std::int16_t role_id{-1};
        std::uint8_t page{};
    };

    struct PlayerTargetEffectState {
        BattlePlayerAction action{};
        bool ai_controlled{};
        BattleMagicAnimationPlan magic_animation;
        std::optional<BattleEffectAnimationPlan> effect_animation;
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
    std::size_t fade_frame_{};
    std::size_t current_actor_slot_{};
    std::int16_t legacy_ai_target_slot_{};
    std::int16_t owned_legacy_player_item_slot_{-1};
    std::int16_t* legacy_player_item_slot_{};
    bool confirmation_state_{};
    bool clear_confirmation_states_requested_{};
    bool player_menu_down_state_{};
    bool player_menu_up_state_{};
    std::uint8_t clear_player_menu_direction_requested_{};
    bool cursor_down_state_{};
    bool cursor_right_state_{};
    bool cursor_left_state_{};
    bool cursor_up_state_{};
    bool cursor_escape_state_{};
    std::uint8_t clear_cursor_selection_key_requested_{};
    std::unique_ptr<render::IndexedFramebuffer> player_action_frame_;
    bool player_action_retain_callee_frame_{};
    bool player_automatic_action_{};
    BattleOutcome outcome_{BattleOutcome::ongoing};
    BattleStepResult result_{BattleStepResult::stay};
    std::optional<BattlePostBattleResult> post_battle_result_;
    std::vector<PostBattleMessage> post_battle_messages_;
    std::size_t post_battle_message_index_{};
    std::size_t post_battle_role_index_{};
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
    std::uint32_t ai_item_wait_tick_{};
    std::int32_t ai_item_wait_tick_changes_remaining_{};
    std::uint32_t ai_movement_wait_tick_{};
    std::int32_t ai_movement_wait_tick_changes_remaining_{};
    BattlePlayerActionMenuState player_action_menu_{};
    std::optional<BattleMagicSelectionState> player_magic_selection_;
    std::uint8_t player_magic_presentations_before_input_{};
    std::optional<BattleCursorSelectionState> player_cursor_selection_;
    std::uint8_t cursor_presentations_before_input_{};
    std::optional<BattlePathCoord> selected_player_target_;
    std::unique_ptr<PlayerAttackState> player_attack_;
    std::uint8_t player_attack_direction_presentations_before_input_{};
    std::unique_ptr<PlayerItemState> player_item_;
    std::uint8_t player_item_presentations_before_input_{};
    std::optional<PlayerStatusState> player_status_;
    std::unique_ptr<PlayerTargetEffectState> player_target_effect_;
    std::int16_t selected_magic_slot_{};
    std::int16_t* legacy_magic_slot_{};
    std::optional<BattlePlayerMovementPlan> player_movement_plan_;
    std::uint32_t player_movement_wait_tick_{};
    std::int32_t player_movement_wait_tick_changes_remaining_{};
    std::vector<std::uint8_t> selection_background_ =
        std::vector<std::uint8_t>(compat::kLegacyPixelCount);
    compat::LegacyPalette selection_palette_{};
    bool selection_background_captured_{};
    bool frame_rendered_{};
    bool player_menu_redraw_pending_{};
    bool grants_experience_{};
    std::string error_;
};

}  // namespace openlegend::battle
