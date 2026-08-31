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

BattleSetup::BattleSetup(BattleData& data, const model::RangerState& ranger)
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
