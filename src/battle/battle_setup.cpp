#include "openlegend/battle/battle_setup.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace openlegend::battle {
namespace {

constexpr std::array<std::int16_t, kBattleCombatantWords> kInitialCombatantWords{
    -1, -1, 0, 0, 0, 0, 0, 0, 5098, 0, 0, -1, -1, 0};
constexpr std::size_t kPresetPartyBegin = 9U;
constexpr std::size_t kFixedPartyBegin = 15U;
constexpr std::size_t kPartyXBegin = 21U;
constexpr std::size_t kPartyYBegin = 27U;
constexpr std::size_t kEnemyBegin = 33U;
constexpr std::size_t kEnemyXBegin = 53U;
constexpr std::size_t kEnemyYBegin = 73U;
constexpr std::int16_t kBattleSpriteBase = 5106;

[[nodiscard]] constexpr std::int16_t wrapping_i16(const std::int32_t value) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

}  // namespace

BattleSetup::BattleSetup(BattleData& data, model::RangerState& ranger)
    : data_(data), ranger_(ranger) {
    initialize_combatants();
    if (!data_.valid()) {
        error_ = data_.error();
        return;
    }
    if (!ranger_.valid()) {
        error_ = "battle setup requires a complete ranger state";
        return;
    }
    std::ranges::fill(data_.occupancy(), static_cast<std::int16_t>(-1));
    initialize_party();
}

void BattleSetup::initialize_combatants() {
    for (auto& combatant : combatants_) {
        combatant.words = kInitialCombatantWords;
    }
}

void BattleSetup::initialize_party() {
    for (std::size_t index = 1U; index < kBattlePartySlots; ++index) {
        if (ranger_.header.team_member(index).value <= 0) {
            party_prefix_length_ = index;
            break;
        }
    }

    const auto definition = data_.definition();
    const auto has_fixed_party = std::ranges::any_of(
        definition.subspan<kFixedPartyBegin, kBattlePartySlots>(),
        [](const std::int16_t role_id) { return role_id != -1; });
    if (has_fixed_party) {
        for (std::size_t index = 0U; index < kBattlePartySlots; ++index) {
            const auto role_id = definition[kFixedPartyBegin + index];
            if (role_id == -1) {
                continue;
            }
            if (!append_combatant(
                    role_id,
                    0,
                    definition[kPartyXBegin + index],
                    definition[kPartyYBegin + index],
                    2)) {
                return;
            }
            selection_states_[index] = 2;
        }
        static_cast<void>(append_enemies());
        return;
    }

    for (std::size_t index = 0U; index < kBattlePartySlots; ++index) {
        const auto role_id = definition[kPresetPartyBegin + index];
        if (role_id == -1) {
            continue;
        }
        if (!append_combatant(
                role_id,
                0,
                definition[kPartyXBegin + index],
                definition[kPartyYBegin + index],
                2)) {
            return;
        }
        for (std::size_t party = 0U; party < party_prefix_length_; ++party) {
            if (ranger_.header.team_member(party).value == role_id) {
                selection_states_[party] = 2;
            }
        }
    }
    waiting_ = true;
}

bool BattleSetup::append_combatant(
    const std::int16_t role_id,
    const std::int16_t side,
    const std::int16_t x,
    const std::int16_t y,
    const std::int16_t initial_mode) {
    if (combatant_count_ < 0 ||
        static_cast<std::size_t>(combatant_count_) >= combatants_.size()) {
        error_ = "battle setup exceeds 26 combatant slots";
        return false;
    }
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        error_ = "battle setup role id is outside ranger records";
        return false;
    }
    if (x < 0 || x >= static_cast<std::int16_t>(kBattleExtent) || y < 0 ||
        y >= static_cast<std::int16_t>(kBattleExtent)) {
        error_ = "battle setup coordinate is outside 64x64 occupancy";
        return false;
    }

    const auto slot = static_cast<std::size_t>(combatant_count_);
    auto& words = combatants_[slot].words;
    words[combatant_word::role_id] = role_id;
    words[combatant_word::side] = side;
    words[combatant_word::x] = x;
    words[combatant_word::y] = y;
    words[combatant_word::initial_mode] = initial_mode;
    words[combatant_word::sprite] = sprite_word(role_id, initial_mode);
    const auto occupancy_index = static_cast<std::size_t>(y) * kBattleExtent +
        static_cast<std::size_t>(x);
    data_.occupancy()[occupancy_index] = combatant_count_;
    combatant_count_ = static_cast<std::int16_t>(combatant_count_ + 1);
    return true;
}

