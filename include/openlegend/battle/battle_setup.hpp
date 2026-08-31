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
    std::string error_;
};

}  // namespace openlegend::battle
