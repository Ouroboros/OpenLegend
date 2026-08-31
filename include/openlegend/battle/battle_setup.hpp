#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_pathing.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"

namespace openlegend::battle {

inline constexpr std::size_t kBattleCombatantCount = 26U;
inline constexpr std::size_t kBattleCombatantWords = 14U;
inline constexpr std::size_t kBattlePartySlots = 6U;
inline constexpr std::size_t kBattleEnemySlots = 20U;

namespace combatant_word {
inline constexpr std::size_t role_id = 0U;
inline constexpr std::size_t side = 1U;
inline constexpr std::size_t x = 2U;
inline constexpr std::size_t y = 3U;
inline constexpr std::size_t initial_mode = 4U;
inline constexpr std::size_t occupancy_hidden = 5U;
inline constexpr std::size_t round_value = 6U;
inline constexpr std::size_t action_done = 7U;
inline constexpr std::size_t sprite = 8U;
inline constexpr std::size_t damage_value = 9U;
inline constexpr std::size_t ai_action = 10U;
inline constexpr std::size_t ai_target = 11U;
inline constexpr std::size_t ai_poison_target = 12U;
inline constexpr std::size_t attack_counter = 13U;
}  // namespace combatant_word

struct BattleCombatant {
    std::array<std::int16_t, kBattleCombatantWords> words{};
};

struct BattleAttackProfile {
    std::int16_t magic_slot{};
    std::int16_t magic_id{};
    std::int16_t level_index{};
    std::int16_t select_distance{};
    std::int16_t attack_distance{};
    std::int16_t area_type{};
    std::int16_t hurt_type{};
    std::int16_t attack_count{};
    std::int16_t need_mp{};
};

struct BattleHpDamageResult {
    std::int16_t damage{};
    std::int16_t cost_scale{};
};

struct BattleAreaResult {
    std::int16_t hit_count{};
    std::optional<std::int16_t> effect_kind;
};

struct BattleItemSelectionState {
    std::array<std::int16_t, model::kInventoryCount> inventory_slots{};
    std::int16_t count{};
};

struct BattleThrownItemResult {
    std::int16_t hit_count{};
    std::optional<std::int16_t> effect_id;
    std::int16_t damage{};
    bool inventory_consumed{};
};

struct BattleItemEffectResult {
    std::array<std::int16_t, 23U> deltas{};
    std::int16_t effect_count{};
    std::int16_t panel_x{70};
    std::int16_t panel_y{18};
    std::int16_t panel_width{148};
    std::int16_t panel_height{30};
    std::int16_t post_effect_tick_changes{9};
    bool has_effect{};
    bool battle_redraw_required{};
    bool wait_for_input{};
    bool item_consumed{};
};

struct BattleRestResult {
    std::int16_t physical_power{};
    std::int16_t hp{};
    std::int16_t mp{};
};

enum class BattleAiAction : std::int16_t {
    none = 0,
    move = 1,
    attack = 2,
    use_poison = 3,
    detox = 4,
    medicine = 5,
    item = 6,
    wait = 7,
    request_medicine = 8,
    request_detox = 9,
    throwing_weapon = 10,
    escape = 11,
};

enum class BattleAiItemSource : std::int16_t {
    none,
    inventory,
    carried,
};

struct BattleAiChoice {
    BattleAiAction action{BattleAiAction::none};
    std::int16_t target_slot{-1};
    BattlePathCoord target{};
    BattleAiItemSource item_source{BattleAiItemSource::none};
    std::int16_t item_slot{-1};
    bool action_code_written{};
};

enum class BattleAiHandler : std::int16_t {
    rest,
    move,
    attack,
    use_poison,
    detox,
    medicine,
    item,
    request_medicine,
    request_detox,
    throwing_weapon,
    escape,
};

struct BattleAiTurnPrelude {
    std::int16_t allied_total{};
    std::int16_t opponent_total{};
    std::int16_t allied_count{};
    std::int16_t opponent_count{};
    std::int16_t wait_ticks{300};
    bool render_required{true};
    bool present_required{true};
};

struct BattleAiTurnDecision {
    BattleAiChoice choice;
    BattleAiHandler handler{BattleAiHandler::rest};
};

struct BattleAiEscapePlan {
    std::optional<BattlePathCoord> destination;
    std::int32_t maximum_enemy_distance_sum{};
    bool rest_after_move{};
};

enum class BattleAiTargetStrategy : std::int16_t {
    strongest_attack,
    weakest_attack,
    specialist,
    nearest,
};

struct BattleAiTargetSelection {
    std::int16_t target_slot{-1};
    BattleAiTargetStrategy strategy{BattleAiTargetStrategy::nearest};
    bool target_written{};
};

enum class BattleAiAttackNextStep : std::int16_t {
    attack,
    move,
    rest,
    finish,
};

struct BattleAiAttackPlan {
    std::int16_t magic_slot{};
    std::int16_t magic_id{};
    std::int16_t special_attack_bonus{};
    std::int16_t select_distance{};
    std::int16_t area_type{};
    std::int16_t target_slot{-1};
    std::int16_t target_distance{};
    std::int16_t movement_mode{};
    BattleAiTargetStrategy target_strategy{BattleAiTargetStrategy::nearest};
    BattleAiAttackNextStep next_step{BattleAiAttackNextStep::finish};
    bool target_reselected{};
    bool automatic_attack{true};
    bool mark_action_done_after_step{true};
};

enum class BattleAiPoisonTargetStrategy : std::int16_t {
    strongest_attack,
    first_eligible_stale_distance,
    none,
};

struct BattleAiPoisonTargetSelection {
    std::int16_t target_slot{-1};
    std::int16_t stale_target_distance{};
    BattleAiPoisonTargetStrategy strategy{BattleAiPoisonTargetStrategy::none};
    bool target_written{};
};

enum class BattleAiPoisonNextStep : std::int16_t {
    poison,
    move,
    attack_fallback,
    rest,
};

struct BattleAiPoisonPlan {
    std::int16_t target_slot{-1};
    std::int16_t targeting_range{};
    std::int16_t target_distance{};
    std::int16_t range_check_count{};
    std::int16_t movement_mode{3};
    std::int16_t allied_total{};
    std::int16_t allied_count{};
    std::int32_t doubled_target_attack{};
    std::int32_t doubled_allied_average{};
    BattleAiPoisonTargetStrategy target_strategy{BattleAiPoisonTargetStrategy::none};
    BattleAiPoisonNextStep next_step{BattleAiPoisonNextStep::attack_fallback};
    bool outer_marks_action_done_after_handler{true};
};

enum class BattleAiItemNextStep : std::int16_t {
    use_item,
    move,
    attack_fallback,
};

struct BattleAiItemPlan {
    BattleAiItemSource item_source{BattleAiItemSource::none};
    std::int16_t item_slot{-1};
    std::int16_t item_id{-1};
    std::int16_t use_mode{};
    std::optional<BattlePathCoord> relocation_destination;
    std::int32_t maximum_enemy_distance_sum{};
    std::int16_t target_slot{-1};
    std::int16_t targeting_range{};
    std::int16_t target_distance{};
    std::int16_t range_check_count{};
    std::int16_t movement_mode{1};
    BattleAiTargetStrategy target_strategy{BattleAiTargetStrategy::nearest};
    BattleAiItemNextStep next_step{BattleAiItemNextStep::use_item};
    bool target_written{};
    bool outer_marks_action_done_after_handler{true};
};

enum class BattleAiRequestNextStep : std::int16_t {
    move,
    automatic_attack,
};

struct BattleAiRequestPlan {
    BattleAiAction request_action{BattleAiAction::request_medicine};
    std::int16_t target_slot{-1};
    BattlePathCoord target{};
    std::int16_t movement_mode{};
    std::int16_t movement_value{};
    BattleAiRequestNextStep next_step{BattleAiRequestNextStep::automatic_attack};
    bool restore_request_target_before_attack{true};
    bool outer_marks_action_done_after_handler{true};
};

enum class BattleAiSupportNextStep : std::int16_t {
    apply_support,
    move,
    automatic_attack,
    rest,
};

struct BattleAiSupportPlan {
    BattleAiAction support_action{BattleAiAction::medicine};
    std::int16_t target_slot{-1};
    BattlePathCoord target{};
    std::int16_t targeting_range{};
    std::int16_t target_distance{};
    std::int16_t range_check_count{};
    std::int16_t movement_mode{1};
    std::int16_t movement_value{};
    std::int16_t allied_total{};
    std::int16_t allied_count{};
    std::int32_t doubled_actor_attack{};
    std::int32_t doubled_allied_average{};
    BattleAiSupportNextStep next_step{BattleAiSupportNextStep::apply_support};
    bool restore_target_after_move{true};
    bool outer_marks_action_done_after_handler{true};
};

enum class BattleRenderCommandKind : std::int16_t {
    legacy_sprite,
    cursor_overlay,
    highlighted_sprite,
    damage_text,
};

struct BattleRenderCommand {
    BattleRenderCommandKind kind{};
    std::int16_t map_x{};
    std::int16_t map_y{};
    std::int32_t screen_x{};
    std::int32_t screen_y{};
    std::int32_t sprite_id{};
    std::int16_t overlay_variant{};
    std::int16_t style{};
    std::int16_t value{};
};

struct BattleRenderState {
    std::int16_t view_x{};
    std::int16_t view_y{};
    std::int16_t path_limit{};
    BattlePathCoord primary_cursor{};
    bool primary_cursor_alternate{};
    bool secondary_cursor_visible{};
    BattlePathCoord secondary_cursor{};
    bool highlight_enabled{};
    std::int16_t highlight_mode{};
    bool effect_visible{};
    std::int16_t effect_id{};
    std::int16_t effect_frame_offset{};
    std::int16_t damage_kind{};
    std::int16_t damage_text_offset{};
};

struct BattleRenderPlan {
    std::vector<BattleRenderCommand> commands;
};

struct BattleMagicAnimationFrame {
    std::int16_t actor_sprite{};
    std::int16_t effect_frame{};
    std::int16_t wait_ticks{};
    bool actor_sprite_updated{};
    bool effect_visible{};
    bool dispatch_magic_sample{};
    bool dispatch_effect_sample{};
};

struct BattleMagicAnimationPlan {
    std::int16_t fight_head_id{};
    std::int16_t magic_sample_id{};
    std::int16_t effect_sample_id{};
    bool clear_effect_after_frames{};
    std::vector<BattleMagicAnimationFrame> frames;
};

struct BattleEffectAnimationPlan {
    std::int16_t magic_sample_id{};
    std::int16_t effect_sample_id{};
    std::int16_t prelude_wait_ticks{};
    bool dispatch_magic_before_prelude{};
    bool dispatch_effect_after_prelude{};
    bool clear_effect_after_frames{};
    std::vector<BattleMagicAnimationFrame> frames;
};

struct BattleDamageAnimationFrame {
    std::int16_t phase{};
    std::int16_t wait_ticks{};
    bool flash{};
};

struct BattleMagicSelectionState {
    std::array<std::int16_t, model::role_word::magic_count> available_slots{};
    std::int16_t learned_count{};
    std::int16_t available_count{};
    std::int16_t cursor{};
    std::optional<std::int16_t> selected_slot;
    bool cancelled{};
};

enum class BattleMagicSelectionAction {
    next,
    previous,
    activate,
    cancel,
};

enum class BattleMagicSelectionResult {
    changed,
    selected,
    cancelled,
    invalid,
};

enum class BattleOutcome {
    ongoing,
    defeat,
    victory,
};

enum class BattleMovementStopRule {
    destination,
    in_range,
    aligned_in_range,
};

enum class PartySelectionAction {
    next,
    previous,
    activate,
};

enum class PartySelectionResult {
    waiting,
    changed,
    complete,
    invalid,
};

class BattleSetup {
public:
    BattleSetup(BattleData& data, model::RangerState& ranger);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] bool waiting_for_party_selection() const noexcept { return waiting_; }
    [[nodiscard]] std::size_t party_prefix_length() const noexcept { return party_prefix_length_; }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    [[nodiscard]] std::int16_t combatant_count() const noexcept { return combatant_count_; }
    [[nodiscard]] std::span<const std::int16_t, kBattlePartySlots> selection_states() const noexcept {
        return selection_states_;
    }
    [[nodiscard]] std::span<const BattleCombatant, kBattleCombatantCount> combatants() const noexcept {
        return combatants_;
    }
    [[nodiscard]] std::span<BattleCombatant, kBattleCombatantCount> combatants() noexcept {
        return combatants_;
    }