bool BattleSetup::append_enemies() {
    const auto definition = data_.definition();
    for (std::size_t index = 0U; index < kBattleEnemySlots; ++index) {
        const auto role_id = definition[kEnemyBegin + index];
        if (role_id == -1) {
            continue;
        }
        if (!append_combatant(
                role_id,
                1,
                definition[kEnemyXBegin + index],
                definition[kEnemyYBegin + index],
                1)) {
            return false;
        }
    }
    return true;
}

std::int16_t BattleSetup::sprite_word(
    const std::int16_t role_id, const std::int16_t initial_mode) const noexcept {
    const auto head_id = ranger_.roles[static_cast<std::size_t>(role_id)].word(model::role_word::head_id);
    return wrapping_i16(
        8 * static_cast<std::int32_t>(head_id) + kBattleSpriteBase +
        2 * static_cast<std::int32_t>(initial_mode));
}

std::int16_t BattleSetup::effective_speed(const std::size_t slot) {
    const auto role_id = combatants_[slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        error_ = "battle combatant role id is outside ranger records";
        return 0;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    auto speed = role.word(model::role_word::speed);
    for (std::size_t equipment = 0U; equipment < model::role_word::equipment_count; ++equipment) {
        const auto item_id = role.word(model::role_word::equipment_begin + equipment);
        if (item_id < 0) {
            continue;
        }
        if (static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            error_ = "battle equipment id is outside ranger item records";
            return 0;
        }
        speed = wrapping_i16(
            static_cast<std::int32_t>(speed) +
            ranger_.items[static_cast<std::size_t>(item_id)].word(model::item_word::add_speed));
    }
    return speed;
}

void BattleSetup::update_occupancy(const std::size_t slot) {
    const auto& words = combatants_[slot].words;
    const auto index = static_cast<std::size_t>(words[combatant_word::y]) * kBattleExtent +
        static_cast<std::size_t>(words[combatant_word::x]);
    data_.occupancy()[index] = words[combatant_word::occupancy_hidden] == 0
        ? static_cast<std::int16_t>(slot)
        : static_cast<std::int16_t>(-1);
}

void BattleSetup::swap_combatants(const std::size_t first, const std::size_t second) {
    const auto saved = combatants_[first].words;
    for (std::size_t word = 0U; word < kBattleCombatantWords; ++word) {
        if (word != combatant_word::sprite) {
            combatants_[first].words[word] = combatants_[second].words[word];
        }
    }
    update_occupancy(first);
    for (std::size_t word = 0U; word < kBattleCombatantWords; ++word) {
        if (word != combatant_word::sprite) {
            combatants_[second].words[word] = saved[word];
        }
    }
    update_occupancy(second);
    auto& first_words = combatants_[first].words;
    auto& second_words = combatants_[second].words;
    first_words[combatant_word::sprite] = sprite_word(
        first_words[combatant_word::role_id], first_words[combatant_word::initial_mode]);
    second_words[combatant_word::sprite] = sprite_word(
        second_words[combatant_word::role_id], second_words[combatant_word::initial_mode]);
}

bool BattleSetup::sort_by_effective_speed() {
    if (!valid()) {
        return false;
    }
    const auto count = static_cast<std::size_t>(combatant_count_);
    for (std::size_t first = 0U; first + 1U < count; ++first) {
        for (std::size_t second = first + 1U; second < count; ++second) {
            const auto first_speed = effective_speed(first);
            if (!valid()) {
                return false;
            }
            const auto second_speed = effective_speed(second);
            if (!valid()) {
                return false;
            }
            if (first_speed < second_speed) {
                swap_combatants(first, second);
            }
        }
    }
    return true;
}

bool BattleSetup::prepare_round() {
    if (!sort_by_effective_speed()) {
        return false;
    }
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto speed = effective_speed(slot);
        if (!valid()) {
            return false;
        }
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        const auto hurt = ranger_.roles[static_cast<std::size_t>(role_id)].word(model::role_word::hurt);
        const auto value = static_cast<std::int16_t>(speed / 15 - hurt / 40);
        combatants_[slot].words[combatant_word::round_value] = std::max<std::int16_t>(value, 0);
    }
    return true;
}

