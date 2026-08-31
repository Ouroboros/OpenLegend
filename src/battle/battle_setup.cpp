#include "openlegend/battle/battle_setup.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

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