    [[nodiscard]] PartySelectionResult apply(PartySelectionAction action);
    [[nodiscard]] bool sort_by_effective_speed();
    [[nodiscard]] bool prepare_round();
    [[nodiscard]] BattleOutcome evaluate_outcome();
    [[nodiscard]] std::optional<BattlePathCoord> move_one_marked_step(
        BattlePathing& pathing, std::size_t slot);
    [[nodiscard]] bool movement_should_stop(
        std::size_t slot,
        BattlePathCoord destination,
        std::size_t target_slot,
        BattleMovementStopRule rule,
        std::int16_t range) const noexcept;
    [[nodiscard]] std::size_t learned_magic_count(std::size_t slot) const noexcept;
    [[nodiscard]] std::int16_t automatic_magic_slot(
        std::size_t slot, random::LegacyRandom& random) const noexcept;
    [[nodiscard]] std::optional<BattleAttackProfile> attack_profile(
        std::size_t slot, std::int16_t magic_slot) const noexcept;
    [[nodiscard]] std::optional<BattleMagicSelectionState> begin_magic_selection(
        std::size_t slot) const noexcept;
    [[nodiscard]] static BattleMagicSelectionResult apply_magic_selection(
        BattleMagicSelectionState& state,
        BattleMagicSelectionAction action) noexcept;
    [[nodiscard]] bool commit_attack_iteration(
        std::size_t slot,
        std::int16_t magic_slot,
        std::int16_t cost_scale,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleHpDamageResult> apply_hp_damage(
        std::size_t actor_slot,
        std::size_t target_slot,
        std::int16_t magic_slot,
        std::int16_t distance,
        std::int16_t special_attack_bonus,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<std::int32_t> apply_mp_damage(
        std::size_t actor_slot,
        std::size_t target_slot,
        std::int16_t magic_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<std::int16_t> poison_targeting_range(
        std::size_t actor_slot) const noexcept;
    [[nodiscard]] std::optional<std::int16_t> apply_poison_value(
        std::size_t actor_slot,
        std::size_t target_slot);
    [[nodiscard]] std::optional<BattleAreaResult> apply_poison_target(
        std::size_t actor_slot,
        BattlePathCoord target);
    [[nodiscard]] bool finish_poison_action(std::size_t actor_slot);
    [[nodiscard]] std::optional<std::int16_t> detox_targeting_range(
        std::size_t actor_slot) const noexcept;
    [[nodiscard]] std::optional<std::int16_t> apply_detox_value(
        std::size_t actor_slot,
        std::size_t target_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAreaResult> apply_detox_target(
        std::size_t actor_slot,
        BattlePathCoord target,
        random::LegacyRandom& random);
    [[nodiscard]] bool finish_detox_action(std::size_t actor_slot);
    [[nodiscard]] std::optional<std::int16_t> medicine_targeting_range(
        std::size_t actor_slot) const noexcept;
    [[nodiscard]] std::optional<std::int32_t> apply_medicine_value(
        std::size_t actor_slot,
        std::size_t target_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAreaResult> apply_medicine_target(
        std::size_t actor_slot,
        BattlePathCoord target,
        random::LegacyRandom& random);
    [[nodiscard]] bool finish_medicine_action(std::size_t actor_slot);
    [[nodiscard]] BattleItemSelectionState begin_item_selection() const noexcept;
    [[nodiscard]] bool remove_carried_item_slot(
        std::size_t actor_slot,
        std::size_t item_slot) noexcept;
    [[nodiscard]] std::optional<std::int16_t> throwing_weapon_targeting_range(
        std::size_t actor_slot) const noexcept;
    [[nodiscard]] std::optional<BattleThrownItemResult> apply_throwing_weapon_target(
        std::size_t actor_slot,
        BattlePathCoord target,
        std::size_t inventory_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleThrownItemResult> apply_ai_throwing_weapon_target(
        std::size_t actor_slot,
        BattlePathCoord target,
        const BattleAiChoice& choice,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleItemEffectResult> apply_ai_item_effect(
        std::size_t actor_slot,
        const BattleAiChoice& choice,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleRestResult> rest_actor(
        std::size_t actor_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiChoice> choose_ai_low_hp_action(std::size_t actor_slot);
    [[nodiscard]] std::optional<BattleAiChoice> choose_ai_poisoned_action(std::size_t actor_slot);
    [[nodiscard]] std::optional<BattleAiChoice> choose_ai_low_mp_action(std::size_t actor_slot);
    [[nodiscard]] std::optional<BattleAiChoice> choose_ai_medicine_target(
        std::size_t actor_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiChoice> choose_ai_detox_target(
        std::size_t actor_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiChoice> choose_ai_offensive_action(
        std::size_t actor_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiTurnPrelude> begin_ai_turn(
        std::size_t actor_slot) const noexcept;
    [[nodiscard]] std::optional<BattleAiTurnDecision> choose_ai_turn_action(
        std::size_t actor_slot,
        random::LegacyRandom& random);
    [[nodiscard]] bool finish_ai_turn(std::size_t actor_slot) noexcept;
    [[nodiscard]] std::optional<BattleAiEscapePlan> ai_escape_plan(
        std::size_t actor_slot,
        bool rest_after_move) const;
    [[nodiscard]] std::optional<BattleAiTargetSelection> choose_ai_attack_target(
        std::size_t actor_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiAttackPlan> begin_ai_attack_plan(
        std::size_t actor_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiAttackPlan> resume_ai_attack_after_move(
        std::size_t actor_slot,
        BattleAiAttackPlan plan);
    [[nodiscard]] std::optional<BattleAiPoisonTargetSelection> choose_ai_poison_target(
        std::size_t actor_slot,
        std::size_t stale_target_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiPoisonPlan> begin_ai_poison_plan(
        std::size_t actor_slot,
        std::size_t stale_target_slot,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiPoisonPlan> resume_ai_poison_after_move(
        std::size_t actor_slot,
        BattleAiPoisonPlan plan);
    [[nodiscard]] std::optional<BattleAiItemPlan> begin_ai_item_plan(
        std::size_t actor_slot,
        const BattleAiChoice& choice) const;
    [[nodiscard]] std::optional<BattleAiItemPlan> resume_ai_item_after_relocation(
        std::size_t actor_slot,
        BattleAiItemPlan plan) const noexcept;
    [[nodiscard]] std::optional<BattleAiItemPlan> begin_ai_throwing_weapon_plan(
        std::size_t actor_slot,
        const BattleAiChoice& choice,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAiItemPlan> resume_ai_throwing_weapon_after_move(
        std::size_t actor_slot,
        BattleAiItemPlan plan);
    [[nodiscard]] std::optional<BattleAiRequestPlan> begin_ai_request_plan(
        std::size_t actor_slot,
        const BattleAiChoice& choice) const noexcept;
    [[nodiscard]] std::optional<BattleAiRequestPlan> resume_ai_request_after_move(
        std::size_t actor_slot,
        BattleAiRequestPlan plan) const noexcept;
    [[nodiscard]] std::optional<BattleAiSupportPlan> begin_ai_support_plan(
        std::size_t actor_slot,
        const BattleAiChoice& choice);
    [[nodiscard]] std::optional<BattleAiSupportPlan> resume_ai_support_after_move(
        std::size_t actor_slot,
        BattleAiSupportPlan plan);
    [[nodiscard]] std::optional<std::size_t> defer_turn_to_end(std::size_t actor_slot);
    void enable_automatic_mode() noexcept { automatic_enabled_ = true; }
    [[nodiscard]] bool automatic_enabled() const noexcept { return automatic_enabled_; }
    [[nodiscard]] std::optional<BattleRenderPlan> battle_render_plan(
        const BattleRenderState& state,
        std::span<const std::int16_t> path_values) const;
    void clear_attack_effects() noexcept;
    [[nodiscard]] std::span<const std::int16_t> attack_effects() const noexcept {
        return attack_effects_;
    }
    [[nodiscard]] std::int16_t last_hp_cost_scale() const noexcept {
        return last_hp_cost_scale_;
    }
    [[nodiscard]] std::optional<BattleAreaResult> apply_attack_area(
        std::size_t actor_slot,
        std::int16_t magic_slot,
        BattlePathCoord target,
        std::int16_t special_attack_bonus,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleAreaResult> apply_line_attack_area(
        std::size_t actor_slot,
        std::int16_t magic_slot,
        std::int16_t direction,
        std::int16_t special_attack_bonus,
        random::LegacyRandom& random);
    [[nodiscard]] std::optional<BattleMagicAnimationPlan> magic_animation_plan(
        std::size_t actor_slot,
        std::int16_t magic_slot,
        std::int16_t fight_frame_count) const;
    [[nodiscard]] static std::optional<BattleEffectAnimationPlan> effect_animation_plan(
        std::int16_t effect_id);
    [[nodiscard]] static std::array<BattleDamageAnimationFrame, 10>
    damage_animation_frames(bool suppress_flash) noexcept;
    [[nodiscard]] bool finish_attack(std::size_t slot);

private:
    void initialize_combatants();
    void initialize_party();
    [[nodiscard]] bool append_combatant(
        std::int16_t role_id,
        std::int16_t side,
        std::int16_t x,
        std::int16_t y,
        std::int16_t initial_mode);
    [[nodiscard]] bool append_enemies();
    [[nodiscard]] std::int16_t sprite_word(
        std::int16_t role_id, std::int16_t initial_mode) const noexcept;
    [[nodiscard]] std::int16_t effective_speed(std::size_t slot);
    void swap_combatants(std::size_t first, std::size_t second);
    void update_occupancy(std::size_t slot);
    [[nodiscard]] BattleAiChoice commit_ai_choice(
        std::size_t actor_slot,
        BattleAiAction action,
        std::int16_t target_slot,
        BattleAiItemSource item_source = BattleAiItemSource::none,
        std::int16_t item_slot = -1,
        bool write_action_code = true) noexcept;
    [[nodiscard]] std::optional<bool> choose_ai_strongest_attack_target(
        std::size_t actor_slot);
    [[nodiscard]] std::optional<bool> choose_ai_weakest_attack_target(
        std::size_t actor_slot);
    [[nodiscard]] std::optional<bool> choose_ai_specialist_target(std::size_t actor_slot);
    [[nodiscard]] std::optional<bool> choose_ai_nearest_target(std::size_t actor_slot);
    [[nodiscard]] bool update_ai_attack_target_range(
        std::size_t actor_slot,
        std::size_t target_slot,
        BattleAiAttackPlan& plan) const;
    [[nodiscard]] std::optional<bool> choose_ai_strongest_poison_target(
        std::size_t actor_slot);
    [[nodiscard]] std::optional<bool> choose_ai_first_poison_target(
        std::size_t actor_slot,
        std::size_t stale_target_slot,
        std::int16_t& stale_target_distance);
    [[nodiscard]] bool update_ai_poison_target_range(
        std::size_t actor_slot,
        std::size_t target_slot,
        BattleAiPoisonPlan& plan) const;
    [[nodiscard]] bool update_ai_poison_fallback(
        std::size_t actor_slot,
        std::size_t target_slot,
        BattleAiPoisonPlan& plan);
    [[nodiscard]] bool update_ai_support_target_range(
        std::size_t actor_slot,
        std::size_t target_slot,
        BattleAiSupportPlan& plan) const;
    [[nodiscard]] bool update_ai_support_fallback(
        std::size_t actor_slot,
        BattleAiSupportPlan& plan);
    [[nodiscard]] std::optional<std::int16_t> ai_item_id(
        std::size_t actor_slot,
        const BattleAiChoice& choice) const noexcept;
    [[nodiscard]] bool update_ai_throwing_weapon_target_range(
        std::size_t actor_slot,
        std::size_t target_slot,
        BattleAiItemPlan& plan) const;

    BattleData& data_;
    model::RangerState& ranger_;
    std::array<BattleCombatant, kBattleCombatantCount> combatants_{};
    std::array<std::int16_t, kBattlePartySlots> selection_states_{};
    std::array<std::int16_t, kBattleOccupancyCells> attack_effects_{};
    std::int16_t combatant_count_{};
    std::int16_t last_hp_cost_scale_{};
    std::size_t party_prefix_length_{kBattlePartySlots};
    std::size_t cursor_{};
    bool waiting_{};
    bool automatic_enabled_{};
    std::string error_;
};

}  // namespace openlegend::battle