BattleOutcome BattleSetup::evaluate_outcome() {
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        auto& words = combatants_[slot].words;
        const auto role_id = words[combatant_word::role_id];
        if (ranger_.roles[static_cast<std::size_t>(role_id)].word(model::role_word::hp) <= 0 &&
            words[combatant_word::occupancy_hidden] == 0) {
            const auto occupancy_index = static_cast<std::size_t>(words[combatant_word::y]) *
                    kBattleExtent +
                static_cast<std::size_t>(words[combatant_word::x]);
            data_.occupancy()[occupancy_index] = -1;
            words[combatant_word::occupancy_hidden] = 1;
        }
    }

    auto party_alive = false;
    auto enemy_alive = false;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& words = combatants_[slot].words;
        if (words[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        if (words[combatant_word::side] == 0) {
            party_alive = true;
        } else {
            enemy_alive = true;
        }
    }
    auto outcome = BattleOutcome::ongoing;
    if (!party_alive) {
        outcome = BattleOutcome::defeat;
    }
    if (!enemy_alive) {
        outcome = BattleOutcome::victory;
    }
    return outcome;
}

std::optional<BattlePathCoord> BattleSetup::move_one_marked_step(
    BattlePathing& pathing, const std::size_t slot) {
    if (!valid() || slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    auto& words = combatants_[slot].words;
    const BattlePathCoord current{
        words[combatant_word::x],
        words[combatant_word::y],
    };
    const auto next = pathing.next_marked_step(current);
    if (!next) {
        return std::nullopt;
    }

    pathing.consume(current);
    const auto current_index = static_cast<std::size_t>(current.y) * kBattleExtent +
        static_cast<std::size_t>(current.x);
    const auto next_index = static_cast<std::size_t>(next->y) * kBattleExtent +
        static_cast<std::size_t>(next->x);
    data_.occupancy()[current_index] = -1;
    data_.occupancy()[next_index] = static_cast<std::int16_t>(slot);
    words[combatant_word::x] = next->x;
    words[combatant_word::y] = next->y;

    std::int16_t direction = 3;
    if (next->y < current.y) {
        direction = 0;
    } else if (next->x > current.x) {
        direction = 1;
    } else if (next->x < current.x) {
        direction = 2;
    }
    words[combatant_word::initial_mode] = direction;

    const auto role_id = words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        error_ = "battle movement role id is outside ranger records";
        return std::nullopt;
    }
    auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    words[combatant_word::sprite] = sprite_word(role_id, direction);
    const auto speed_step = static_cast<std::int16_t>(role.word(model::role_word::speed) / 10);
    if (words[combatant_word::round_value] == speed_step) {
        auto physical_power = wrapping_i16(
            static_cast<std::int32_t>(role.word(model::role_word::physical_power)) - 1);
        if (physical_power < 0) {
            physical_power = 0;
        }
        role.set_word(model::role_word::physical_power, physical_power);
    }
    words[combatant_word::round_value] = wrapping_i16(
        static_cast<std::int32_t>(words[combatant_word::round_value]) - 1);
    return next;
}

bool BattleSetup::movement_should_stop(
    const std::size_t slot,
    const BattlePathCoord destination,
    const std::size_t target_slot,
    const BattleMovementStopRule rule,
    const std::int16_t range) const noexcept {
    if (!valid() || slot >= static_cast<std::size_t>(combatant_count_)) {
        return true;
    }
    const auto& actor = combatants_[slot].words;
    if (actor[combatant_word::x] == destination.x &&
        actor[combatant_word::y] == destination.y) {
        return true;
    }
    if (rule == BattleMovementStopRule::destination) {
        return false;
    }
    if (actor[combatant_word::round_value] <= 0 ||
        target_slot >= static_cast<std::size_t>(combatant_count_)) {
        return true;
    }

    const auto& target = combatants_[target_slot].words;
    const auto x_difference = static_cast<std::int32_t>(target[combatant_word::x]) -
        actor[combatant_word::x];
    const auto y_difference = static_cast<std::int32_t>(target[combatant_word::y]) -
        actor[combatant_word::y];
    const auto distance = (x_difference < 0 ? -x_difference : x_difference) +
        (y_difference < 0 ? -y_difference : y_difference);
    if (distance > range) {
        return false;
    }
    if (rule == BattleMovementStopRule::in_range) {
        return true;
    }
    return actor[combatant_word::x] == target[combatant_word::x] ||
        actor[combatant_word::y] == target[combatant_word::y];
}

std::size_t BattleSetup::learned_magic_count(const std::size_t slot) const noexcept {
    if (!valid() || slot >= static_cast<std::size_t>(combatant_count_)) {
        return 0U;
    }
    const auto role_id = combatants_[slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return 0U;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    std::size_t count = 0U;
    for (std::size_t magic = 0U; magic < model::role_word::magic_count; ++magic) {
        if (role.word(model::role_word::magic_id_begin + magic) > 0) {
            ++count;
        }
    }
    return count;
}

std::int16_t BattleSetup::automatic_magic_slot(
    const std::size_t slot, random::LegacyRandom& random) const noexcept {
    return static_cast<std::int16_t>(
        random.bounded(static_cast<std::int32_t>(learned_magic_count(slot))));
}

std::optional<BattleAttackProfile> BattleSetup::attack_profile(
    const std::size_t slot, const std::int16_t magic_slot) const noexcept {
    if (!valid() || slot >= static_cast<std::size_t>(combatant_count_) || magic_slot < 0 ||
        static_cast<std::size_t>(magic_slot) >= model::role_word::magic_count) {
        return std::nullopt;
    }
    const auto role_id = combatants_[slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    const auto slot_index = static_cast<std::size_t>(magic_slot);
    const auto magic_id = role.word(model::role_word::magic_id_begin + slot_index);
    if (magic_id < 0 || static_cast<std::size_t>(magic_id) >= ranger_.magics.size()) {
        return std::nullopt;
    }
    const auto level_index = static_cast<std::size_t>(
        role.unsigned_word(model::role_word::magic_level_begin + slot_index) / 100U);
    if (level_index >= model::magic_word::level_value_count) {
        return std::nullopt;
    }
    const auto& magic = ranger_.magics[static_cast<std::size_t>(magic_id)];
    return BattleAttackProfile{
        magic_slot,
        magic_id,
        static_cast<std::int16_t>(level_index),
        magic.word(model::magic_word::select_distance_begin + level_index),
        magic.word(model::magic_word::attack_distance_begin + level_index),
        magic.word(model::magic_word::attack_area_type),
        magic.word(model::magic_word::hurt_type),
        static_cast<std::int16_t>(
            role.word(model::role_word::attack_twice) == 1 ? 2 : 1),
        magic.word(model::magic_word::need_mp),
    };
}

bool BattleSetup::commit_attack_iteration(
    const std::size_t slot,
    const std::int16_t magic_slot,
    const std::int16_t cost_scale,
    random::LegacyRandom& random) {
    const auto profile = attack_profile(slot, magic_slot);
    if (!profile) {
        error_ = "battle attack profile is outside ranger records";
        return false;
    }
    auto& words = combatants_[slot].words;
    words[combatant_word::action_done] = 1;
    words[combatant_word::attack_counter] = wrapping_i16(
        static_cast<std::int32_t>(words[combatant_word::attack_counter]) + 2);

    auto& role = ranger_.roles[static_cast<std::size_t>(words[combatant_word::role_id])];
    const auto experience_word = model::role_word::magic_level_begin +
        static_cast<std::size_t>(magic_slot);
    const auto previous_rank = role.unsigned_word(experience_word) / 100U + 1U;
    auto experience = static_cast<std::uint16_t>(
        role.unsigned_word(experience_word) + random.bounded(2) + 1);
    if (experience > 999U) {
        experience = 999U;
    }
    role.set_word(experience_word, static_cast<std::int16_t>(experience));
    const auto current_rank = experience / 100U + 1U;

    const auto cost = static_cast<std::int32_t>(cost_scale / 2) * profile->need_mp;
    auto mp = wrapping_i16(static_cast<std::int32_t>(role.word(model::role_word::mp)) - cost);
    if (mp < 0) {
        mp = 0;
    }
    role.set_word(model::role_word::mp, mp);
    return current_rank > previous_rank;
}

std::optional<BattleHpDamageResult> BattleSetup::apply_hp_damage(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    const std::int16_t magic_slot,
    const std::int16_t distance,
    const std::int16_t special_attack_bonus,
    random::LegacyRandom& random) {
    const auto profile = attack_profile(actor_slot, magic_slot);
    if (!profile || target_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle HP damage arguments are outside battle records";
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    const auto target_role_id = combatants_[target_slot].words[combatant_word::role_id];
    if (target_role_id < 0 || static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle HP damage target role is outside ranger records";
        return std::nullopt;
    }
    auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    const auto& magic = ranger_.magics[static_cast<std::size_t>(profile->magic_id)];

    std::int32_t allied_knowledge = 0;
    std::int32_t enemy_knowledge = 0;
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            error_ = "battle HP damage combatant role is outside ranger records";
            return std::nullopt;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        const auto knowledge = role.word(model::role_word::knowledge);
        if (knowledge <= 80 || role.word(model::role_word::hp) <= 0 ||
            combatants_[slot].words[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        if (combatants_[slot].words[combatant_word::side] == actor_side) {
            allied_knowledge += 2 * static_cast<std::int32_t>(knowledge);
        } else {
            enemy_knowledge += 2 * static_cast<std::int32_t>(knowledge);
        }
    }

    std::int16_t cost_scale = 0;
    for (std::int16_t level = profile->level_index; level >= 0; --level) {
        const auto required_mp = static_cast<std::int32_t>(profile->need_mp) *
            static_cast<std::int32_t>((level + 1) / 2);
        if (actor.word(model::role_word::mp) >= required_mp) {
            cost_scale = static_cast<std::int16_t>(level + 1);
            break;
        }
    }
    last_hp_cost_scale_ = cost_scale;

    const auto equipment_bonus = [this](
                                     const model::RoleRecord& role,
                                     const std::size_t item_word)
        -> std::optional<std::int16_t> {
        std::int32_t bonus = 0;
        for (std::size_t index = 0U; index < model::role_word::equipment_count; ++index) {
            const auto item_id = role.word(model::role_word::equipment_begin + index);
            if (item_id < 0) {
                continue;
            }
            if (static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
                return std::nullopt;
            }
            bonus += ranger_.items[static_cast<std::size_t>(item_id)].word(item_word);
        }
        return wrapping_i16(bonus);
    };
    const auto attack_equipment = equipment_bonus(actor, model::item_word::add_attack);
    const auto defence_equipment = equipment_bonus(target, model::item_word::add_defence);
    if (!attack_equipment || !defence_equipment) {
        error_ = "battle HP damage equipment id is outside ranger records";
        return std::nullopt;
    }

    auto attack_total = wrapping_i16(
        (3 * static_cast<std::int32_t>(actor.word(model::role_word::attack)) +
         magic.word(model::magic_word::with_poison + static_cast<std::size_t>(cost_scale))) /
        2);
    attack_total = wrapping_i16(
        static_cast<std::int32_t>(attack_total) + *attack_equipment + special_attack_bonus +
        allied_knowledge);
    auto defence_total = wrapping_i16(
        static_cast<std::int32_t>(target.word(model::role_word::defence)) +
        *defence_equipment + enemy_knowledge);

    const auto first_attack_variance = random.bounded(20);
    const auto second_attack_variance = random.bounded(20);
    auto damage = wrapping_i16(
        2 * (static_cast<std::int32_t>(attack_total) -
             3 * static_cast<std::int32_t>(defence_total)) /
            3 +
        first_attack_variance - second_attack_variance);
    if (damage <= 0) {
        const auto first_fallback_variance = random.bounded(4);
        const auto second_fallback_variance = random.bounded(4);
        damage = wrapping_i16(
            static_cast<std::int32_t>(attack_total) / 10 + first_fallback_variance -
            second_fallback_variance);
    }
    if (damage < 0) {
        damage = 0;
    } else {
        damage = wrapping_i16(
            static_cast<std::int32_t>(damage) +
            actor.word(model::role_word::physical_power) / 15 +
            target.word(model::role_word::hurt) / 20);
        const auto factor = distance > 10
            ? 2
            : 100 - 3 * (static_cast<std::int32_t>(distance) - 1);
        const auto divisor = distance > 10 ? 3U : 100U;
        const auto product = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(damage) * factor);
        damage = wrapping_i16(static_cast<std::int32_t>(product / divisor));
    }
    if (damage < 1) {
        damage = 1;
    }

    auto& actor_counter = combatants_[actor_slot].words[combatant_word::attack_counter];
    actor_counter = wrapping_i16(
        static_cast<std::int32_t>(actor_counter) + damage / 5);
    auto target_hp = wrapping_i16(
        static_cast<std::int32_t>(target.word(model::role_word::hp)) - damage);
    target.set_word(model::role_word::hp, target_hp);
    if (target_hp < 0) {
        target.set_word(model::role_word::hp, 0);
        actor_counter = wrapping_i16(
            static_cast<std::int32_t>(actor_counter) +
            10 * static_cast<std::int32_t>(target.word(model::role_word::level)));
    }

    auto hurt = wrapping_i16(
        static_cast<std::int32_t>(target.word(model::role_word::hurt)) + damage / 10);
    target.set_word(model::role_word::hurt, hurt);
    if (hurt > 99) {
        target.set_word(model::role_word::hurt, 99);
    }

    const auto poison_power = static_cast<std::int32_t>(
                                  actor.word(model::role_word::attack_with_poison)) +
        magic.word(
            model::magic_word::attack_begin + static_cast<std::size_t>(profile->level_index));
    const auto anti_poison = target.word(model::role_word::anti_poison);
    if (poison_power > anti_poison && anti_poison < 90) {
        auto poison = wrapping_i16(
            static_cast<std::int32_t>(target.word(model::role_word::poison)) +
            (poison_power - anti_poison) / 15);
        target.set_word(model::role_word::poison, poison);
        if (poison > 100) {
            target.set_word(model::role_word::poison, 99);
        }
        if (target.word(model::role_word::poison) < 0) {
            target.set_word(model::role_word::poison, 0);
        }
    }
    return BattleHpDamageResult{damage, cost_scale};
}

std::optional<std::int32_t> BattleSetup::apply_mp_damage(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    const std::int16_t magic_slot,
    random::LegacyRandom& random) {
    const auto profile = attack_profile(actor_slot, magic_slot);
    if (!profile || target_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle MP damage arguments are outside battle records";
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    const auto target_role_id = combatants_[target_slot].words[combatant_word::role_id];
    if (target_role_id < 0 || static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle MP damage target role is outside ranger records";
        return std::nullopt;
    }
    auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    const auto& magic = ranger_.magics[static_cast<std::size_t>(profile->magic_id)];
    const auto level = static_cast<std::size_t>(profile->level_index);
    const auto target_mp_before = target.word(model::role_word::mp);

    const auto first_actor_variance = random.bounded(3);
    const auto second_actor_variance = random.bounded(3);
    const auto actor_variance = first_actor_variance - second_actor_variance;
    const auto add_mp = magic.word(model::magic_word::add_mp_begin + level);
    actor.set_word(
        model::role_word::mp,
        wrapping_i16(static_cast<std::int32_t>(actor.word(model::role_word::mp)) + add_mp));
    auto maximum_mp = wrapping_i16(
        static_cast<std::int32_t>(actor.word(model::role_word::maximum_mp)) +
        random.bounded(add_mp / 2));
    if (maximum_mp >= 999) {
        maximum_mp = 999;
    }
    actor.set_word(model::role_word::maximum_mp, maximum_mp);
    auto actor_mp = wrapping_i16(
        static_cast<std::int32_t>(actor.word(model::role_word::mp)) + actor_variance);
    if (actor_mp >= maximum_mp) {
        actor_mp = maximum_mp;
    }
    actor.set_word(model::role_word::mp, actor_mp);

    const auto first_target_variance = random.bounded(3);
    const auto second_target_variance = random.bounded(3);
    const auto target_variance = first_target_variance - second_target_variance;
    auto target_mp = wrapping_i16(
        static_cast<std::int32_t>(target.word(model::role_word::mp)) -
        magic.word(model::magic_word::hurt_mp_begin + level) - target_variance);
    if (target_mp <= 0) {
        target_mp = 0;
    }
    target.set_word(model::role_word::mp, target_mp);
    return static_cast<std::int32_t>(target_mp_before) - target_mp;
}

void BattleSetup::clear_attack_effects() noexcept {
    std::ranges::fill(attack_effects_, static_cast<std::int16_t>(0));
}

std::optional<BattleAreaResult> BattleSetup::apply_attack_area(
    const std::size_t actor_slot,
    const std::int16_t magic_slot,
    const BattlePathCoord target,
    const std::int16_t special_attack_bonus,
    random::LegacyRandom& random) {
    const auto profile = attack_profile(actor_slot, magic_slot);
    if (!profile) {
        error_ = "battle attack area profile is outside ranger records";
        return std::nullopt;
    }
    if (profile->area_type != 0 && profile->area_type != 2 && profile->area_type != 3) {
        error_ = "battle line attack area requires its direction handler";
        return std::nullopt;
    }
    if (profile->select_distance < 0 || profile->attack_distance < 0) {
        error_ = "battle attack area distance is negative";
        return std::nullopt;
    }

    auto& actor_words = combatants_[actor_slot].words;
    const auto actor_side = actor_words[combatant_word::side];
    BattleAreaResult result{};
    const auto apply_cell = [this,
                             actor_slot,
                             magic_slot,
                             special_attack_bonus,
                             actor_side,
                             hurt_type = profile->hurt_type,
                             &random,
                             &result](
                                const std::int32_t x,
                                const std::int32_t y,
                                const std::int16_t distance,
                                const bool force_hp) -> bool {
        if (x < 0 || x >= static_cast<std::int32_t>(kBattleExtent) || y < 0 ||
            y >= static_cast<std::int32_t>(kBattleExtent)) {
            return true;
        }
        const auto index = static_cast<std::size_t>(y) * kBattleExtent +
            static_cast<std::size_t>(x);
        const auto target_slot = data_.occupancy()[index];
        if (target_slot != -1) {
            if (target_slot < 0 || target_slot >= combatant_count_) {
                error_ = "battle attack area occupancy is outside combatant slots";
                return false;
            }
            if (combatants_[static_cast<std::size_t>(target_slot)]
                    .words[combatant_word::side] == actor_side) {
                return true;
            }
        }
        attack_effects_[index] = 1;
        if (target_slot == -1) {
            return true;
        }

        const auto target_index = static_cast<std::size_t>(target_slot);
        if (force_hp || hurt_type == 0) {
            const auto damage = apply_hp_damage(
                actor_slot,
                target_index,
                magic_slot,
                distance,
                special_attack_bonus,
                random);
            if (!damage) {
                return false;
            }
            combatants_[target_index].words[combatant_word::damage_value] = damage->damage;
            result.hit_count = wrapping_i16(static_cast<std::int32_t>(result.hit_count) + 1);
            result.effect_kind = 1;
        } else if (hurt_type == 1) {
            const auto damage = apply_mp_damage(actor_slot, target_index, magic_slot, random);
            if (!damage) {
                return false;
            }
            combatants_[target_index].words[combatant_word::damage_value] =
                wrapping_i16(*damage);
            result.hit_count = wrapping_i16(static_cast<std::int32_t>(result.hit_count) + 1);
            result.effect_kind = 3;
        }
        return true;
    };

    if (profile->area_type == 2) {
        const auto actor_x = actor_words[combatant_word::x];
        const auto actor_y = actor_words[combatant_word::y];
        for (std::int32_t distance = 1; distance <= profile->select_distance; ++distance) {
            const auto legacy_distance = static_cast<std::int16_t>(distance);
            if (!apply_cell(actor_x, actor_y - distance, legacy_distance, true) ||
                !apply_cell(actor_x, actor_y + distance, legacy_distance, true) ||
                !apply_cell(actor_x - distance, actor_y, legacy_distance, true) ||
                !apply_cell(actor_x + distance, actor_y, legacy_distance, true)) {
                return std::nullopt;
            }
        }
        return result;
    }

    const auto delta_x = static_cast<std::int32_t>(target.x) -
        actor_words[combatant_word::x];
    const auto delta_y = static_cast<std::int32_t>(target.y) -
        actor_words[combatant_word::y];
    if (std::abs(delta_x) < std::abs(delta_y)) {
        actor_words[combatant_word::initial_mode] = delta_y <= 0 ? 0 : 3;
    } else {
        actor_words[combatant_word::initial_mode] = delta_x <= 0 ? 2 : 1;
    }

    const auto radius = static_cast<std::int32_t>(profile->attack_distance);
    for (std::int32_t x = static_cast<std::int32_t>(target.x) - radius;
         x <= static_cast<std::int32_t>(target.x) + radius;
         ++x) {
        for (std::int32_t y = static_cast<std::int32_t>(target.y) - radius;
             y <= static_cast<std::int32_t>(target.y) + radius;
             ++y) {
            const auto distance = static_cast<std::int16_t>(
                std::abs(static_cast<std::int32_t>(actor_words[combatant_word::x]) - x) +
                std::abs(static_cast<std::int32_t>(actor_words[combatant_word::y]) - y));
            if (!apply_cell(x, y, distance, false)) {
                return std::nullopt;
            }
        }
    }
    return result;
}

std::optional<BattleAreaResult> BattleSetup::apply_line_attack_area(
    const std::size_t actor_slot,
    const std::int16_t magic_slot,
    const std::int16_t direction,
    const std::int16_t special_attack_bonus,
    random::LegacyRandom& random) {
    const auto profile = attack_profile(actor_slot, magic_slot);
    if (!profile) {
        error_ = "battle line attack profile is outside ranger records";
        return std::nullopt;
    }
    BattleAreaResult result{};
    if (direction < 0 || direction > 3 || profile->select_distance < 1) {
        return result;
    }
    constexpr std::array<BattlePathCoord, 4> kDirections{
        BattlePathCoord{0, -1},
        BattlePathCoord{1, 0},
        BattlePathCoord{-1, 0},
        BattlePathCoord{0, 1},
    };
    const auto delta = kDirections[static_cast<std::size_t>(direction)];
    const auto& actor_words = combatants_[actor_slot].words;
    const auto actor_side = actor_words[combatant_word::side];
    const auto actor_x = actor_words[combatant_word::x];
    const auto actor_y = actor_words[combatant_word::y];
    for (std::int32_t distance = 1; distance <= profile->select_distance; ++distance) {
        const auto x = static_cast<std::int32_t>(actor_x) + distance * delta.x;
        const auto y = static_cast<std::int32_t>(actor_y) + distance * delta.y;
        if (x < 0 || x >= static_cast<std::int32_t>(kBattleExtent) || y < 0 ||
            y >= static_cast<std::int32_t>(kBattleExtent)) {
            continue;
        }
        const auto index = static_cast<std::size_t>(y) * kBattleExtent +
            static_cast<std::size_t>(x);
        const auto target_slot = data_.occupancy()[index];
        if (target_slot != -1) {
            if (target_slot < 0 || target_slot >= combatant_count_) {
                error_ = "battle line attack occupancy is outside combatant slots";
                return std::nullopt;
            }
            if (combatants_[static_cast<std::size_t>(target_slot)]
                    .words[combatant_word::side] == actor_side) {
                continue;
            }
        }
        attack_effects_[index] = 1;
        if (target_slot == -1) {
            continue;
        }
        const auto target_index = static_cast<std::size_t>(target_slot);
        const auto damage = apply_hp_damage(
            actor_slot,
            target_index,
            magic_slot,
            static_cast<std::int16_t>(distance),
            special_attack_bonus,
            random);
        if (!damage) {
            return std::nullopt;
        }
        combatants_[target_index].words[combatant_word::damage_value] = damage->damage;
        result.hit_count = wrapping_i16(static_cast<std::int32_t>(result.hit_count) + 1);
        result.effect_kind = 1;
    }
    return result;
}

bool BattleSetup::finish_attack(const std::size_t slot) {
    if (!valid() || slot >= static_cast<std::size_t>(combatant_count_)) {
        return false;
    }
    const auto role_id = combatants_[slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return false;
    }
    auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    auto physical_power = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::physical_power)) - 3);
    if (physical_power < 0) {
        physical_power = 0;
    }
    role.set_word(model::role_word::physical_power, physical_power);
    return true;
}

PartySelectionResult BattleSetup::apply(const PartySelectionAction action) {
    if (!valid()) {
        return PartySelectionResult::invalid;
    }
    if (!waiting_) {
        return PartySelectionResult::complete;
    }

    if (action == PartySelectionAction::next) {
        cursor_ = cursor_ == party_prefix_length_ ? 0U : cursor_ + 1U;
        return PartySelectionResult::changed;
    }
    if (action == PartySelectionAction::previous) {
        cursor_ = cursor_ == 0U ? party_prefix_length_ : cursor_ - 1U;
        return PartySelectionResult::changed;
    }
    if (cursor_ != party_prefix_length_) {
        auto& state = selection_states_[cursor_];
        if (state != 2) {
            state = static_cast<std::int16_t>((state + 1) % 2);
        }
        return PartySelectionResult::changed;
    }

    const auto definition = data_.definition();
    for (std::size_t party = 0U; party < kBattlePartySlots; ++party) {
        if (selection_states_[party] != 1) {
            continue;
        }
        const auto coordinate_index = static_cast<std::size_t>(combatant_count_);
        if (!append_combatant(
                ranger_.header.team_member(party).value,
                0,
                definition[kPartyXBegin + coordinate_index],
                definition[kPartyYBegin + coordinate_index],
                2)) {
            return PartySelectionResult::invalid;
        }
    }
    if (combatant_count_ == 0) {
        return PartySelectionResult::waiting;
    }
    if (!append_enemies()) {
        return PartySelectionResult::invalid;
    }
    waiting_ = false;
    return PartySelectionResult::complete;
}

}  // namespace openlegend::battle
