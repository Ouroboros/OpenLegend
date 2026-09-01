#include "openlegend/battle/battle_setup.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace openlegend::battle {
namespace {

constexpr std::array<std::int16_t, kBattleCombatantWords> kInitialCombatantWords{
    -1, -1, 0, 0, 0, 0, 0, 0, 5098, 0, 0, -1, -1, 0};
constexpr std::array<BattlePathCoord, 4> kLegacyPathDirections{{
    {0, -1},
    {1, 0},
    {-1, 0},
    {0, 1},
}};
constexpr std::size_t kPresetPartyBegin = 9U;
constexpr std::size_t kFixedPartyBegin = 15U;
constexpr std::size_t kPartyXBegin = 21U;
constexpr std::size_t kPartyYBegin = 27U;
constexpr std::size_t kEnemyBegin = 33U;
constexpr std::size_t kEnemyXBegin = 53U;
constexpr std::size_t kEnemyYBegin = 73U;
constexpr std::int16_t kBattleSpriteBase = 5106;
constexpr std::array<std::int16_t, 53> kBattleEffectFrameCounts{
    10, 14, 17, 9,  13, 17, 17, 17, 18, 19, 19, 15, 13, 10, 10, 15, 21, 16,
    9,  11, 8,  9,  8,  8,  7,  8,  8,  9,  12, 19, 11, 14, 12, 17, 8,  11,
    9,  13, 10, 19, 14, 17, 19, 14, 21, 16, 13, 18, 14, 17, 17, 16, 7};
constexpr std::array<std::uint16_t, 30> kLevelExperienceThresholds{
    0,     50,    150,   300,   500,   750,   1050,  1400,  1800,  2250,
    2750,  3850,  5050,  6350,  7750,  9250,  10850, 12550, 14350, 16750,
    18250, 21400, 24700, 28150, 31750, 35500, 39400, 43450, 47650, 52000};
struct BattleAiSpecialAttackBonus {
    std::int16_t weapon_id{};
    std::int16_t magic_id{};
    std::int16_t bonus{};
};
constexpr std::array<BattleAiSpecialAttackBonus, 7> kBattleAiSpecialAttackBonuses{{
    {106, 57, 100},
    {107, 49, 50},
    {108, 49, 50},
    {110, 54, 80},
    {115, 63, 50},
    {116, 67, 70},
    {119, 68, 100},
}};

[[nodiscard]] constexpr std::int16_t wrapping_i16(const std::int32_t value) noexcept {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::optional<std::size_t> legacy_cursor_index(
    const BattlePathCoord coordinate) noexcept {
    const auto index = static_cast<std::int32_t>(coordinate.y) *
            static_cast<std::int32_t>(kBattleExtent) +
        coordinate.x;
    if (index < 0 || index >= static_cast<std::int32_t>(kBattleOccupancyCells)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(index);
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

std::optional<BattleLevelUpResult> BattleSetup::apply_battle_level_up(
    const std::size_t role_id,
    const bool suppress_message,
    random::LegacyRandom& random) {
    if (!valid() || role_id >= ranger_.roles.size()) {
        return std::nullopt;
    }
    auto& role = ranger_.roles[role_id];
    BattleLevelUpResult result{
        .role_id = static_cast<std::int16_t>(role_id),
        .old_level = role.word(model::role_word::level),
        .new_level = role.word(model::role_word::level),
    };
    if (result.old_level < 0 || result.old_level >= 30) {
        return result;
    }

    const auto experience = role.unsigned_word(model::role_word::experience);
    if (experience < kLevelExperienceThresholds[static_cast<std::size_t>(result.old_level)]) {
        return result;
    }
    auto new_level = result.old_level;
    for (std::int16_t level = result.old_level; level < 30; ++level) {
        if (experience >= kLevelExperienceThresholds[static_cast<std::size_t>(level)]) {
            new_level = static_cast<std::int16_t>(level + 1);
        }
    }
    const auto levels_gained = static_cast<std::int16_t>(new_level - result.old_level);
    if (levels_gained <= 0) {
        return result;
    }

    const auto iq = role.word(model::role_word::iq);
    const auto growth_bound = iq < 30 ? 2 : iq < 50 ? 3 : iq < 70 ? 4 : iq < 90 ? 5 : 6;
    const auto growth_roll = static_cast<std::int16_t>(random.bounded(growth_bound) + 1);
    role.set_word(
        model::role_word::level,
        wrapping_i16(static_cast<std::int32_t>(role.word(model::role_word::level)) +
                     levels_gained));

    auto maximum_hp = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::maximum_hp)) +
        (random.bounded(3) + role.word(model::role_word::increased_life)) * 3 *
            levels_gained);
    if (maximum_hp > 999) {
        maximum_hp = 999;
    }
    role.set_word(model::role_word::maximum_hp, maximum_hp);
    role.set_word(model::role_word::hp, maximum_hp);
    role.set_word(model::role_word::hurt, 0);
    role.set_word(model::role_word::poison, 0);
    role.set_word(model::role_word::physical_power, 100);

    auto maximum_mp = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::maximum_mp)) +
        (9 - growth_roll) * 4 * levels_gained);
    if (maximum_mp > 999) {
        maximum_mp = 999;
    }
    role.set_word(model::role_word::maximum_mp, maximum_mp);
    role.set_word(model::role_word::mp, maximum_mp);

    const auto primary_gain = static_cast<std::int32_t>(growth_roll) * levels_gained;
    for (const auto word : {
             model::role_word::attack,
             model::role_word::speed,
             model::role_word::defence,
         }) {
        role.set_word(
            word,
            wrapping_i16(static_cast<std::int32_t>(role.word(word)) + primary_gain));
    }
    for (const auto word : {
             model::role_word::medicine,
             model::role_word::use_poison,
             model::role_word::detoxification,
             model::role_word::fist,
             model::role_word::sword,
             model::role_word::knife,
         }) {
        if (role.word(word) > 20) {
            role.set_word(
                word,
                wrapping_i16(
                    static_cast<std::int32_t>(role.word(word)) + random.bounded(3)));
        }
    }
    role.set_word(
        model::role_word::hidden_weapon,
        wrapping_i16(
            static_cast<std::int32_t>(role.word(model::role_word::hidden_weapon)) +
            random.bounded(3)));
    for (const auto word : {
             model::role_word::attack,
             model::role_word::speed,
             model::role_word::defence,
             model::role_word::medicine,
             model::role_word::use_poison,
             model::role_word::detoxification,
             model::role_word::hidden_weapon,
             model::role_word::fist,
             model::role_word::sword,
             model::role_word::knife,
         }) {
        if (role.word(word) > 100) {
            role.set_word(word, 100);
        }
    }

    result.new_level = role.word(model::role_word::level);
    result.levels_gained = levels_gained;
    result.growth_roll = growth_roll;
    result.maximum_hp = role.word(model::role_word::maximum_hp);
    result.maximum_mp = role.word(model::role_word::maximum_mp);
    result.changed = true;
    result.message_required = !suppress_message;
    result.present_required = !suppress_message;
    result.wait_for_input = !suppress_message;
    return result;
}

std::optional<BattlePracticeResult> BattleSetup::apply_battle_practice(
    const std::size_t role_id,
    const bool suppress_message) {
    if (!valid() || role_id >= ranger_.roles.size()) {
        return std::nullopt;
    }
    auto& role = ranger_.roles[role_id];
    const auto item_id = role.word(model::role_word::practice_item);
    BattlePracticeResult result{
        .role_id = static_cast<std::int16_t>(role_id),
        .item_id = item_id,
    };
    if (item_id == -1) {
        return result;
    }
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
        error_ = "battle practice item is outside item records";
        return std::nullopt;
    }
    const auto& item = ranger_.items[static_cast<std::size_t>(item_id)];
    const auto magic_id = item.word(model::item_word::magic_id);
    result.magic_id = magic_id;
    std::int16_t magic_slot = -1;
    std::uint16_t magic_rank = 0;
    if (magic_id != -1) {
        for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
            if (role.word(model::role_word::magic_id_begin + slot) == magic_id) {
                magic_slot = static_cast<std::int16_t>(slot);
                magic_rank = static_cast<std::uint16_t>(
                    role.unsigned_word(model::role_word::magic_level_begin + slot) / 100U);
                break;
            }
        }
    }
    result.magic_slot = magic_slot;
    const auto factor = 7 - role.word(model::role_word::iq) / 15;
    result.required_experience = static_cast<std::int32_t>(
        item.word(model::item_word::need_experience)) * factor *
        (magic_id == -1 ? 2 : static_cast<std::int32_t>(magic_rank) + 1);
    if (magic_rank >= 9U) {
        result.maximum_magic_level = true;
        return result;
    }
    if (static_cast<std::int32_t>(role.unsigned_word(model::role_word::item_experience)) <
        result.required_experience) {
        return result;
    }

    auto maximum_hp = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::maximum_hp)) +
        item.word(model::item_word::add_maximum_hp));
    if (maximum_hp > 999) {
        maximum_hp = 999;
    }
    role.set_word(model::role_word::maximum_hp, maximum_hp);
    if (item.word(model::item_word::change_mp_type) == 2) {
        role.set_word(model::role_word::mp_type, 2);
    }
    auto maximum_mp = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::maximum_mp)) +
        item.word(model::item_word::add_maximum_mp));
    if (maximum_mp > 999) {
        maximum_mp = 999;
    }
    role.set_word(model::role_word::maximum_mp, maximum_mp);

    for (std::size_t offset = 0U; offset <=
            model::item_word::add_morality - model::item_word::add_attack;
         ++offset) {
        const auto role_word = model::role_word::attack + offset;
        auto changed = wrapping_i16(
            static_cast<std::int32_t>(role.word(role_word)) +
            item.word(model::item_word::add_attack + offset));
        if (changed >= 100) {
            changed = 100;
        }
        if (changed <= 0) {
            changed = 0;
        }
        role.set_word(role_word, changed);
    }
    if (role.word(model::role_word::attack_twice) == 0) {
        role.set_word(
            model::role_word::attack_twice,
            item.word(model::item_word::add_attack_twice));
    }
    auto attack_with_poison = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::attack_with_poison)) +
        item.word(model::item_word::add_attack_with_poison));
    if (attack_with_poison >= 100) {
        attack_with_poison = 100;
    }
    if (attack_with_poison <= 0) {
        attack_with_poison = 0;
    }
    role.set_word(model::role_word::attack_with_poison, attack_with_poison);
    role.set_word(model::role_word::item_experience, 0);

    result.practiced = true;
    result.practice_message_required = !suppress_message;
    result.present_required = !suppress_message;
    result.wait_for_input = !suppress_message;
    if (magic_id > 0) {
        if (magic_slot >= 0) {
            const auto word = model::role_word::magic_level_begin +
                static_cast<std::size_t>(magic_slot);
            if (role.unsigned_word(word) < 899U) {
                role.set_word(
                    word,
                    wrapping_i16(static_cast<std::int32_t>(role.word(word)) + 100));
                result.increased_magic_level = true;
                result.magic_message_required = true;
                result.present_required = true;
                result.wait_for_input = true;
            }
        } else {
            for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
                if (role.word(model::role_word::magic_id_begin + slot) <= 0) {
                    role.set_word(model::role_word::magic_id_begin + slot, magic_id);
                    result.magic_slot = static_cast<std::int16_t>(slot);
                    result.learned_magic = true;
                    break;
                }
            }
        }
    }
    return result;
}

void BattleSetup::remove_inventory_slot(const std::size_t slot) noexcept {
    if (slot >= model::kInventoryCount) {
        return;
    }
    for (std::size_t source = slot + 1U; source < model::kInventoryCount; ++source) {
        ranger_.header.set_inventory(
            source - 1U,
            ranger_.header.inventory_item(source),
            ranger_.header.inventory_count(source));
    }
    ranger_.header.set_inventory(model::kInventoryCount - 1U, model::ItemId{-1}, 0);
}

std::optional<BattleCraftResult> BattleSetup::apply_battle_crafting(
    const std::size_t role_id,
    const bool suppress_message,
    random::LegacyRandom& random) {
    if (!valid() || role_id >= ranger_.roles.size()) {
        return std::nullopt;
    }
    auto& role = ranger_.roles[role_id];
    const auto practice_item_id = role.word(model::role_word::practice_item);
    BattleCraftResult result{
        .role_id = static_cast<std::int16_t>(role_id),
        .practice_item_id = practice_item_id,
    };
    if (practice_item_id < 0 ||
        static_cast<std::size_t>(practice_item_id) >= ranger_.items.size()) {
        return result;
    }
    const auto& item = ranger_.items[static_cast<std::size_t>(practice_item_id)];
    const auto factor = 7 - role.word(model::role_word::iq) / 15;
    const auto need_experience = item.word(model::item_word::need_make_item_experience);
    result.required_experience = static_cast<std::int32_t>(need_experience) * factor;
    if (need_experience <= 0 ||
        static_cast<std::int32_t>(
            role.unsigned_word(model::role_word::make_item_experience)) <
            result.required_experience) {
        return result;
    }

    result.material_item_id = item.word(model::item_word::need_material);
    std::optional<std::size_t> material_slot;
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        if (ranger_.header.inventory_item(slot).value == result.material_item_id) {
            material_slot = slot;
            break;
        }
    }
    if (!material_slot) {
        return result;
    }

    std::array<bool, model::item_word::make_item_count> eligible{};
    auto eligible_count = 0U;
    for (std::size_t recipe = 0U; recipe < eligible.size(); ++recipe) {
        const auto product = item.word(model::item_word::make_item_begin + recipe);
        const auto material_count = item.word(
            model::item_word::make_item_count_begin + recipe);
        if (product != -1 && ranger_.header.inventory_count(*material_slot) >= material_count) {
            eligible[recipe] = true;
            ++eligible_count;
        }
    }
    if (eligible_count == 0U) {
        return result;
    }
    result.recipe_available = true;
    std::size_t recipe = 0U;
    do {
        recipe = static_cast<std::size_t>(random.bounded(5));
    } while (!eligible[recipe]);
    result.recipe_slot = static_cast<std::int16_t>(recipe);
    result.product_item_id = item.word(model::item_word::make_item_begin + recipe);
    result.material_count_removed = item.word(
        model::item_word::make_item_count_begin + recipe);
    if (suppress_message) {
        return result;
    }
    if (result.product_item_id < 0 ||
        static_cast<std::size_t>(result.product_item_id) >= ranger_.items.size()) {
        error_ = "battle crafted product is outside item records";
        return std::nullopt;
    }
    result.message_required = true;
    result.present_required = true;
    result.wait_for_input = true;

    std::optional<std::size_t> product_slot;
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        if (ranger_.header.inventory_item(slot).value == result.product_item_id) {
            product_slot = slot;
            break;
        }
    }
    if (product_slot) {
        result.product_count_added = static_cast<std::int16_t>(random.bounded(3) + 1);
        ranger_.header.set_inventory(
            *product_slot,
            model::ItemId{result.product_item_id},
            wrapping_i16(
                static_cast<std::int32_t>(ranger_.header.inventory_count(*product_slot)) +
                result.product_count_added));
    } else {
        for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
            if (ranger_.header.inventory_item(slot).value == -1) {
                product_slot = slot;
                result.product_count_added = 1;
                result.created_inventory_slot = true;
                ranger_.header.set_inventory(
                    slot,
                    model::ItemId{result.product_item_id},
                    wrapping_i16(
                        static_cast<std::int32_t>(ranger_.header.inventory_count(slot)) + 1));
                break;
            }
        }
        if (!product_slot) {
            result.inventory_full = true;
            return result;
        }
    }

    const auto remaining_material = wrapping_i16(
        static_cast<std::int32_t>(ranger_.header.inventory_count(*material_slot)) -
        result.material_count_removed);
    ranger_.header.set_inventory(
        *material_slot,
        model::ItemId{result.material_item_id},
        remaining_material);
    if (remaining_material <= 0) {
        remove_inventory_slot(*material_slot);
    }
    role.set_word(model::role_word::make_item_experience, 0);
    result.crafted = true;
    return result;
}

std::optional<BattlePostBattleResult> BattleSetup::settle_battle(
    const BattleOutcome outcome,
    const bool grant_experience,
    random::LegacyRandom& random) {
    if (!valid() || outcome == BattleOutcome::ongoing) {
        return std::nullopt;
    }
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            error_ = "post-battle combatant role is outside ranger records";
            return std::nullopt;
        }
    }

    BattlePostBattleResult result{
        .outcome = outcome,
        .total_experience = data_.definition()[7U],
    };
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& words = combatants_[slot].words;
        auto& role = ranger_.roles[static_cast<std::size_t>(words[combatant_word::role_id])];
        if (words[combatant_word::side] == 1) {
            role.set_word(model::role_word::hp, role.word(model::role_word::maximum_hp));
            role.set_word(model::role_word::mp, role.word(model::role_word::maximum_mp));
            role.set_word(model::role_word::physical_power, 100);
            role.set_word(model::role_word::hurt, 0);
            role.set_word(model::role_word::poison, 0);
        } else if (role.word(model::role_word::hp) > 0) {
            result.living_party_count = wrapping_i16(
                static_cast<std::int32_t>(result.living_party_count) + 1);
        }
    }
    if (outcome == BattleOutcome::victory) {
        if (result.living_party_count == 0) {
            result.living_party_count = 1;
        }
        result.shared_experience = static_cast<std::int16_t>(
            result.total_experience / result.living_party_count);
        for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
            auto& words = combatants_[slot].words;
            const auto& role = ranger_.roles[static_cast<std::size_t>(
                words[combatant_word::role_id])];
            if (words[combatant_word::side] == 0 &&
                role.word(model::role_word::hp) > 0) {
                words[combatant_word::reward_experience] = wrapping_i16(
                    static_cast<std::int32_t>(
                        words[combatant_word::reward_experience]) +
                    result.shared_experience);
            }
        }
    }

    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& words = combatants_[slot].words;
        if (words[combatant_word::side] != 0) {
            continue;
        }
        auto& role = ranger_.roles[static_cast<std::size_t>(words[combatant_word::role_id])];
        const auto floor_hp = static_cast<std::int16_t>(
            role.word(model::role_word::maximum_hp) / 5);
        if (role.word(model::role_word::hp) > 0) {
            if (role.word(model::role_word::hp) < floor_hp) {
                role.set_word(model::role_word::hp, floor_hp);
            }
        } else {
            role.set_word(model::role_word::hp, floor_hp);
            if (role.word(model::role_word::physical_power) < 10) {
                role.set_word(model::role_word::physical_power, 10);
            }
        }
    }

    const auto add_capped_experience = [](model::RoleRecord& role,
                                          const std::size_t word,
                                          const std::uint16_t amount) {
        auto changed = static_cast<std::uint16_t>(role.unsigned_word(word) + amount);
        if (changed > 60'000U) {
            changed = 60'000U;
        }
        role.set_word(word, std::bit_cast<std::int16_t>(changed));
    };
    result.roles.reserve(static_cast<std::size_t>(combatant_count_));
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        auto& words = combatants_[slot].words;
        const auto role_id = words[combatant_word::role_id];
        auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        const auto reward = words[combatant_word::reward_experience];
        const auto unsigned_reward = static_cast<std::uint16_t>(reward);
        add_capped_experience(role, model::role_word::experience, unsigned_reward);
        const auto scaled_reward = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(reward) * 8);
        const auto training_reward = static_cast<std::uint16_t>(scaled_reward / 10U);
        add_capped_experience(role, model::role_word::item_experience, training_reward);
        add_capped_experience(
            role, model::role_word::make_item_experience, training_reward);

        BattlePostBattleRoleResult role_result{
            .combatant_slot = slot,
            .role_id = role_id,
            .experience_gained = reward,
        };
        if (words[combatant_word::side] == 0 &&
            (grant_experience || outcome == BattleOutcome::victory)) {
            role_result.experience_message_required = true;
            result.render_required = true;
            result.present_required = true;
            result.wait_for_input = true;
            if (role.word(model::role_word::level) < 30) {
                const auto level_up = apply_battle_level_up(
                    static_cast<std::size_t>(role_id), false, random);
                if (!level_up) {
                    return std::nullopt;
                }
                role_result.level_up = *level_up;
            }
            if (role.word(model::role_word::practice_item) != -1) {
                const auto practice = apply_battle_practice(
                    static_cast<std::size_t>(role_id), false);
                if (!practice) {
                    return std::nullopt;
                }
                role_result.practice = *practice;
                const auto craft = apply_battle_crafting(
                    static_cast<std::size_t>(role_id), false, random);
                if (!craft) {
                    return std::nullopt;
                }
                role_result.craft = *craft;
            }
        }
        result.roles.push_back(role_result);
    }
    return result;
}

std::optional<BattleRoundStatusDamageResult> BattleSetup::apply_round_status_damage() {
    if (!valid()) {
        return std::nullopt;
    }
    BattleRoundStatusDamageResult result{};
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& words = combatants_[slot].words;
        const auto role_id = words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            error_ = "round-status combatant role is outside ranger records";
            return std::nullopt;
        }
        auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        const auto hurt = role.word(model::role_word::hurt);
        const auto poison = role.word(model::role_word::poison);
        if (!(hurt > 0 ||
              (poison > 0 && role.word(model::role_word::hp) > 0 &&
               role.word(model::role_word::physical_power) > 0 &&
               words[combatant_word::occupancy_hidden] == 0))) {
            continue;
        }

        BattleRoundStatusDamageEntry entry{
            .combatant_slot = slot,
            .role_id = role_id,
            .hp_before = role.word(model::role_word::hp),
            .hurt_damage = static_cast<std::int16_t>(hurt / 20),
            .poison_damage = static_cast<std::int16_t>(poison / 10),
        };
        auto hp = wrapping_i16(
            static_cast<std::int32_t>(role.word(model::role_word::hp)) -
            entry.hurt_damage);
        hp = wrapping_i16(static_cast<std::int32_t>(hp) - entry.poison_damage);
        role.set_word(model::role_word::hp, hp);
        if (role.word(model::role_word::physical_power) < 0) {
            role.set_word(model::role_word::physical_power, 1);
            entry.physical_power_floored = true;
        }
        if (role.word(model::role_word::hp) < 0) {
            role.set_word(model::role_word::hp, 1);
            entry.hp_floored = true;
        }
        entry.hp_after = role.word(model::role_word::hp);
        result.entries.push_back(entry);
    }
    return result;
}

std::optional<BattleAiTargetCleanupResult> BattleSetup::clear_hidden_ai_targets() {
    if (!valid()) {
        return std::nullopt;
    }
    BattleAiTargetCleanupResult result{};
    const auto clear_if_hidden = [this](std::int16_t& target) {
        if (target < 0 ||
            target >= static_cast<std::int16_t>(kBattleCombatantCount)) {
            return false;
        }
        if (combatants_[static_cast<std::size_t>(target)]
                .words[combatant_word::occupancy_hidden] != 1) {
            return false;
        }
        target = -1;
        return true;
    };
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        auto& words = combatants_[slot].words;
        if (clear_if_hidden(words[combatant_word::ai_target])) {
            result.attack_targets_cleared = wrapping_i16(
                static_cast<std::int32_t>(result.attack_targets_cleared) + 1);
        }
        if (clear_if_hidden(words[combatant_word::ai_poison_target])) {
            result.poison_targets_cleared = wrapping_i16(
                static_cast<std::int32_t>(result.poison_targets_cleared) + 1);
        }
    }
    return result;
}

std::optional<BattlePlayerActionAvailability> BattleSetup::player_action_availability(
    const std::size_t combatant_slot) const noexcept {
    if (!valid() || combatant_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto& words = combatants_[combatant_slot].words;
    const auto role_id = words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    BattlePlayerActionAvailability result{};
    const auto physical_power = role.word(model::role_word::physical_power);
    result.available[0U] = static_cast<std::int16_t>(
        physical_power > 5 && words[combatant_word::round_value] > 0 ? 1 : 0);

    std::int16_t minimum_magic_cost = 1'000;
    if (physical_power > 10) {
        for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
            const auto magic_id = role.word(model::role_word::magic_id_begin + slot);
            if (magic_id == 0) {
                continue;
            }
            if (magic_id < 0 || static_cast<std::size_t>(magic_id) >= ranger_.magics.size()) {
                return std::nullopt;
            }
            minimum_magic_cost = std::min(
                minimum_magic_cost,
                ranger_.magics[static_cast<std::size_t>(magic_id)]
                    .word(model::magic_word::need_mp));
        }
        result.available[1U] = static_cast<std::int16_t>(
            minimum_magic_cost <= role.word(model::role_word::mp) ? 1 : 0);
    }
    result.available[2U] = static_cast<std::int16_t>(
        physical_power > 10 && role.word(model::role_word::use_poison) >= 20 ? 1 : 0);
    result.available[3U] = static_cast<std::int16_t>(
        physical_power > 50 && role.word(model::role_word::detoxification) >= 20 ? 1 : 0);
    result.available[4U] = static_cast<std::int16_t>(
        physical_power > 50 && role.word(model::role_word::medicine) >= 20 ? 1 : 0);
    std::fill(result.available.begin() + 5, result.available.end(), std::int16_t{1});
    result.available_count = static_cast<std::int16_t>(
        std::ranges::count(result.available, std::int16_t{1}));
    return result;
}

std::optional<BattleStatusPanelPlan> BattleSetup::status_panel_plan(
    const std::size_t combatant_slot) const noexcept {
    if (!valid() || combatant_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto& words = combatants_[combatant_slot].words;
    const auto role_id = words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    BattleStatusPanelPlan plan{
        .combatant_slot = combatant_slot,
        .role_id = role_id,
        .side_offset = static_cast<std::int16_t>(
            words[combatant_word::side] == 0 ? 0 : 220),
        .portrait_id = role.word(model::role_word::head_id),
        .physical_power = role.word(model::role_word::physical_power),
        .hp = role.word(model::role_word::hp),
        .maximum_hp = role.word(model::role_word::maximum_hp),
        .mp = role.word(model::role_word::mp),
        .maximum_mp = role.word(model::role_word::maximum_mp),
    };
    plan.panel_x = static_cast<std::int16_t>(220 - plan.side_offset);
    plan.portrait_x = static_cast<std::int16_t>(242 - plan.side_offset);
    std::copy_n(
        role.bytes.begin() + static_cast<std::ptrdiff_t>(model::role_word::name_byte),
        model::role_word::name_bytes,
        plan.name_bytes.begin());
    for (std::size_t byte = 1U; byte <= 8U; ++byte) {
        if (plan.name_bytes[byte] == 0U) {
            plan.name_x = static_cast<std::int16_t>(
                270 - static_cast<std::int16_t>(byte * 4U) - plan.side_offset);
            break;
        }
    }
    const auto hurt = role.word(model::role_word::hurt);
    plan.hurt_color = hurt > 66 ? 5'142 : hurt > 33 ? 3'600 : 1'797;
    const auto poison = role.word(model::role_word::poison);
    plan.poison_color = poison == 0 ? 8'993 : poison >= 50 ? 13'623 : 12'338;
    switch (role.word(model::role_word::mp_type)) {
    case 0:
        plan.mp_color = 20'558;
        break;
    case 1:
        plan.mp_color = 1'797;
        break;
    case 2:
        plan.mp_color = 26'211;
        break;
    default:
        plan.mp_color = plan.poison_color;
        break;
    }
    return plan;
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

std::optional<std::int16_t> BattleSetup::attack_special_bonus(
    const std::size_t slot, const std::int16_t magic_slot) const noexcept {
    const auto profile = attack_profile(slot, magic_slot);
    if (!profile) {
        return std::nullopt;
    }
    const auto role_id = combatants_[slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    std::int16_t bonus = 0;
    for (const auto entry : kBattleAiSpecialAttackBonuses) {
        if (entry.weapon_id == role.word(model::role_word::equipment_begin) &&
            entry.magic_id == profile->magic_id) {
            bonus = entry.bonus;
        }
    }
    return bonus;
}

std::optional<BattleMagicSelectionState> BattleSetup::begin_magic_selection(
    const std::size_t slot) const noexcept {
    if (!valid() || slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto role_id = combatants_[slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    BattleMagicSelectionState state{};
    std::ranges::fill(state.available_slots, static_cast<std::int16_t>(-1));
    for (std::size_t magic_slot = 0U; magic_slot < model::role_word::magic_count; ++magic_slot) {
        const auto magic_id = role.word(model::role_word::magic_id_begin + magic_slot);
        if (magic_id <= 0) {
            continue;
        }
        state.learned_count = wrapping_i16(
            static_cast<std::int32_t>(state.learned_count) + 1);
        if (static_cast<std::size_t>(magic_id) >= ranger_.magics.size()) {
            return std::nullopt;
        }
        const auto& magic = ranger_.magics[static_cast<std::size_t>(magic_id)];
        if (role.word(model::role_word::mp) < magic.word(model::magic_word::need_mp)) {
            continue;
        }
        state.available_slots[static_cast<std::size_t>(state.available_count)] =
            static_cast<std::int16_t>(magic_slot);
        state.available_count = wrapping_i16(
            static_cast<std::int32_t>(state.available_count) + 1);
    }
    if (state.available_count <= 0) {
        return std::nullopt;
    }
    return state;
}

BattleMagicSelectionResult BattleSetup::apply_magic_selection(
    BattleMagicSelectionState& state,
    const BattleMagicSelectionAction action) noexcept {
    if (state.available_count <= 0 || state.available_count > 10 || state.cursor < 0 ||
        state.cursor >= state.available_count || state.selected_slot.has_value() ||
        state.cancelled) {
        return BattleMagicSelectionResult::invalid;
    }
    switch (action) {
        case BattleMagicSelectionAction::next:
            state.cursor = state.cursor == state.available_count - 1
                ? 0
                : wrapping_i16(static_cast<std::int32_t>(state.cursor) + 1);
            return BattleMagicSelectionResult::changed;
        case BattleMagicSelectionAction::previous:
            state.cursor = state.cursor == 0
                ? wrapping_i16(static_cast<std::int32_t>(state.available_count) - 1)
                : wrapping_i16(static_cast<std::int32_t>(state.cursor) - 1);
            return BattleMagicSelectionResult::changed;
        case BattleMagicSelectionAction::activate:
            state.selected_slot = state.available_slots[static_cast<std::size_t>(state.cursor)];
            return BattleMagicSelectionResult::selected;
        case BattleMagicSelectionAction::cancel:
            state.cancelled = true;
            return BattleMagicSelectionResult::cancelled;
    }
    return BattleMagicSelectionResult::invalid;
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

std::optional<std::int16_t> BattleSetup::poison_targeting_range(
    const std::size_t actor_slot) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    return wrapping_i16(
        static_cast<std::int32_t>(
            ranger_.roles[static_cast<std::size_t>(role_id)].word(model::role_word::use_poison)) /
            15 +
        1);
}

std::optional<std::int16_t> BattleSetup::apply_poison_value(
    const std::size_t actor_slot,
    const std::size_t target_slot) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        target_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle poison actors are outside combatant slots";
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    const auto target_role_id = combatants_[target_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || target_role_id < 0 ||
        static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size() ||
        static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle poison role is outside ranger records";
        return std::nullopt;
    }
    const auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    auto amount = wrapping_i16(
        (static_cast<std::int32_t>(actor.word(model::role_word::use_poison)) -
         target.word(model::role_word::anti_poison)) /
        4);
    if (amount > 99) {
        amount = 99;
    }
    if (amount < 0) {
        amount = 0;
    }
    if (static_cast<std::int32_t>(amount) + target.word(model::role_word::poison) > 99) {
        amount = wrapping_i16(
            99 - static_cast<std::int32_t>(target.word(model::role_word::poison)));
    }
    auto poison = wrapping_i16(
        static_cast<std::int32_t>(target.word(model::role_word::poison)) + amount);
    target.set_word(model::role_word::poison, poison);
    if (poison > 99) {
        target.set_word(model::role_word::poison, 99);
    }
    if (target.word(model::role_word::poison) < 0) {
        target.set_word(model::role_word::poison, 0);
    }
    return amount;
}

std::optional<BattleAreaResult> BattleSetup::apply_poison_target(
    const std::size_t actor_slot,
    const BattlePathCoord target) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle poison actor is outside combatant slots";
        return std::nullopt;
    }
    auto& actor_words = combatants_[actor_slot].words;
    const auto delta_x = static_cast<std::int32_t>(target.x) -
        actor_words[combatant_word::x];
    const auto delta_y = static_cast<std::int32_t>(target.y) -
        actor_words[combatant_word::y];
    if (delta_x != 0 || delta_y != 0) {
        if (std::abs(delta_y) > std::abs(delta_x)) {
            actor_words[combatant_word::initial_mode] = delta_y <= 0 ? 0 : 3;
        } else {
            actor_words[combatant_word::initial_mode] = delta_x <= 0 ? 2 : 1;
        }
    }
    clear_attack_effects();
    BattleAreaResult result{};
    if (target.x < 0 || target.x >= static_cast<std::int16_t>(kBattleExtent) || target.y < 0 ||
        target.y >= static_cast<std::int16_t>(kBattleExtent)) {
        return result;
    }
    const auto index = static_cast<std::size_t>(target.y) * kBattleExtent +
        static_cast<std::size_t>(target.x);
    const auto target_slot = data_.occupancy()[index];
    if (target_slot != -1) {
        if (target_slot < 0 || target_slot >= combatant_count_) {
            error_ = "battle poison occupancy is outside combatant slots";
            return std::nullopt;
        }
        if (combatants_[static_cast<std::size_t>(target_slot)].words[combatant_word::side] ==
            actor_words[combatant_word::side]) {
            return result;
        }
    }
    attack_effects_[index] = 1;
    if (target_slot == -1) {
        return result;
    }
    const auto target_index = static_cast<std::size_t>(target_slot);
    const auto amount = apply_poison_value(actor_slot, target_index);
    if (!amount) {
        return std::nullopt;
    }
    combatants_[target_index].words[combatant_word::damage_value] = *amount;
    result.hit_count = 1;
    result.effect_kind = 2;
    return result;
}

bool BattleSetup::finish_poison_action(const std::size_t actor_slot) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return false;
    }
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            error_ = "battle poison sprite role is outside ranger records";
            return false;
        }
        combatants_[slot].words[combatant_word::sprite] = sprite_word(
            role_id, combatants_[slot].words[combatant_word::initial_mode]);
    }
    auto& actor_words = combatants_[actor_slot].words;
    actor_words[combatant_word::action_done] = 1;
    actor_words[combatant_word::attack_counter] = wrapping_i16(
        static_cast<std::int32_t>(actor_words[combatant_word::attack_counter]) + 1);
    const auto role_id = actor_words[combatant_word::role_id];
    auto& actor = ranger_.roles[static_cast<std::size_t>(role_id)];
    auto physical_power = wrapping_i16(
        static_cast<std::int32_t>(actor.word(model::role_word::physical_power)) - 2);
    if (physical_power < 0) {
        physical_power = 0;
    }
    actor.set_word(model::role_word::physical_power, physical_power);
    return true;
}

std::optional<std::int16_t> BattleSetup::detox_targeting_range(
    const std::size_t actor_slot) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    return wrapping_i16(
        static_cast<std::int32_t>(ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::detoxification)) /
            15 +
        1);
}

std::optional<std::int16_t> BattleSetup::apply_detox_value(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        target_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle detox actors are outside combatant slots";
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    const auto target_role_id = combatants_[target_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || target_role_id < 0 ||
        static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size() ||
        static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle detox role is outside ranger records";
        return std::nullopt;
    }
    const auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    const auto first_variance = random.bounded(10);
    const auto second_variance = random.bounded(10);
    auto amount = wrapping_i16(
        static_cast<std::int32_t>(actor.word(model::role_word::detoxification)) / 3 +
        first_variance - second_variance);
    if (amount > 99) {
        amount = 99;
    }
    if (amount < 0) {
        amount = 0;
    }
    if (target.word(model::role_word::poison) >
        static_cast<std::int32_t>(actor.word(model::role_word::detoxification)) + 20) {
        amount = 0;
    }
    if (amount > target.word(model::role_word::poison)) {
        amount = target.word(model::role_word::poison);
    }
    const auto poison = wrapping_i16(
        static_cast<std::int32_t>(target.word(model::role_word::poison)) - amount);
    target.set_word(model::role_word::poison, poison);
    if (target.word(model::role_word::poison) < 0) {
        target.set_word(model::role_word::poison, 0);
    }
    if (target.word(model::role_word::poison) > 100) {
        target.set_word(model::role_word::poison, 99);
    }
    return amount;
}

std::optional<BattleAreaResult> BattleSetup::apply_detox_target(
    const std::size_t actor_slot,
    const BattlePathCoord target,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle detox actor is outside combatant slots";
        return std::nullopt;
    }
    auto& actor_words = combatants_[actor_slot].words;
    const auto delta_x = static_cast<std::int32_t>(target.x) -
        actor_words[combatant_word::x];
    const auto delta_y = static_cast<std::int32_t>(target.y) -
        actor_words[combatant_word::y];
    if (delta_x != 0 || delta_y != 0) {
        if (std::abs(delta_y) > std::abs(delta_x)) {
            actor_words[combatant_word::initial_mode] = delta_y <= 0 ? 0 : 3;
        } else {
            actor_words[combatant_word::initial_mode] = delta_x <= 0 ? 2 : 1;
        }
    }
    clear_attack_effects();
    BattleAreaResult result{};
    if (target.x < 0 || target.x >= static_cast<std::int16_t>(kBattleExtent) || target.y < 0 ||
        target.y >= static_cast<std::int16_t>(kBattleExtent)) {
        return result;
    }
    const auto index = static_cast<std::size_t>(target.y) * kBattleExtent +
        static_cast<std::size_t>(target.x);
    const auto target_slot = data_.occupancy()[index];
    if (target_slot != -1) {
        if (target_slot < 0 || target_slot >= combatant_count_) {
            error_ = "battle detox occupancy is outside combatant slots";
            return std::nullopt;
        }
        if (combatants_[static_cast<std::size_t>(target_slot)].words[combatant_word::side] !=
            actor_words[combatant_word::side]) {
            return result;
        }
    }
    attack_effects_[index] = 1;
    if (target_slot == -1) {
        return result;
    }
    const auto target_index = static_cast<std::size_t>(target_slot);
    const auto amount = apply_detox_value(actor_slot, target_index, random);
    if (!amount) {
        return std::nullopt;
    }
    combatants_[target_index].words[combatant_word::damage_value] = *amount;
    result.hit_count = 1;
    result.effect_kind = 3;
    return result;
}

bool BattleSetup::finish_detox_action(const std::size_t actor_slot) {
    return finish_poison_action(actor_slot);
}

std::optional<std::int16_t> BattleSetup::medicine_targeting_range(
    const std::size_t actor_slot) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    return wrapping_i16(
        static_cast<std::int32_t>(
            ranger_.roles[static_cast<std::size_t>(role_id)].word(model::role_word::medicine)) /
            15 +
        1);
}

std::optional<std::int32_t> BattleSetup::apply_medicine_value(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        target_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle medicine actors are outside combatant slots";
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    const auto target_role_id = combatants_[target_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || target_role_id < 0 ||
        static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size() ||
        static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle medicine role is outside ranger records";
        return std::nullopt;
    }
    auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    if (actor.word(model::role_word::physical_power) < 50) {
        return 0;
    }

    auto medicine = static_cast<std::int32_t>(actor.word(model::role_word::medicine));
    if (medicine < 0) {
        medicine = 0;
    }
    const auto hurt = target.word(model::role_word::hurt);
    std::int32_t base = 0;
    if (hurt <= 25) {
        base = (4 * medicine) / 5;
    } else if (hurt <= 50) {
        base = (3 * medicine) / 4;
    } else if (hurt <= 75) {
        base = (2 * medicine) / 3;
    } else {
        base = medicine / 2;
    }
    auto amount = base + random.bounded(5);
    if (hurt > static_cast<std::int32_t>(actor.word(model::role_word::medicine)) + 20) {
        amount = 0;
        medicine = 0;
    }

    const auto hp = static_cast<std::int32_t>(target.word(model::role_word::hp));
    const auto maximum_hp = static_cast<std::int32_t>(target.word(model::role_word::maximum_hp));
    if (hp + amount > maximum_hp) {
        amount = maximum_hp - hp;
    }
    target.set_word(
        model::role_word::hp,
        wrapping_i16(hp + wrapping_i16(amount)));
    if (target.word(model::role_word::hp) > target.word(model::role_word::maximum_hp)) {
        target.set_word(model::role_word::hp, target.word(model::role_word::maximum_hp));
    }

    auto target_hurt = wrapping_i16(
        static_cast<std::int32_t>(target.word(model::role_word::hurt)) -
        wrapping_i16(medicine));
    if (target_hurt < 0) {
        target_hurt = 0;
    }
    target.set_word(model::role_word::hurt, target_hurt);
    actor.set_word(
        model::role_word::physical_power,
        wrapping_i16(static_cast<std::int32_t>(actor.word(model::role_word::physical_power)) - 2));
    return amount;
}

std::optional<BattleAreaResult> BattleSetup::apply_medicine_target(
    const std::size_t actor_slot,
    const BattlePathCoord target,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle medicine actor is outside combatant slots";
        return std::nullopt;
    }
    auto& actor_words = combatants_[actor_slot].words;
    const auto delta_x = static_cast<std::int32_t>(target.x) -
        actor_words[combatant_word::x];
    const auto delta_y = static_cast<std::int32_t>(target.y) -
        actor_words[combatant_word::y];
    if (delta_x != 0 || delta_y != 0) {
        if (std::abs(delta_y) > std::abs(delta_x)) {
            actor_words[combatant_word::initial_mode] = delta_y <= 0 ? 0 : 3;
        } else {
            actor_words[combatant_word::initial_mode] = delta_x <= 0 ? 2 : 1;
        }
    }
    clear_attack_effects();
    BattleAreaResult result{};
    if (target.x < 0 || target.x >= static_cast<std::int16_t>(kBattleExtent) || target.y < 0 ||
        target.y >= static_cast<std::int16_t>(kBattleExtent)) {
        return result;
    }
    const auto index = static_cast<std::size_t>(target.y) * kBattleExtent +
        static_cast<std::size_t>(target.x);
    const auto target_slot = data_.occupancy()[index];
    if (target_slot != -1) {
        if (target_slot < 0 || target_slot >= combatant_count_) {
            error_ = "battle medicine occupancy is outside combatant slots";
            return std::nullopt;
        }
        if (combatants_[static_cast<std::size_t>(target_slot)].words[combatant_word::side] !=
            actor_words[combatant_word::side]) {
            return result;
        }
    }
    attack_effects_[index] = 1;
    if (target_slot == -1) {
        return result;
    }
    const auto target_index = static_cast<std::size_t>(target_slot);
    const auto amount = apply_medicine_value(actor_slot, target_index, random);
    if (!amount) {
        return std::nullopt;
    }
    combatants_[target_index].words[combatant_word::damage_value] = wrapping_i16(*amount);
    result.hit_count = 1;
    result.effect_kind = 4;
    return result;
}

bool BattleSetup::finish_medicine_action(const std::size_t actor_slot) {
    return finish_poison_action(actor_slot);
}

BattleItemSelectionState BattleSetup::begin_item_selection() const noexcept {
    BattleItemSelectionState state{};
    std::ranges::fill(state.inventory_slots, static_cast<std::int16_t>(-1));
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        const auto item_id = ranger_.header.inventory_item(slot).value;
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            continue;
        }
        const auto item_type =
            ranger_.items[static_cast<std::size_t>(item_id)].word(model::item_word::item_type);
        if (item_type == 3 || item_type == 4) {
            state.inventory_slots[static_cast<std::size_t>(state.count)] =
                static_cast<std::int16_t>(slot);
            state.count = static_cast<std::int16_t>(state.count + 1);
        }
    }
    return state;
}

bool BattleSetup::consume_inventory_item_slot(const std::size_t inventory_slot) noexcept {
    if (!valid() || inventory_slot >= model::kInventoryCount) {
        return false;
    }
    const auto item_id = ranger_.header.inventory_item(inventory_slot).value;
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
        return false;
    }
    const auto remaining = wrapping_i16(
        static_cast<std::int32_t>(ranger_.header.inventory_count(inventory_slot)) - 1);
    ranger_.header.set_inventory(inventory_slot, model::ItemId{item_id}, remaining);
    if (remaining > 0) {
        return true;
    }
    for (std::size_t source = inventory_slot + 1U; source < model::kInventoryCount; ++source) {
        ranger_.header.set_inventory(
            source - 1U,
            ranger_.header.inventory_item(source),
            ranger_.header.inventory_count(source));
    }
    ranger_.header.set_inventory(model::kInventoryCount - 1U, model::ItemId{-1}, 0);
    return true;
}

std::optional<BattleItemEffectResult> BattleSetup::apply_player_item_effect(
    const std::size_t actor_slot,
    const std::size_t inventory_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        inventory_slot >= model::kInventoryCount) {
        error_ = "battle player item action is outside legacy state";
        return std::nullopt;
    }
    const auto item_id = ranger_.header.inventory_item(inventory_slot).value;
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size() ||
        ranger_.items[static_cast<std::size_t>(item_id)].word(model::item_word::item_type) != 3) {
        error_ = "battle player item is outside usable item records";
        return std::nullopt;
    }
    const BattleAiChoice choice{
        .action = BattleAiAction::item,
        .target_slot = static_cast<std::int16_t>(actor_slot),
        .item_source = BattleAiItemSource::inventory,
        .item_slot = static_cast<std::int16_t>(inventory_slot),
        .action_code_written = false,
    };
    const auto saved_attack_effects = attack_effects_;
    auto result = apply_ai_item_effect(actor_slot, choice, random, false);
    attack_effects_ = saved_attack_effects;
    return result;
}

bool BattleSetup::finish_player_item_action(const std::size_t actor_slot) noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return false;
    }
    combatants_[actor_slot].words[combatant_word::action_done] = 1;
    return true;
}

bool BattleSetup::remove_carried_item_slot(
    const std::size_t actor_slot,
    const std::size_t item_slot) noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        item_slot >= model::role_word::taking_item_count) {
        return false;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return false;
    }
    auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    for (std::size_t source = item_slot + 1U;
         source < model::role_word::taking_item_count;
         ++source) {
        role.set_word(
            model::role_word::taking_item_begin + source - 1U,
            role.word(model::role_word::taking_item_begin + source));
        role.set_word(
            model::role_word::taking_item_count_begin + source - 1U,
            role.word(model::role_word::taking_item_count_begin + source));
    }
    role.set_word(
        model::role_word::taking_item_begin + model::role_word::taking_item_count - 1U,
        -1);
    role.set_word(
        model::role_word::taking_item_count_begin + model::role_word::taking_item_count - 1U,
        0);
    return true;
}

std::optional<std::int16_t> BattleSetup::throwing_weapon_targeting_range(
    const std::size_t actor_slot) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    return wrapping_i16(
        static_cast<std::int32_t>(
            ranger_.roles[static_cast<std::size_t>(role_id)].word(
                model::role_word::hidden_weapon)) /
            15 +
        1);
}

std::optional<BattleThrownItemResult> BattleSetup::apply_throwing_weapon_target(
    const std::size_t actor_slot,
    const BattlePathCoord target,
    const std::size_t inventory_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle throwing-weapon actor is outside combatant slots";
        return std::nullopt;
    }
    auto& actor_words = combatants_[actor_slot].words;
    const auto actor_role_id = actor_words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        error_ = "battle throwing-weapon actor role is outside ranger records";
        return std::nullopt;
    }
    const auto refresh_sprites = [this]() {
        for (std::int16_t slot = 0; slot < combatant_count_; ++slot) {
            auto& words = combatants_[static_cast<std::size_t>(slot)].words;
            words[combatant_word::sprite] =
                sprite_word(words[combatant_word::role_id], words[combatant_word::initial_mode]);
        }
    };

    const auto delta_x = static_cast<std::int32_t>(target.x) - actor_words[combatant_word::x];
    const auto delta_y = static_cast<std::int32_t>(target.y) - actor_words[combatant_word::y];
    if (delta_x != 0 || delta_y != 0) {
        if (std::abs(delta_y) > std::abs(delta_x)) {
            actor_words[combatant_word::initial_mode] = delta_y <= 0 ? 0 : 3;
        } else {
            actor_words[combatant_word::initial_mode] = delta_x <= 0 ? 2 : 1;
        }
    }
    clear_attack_effects();
    BattleThrownItemResult result{};
    if (target.x < 0 || target.x >= static_cast<std::int16_t>(kBattleExtent) || target.y < 0 ||
        target.y >= static_cast<std::int16_t>(kBattleExtent)) {
        refresh_sprites();
        return result;
    }
    const auto cell = static_cast<std::size_t>(target.y) * kBattleExtent +
        static_cast<std::size_t>(target.x);
    const auto target_slot = data_.occupancy()[cell];
    if (target_slot != -1) {
        if (target_slot < 0 || target_slot >= combatant_count_) {
            error_ = "battle throwing-weapon occupancy is outside combatant slots";
            return std::nullopt;
        }
        if (combatants_[static_cast<std::size_t>(target_slot)].words[combatant_word::side] ==
            actor_words[combatant_word::side]) {
            refresh_sprites();
            return result;
        }
    }
    attack_effects_[cell] = 1;
    if (target_slot == -1) {
        refresh_sprites();
        return result;
    }
    if (inventory_slot >= model::kInventoryCount) {
        error_ = "battle throwing-weapon inventory slot is outside ranger header";
        return std::nullopt;
    }
    const auto item_id = ranger_.header.inventory_item(inventory_slot).value;
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
        error_ = "battle throwing-weapon item is outside ranger records";
        return std::nullopt;
    }

    const auto target_index = static_cast<std::size_t>(target_slot);
    auto& target_words = combatants_[target_index].words;
    const auto target_role_id = target_words[combatant_word::role_id];
    if (target_role_id < 0 || static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle throwing-weapon target role is outside ranger records";
        return std::nullopt;
    }
    auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target_role = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    const auto& item = ranger_.items[static_cast<std::size_t>(item_id)];
    const auto hurt = target_role.word(model::role_word::hurt);
    if (hurt < 0) {
        error_ = "battle throwing-weapon target hurt is outside legacy domain";
        return std::nullopt;
    }

    std::int32_t divisor = 1;
    if (hurt == 0) {
        divisor = 4;
    } else if (hurt <= 33) {
        divisor = 3;
    } else if (hurt <= 66) {
        divisor = 2;
    }
    const auto base_delta =
        static_cast<std::int32_t>(item.word(model::item_word::add_hp)) / divisor -
        random.bounded(5);
    const auto hp_delta = wrapping_i16(
        (base_delta -
         2 * static_cast<std::int32_t>(actor.word(model::role_word::hidden_weapon))) /
        3);

    auto changed_hurt = wrapping_i16(
        static_cast<std::int32_t>(target_role.word(model::role_word::hurt)) - hp_delta / 4);
    if (changed_hurt > 99) {
        changed_hurt = 99;
    }
    if (changed_hurt < 0) {
        changed_hurt = 0;
    }
    target_role.set_word(model::role_word::hurt, changed_hurt);

    const auto old_hp = target_role.word(model::role_word::hp);
    auto changed_hp = wrapping_i16(static_cast<std::int32_t>(old_hp) + hp_delta);
    if (changed_hp >= target_role.word(model::role_word::maximum_hp)) {
        changed_hp = target_role.word(model::role_word::maximum_hp);
    }
    if (changed_hp <= 0) {
        changed_hp = 0;
    }
    target_role.set_word(model::role_word::hp, changed_hp);
    const auto damage = wrapping_i16(std::abs(
        static_cast<std::int32_t>(changed_hp) - static_cast<std::int32_t>(old_hp)));
    target_words[combatant_word::damage_value] = damage;

    const auto item_poison = item.word(model::item_word::add_poison);
    std::int16_t poison_delta = 0;
    if (item_poison > 0) {
        poison_delta = wrapping_i16(
            (static_cast<std::int32_t>(item_poison) -
             actor.word(model::role_word::hidden_weapon)) /
                2 -
            target_role.word(model::role_word::anti_poison));
        if (target_role.word(model::role_word::anti_poison) >= 100 || poison_delta < 0) {
            poison_delta = 0;
        }
        poison_delta = wrapping_i16(static_cast<std::int32_t>(poison_delta) / 2);
    } else {
        poison_delta = wrapping_i16(
            static_cast<std::int32_t>(item_poison) / 2 + random.bounded(5) - random.bounded(5));
    }
    auto changed_poison = wrapping_i16(
        static_cast<std::int32_t>(target_role.word(model::role_word::poison)) + poison_delta);
    if (changed_poison >= 99) {
        changed_poison = 99;
    }
    if (changed_poison <= 0) {
        changed_poison = 0;
    }
    target_role.set_word(model::role_word::poison, changed_poison);

    result.hit_count = 1;
    result.effect_id = item.word(model::item_word::hidden_weapon_effect_id);
    result.damage = damage;
    result.inventory_consumed = false;
    return result;
}

bool BattleSetup::finish_throwing_weapon_action(
    const std::size_t actor_slot,
    const std::size_t inventory_slot) noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        !consume_inventory_item_slot(inventory_slot)) {
        return false;
    }
    combatants_[actor_slot].words[combatant_word::action_done] = 1;
    return refresh_combatant_sprites();
}

bool BattleSetup::consume_ai_item(
    const std::size_t actor_slot,
    const BattleAiChoice& choice) noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        (choice.action != BattleAiAction::item &&
         choice.action != BattleAiAction::throwing_weapon) ||
        choice.item_slot < 0 || !ai_item_id(actor_slot, choice).has_value()) {
        return false;
    }
    const auto slot = static_cast<std::size_t>(choice.item_slot);
    if (choice.item_source == BattleAiItemSource::inventory) {
        return consume_inventory_item_slot(slot);
    }
    if (choice.item_source != BattleAiItemSource::carried ||
        slot >= model::role_word::taking_item_count) {
        return false;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return false;
    }
    auto& actor = ranger_.roles[static_cast<std::size_t>(role_id)];
    const auto remaining = wrapping_i16(
        static_cast<std::int32_t>(
            actor.word(model::role_word::taking_item_count_begin + slot)) -
        1);
    actor.set_word(model::role_word::taking_item_count_begin + slot, remaining);
    return remaining > 0 || remove_carried_item_slot(actor_slot, slot);
}

std::optional<BattleThrownItemResult> BattleSetup::apply_ai_throwing_weapon_target(
    const std::size_t actor_slot,
    const BattlePathCoord target,
    const BattleAiChoice& choice,
    random::LegacyRandom& random,
    const bool consume_item) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        choice.action != BattleAiAction::throwing_weapon) {
        error_ = "battle AI throwing-weapon action is outside legacy state";
        return std::nullopt;
    }
    const auto item_id = ai_item_id(actor_slot, choice);
    if (!item_id) {
        error_ = "battle AI throwing-weapon item is outside legacy records";
        return std::nullopt;
    }
    auto& actor_words = combatants_[actor_slot].words;
    const auto actor_role_id = actor_words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        error_ = "battle AI throwing-weapon actor role is outside ranger records";
        return std::nullopt;
    }

    clear_attack_effects();
    if (target.x < 0 || target.x >= static_cast<std::int16_t>(kBattleExtent) || target.y < 0 ||
        target.y >= static_cast<std::int16_t>(kBattleExtent)) {
        error_ = "battle AI throwing-weapon target is outside battlefield";
        return std::nullopt;
    }
    const auto cell = static_cast<std::size_t>(target.y) * kBattleExtent +
        static_cast<std::size_t>(target.x);
    attack_effects_[cell] = 1;
    const auto target_slot = data_.occupancy()[cell];
    if (target_slot < 0 || target_slot >= combatant_count_) {
        error_ = "battle AI throwing-weapon occupancy is outside combatant slots";
        return std::nullopt;
    }
    const auto target_index = static_cast<std::size_t>(target_slot);
    auto& target_words = combatants_[target_index].words;
    const auto target_role_id = target_words[combatant_word::role_id];
    if (target_role_id < 0 || static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle AI throwing-weapon target role is outside ranger records";
        return std::nullopt;
    }

    auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target_role = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    const auto& item = ranger_.items[static_cast<std::size_t>(*item_id)];
    const auto hurt = target_role.word(model::role_word::hurt);
    if (hurt < 0) {
        error_ = "battle AI throwing-weapon target hurt is outside legacy domain";
        return std::nullopt;
    }
    std::int32_t divisor = 1;
    if (hurt == 0) {
        divisor = 4;
    } else if (hurt <= 33) {
        divisor = 3;
    } else if (hurt <= 66) {
        divisor = 2;
    }
    const auto base_delta =
        static_cast<std::int32_t>(item.word(model::item_word::add_hp)) / divisor -
        random.bounded(5);
    const auto hp_delta = wrapping_i16(
        (base_delta -
         2 * static_cast<std::int32_t>(actor.word(model::role_word::hidden_weapon))) /
        3);

    auto changed_hurt = wrapping_i16(
        static_cast<std::int32_t>(target_role.word(model::role_word::hurt)) - hp_delta / 4);
    if (changed_hurt > 99) {
        changed_hurt = 99;
    }
    if (changed_hurt < 0) {
        changed_hurt = 0;
    }
    target_role.set_word(model::role_word::hurt, changed_hurt);

    const auto old_hp = target_role.word(model::role_word::hp);
    auto changed_hp = wrapping_i16(static_cast<std::int32_t>(old_hp) + hp_delta);
    if (changed_hp >= target_role.word(model::role_word::maximum_hp)) {
        changed_hp = target_role.word(model::role_word::maximum_hp);
    }
    if (changed_hp <= 0) {
        changed_hp = 0;
    }
    target_role.set_word(model::role_word::hp, changed_hp);
    const auto damage = wrapping_i16(std::abs(
        static_cast<std::int32_t>(changed_hp) - static_cast<std::int32_t>(old_hp)));
    target_words[combatant_word::damage_value] = damage;

    const auto item_poison = item.word(model::item_word::add_poison);
    const auto poison_delta = item_poison < 0
        ? wrapping_i16(
              (static_cast<std::int32_t>(item_poison) -
               actor.word(model::role_word::hidden_weapon)) /
              2)
        : item_poison;
    auto changed_poison = wrapping_i16(
        static_cast<std::int32_t>(target_role.word(model::role_word::poison)) + poison_delta);
    if (changed_poison >= 99) {
        changed_poison = 99;
    }
    if (changed_poison <= 0) {
        changed_poison = 0;
    }
    target_role.set_word(model::role_word::poison, changed_poison);

    if (consume_item && !consume_ai_item(actor_slot, choice)) {
        return std::nullopt;
    }

    BattleThrownItemResult result{};
    result.hit_count = 1;
    result.effect_id = item.word(model::item_word::hidden_weapon_effect_id);
    result.damage = damage;
    result.inventory_consumed = consume_item;
    return result;
}

std::optional<BattleItemEffectResult> BattleSetup::apply_ai_item_effect(
    const std::size_t actor_slot,
    const BattleAiChoice& choice,
    random::LegacyRandom& random,
    const bool consume_item) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        choice.action != BattleAiAction::item) {
        error_ = "battle AI item action is outside legacy state";
        return std::nullopt;
    }
    const auto item_id = ai_item_id(actor_slot, choice);
    if (!item_id) {
        error_ = "battle AI item is outside legacy records";
        return std::nullopt;
    }
    if (choice.target_slot < 0 || choice.target_slot >= combatant_count_) {
        error_ = "battle AI item target is outside combatant slots";
        return std::nullopt;
    }
    auto& actor_words = combatants_[actor_slot].words;
    auto& target_words = combatants_[static_cast<std::size_t>(choice.target_slot)].words;
    const auto actor_role_id = actor_words[combatant_word::role_id];
    const auto target_role_id = target_words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size() ||
        target_role_id < 0 || static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle AI item actor or target role is outside ranger records";
        return std::nullopt;
    }
    const auto x = target_words[combatant_word::x];
    const auto y = target_words[combatant_word::y];
    if (x < 0 || x >= static_cast<std::int16_t>(kBattleExtent) || y < 0 ||
        y >= static_cast<std::int16_t>(kBattleExtent)) {
        error_ = "battle AI item target coordinate is outside battlefield";
        return std::nullopt;
    }
    clear_attack_effects();
    attack_effects_[static_cast<std::size_t>(y) * kBattleExtent +
                    static_cast<std::size_t>(x)] = 1;

    auto& actor = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    auto& target = ranger_.roles[static_cast<std::size_t>(target_role_id)];
    const auto& item = ranger_.items[static_cast<std::size_t>(*item_id)];
    BattleItemEffectResult result{};

    const auto item_hp = item.word(model::item_word::add_hp);
    if (item_hp != 0) {
        const auto old_hp = target.word(model::role_word::hp);
        std::int32_t hp_delta = 0;
        if (item_hp > 0) {
            hp_delta = static_cast<std::int32_t>(item_hp) -
                static_cast<std::int32_t>(target.word(model::role_word::hurt)) / 2 +
                random.bounded(10);
            if (hp_delta < 0) {
                hp_delta = random.bounded(5) + 5;
            }
            auto changed_hurt = wrapping_i16(
                static_cast<std::int32_t>(target.word(model::role_word::hurt)) - item_hp / 4);
            if (changed_hurt < 0) {
                changed_hurt = 0;
            }
            if (changed_hurt > 99) {
                changed_hurt = 99;
            }
            target.set_word(model::role_word::hurt, changed_hurt);
        } else {
            auto damage_base = static_cast<std::int32_t>(item_hp) + 50 -
                static_cast<std::int32_t>(target.word(model::role_word::hurt)) / 2 -
                random.bounded(10);
            if (damage_base > 0) {
                damage_base = -5 - random.bounded(5);
            }
            hp_delta = (damage_base -
                        3 * static_cast<std::int32_t>(
                                actor.word(model::role_word::hidden_weapon))) /
                3;
            auto changed_hurt = wrapping_i16(
                static_cast<std::int32_t>(target.word(model::role_word::hurt)) - hp_delta / 10);
            if (changed_hurt > 99) {
                changed_hurt = 99;
            }
            if (changed_hurt < 0) {
                changed_hurt = 0;
            }
            target.set_word(model::role_word::hurt, changed_hurt);
        }
        auto changed_hp = wrapping_i16(static_cast<std::int32_t>(old_hp) + hp_delta);
        if (changed_hp >= target.word(model::role_word::maximum_hp)) {
            changed_hp = target.word(model::role_word::maximum_hp);
        }
        if (changed_hp <= 0) {
            changed_hp = 0;
        }
        target.set_word(model::role_word::hp, changed_hp);
        result.deltas[0U] = wrapping_i16(
            static_cast<std::int32_t>(changed_hp) - static_cast<std::int32_t>(old_hp));
    }

    const auto add_maximum_hp = item.word(model::item_word::add_maximum_hp);
    if (add_maximum_hp != 0) {
        auto changed = wrapping_i16(
            static_cast<std::int32_t>(target.word(model::role_word::maximum_hp)) +
            add_maximum_hp);
        if (changed <= 0) {
            changed = 0;
        }
        if (changed >= 999) {
            changed = 999;
        }
        target.set_word(model::role_word::maximum_hp, changed);
        if (target.word(model::role_word::hp) >= changed) {
            target.set_word(model::role_word::hp, changed);
        }
        result.deltas[1U] = add_maximum_hp;
    }

    const auto item_poison = item.word(model::item_word::add_poison);
    if (item_poison != 0) {
        const auto old_poison = target.word(model::role_word::poison);
        std::int32_t poison_delta = 0;
        if (item_poison > 0) {
            poison_delta =
                (static_cast<std::int32_t>(actor.word(model::role_word::hidden_weapon)) +
                 item_poison) /
                    2 -
                target.word(model::role_word::anti_poison);
            if (target.word(model::role_word::anti_poison) >= 100 || poison_delta < 0) {
                poison_delta = 0;
            }
            poison_delta /= 2;
        } else {
            poison_delta = static_cast<std::int32_t>(item_poison) / 2 + random.bounded(5) -
                random.bounded(5);
        }
        auto changed = wrapping_i16(static_cast<std::int32_t>(old_poison) + poison_delta);
        if (changed >= 99) {
            changed = 99;
        }
        if (changed <= 0) {
            changed = 0;
        }
        target.set_word(model::role_word::poison, changed);
        result.deltas[2U] = wrapping_i16(
            static_cast<std::int32_t>(changed) - static_cast<std::int32_t>(old_poison));
    }

    const auto add_physical_power = item.word(model::item_word::add_physical_power);
    if (add_physical_power != 0) {
        const auto old_value = target.word(model::role_word::physical_power);
        auto changed = wrapping_i16(static_cast<std::int32_t>(old_value) + add_physical_power);
        if (changed >= 100) {
            changed = 100;
        }
        if (changed <= 0) {
            changed = 0;
        }
        target.set_word(model::role_word::physical_power, changed);
        result.deltas[3U] = wrapping_i16(
            static_cast<std::int32_t>(changed) - static_cast<std::int32_t>(old_value));
    }

    const auto change_mp_type = item.word(model::item_word::change_mp_type);
    if (change_mp_type == 2) {
        target.set_word(model::role_word::mp_type, 2);
        result.deltas[4U] = 2;
    }

    const auto add_mp = item.word(model::item_word::add_mp);
    if (add_mp != 0) {
        const auto old_value = target.word(model::role_word::mp);
        auto changed = wrapping_i16(static_cast<std::int32_t>(old_value) + add_mp);
        if (changed >= target.word(model::role_word::maximum_mp)) {
            changed = target.word(model::role_word::maximum_mp);
        }
        if (changed <= 0) {
            changed = 0;
        }
        target.set_word(model::role_word::mp, changed);
        result.deltas[5U] = wrapping_i16(
            static_cast<std::int32_t>(changed) - static_cast<std::int32_t>(old_value));
    }

    const auto add_maximum_mp = item.word(model::item_word::add_maximum_mp);
    if (add_maximum_mp != 0) {
        auto changed = wrapping_i16(
            static_cast<std::int32_t>(target.word(model::role_word::maximum_mp)) +
            add_maximum_mp);
        if (changed <= 0) {
            changed = 0;
        }
        if (changed >= 999) {
            changed = 999;
        }
        target.set_word(model::role_word::maximum_mp, changed);
        if (target.word(model::role_word::mp) >= changed) {
            target.set_word(model::role_word::mp, changed);
        }
        result.deltas[6U] = add_maximum_mp;
    }

    for (std::size_t index = 0U; index < 13U; ++index) {
        const auto delta = item.word(model::item_word::add_attack + index);
        const auto role_word_index = model::role_word::attack + index;
        target.set_word(
            role_word_index,
            wrapping_i16(static_cast<std::int32_t>(target.word(role_word_index)) + delta));
        result.deltas[7U + index] = delta;
    }
    result.deltas[20U] = item.word(model::item_word::add_morality);
    result.deltas[21U] = item.word(model::item_word::add_attack_twice);
    const auto add_attack_with_poison = item.word(model::item_word::add_attack_with_poison);
    target.set_word(
        model::role_word::attack_with_poison,
        wrapping_i16(
            static_cast<std::int32_t>(
                target.word(model::role_word::attack_with_poison)) +
            add_attack_with_poison));
    result.deltas[22U] = add_attack_with_poison;

    result.effect_count = static_cast<std::int16_t>(std::ranges::count_if(
        result.deltas, [](const std::int16_t value) { return value != 0; }));
    result.has_effect = result.effect_count > 0;
    result.panel_height = static_cast<std::int16_t>(20 * result.effect_count + 30);
    result.battle_redraw_required = result.has_effect;
    result.wait_for_input = result.has_effect;

    if (!consume_item) {
        return result;
    }
    if (!consume_ai_item(actor_slot, choice)) {
        return std::nullopt;
    }
    result.item_consumed = true;
    return result;
}

std::optional<BattleRestResult> BattleSetup::rest_actor(
    const std::size_t actor_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle rest actor is outside combatant slots";
        return std::nullopt;
    }
    auto& words = combatants_[actor_slot].words;
    const auto role_id = words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        error_ = "battle rest actor role is outside ranger records";
        return std::nullopt;
    }
    words[combatant_word::action_done] = 1;
    auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    const auto speed_tenth = static_cast<std::int32_t>(role.word(model::role_word::speed)) / 10;
    const auto physical_gain = random.bounded(3) +
        (words[combatant_word::round_value] == speed_tenth ? 3 : 2);
    auto physical_power = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::physical_power)) + physical_gain);
    if (physical_power > 100) {
        physical_power = 100;
    }
    role.set_word(model::role_word::physical_power, physical_power);

    if (physical_power >= 30) {
        const auto bound = static_cast<std::int32_t>(physical_power) / 10 - 2;
        auto hp = wrapping_i16(
            static_cast<std::int32_t>(role.word(model::role_word::hp)) + random.bounded(bound) + 3);
        if (hp > role.word(model::role_word::maximum_hp)) {
            hp = role.word(model::role_word::maximum_hp);
        }
        role.set_word(model::role_word::hp, hp);

        auto mp = wrapping_i16(
            static_cast<std::int32_t>(role.word(model::role_word::mp)) + random.bounded(bound) + 3);
        if (mp > role.word(model::role_word::maximum_mp)) {
            mp = role.word(model::role_word::maximum_mp);
        }
        role.set_word(model::role_word::mp, mp);
    }
    return BattleRestResult{
        role.word(model::role_word::physical_power),
        role.word(model::role_word::hp),
        role.word(model::role_word::mp)};
}

BattleAiChoice BattleSetup::commit_ai_choice(
    const std::size_t actor_slot,
    const BattleAiAction action,
    const std::int16_t target_slot,
    const BattleAiItemSource item_source,
    const std::int16_t item_slot,
    const bool write_action_code) noexcept {
    BattleAiChoice choice{
        .action = action,
        .target_slot = target_slot,
        .item_source = item_source,
        .item_slot = item_slot,
        .action_code_written = write_action_code,
    };
    if (target_slot >= 0 && target_slot < combatant_count_) {
        const auto& target = combatants_[static_cast<std::size_t>(target_slot)].words;
        choice.target = BattlePathCoord{target[combatant_word::x], target[combatant_word::y]};
    }
    if (write_action_code) {
        combatants_[actor_slot].words[combatant_word::ai_action] =
            static_cast<std::int16_t>(action);
    }
    return choice;
}

std::optional<BattleAiChoice> BattleSetup::choose_ai_low_hp_action(
    const std::size_t actor_slot) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& actor_role = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    if (actor_role.word(model::role_word::medicine) >= 20 &&
        actor_role.word(model::role_word::physical_power) >= 50 &&
        actor_role.word(model::role_word::medicine) >
            static_cast<std::int32_t>(actor_role.word(model::role_word::hurt)) - 30) {
        return commit_ai_choice(
            actor_slot,
            BattleAiAction::medicine,
            static_cast<std::int16_t>(actor_slot));
    }

    const auto side = combatants_[actor_slot].words[combatant_word::side];
    const auto select_item = [&](const std::int16_t item_id,
                                 const BattleAiItemSource source,
                                 const std::int16_t item_slot)
        -> std::optional<BattleAiChoice> {
        if (item_id < 0) {
            return BattleAiChoice{};
        }
        if (static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            return std::nullopt;
        }
        if (ranger_.items[static_cast<std::size_t>(item_id)].word(model::item_word::add_hp) > 0) {
            return commit_ai_choice(
                actor_slot,
                BattleAiAction::item,
                static_cast<std::int16_t>(actor_slot),
                source,
                item_slot);
        }
        return BattleAiChoice{};
    };
    if (side == 0) {
        for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
            const auto selected = select_item(
                ranger_.header.inventory_item(slot).value,
                BattleAiItemSource::inventory,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    } else {
        for (std::size_t slot = 0U; slot < model::role_word::taking_item_count; ++slot) {
            const auto selected = select_item(
                actor_role.word(model::role_word::taking_item_begin + slot),
                BattleAiItemSource::carried,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    }

    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        if (slot == actor_slot || combatants_[slot].words[combatant_word::side] != side ||
            combatants_[slot].words[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto medicine = ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::medicine);
        if (medicine > 20 && medicine >
                static_cast<std::int32_t>(actor_role.word(model::role_word::hurt)) - 30) {
            return commit_ai_choice(
                actor_slot,
                BattleAiAction::request_medicine,
                static_cast<std::int16_t>(slot));
        }
    }
    return commit_ai_choice(
        actor_slot, BattleAiAction::none, -1, BattleAiItemSource::none, -1, false);
}

std::optional<BattleAiChoice> BattleSetup::choose_ai_poisoned_action(
    const std::size_t actor_slot) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& actor_role = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    if (actor_role.word(model::role_word::detoxification) > 20 &&
        actor_role.word(model::role_word::detoxification) >
            static_cast<std::int32_t>(actor_role.word(model::role_word::poison)) - 30 &&
        actor_role.word(model::role_word::physical_power) > 50) {
        return commit_ai_choice(
            actor_slot,
            BattleAiAction::detox,
            static_cast<std::int16_t>(actor_slot));
    }

    const auto side = combatants_[actor_slot].words[combatant_word::side];
    const auto property = side == 0 ? model::item_word::add_use_poison :
                                     model::item_word::add_poison;
    const auto select_item = [&](const std::int16_t item_id,
                                 const BattleAiItemSource source,
                                 const std::int16_t item_slot)
        -> std::optional<BattleAiChoice> {
        if (item_id < 0) {
            return BattleAiChoice{};
        }
        if (static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            return std::nullopt;
        }
        if (ranger_.items[static_cast<std::size_t>(item_id)].word(property) < 0) {
            return commit_ai_choice(
                actor_slot,
                BattleAiAction::item,
                static_cast<std::int16_t>(actor_slot),
                source,
                item_slot);
        }
        return BattleAiChoice{};
    };
    if (side == 0) {
        for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
            const auto selected = select_item(
                ranger_.header.inventory_item(slot).value,
                BattleAiItemSource::inventory,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    } else {
        for (std::size_t slot = 0U; slot < model::role_word::taking_item_count; ++slot) {
            const auto selected = select_item(
                actor_role.word(model::role_word::taking_item_begin + slot),
                BattleAiItemSource::carried,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    }

    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        if (slot == actor_slot || combatants_[slot].words[combatant_word::side] != side ||
            combatants_[slot].words[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto detoxification = ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::detoxification);
        if (detoxification > 20 && detoxification >
                static_cast<std::int32_t>(actor_role.word(model::role_word::poison)) - 30) {
            return commit_ai_choice(
                actor_slot,
                BattleAiAction::request_detox,
                static_cast<std::int16_t>(slot));
        }
    }
    return commit_ai_choice(
        actor_slot, BattleAiAction::none, -1, BattleAiItemSource::none, -1, false);
}

std::optional<BattleAiChoice> BattleSetup::choose_ai_low_mp_action(
    const std::size_t actor_slot) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& actor_role = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    const auto side = combatants_[actor_slot].words[combatant_word::side];
    const auto select_item = [&](const std::int16_t item_id,
                                 const BattleAiItemSource source,
                                 const std::int16_t item_slot)
        -> std::optional<BattleAiChoice> {
        if (item_id < 0) {
            return BattleAiChoice{};
        }
        if (static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            return std::nullopt;
        }
        if (ranger_.items[static_cast<std::size_t>(item_id)].word(model::item_word::add_mp) > 0) {
            return commit_ai_choice(
                actor_slot,
                BattleAiAction::item,
                static_cast<std::int16_t>(actor_slot),
                source,
                item_slot);
        }
        return BattleAiChoice{};
    };
    if (side == 0) {
        for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
            const auto selected = select_item(
                ranger_.header.inventory_item(slot).value,
                BattleAiItemSource::inventory,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    } else {
        for (std::size_t slot = 0U; slot < model::role_word::taking_item_count; ++slot) {
            const auto selected = select_item(
                actor_role.word(model::role_word::taking_item_begin + slot),
                BattleAiItemSource::carried,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    }
    return commit_ai_choice(
        actor_slot, BattleAiAction::none, -1, BattleAiItemSource::none, -1, false);
}

std::optional<BattleAiChoice> BattleSetup::choose_ai_medicine_target(
    const std::size_t actor_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto medicine = ranger_.roles[static_cast<std::size_t>(actor_role_id)].word(
        model::role_word::medicine);
    const auto side = combatants_[actor_slot].words[combatant_word::side];
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        if (slot == actor_slot || combatants_[slot].words[combatant_word::side] != side ||
            combatants_[slot].words[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        if (medicine <= static_cast<std::int32_t>(role.word(model::role_word::hurt)) - 30) {
            continue;
        }
        auto selected = combatants_[slot].words[combatant_word::ai_action] ==
                static_cast<std::int16_t>(BattleAiAction::request_medicine) ||
            role.word(model::role_word::hp) < 20 || role.word(model::role_word::hurt) > 40;
        if (!selected && role.word(model::role_word::hp) <
                role.word(model::role_word::maximum_hp) / 2) {
            selected = random.bounded(10) < 7;
        }
        if (!selected && role.word(model::role_word::hp) <
                role.word(model::role_word::maximum_hp) / 3) {
            selected = random.bounded(10) < 8;
        }
        if (!selected && role.word(model::role_word::hp) <
                role.word(model::role_word::maximum_hp) / 4) {
            selected = random.bounded(10) < 9;
        }
        if (!selected && role.word(model::role_word::hp) <
                role.word(model::role_word::maximum_hp) / 5) {
            selected = true;
        }
        if (selected) {
            return commit_ai_choice(
                actor_slot,
                BattleAiAction::medicine,
                static_cast<std::int16_t>(slot));
        }
    }
    return commit_ai_choice(
        actor_slot, BattleAiAction::none, -1, BattleAiItemSource::none, -1, false);
}

std::optional<BattleAiChoice> BattleSetup::choose_ai_detox_target(
    const std::size_t actor_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto detoxification = ranger_.roles[static_cast<std::size_t>(actor_role_id)].word(
        model::role_word::detoxification);
    const auto side = combatants_[actor_slot].words[combatant_word::side];
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        if (slot == actor_slot || combatants_[slot].words[combatant_word::side] != side ||
            combatants_[slot].words[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        if (detoxification <= static_cast<std::int32_t>(role.word(model::role_word::poison)) - 30) {
            continue;
        }
        auto selected = combatants_[slot].words[combatant_word::ai_action] ==
            static_cast<std::int16_t>(BattleAiAction::request_detox);
        if (!selected && role.word(model::role_word::poison) > 10) {
            selected = random.bounded(10) < 4;
        }
        if (!selected && role.word(model::role_word::poison) > 20) {
            selected = random.bounded(10) < 6;
        }
        if (!selected && role.word(model::role_word::poison) > 30) {
            selected = random.bounded(10) < 8;
        }
        if (!selected && role.word(model::role_word::poison) > 40) {
            selected = true;
        }
        if (selected) {
            return commit_ai_choice(
                actor_slot,
                BattleAiAction::detox,
                static_cast<std::int16_t>(slot));
        }
    }
    return commit_ai_choice(
        actor_slot, BattleAiAction::none, -1, BattleAiItemSource::none, -1, false);
}

std::optional<BattleAiChoice> BattleSetup::choose_ai_offensive_action(
    const std::size_t actor_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& actor_role = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    const auto side = combatants_[actor_slot].words[combatant_word::side];
    std::int16_t allied_total = 0;
    std::int16_t opponent_total = 0;
    std::int16_t allied_count = 0;
    std::int16_t opponent_count = 0;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        auto& total = combatants_[slot].words[combatant_word::side] == side ? allied_total :
                                                                          opponent_total;
        auto& count = combatants_[slot].words[combatant_word::side] == side ? allied_count :
                                                                          opponent_count;
        total = wrapping_i16(static_cast<std::int32_t>(total) + role.word(model::role_word::attack));
        total = wrapping_i16(static_cast<std::int32_t>(total) + role.word(model::role_word::hp));
        count = wrapping_i16(static_cast<std::int32_t>(count) + 1);
    }
    if (opponent_count == 0) {
        return std::nullopt;
    }

    const auto opponent_average_half =
        (static_cast<std::int32_t>(opponent_total) / opponent_count) / 2;
    const auto actor_power = static_cast<std::int32_t>(actor_role.word(model::role_word::attack)) +
        actor_role.word(model::role_word::hp);
    if (opponent_average_half > actor_power &&
        static_cast<std::int32_t>(allied_total) > 2 * static_cast<std::int32_t>(opponent_total)) {
        std::int16_t best_value = 0;
        std::int16_t best_slot = 0;
        BattleAiAction aid_action = BattleAiAction::none;
        if (actor_role.word(model::role_word::medicine) >= 20 &&
            actor_role.word(model::role_word::physical_power) >= 50) {
            for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
                if (slot == actor_slot || combatants_[slot].words[combatant_word::side] != side ||
                    combatants_[slot].words[combatant_word::occupancy_hidden] != 0) {
                    continue;
                }
                const auto role_id = combatants_[slot].words[combatant_word::role_id];
                const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
                if (role.word(model::role_word::hp) < role.word(model::role_word::maximum_hp)) {
                    const auto missing = wrapping_i16(
                        static_cast<std::int32_t>(role.word(model::role_word::maximum_hp)) -
                        role.word(model::role_word::hp));
                    if (missing > best_value) {
                        best_value = missing;
                        best_slot = static_cast<std::int16_t>(slot);
                        aid_action = BattleAiAction::medicine;
                    }
                }
            }
        } else if (actor_role.word(model::role_word::detoxification) >= 20 &&
                   actor_role.word(model::role_word::physical_power) >= 50) {
            for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
                if (slot == actor_slot || combatants_[slot].words[combatant_word::side] != side ||
                    combatants_[slot].words[combatant_word::occupancy_hidden] != 0) {
                    continue;
                }
                const auto role_id = combatants_[slot].words[combatant_word::role_id];
                const auto poison = ranger_.roles[static_cast<std::size_t>(role_id)].word(
                    model::role_word::poison);
                if (poison > best_value) {
                    best_value = poison;
                    best_slot = static_cast<std::int16_t>(slot);
                    aid_action = BattleAiAction::detox;
                }
            }
        }
        if (best_value != 0) {
            return commit_ai_choice(actor_slot, aid_action, best_slot);
        }
    }

    const auto poison_advantage =
        static_cast<std::int32_t>(actor_role.word(model::role_word::use_poison)) -
        actor_role.word(model::role_word::attack);
    const auto poison_gate = random.bounded(50);
    if (poison_advantage > poison_gate) {
        const auto poison_roll = random.bounded(150);
        if (poison_roll < actor_role.word(model::role_word::use_poison)) {
            return commit_ai_choice(actor_slot, BattleAiAction::use_poison, -1);
        }
    }

    const auto attack = static_cast<std::int32_t>(actor_role.word(model::role_word::attack));
    const auto party_threshold = (3 * attack) / 2;
    const auto select_throwing_item = [&](const std::int16_t item_id,
                                          const BattleAiItemSource source,
                                          const std::int16_t item_slot)
        -> std::optional<BattleAiChoice> {
        if (item_id < 0) {
            return BattleAiChoice{};
        }
        if (static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            return std::nullopt;
        }
        const auto& item = ranger_.items[static_cast<std::size_t>(item_id)];
        const auto add_hp = static_cast<std::int32_t>(item.word(model::item_word::add_hp));
        if (add_hp < 0) {
            const auto magnitude = -add_hp;
            if (source == BattleAiItemSource::inventory) {
                if (magnitude > party_threshold) {
                    const auto roll = random.bounded(
                        actor_role.word(model::role_word::hidden_weapon));
                    if (roll > 20) {
                        return commit_ai_choice(
                            actor_slot,
                            BattleAiAction::throwing_weapon,
                            -1,
                            source,
                            item_slot);
                    }
                }
            } else if (magnitude > attack) {
                const auto roll = random.bounded(10);
                if (roll < 6) {
                    return commit_ai_choice(
                        actor_slot,
                        BattleAiAction::throwing_weapon,
                        -1,
                        source,
                        item_slot);
                }
            }
        }
        const auto add_poison = static_cast<std::int32_t>(
            item.word(model::item_word::add_poison));
        const auto poison_threshold = source == BattleAiItemSource::inventory ? party_threshold :
                                                                                 attack;
        if (add_poison > 0 && add_poison > poison_threshold) {
            const auto roll = random.bounded(10);
            if (roll < 3) {
                return commit_ai_choice(
                    actor_slot,
                    BattleAiAction::throwing_weapon,
                    -1,
                    source,
                    item_slot);
            }
        }
        return BattleAiChoice{};
    };
    if (side == 0) {
        for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
            const auto selected = select_throwing_item(
                ranger_.header.inventory_item(slot).value,
                BattleAiItemSource::inventory,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    } else {
        for (std::size_t slot = 0U; slot < model::role_word::taking_item_count; ++slot) {
            const auto selected = select_throwing_item(
                actor_role.word(model::role_word::taking_item_begin + slot),
                BattleAiItemSource::carried,
                static_cast<std::int16_t>(slot));
            if (!selected) {
                return std::nullopt;
            }
            if (selected->action != BattleAiAction::none) {
                return selected;
            }
        }
    }

    if (actor_role.word(model::role_word::physical_power) <= 10) {
        return commit_ai_choice(
            actor_slot, BattleAiAction::none, -1, BattleAiItemSource::none, -1, false);
    }
    std::int16_t minimum_mp = 1'000;
    for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
        const auto magic_id = actor_role.word(model::role_word::magic_id_begin + slot);
        if (magic_id == 0) {
            continue;
        }
        if (magic_id < 0 || static_cast<std::size_t>(magic_id) >= ranger_.magics.size()) {
            return std::nullopt;
        }
        const auto need_mp = ranger_.magics[static_cast<std::size_t>(magic_id)].word(
            model::magic_word::need_mp);
        if (minimum_mp > need_mp) {
            minimum_mp = need_mp;
        }
    }
    if (actor_role.word(model::role_word::mp) < minimum_mp) {
        return commit_ai_choice(
            actor_slot, BattleAiAction::none, -1, BattleAiItemSource::none, -1, false);
    }
    return commit_ai_choice(
        actor_slot, BattleAiAction::attack, -1, BattleAiItemSource::none, -1, false);
}

std::optional<BattleAiTurnPrelude> BattleSetup::begin_ai_turn(
    const std::size_t actor_slot) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    BattleAiTurnPrelude prelude{};
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto role_id = combatants_[slot].words[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        auto& total = combatants_[slot].words[combatant_word::side] == actor_side ?
            prelude.allied_total : prelude.opponent_total;
        auto& count = combatants_[slot].words[combatant_word::side] == actor_side ?
            prelude.allied_count : prelude.opponent_count;
        total = wrapping_i16(static_cast<std::int32_t>(total) + role.word(model::role_word::attack));
        total = wrapping_i16(static_cast<std::int32_t>(total) + role.word(model::role_word::hp));
        count = wrapping_i16(static_cast<std::int32_t>(count) + 1);
    }
    return prelude;
}

std::optional<BattleAiTurnDecision> BattleSetup::choose_ai_turn_action(
    const std::size_t actor_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& actor_role = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    BattleAiChoice choice{};
    if (actor_role.word(model::role_word::physical_power) < 10) {
        choice.action = BattleAiAction::wait;
    }

    auto choose_low_hp = actor_role.word(model::role_word::hp) < 20 ||
        actor_role.word(model::role_word::hurt) > 50;
    if (!choose_low_hp && actor_role.word(model::role_word::hp) <
            actor_role.word(model::role_word::maximum_hp) / 2) {
        choose_low_hp = random.bounded(10) < 3;
    }
    if (!choose_low_hp && actor_role.word(model::role_word::hp) <
            actor_role.word(model::role_word::maximum_hp) / 3) {
        choose_low_hp = random.bounded(10) < 5;
    }
    if (!choose_low_hp && actor_role.word(model::role_word::hp) <
            actor_role.word(model::role_word::maximum_hp) / 4) {
        choose_low_hp = random.bounded(10) < 7;
    }
    if (!choose_low_hp && actor_role.word(model::role_word::hp) <
            actor_role.word(model::role_word::maximum_hp) / 5) {
        choose_low_hp = random.bounded(10) < 9;
    }
    if (choose_low_hp) {
        const auto selected = choose_ai_low_hp_action(actor_slot);
        if (!selected) {
            return std::nullopt;
        }
        choice = *selected;
    }

    if (choice.action == BattleAiAction::none) {
        const auto poison_gate = static_cast<std::int32_t>(
            actor_role.word(model::role_word::poison)) / 10;
        if (random.bounded(10) < poison_gate) {
            const auto selected = choose_ai_poisoned_action(actor_slot);
            if (!selected) {
                return std::nullopt;
            }
            choice = *selected;
        }
    }

    if (choice.action == BattleAiAction::none) {
        auto choose_low_mp = false;
        if (actor_role.word(model::role_word::mp) <
            actor_role.word(model::role_word::maximum_mp) / 2) {
            choose_low_mp = random.bounded(10) < 2;
        }
        if (!choose_low_mp && actor_role.word(model::role_word::mp) <
                actor_role.word(model::role_word::maximum_mp) / 3) {
            choose_low_mp = random.bounded(10) < 4;
        }
        if (!choose_low_mp && actor_role.word(model::role_word::mp) <
                actor_role.word(model::role_word::maximum_mp) / 4) {
            choose_low_mp = random.bounded(10) < 6;
        }
        if (!choose_low_mp && actor_role.word(model::role_word::mp) <
                actor_role.word(model::role_word::maximum_mp) / 5) {
            choose_low_mp = random.bounded(10) < 8;
        }
        if (choose_low_mp) {
            const auto selected = choose_ai_low_mp_action(actor_slot);
            if (!selected) {
                return std::nullopt;
            }
            choice = *selected;
        }
    }

    if (choice.action == BattleAiAction::none &&
        actor_role.word(model::role_word::physical_power) > 50) {
        auto choose_medicine = false;
        if (actor_role.word(model::role_word::medicine) >= 20) {
            choose_medicine = random.bounded(10) < 4;
        }
        if (!choose_medicine && actor_role.word(model::role_word::medicine) >= 40) {
            choose_medicine = random.bounded(10) < 6;
        }
        if (!choose_medicine && actor_role.word(model::role_word::medicine) >= 60) {
            choose_medicine = random.bounded(10) < 8;
        }
        if (!choose_medicine && actor_role.word(model::role_word::medicine) >= 80) {
            choose_medicine = true;
        }
        if (choose_medicine) {
            const auto selected = choose_ai_medicine_target(actor_slot, random);
            if (!selected) {
                return std::nullopt;
            }
            choice = *selected;
        }
    }

    if (choice.action == BattleAiAction::none &&
        actor_role.word(model::role_word::physical_power) > 50) {
        auto choose_detox = false;
        if (actor_role.word(model::role_word::detoxification) >= 20) {
            choose_detox = random.bounded(10) < 4;
        }
        if (!choose_detox && actor_role.word(model::role_word::detoxification) >= 40) {
            choose_detox = random.bounded(10) < 6;
        }
        if (!choose_detox && actor_role.word(model::role_word::detoxification) >= 60) {
            choose_detox = random.bounded(10) < 8;
        }
        if (!choose_detox && actor_role.word(model::role_word::detoxification) >= 80) {
            choose_detox = true;
        }
        if (choose_detox) {
            const auto selected = choose_ai_detox_target(actor_slot, random);
            if (!selected) {
                return std::nullopt;
            }
            choice = *selected;
        }
    }

    if (choice.action == BattleAiAction::none && random.bounded(10) < 5) {
        auto choose_escape = actor_role.word(model::role_word::hp) < 20;
        if (!choose_escape && actor_role.word(model::role_word::hp) <
                actor_role.word(model::role_word::maximum_hp) / 4) {
            choose_escape = random.bounded(10) < 6;
        }
        if (!choose_escape && actor_role.word(model::role_word::hp) <
                actor_role.word(model::role_word::maximum_hp) / 5) {
            choose_escape = random.bounded(10) < 8;
        }
        if (choose_escape) {
            choice.action = BattleAiAction::escape;
        }
    }

    if (choice.action == BattleAiAction::none) {
        const auto selected = choose_ai_offensive_action(actor_slot, random);
        if (!selected) {
            return std::nullopt;
        }
        choice = *selected;
    }

    BattleAiHandler handler{};
    switch (choice.action) {
    case BattleAiAction::none:
    case BattleAiAction::wait:
        handler = BattleAiHandler::rest;
        break;
    case BattleAiAction::move:
        handler = BattleAiHandler::move;
        break;
    case BattleAiAction::attack:
        handler = BattleAiHandler::attack;
        break;
    case BattleAiAction::use_poison:
        handler = BattleAiHandler::use_poison;
        break;
    case BattleAiAction::detox:
        handler = BattleAiHandler::detox;
        break;
    case BattleAiAction::medicine:
        handler = BattleAiHandler::medicine;
        break;
    case BattleAiAction::item:
        handler = BattleAiHandler::item;
        break;
    case BattleAiAction::request_medicine:
        handler = BattleAiHandler::request_medicine;
        break;
    case BattleAiAction::request_detox:
        handler = BattleAiHandler::request_detox;
        break;
    case BattleAiAction::throwing_weapon:
        handler = BattleAiHandler::throwing_weapon;
        break;
    case BattleAiAction::escape:
        handler = BattleAiHandler::escape;
        break;
    }
    return BattleAiTurnDecision{choice, handler};
}

bool BattleSetup::finish_ai_turn(const std::size_t actor_slot) noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return false;
    }
    combatants_[actor_slot].words[combatant_word::action_done] = 1;
    return true;
}

std::optional<BattleAiEscapePlan> BattleSetup::ai_escape_plan(
    const std::size_t actor_slot,
    const bool rest_after_move) const {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto& actor = combatants_[actor_slot].words;
    BattlePathing pathing{data_};
    pathing.build(
        BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
        BattlePathMode::movement);

    BattleAiEscapePlan plan{.rest_after_move = rest_after_move};
    for (std::int16_t x = 0; x < static_cast<std::int16_t>(kBattleExtent); ++x) {
        for (std::int16_t y = 0; y < static_cast<std::int16_t>(kBattleExtent); ++y) {
            const BattlePathCoord coordinate{x, y};
            if (pathing.value(coordinate) != actor[combatant_word::round_value]) {
                continue;
            }
            std::int32_t score = 0;
            for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
                const auto& other = combatants_[slot].words;
                if (other[combatant_word::side] == actor[combatant_word::side]) {
                    continue;
                }
                score += std::abs(static_cast<std::int32_t>(x) - other[combatant_word::x]);
                score += std::abs(static_cast<std::int32_t>(y) - other[combatant_word::y]);
            }
            if (score > plan.maximum_enemy_distance_sum) {
                plan.maximum_enemy_distance_sum = score;
                plan.destination = coordinate;
            }
        }
    }
    return plan;
}

std::optional<bool> BattleSetup::choose_ai_strongest_attack_target(
    const std::size_t actor_slot) {
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    std::int16_t best_attack = 0;
    bool written = false;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] == actor_side ||
            combatant[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatant[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto attack = ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::attack);
        if (attack > best_attack) {
            best_attack = attack;
            combatants_[actor_slot].words[combatant_word::ai_target] =
                static_cast<std::int16_t>(slot);
            written = true;
        }
    }
    return written;
}

std::optional<bool> BattleSetup::choose_ai_weakest_attack_target(
    const std::size_t actor_slot) {
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    std::int16_t best_attack = 1'000;
    bool written = false;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] == actor_side ||
            combatant[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatant[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto attack = ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::attack);
        if (attack < best_attack) {
            best_attack = attack;
            combatants_[actor_slot].words[combatant_word::ai_target] =
                static_cast<std::int16_t>(slot);
            written = true;
        }
    }
    return written;
}

std::optional<bool> BattleSetup::choose_ai_specialist_target(const std::size_t actor_slot) {
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    auto ally_can_poison = false;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] != actor_side) {
            continue;
        }
        const auto role_id = combatant[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        if (ranger_.roles[static_cast<std::size_t>(role_id)].word(
                model::role_word::use_poison) > 20) {
            ally_can_poison = true;
        }
    }

    std::int16_t best_value = 0;
    bool detox_target_at_least_20 = false;
    bool medicine_target_at_least_20 = false;
    bool written = false;
    if (ally_can_poison) {
        for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
            const auto& combatant = combatants_[slot].words;
            if (combatant[combatant_word::side] == actor_side ||
                combatant[combatant_word::occupancy_hidden] != 0) {
                continue;
            }
            const auto role_id = combatant[combatant_word::role_id];
            if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
                return std::nullopt;
            }
            const auto detoxification = ranger_.roles[static_cast<std::size_t>(role_id)].word(
                model::role_word::detoxification);
            if (detoxification > best_value) {
                best_value = detoxification;
                combatants_[actor_slot].words[combatant_word::ai_target] =
                    static_cast<std::int16_t>(slot);
                written = true;
                if (detoxification >= 20) {
                    detox_target_at_least_20 = true;
                }
            }
        }
    }

    if (!detox_target_at_least_20) {
        for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
            const auto& combatant = combatants_[slot].words;
            if (combatant[combatant_word::side] == actor_side ||
                combatant[combatant_word::occupancy_hidden] != 0) {
                continue;
            }
            const auto role_id = combatant[combatant_word::role_id];
            if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
                return std::nullopt;
            }
            const auto medicine = ranger_.roles[static_cast<std::size_t>(role_id)].word(
                model::role_word::medicine);
            if (medicine > best_value) {
                best_value = medicine;
                combatants_[actor_slot].words[combatant_word::ai_target] =
                    static_cast<std::int16_t>(slot);
                written = true;
                if (medicine >= 20) {
                    medicine_target_at_least_20 = true;
                }
            }
        }
    }

    if (!medicine_target_at_least_20) {
        return choose_ai_weakest_attack_target(actor_slot);
    }
    return written;
}

std::optional<bool> BattleSetup::choose_ai_nearest_target(const std::size_t actor_slot) {
    const auto& actor = combatants_[actor_slot].words;
    BattlePathing pathing{data_};
    pathing.build(
        BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
        BattlePathMode::targeting);
    std::int16_t best_distance = 1'000;
    bool written = false;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] == actor[combatant_word::side] ||
            combatant[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto distance = pathing.value(
            BattlePathCoord{combatant[combatant_word::x], combatant[combatant_word::y]});
        if (distance < best_distance) {
            best_distance = distance;
            combatants_[actor_slot].words[combatant_word::ai_target] =
                static_cast<std::int16_t>(slot);
            written = true;
        }
    }
    return written;
}

std::optional<BattleAiTargetSelection> BattleSetup::choose_ai_attack_target(
    const std::size_t actor_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    const auto selection = [&](const BattleAiTargetStrategy strategy,
                               const std::optional<bool> written)
        -> std::optional<BattleAiTargetSelection> {
        if (!written.has_value()) {
            return std::nullopt;
        }
        return BattleAiTargetSelection{
            combatants_[actor_slot].words[combatant_word::ai_target], strategy, *written};
    };
    if (role.word(model::role_word::morality) >= 75 && random.bounded(10) < 7) {
        return selection(
            BattleAiTargetStrategy::strongest_attack,
            choose_ai_strongest_attack_target(actor_slot));
    }
    if (role.word(model::role_word::morality) <= 25 && random.bounded(10) < 7) {
        return selection(
            BattleAiTargetStrategy::weakest_attack,
            choose_ai_weakest_attack_target(actor_slot));
    }
    if (role.word(model::role_word::iq) >= 70 && random.bounded(10) < 7) {
        return selection(
            BattleAiTargetStrategy::specialist,
            choose_ai_specialist_target(actor_slot));
    }
    return selection(BattleAiTargetStrategy::nearest, choose_ai_nearest_target(actor_slot));
}

bool BattleSetup::update_ai_attack_target_range(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    BattleAiAttackPlan& plan) const {
    const auto& actor = combatants_[actor_slot].words;
    const auto& target = combatants_[target_slot].words;
    BattlePathing pathing{data_};
    pathing.build(
        BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
        BattlePathMode::targeting);
    plan.target_slot = static_cast<std::int16_t>(target_slot);
    plan.target_distance = pathing.value(
        BattlePathCoord{target[combatant_word::x], target[combatant_word::y]});
    if (plan.area_type == 0 || plan.area_type == 3) {
        plan.movement_mode = 1;
        return plan.target_distance <= plan.select_distance;
    }
    if (plan.area_type == 1 || plan.area_type == 2) {
        plan.movement_mode = 2;
        return plan.target_distance <= plan.select_distance &&
            (target[combatant_word::x] == actor[combatant_word::x] ||
             target[combatant_word::y] == actor[combatant_word::y]);
    }
    plan.movement_mode = 0;
    return false;
}

std::optional<BattleAiAttackPlan> BattleSetup::begin_ai_attack_plan(
    const std::size_t actor_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    const auto magic_slot = automatic_magic_slot(actor_slot, random);
    const auto profile = attack_profile(actor_slot, magic_slot);
    if (!profile) {
        error_ = "battle AI attack profile is outside ranger records";
        return std::nullopt;
    }
    const auto special_attack_bonus = attack_special_bonus(actor_slot, magic_slot);
    if (!special_attack_bonus.has_value()) {
        error_ = "battle AI attack role is outside ranger records";
        return std::nullopt;
    }

    const auto target = choose_ai_attack_target(actor_slot, random);
    if (!target) {
        return std::nullopt;
    }
    if (!target->target_written || target->target_slot < 0 ||
        target->target_slot >= combatant_count_) {
        error_ = "battle AI attack target is outside combatant slots";
        return std::nullopt;
    }

    BattleAiAttackPlan plan{
        magic_slot,
        profile->magic_id,
        *special_attack_bonus,
        profile->select_distance,
        profile->area_type,
        target->target_slot,
        0,
        0,
        target->strategy,
        BattleAiAttackNextStep::finish,
        false};
    if (update_ai_attack_target_range(
            actor_slot, static_cast<std::size_t>(target->target_slot), plan)) {
        plan.next_step = BattleAiAttackNextStep::attack;
    } else if (combatants_[actor_slot].words[combatant_word::round_value] > 0) {
        plan.next_step = BattleAiAttackNextStep::move;
    }
    return plan;
}

std::optional<BattleAiAttackPlan> BattleSetup::resume_ai_attack_after_move(
    const std::size_t actor_slot,
    BattleAiAttackPlan plan) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        plan.next_step != BattleAiAttackNextStep::move || plan.target_slot < 0 ||
        plan.target_slot >= combatant_count_) {
        return std::nullopt;
    }
    const auto profile = attack_profile(actor_slot, plan.magic_slot);
    if (!profile || profile->magic_id != plan.magic_id ||
        profile->select_distance != plan.select_distance || profile->area_type != plan.area_type) {
        error_ = "battle AI attack plan no longer matches its magic profile";
        return std::nullopt;
    }
    if (update_ai_attack_target_range(
            actor_slot, static_cast<std::size_t>(plan.target_slot), plan)) {
        plan.next_step = BattleAiAttackNextStep::attack;
        return plan;
    }

    const auto reselected = choose_ai_nearest_target(actor_slot);
    if (!reselected || !*reselected) {
        error_ = "battle AI attack could not reselect a nearest target";
        return std::nullopt;
    }
    const auto target_slot = combatants_[actor_slot].words[combatant_word::ai_target];
    if (target_slot < 0 || target_slot >= combatant_count_) {
        error_ = "battle AI attack reselected target is outside combatant slots";
        return std::nullopt;
    }
    plan.target_strategy = BattleAiTargetStrategy::nearest;
    plan.target_reselected = true;
    plan.next_step = update_ai_attack_target_range(
        actor_slot, static_cast<std::size_t>(target_slot), plan)
        ? BattleAiAttackNextStep::attack
        : BattleAiAttackNextStep::rest;
    return plan;
}

std::optional<bool> BattleSetup::choose_ai_strongest_poison_target(
    const std::size_t actor_slot) {
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    const auto actor_use_poison = ranger_.roles[static_cast<std::size_t>(actor_role_id)].word(
        model::role_word::use_poison);
    std::int16_t best_attack = 0;
    bool written = false;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] == actor_side ||
            combatant[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatant[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        if (role.word(model::role_word::poison) >= 95 ||
            role.word(model::role_word::anti_poison) >= actor_use_poison) {
            continue;
        }
        const auto attack = role.word(model::role_word::attack);
        if (attack > best_attack) {
            best_attack = attack;
            combatants_[actor_slot].words[combatant_word::ai_poison_target] =
                static_cast<std::int16_t>(slot);
            written = true;
        }
    }
    return written;
}

std::optional<bool> BattleSetup::choose_ai_first_poison_target(
    const std::size_t actor_slot,
    const std::size_t stale_target_slot,
    std::int16_t& stale_target_distance) {
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    const auto actor_use_poison = ranger_.roles[static_cast<std::size_t>(actor_role_id)].word(
        model::role_word::use_poison);
    std::int16_t best_distance = 1'000;
    bool written = false;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] == actor_side ||
            combatant[combatant_word::occupancy_hidden] != 0) {
            continue;
        }
        const auto role_id = combatant[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        if (role.word(model::role_word::poison) >= 95 ||
            role.word(model::role_word::anti_poison) >= actor_use_poison) {
            continue;
        }
        if (stale_target_slot >= static_cast<std::size_t>(combatant_count_)) {
            return std::nullopt;
        }
        const auto& actor = combatants_[actor_slot].words;
        const auto& stale_target = combatants_[stale_target_slot].words;
        BattlePathing pathing{data_};
        pathing.build(
            BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
            BattlePathMode::targeting);
        stale_target_distance = pathing.value(BattlePathCoord{
            stale_target[combatant_word::x], stale_target[combatant_word::y]});
        if (stale_target_distance < best_distance) {
            best_distance = stale_target_distance;
            combatants_[actor_slot].words[combatant_word::ai_poison_target] =
                static_cast<std::int16_t>(slot);
            written = true;
        }
    }
    return written;
}

std::optional<BattleAiPoisonTargetSelection> BattleSetup::choose_ai_poison_target(
    const std::size_t actor_slot,
    const std::size_t stale_target_slot,
    random::LegacyRandom& random) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    combatants_[actor_slot].words[combatant_word::ai_poison_target] = -1;
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (actor_role_id < 0 || static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& actor_role = ranger_.roles[static_cast<std::size_t>(actor_role_id)];
    if (actor_role.word(model::role_word::iq) > 60 && random.bounded(10) < 7) {
        const auto written = choose_ai_strongest_poison_target(actor_slot);
        if (!written) {
            return std::nullopt;
        }
        if (*written) {
            return BattleAiPoisonTargetSelection{
                combatants_[actor_slot].words[combatant_word::ai_poison_target],
                0,
                BattleAiPoisonTargetStrategy::strongest_attack,
                true};
        }
    }

    std::int16_t stale_target_distance = 0;
    const auto written = choose_ai_first_poison_target(
        actor_slot, stale_target_slot, stale_target_distance);
    if (!written) {
        return std::nullopt;
    }
    return BattleAiPoisonTargetSelection{
        static_cast<std::int16_t>(
            *written ? combatants_[actor_slot].words[combatant_word::ai_poison_target] : -1),
        stale_target_distance,
        *written ? BattleAiPoisonTargetStrategy::first_eligible_stale_distance
                 : BattleAiPoisonTargetStrategy::none,
        *written};
}

bool BattleSetup::update_ai_poison_target_range(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    BattleAiPoisonPlan& plan) const {
    const auto& actor = combatants_[actor_slot].words;
    const auto& target = combatants_[target_slot].words;
    BattlePathing pathing{data_};
    pathing.build(
        BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
        BattlePathMode::targeting);
    plan.target_slot = static_cast<std::int16_t>(target_slot);
    plan.target_distance = pathing.value(
        BattlePathCoord{target[combatant_word::x], target[combatant_word::y]});
    plan.range_check_count = static_cast<std::int16_t>(plan.range_check_count + 1);
    return plan.target_distance <= plan.targeting_range;
}

bool BattleSetup::update_ai_poison_fallback(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    BattleAiPoisonPlan& plan) {
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    std::int16_t allied_total = 0;
    std::int16_t allied_count = 0;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] != actor_side) {
            continue;
        }
        const auto role_id = combatant[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            error_ = "battle AI poison ally is outside ranger records";
            return false;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        allied_total = wrapping_i16(
            static_cast<std::int32_t>(allied_total) + role.word(model::role_word::attack));
        allied_total = wrapping_i16(
            static_cast<std::int32_t>(allied_total) + role.word(model::role_word::hp));
        allied_count = wrapping_i16(static_cast<std::int32_t>(allied_count) + 1);
    }
    if (allied_count == 0) {
        error_ = "battle AI poison has no allied combatants";
        return false;
    }
    const auto target_role_id = combatants_[target_slot].words[combatant_word::role_id];
    if (target_role_id < 0 ||
        static_cast<std::size_t>(target_role_id) >= ranger_.roles.size()) {
        error_ = "battle AI poison target is outside ranger records";
        return false;
    }
    plan.allied_total = allied_total;
    plan.allied_count = allied_count;
    plan.doubled_target_attack = 2 * static_cast<std::int32_t>(
        ranger_.roles[static_cast<std::size_t>(target_role_id)].word(model::role_word::attack));
    plan.doubled_allied_average =
        2 * static_cast<std::int32_t>(allied_total) / allied_count;
    plan.next_step = plan.doubled_target_attack > plan.doubled_allied_average
        ? BattleAiPoisonNextStep::attack_fallback
        : BattleAiPoisonNextStep::rest;
    return true;
}

std::optional<BattleAiPoisonPlan> BattleSetup::begin_ai_poison_plan(
    const std::size_t actor_slot,
    const std::size_t stale_target_slot,
    random::LegacyRandom& random) {
    const auto selection = choose_ai_poison_target(actor_slot, stale_target_slot, random);
    if (!selection) {
        return std::nullopt;
    }
    BattleAiPoisonPlan plan{};
    plan.target_strategy = selection->strategy;
    if (!selection->target_written) {
        return plan;
    }
    const auto range = poison_targeting_range(actor_slot);
    if (!range || selection->target_slot < 0 || selection->target_slot >= combatant_count_) {
        error_ = "battle AI poison plan target is outside combatant slots";
        return std::nullopt;
    }
    plan.targeting_range = *range;
    const auto target_slot = static_cast<std::size_t>(selection->target_slot);
    const auto in_range = update_ai_poison_target_range(actor_slot, target_slot, plan);
    const auto round_value = combatants_[actor_slot].words[combatant_word::round_value];
    if (in_range && round_value == 0) {
        plan.next_step = BattleAiPoisonNextStep::poison;
    } else if (round_value > 0) {
        plan.next_step = BattleAiPoisonNextStep::move;
    } else if (update_ai_poison_target_range(actor_slot, target_slot, plan)) {
        plan.next_step = BattleAiPoisonNextStep::poison;
    } else if (!update_ai_poison_fallback(actor_slot, target_slot, plan)) {
        return std::nullopt;
    }
    return plan;
}

std::optional<BattleAiPoisonPlan> BattleSetup::resume_ai_poison_after_move(
    const std::size_t actor_slot,
    BattleAiPoisonPlan plan) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        plan.next_step != BattleAiPoisonNextStep::move || plan.target_slot < 0 ||
        plan.target_slot >= combatant_count_) {
        return std::nullopt;
    }
    const auto target_slot = static_cast<std::size_t>(plan.target_slot);
    if (update_ai_poison_target_range(actor_slot, target_slot, plan)) {
        plan.next_step = BattleAiPoisonNextStep::poison;
    } else if (!update_ai_poison_fallback(actor_slot, target_slot, plan)) {
        return std::nullopt;
    }
    return plan;
}

std::optional<std::int16_t> BattleSetup::ai_item_id(
    const std::size_t actor_slot,
    const BattleAiChoice& choice) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        choice.item_slot < 0) {
        return std::nullopt;
    }
    const auto& actor = combatants_[actor_slot].words;
    std::int16_t item_id = -1;
    if (actor[combatant_word::side] == 0) {
        if (choice.item_source != BattleAiItemSource::inventory ||
            static_cast<std::size_t>(choice.item_slot) >= model::kInventoryCount) {
            return std::nullopt;
        }
        item_id = ranger_.header.inventory_item(
            static_cast<std::size_t>(choice.item_slot)).value;
    } else {
        if (choice.item_source != BattleAiItemSource::carried ||
            static_cast<std::size_t>(choice.item_slot) >= model::role_word::taking_item_count) {
            return std::nullopt;
        }
        const auto role_id = actor[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return std::nullopt;
        }
        item_id = ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::taking_item_begin + static_cast<std::size_t>(choice.item_slot));
    }
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
        return std::nullopt;
    }
    return item_id;
}

std::optional<BattleAiItemPlan> BattleSetup::begin_ai_item_plan(
    const std::size_t actor_slot,
    const BattleAiChoice& choice) const {
    if (choice.action != BattleAiAction::item) {
        return std::nullopt;
    }
    const auto relocation = ai_escape_plan(actor_slot, false);
    const auto item_id = ai_item_id(actor_slot, choice);
    if (!relocation || !item_id) {
        return std::nullopt;
    }
    BattleAiItemPlan plan{};
    plan.item_source = choice.item_source;
    plan.item_slot = choice.item_slot;
    plan.item_id = *item_id;
    plan.use_mode = 0;
    plan.target_slot = choice.target_slot;
    plan.relocation_destination = relocation->destination;
    plan.maximum_enemy_distance_sum = relocation->maximum_enemy_distance_sum;
    plan.movement_mode = 0;
    plan.next_step = plan.relocation_destination.has_value()
        ? BattleAiItemNextStep::move
        : BattleAiItemNextStep::use_item;
    return plan;
}

std::optional<BattleAiItemPlan> BattleSetup::resume_ai_item_after_relocation(
    const std::size_t actor_slot,
    BattleAiItemPlan plan) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        plan.use_mode != 0 || plan.next_step != BattleAiItemNextStep::move ||
        !plan.relocation_destination.has_value()) {
        return std::nullopt;
    }
    plan.next_step = BattleAiItemNextStep::use_item;
    return plan;
}

bool BattleSetup::update_ai_throwing_weapon_target_range(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    BattleAiItemPlan& plan) const {
    const auto& actor = combatants_[actor_slot].words;
    const auto& target = combatants_[target_slot].words;
    BattlePathing pathing{data_};
    pathing.build(
        BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
        BattlePathMode::targeting);
    plan.target_slot = static_cast<std::int16_t>(target_slot);
    plan.target_distance = pathing.value(
        BattlePathCoord{target[combatant_word::x], target[combatant_word::y]});
    plan.range_check_count = static_cast<std::int16_t>(plan.range_check_count + 1);
    return plan.target_distance <= plan.targeting_range;
}

std::optional<BattleAiItemPlan> BattleSetup::begin_ai_throwing_weapon_plan(
    const std::size_t actor_slot,
    const BattleAiChoice& choice,
    random::LegacyRandom& random) {
    if (choice.action != BattleAiAction::throwing_weapon) {
        return std::nullopt;
    }
    const auto target = choose_ai_attack_target(actor_slot, random);
    const auto range = throwing_weapon_targeting_range(actor_slot);
    const auto item_id = ai_item_id(actor_slot, choice);
    if (!target || !range || !item_id || target->target_slot < 0 ||
        target->target_slot >= combatant_count_) {
        error_ = "battle AI throwing-weapon plan target or item is outside legacy records";
        return std::nullopt;
    }

    BattleAiItemPlan plan{};
    plan.item_source = choice.item_source;
    plan.item_slot = choice.item_slot;
    plan.item_id = *item_id;
    plan.use_mode = 1;
    plan.target_slot = target->target_slot;
    plan.targeting_range = *range;
    plan.target_strategy = target->strategy;
    plan.target_written = target->target_written;
    const auto target_slot = static_cast<std::size_t>(target->target_slot);
    if (update_ai_throwing_weapon_target_range(actor_slot, target_slot, plan)) {
        plan.next_step = BattleAiItemNextStep::use_item;
    } else if (combatants_[actor_slot].words[combatant_word::round_value] > 0) {
        plan.next_step = BattleAiItemNextStep::move;
    } else if (update_ai_throwing_weapon_target_range(actor_slot, target_slot, plan)) {
        plan.next_step = BattleAiItemNextStep::use_item;
    } else {
        plan.next_step = BattleAiItemNextStep::attack_fallback;
    }
    return plan;
}

std::optional<BattleAiItemPlan> BattleSetup::resume_ai_throwing_weapon_after_move(
    const std::size_t actor_slot,
    BattleAiItemPlan plan) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        plan.use_mode != 1 || plan.next_step != BattleAiItemNextStep::move ||
        plan.target_slot < 0 || plan.target_slot >= combatant_count_) {
        return std::nullopt;
    }
    const auto target_slot = static_cast<std::size_t>(plan.target_slot);
    plan.next_step = update_ai_throwing_weapon_target_range(actor_slot, target_slot, plan)
        ? BattleAiItemNextStep::use_item
        : BattleAiItemNextStep::attack_fallback;
    return plan;
}

std::optional<BattleAiRequestPlan> BattleSetup::begin_ai_request_plan(
    const std::size_t actor_slot,
    const BattleAiChoice& choice) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        (choice.action != BattleAiAction::request_medicine &&
         choice.action != BattleAiAction::request_detox) ||
        choice.target_slot < 0 || choice.target_slot >= combatant_count_) {
        return std::nullopt;
    }
    const auto& target = combatants_[static_cast<std::size_t>(choice.target_slot)].words;
    BattleAiRequestPlan plan{};
    plan.request_action = choice.action;
    plan.target_slot = choice.target_slot;
    plan.target = BattlePathCoord{
        target[combatant_word::x],
        target[combatant_word::y],
    };
    plan.movement_mode = 0;
    plan.movement_value = 0;
    plan.next_step = combatants_[actor_slot].words[combatant_word::round_value] > 0
        ? BattleAiRequestNextStep::move
        : BattleAiRequestNextStep::automatic_attack;
    return plan;
}

std::optional<BattleAiRequestPlan> BattleSetup::resume_ai_request_after_move(
    const std::size_t actor_slot,
    BattleAiRequestPlan plan) const noexcept {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        plan.next_step != BattleAiRequestNextStep::move || plan.target_slot < 0 ||
        plan.target_slot >= combatant_count_ ||
        (plan.request_action != BattleAiAction::request_medicine &&
         plan.request_action != BattleAiAction::request_detox)) {
        return std::nullopt;
    }
    const auto& target = combatants_[static_cast<std::size_t>(plan.target_slot)].words;
    plan.target = BattlePathCoord{
        target[combatant_word::x],
        target[combatant_word::y],
    };
    plan.next_step = BattleAiRequestNextStep::automatic_attack;
    return plan;
}

bool BattleSetup::update_ai_support_target_range(
    const std::size_t actor_slot,
    const std::size_t target_slot,
    BattleAiSupportPlan& plan) const {
    const auto& actor = combatants_[actor_slot].words;
    const auto& target = combatants_[target_slot].words;
    BattlePathing pathing{data_};
    pathing.build(
        BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
        BattlePathMode::targeting);
    plan.target_slot = static_cast<std::int16_t>(target_slot);
    plan.target = BattlePathCoord{
        target[combatant_word::x],
        target[combatant_word::y],
    };
    plan.target_distance = pathing.value(plan.target);
    plan.range_check_count = static_cast<std::int16_t>(plan.range_check_count + 1);
    return plan.targeting_range >= plan.target_distance;
}

bool BattleSetup::update_ai_support_fallback(
    const std::size_t actor_slot,
    BattleAiSupportPlan& plan) {
    const auto actor_side = combatants_[actor_slot].words[combatant_word::side];
    std::int16_t allied_total = 0;
    std::int16_t allied_count = 0;
    for (std::size_t slot = 0U; slot < static_cast<std::size_t>(combatant_count_); ++slot) {
        const auto& combatant = combatants_[slot].words;
        if (combatant[combatant_word::side] != actor_side) {
            continue;
        }
        const auto role_id = combatant[combatant_word::role_id];
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            error_ = "battle AI support ally is outside ranger records";
            return false;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        allied_total = wrapping_i16(
            static_cast<std::int32_t>(allied_total) + role.word(model::role_word::attack));
        allied_total = wrapping_i16(
            static_cast<std::int32_t>(allied_total) + role.word(model::role_word::hp));
        allied_count = wrapping_i16(static_cast<std::int32_t>(allied_count) + 1);
    }
    const auto actor_role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (allied_count == 0 || actor_role_id < 0 ||
        static_cast<std::size_t>(actor_role_id) >= ranger_.roles.size()) {
        error_ = "battle AI support fallback state is invalid";
        return false;
    }
    plan.allied_total = allied_total;
    plan.allied_count = allied_count;
    plan.doubled_actor_attack = 2 * static_cast<std::int32_t>(
        ranger_.roles[static_cast<std::size_t>(actor_role_id)].word(model::role_word::attack));
    plan.doubled_allied_average =
        2 * static_cast<std::int32_t>(allied_total) / allied_count;
    plan.next_step = plan.doubled_actor_attack > plan.doubled_allied_average
        ? BattleAiSupportNextStep::automatic_attack
        : BattleAiSupportNextStep::rest;
    return true;
}

std::optional<BattleAiSupportPlan> BattleSetup::begin_ai_support_plan(
    const std::size_t actor_slot,
    const BattleAiChoice& choice) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        (choice.action != BattleAiAction::medicine &&
         choice.action != BattleAiAction::detox) ||
        choice.target_slot < 0 || choice.target_slot >= combatant_count_) {
        return std::nullopt;
    }
    const auto targeting_range = choice.action == BattleAiAction::medicine
        ? medicine_targeting_range(actor_slot)
        : detox_targeting_range(actor_slot);
    if (!targeting_range) {
        return std::nullopt;
    }
    BattleAiSupportPlan plan{};
    plan.support_action = choice.action;
    plan.targeting_range = *targeting_range;
    plan.movement_mode = 1;
    plan.movement_value = *targeting_range;
    const auto target_slot = static_cast<std::size_t>(choice.target_slot);
    if (update_ai_support_target_range(actor_slot, target_slot, plan)) {
        plan.next_step = BattleAiSupportNextStep::apply_support;
        return plan;
    }
    if (combatants_[actor_slot].words[combatant_word::round_value] > 0) {
        plan.next_step = BattleAiSupportNextStep::move;
        return plan;
    }
    if (update_ai_support_target_range(actor_slot, target_slot, plan)) {
        plan.next_step = BattleAiSupportNextStep::apply_support;
    } else if (!update_ai_support_fallback(actor_slot, plan)) {
        return std::nullopt;
    }
    return plan;
}

std::optional<BattleAiSupportPlan> BattleSetup::resume_ai_support_after_move(
    const std::size_t actor_slot,
    BattleAiSupportPlan plan) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        plan.next_step != BattleAiSupportNextStep::move || plan.target_slot < 0 ||
        plan.target_slot >= combatant_count_ ||
        (plan.support_action != BattleAiAction::medicine &&
         plan.support_action != BattleAiAction::detox)) {
        return std::nullopt;
    }
    const auto target_slot = static_cast<std::size_t>(plan.target_slot);
    if (update_ai_support_target_range(actor_slot, target_slot, plan)) {
        plan.next_step = BattleAiSupportNextStep::apply_support;
    } else if (!update_ai_support_fallback(actor_slot, plan)) {
        return std::nullopt;
    }
    return plan;
}

std::optional<BattleCursorSelectionState> BattleSetup::begin_cursor_selection(
    const std::size_t actor_slot,
    const std::int16_t path_limit,
    const BattleCursorSelectionMode mode) const {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        (mode != BattleCursorSelectionMode::movement &&
         mode != BattleCursorSelectionMode::targeting)) {
        return std::nullopt;
    }
    const auto& actor = combatants_[actor_slot].words;
    BattleCursorSelectionState state{data_};
    state.actor_slot = actor_slot;
    state.source = BattlePathCoord{
        actor[combatant_word::x],
        actor[combatant_word::y],
    };
    state.cursor = state.source;
    state.path_limit = path_limit;
    state.mode = mode;
    state.pathing.build(
        state.source,
        mode == BattleCursorSelectionMode::movement ? BattlePathMode::movement
                                                    : BattlePathMode::targeting);
    return state;
}

BattleCursorSelectionResult BattleSetup::apply_cursor_selection(
    BattleCursorSelectionState& state,
    const BattleCursorSelectionAction action) const noexcept {
    if (!valid() || state.complete ||
        state.actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        (state.mode != BattleCursorSelectionMode::movement &&
         state.mode != BattleCursorSelectionMode::targeting)) {
        return BattleCursorSelectionResult::invalid;
    }

    if (action == BattleCursorSelectionAction::cancel) {
        state.path_limit = 0;
        state.cancelled = true;
        state.complete = true;
        return BattleCursorSelectionResult::cancelled;
    }
    if (action == BattleCursorSelectionAction::activate) {
        const auto index = legacy_cursor_index(state.cursor);
        if (!index) {
            return BattleCursorSelectionResult::unchanged;
        }
        const auto path_value = state.pathing.values()[*index];
        const auto selectable = path_value <= state.path_limit &&
            ((state.mode == BattleCursorSelectionMode::movement && path_value > 0) ||
             (state.mode == BattleCursorSelectionMode::targeting && path_value >= 0));
        if (!selectable) {
            return BattleCursorSelectionResult::unchanged;
        }
        state.selected = true;
        state.complete = true;
        return BattleCursorSelectionResult::selected;
    }

    BattlePathCoord direction{};
    switch (action) {
    case BattleCursorSelectionAction::down:
        direction.y = 1;
        break;
    case BattleCursorSelectionAction::right:
        direction.x = 1;
        break;
    case BattleCursorSelectionAction::left:
        direction.x = -1;
        break;
    case BattleCursorSelectionAction::up:
        direction.y = -1;
        break;
    case BattleCursorSelectionAction::cancel:
    case BattleCursorSelectionAction::activate:
        return BattleCursorSelectionResult::invalid;
    }

    const BattlePathCoord candidate{
        static_cast<std::int16_t>(state.cursor.x + direction.x),
        static_cast<std::int16_t>(state.cursor.y + direction.y),
    };
    const auto index = legacy_cursor_index(candidate);
    if (!index) {
        return BattleCursorSelectionResult::unchanged;
    }
    const auto path_value = state.pathing.values()[*index];
    if (path_value > state.path_limit && data_.occupancy()[*index] == -1) {
        return BattleCursorSelectionResult::unchanged;
    }
    state.cursor = candidate;
    return BattleCursorSelectionResult::moved;
}

std::optional<BattleCursorSelectionState> BattleSetup::begin_player_movement_selection(
    const std::size_t actor_slot) const {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    return begin_cursor_selection(
        actor_slot,
        combatants_[actor_slot].words[combatant_word::round_value],
        BattleCursorSelectionMode::movement);
}

std::optional<BattlePlayerMovementPlan> BattleSetup::finish_player_movement_selection(
    const BattleCursorSelectionState& state) const {
    if (!valid() || state.actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        state.mode != BattleCursorSelectionMode::movement || !state.complete ||
        !state.selected || state.cancelled) {
        return std::nullopt;
    }
    const auto& actor = combatants_[state.actor_slot].words;
    if (state.source != BattlePathCoord{
                            actor[combatant_word::x],
                            actor[combatant_word::y]}) {
        return std::nullopt;
    }

    BattlePlayerMovementPlan plan{state.pathing};
    plan.actor_slot = state.actor_slot;
    plan.source = state.source;
    plan.destination = state.cursor;
    plan.path_marked = plan.pathing.mark_shortest_path(plan.source, plan.destination);
    plan.complete = !plan.path_marked;
    return plan;
}

std::optional<BattleAiMovementStep> BattleSetup::advance_player_movement(
    BattlePlayerMovementPlan& plan) {
    if (!valid() || plan.complete || !plan.path_marked ||
        plan.actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        return std::nullopt;
    }
    auto& actor = combatants_[plan.actor_slot].words;
    const BattlePathCoord from{
        actor[combatant_word::x],
        actor[combatant_word::y],
    };
    const auto moved = move_one_marked_step(plan.pathing, plan.actor_slot);
    if (!moved) {
        plan.complete = true;
        return std::nullopt;
    }
    plan.step_count = wrapping_i16(static_cast<std::int32_t>(plan.step_count) + 1);
    plan.complete = *moved == plan.destination || actor[combatant_word::round_value] <= 0;

    const auto role_id = actor[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    return BattleAiMovementStep{
        .from = from,
        .to = *moved,
        .remaining_round_value = actor[combatant_word::round_value],
        .physical_power = ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::physical_power),
        .view_center_x = moved->x,
        .view_center_y = moved->y,
        .view_x = static_cast<std::int16_t>(std::clamp<std::int32_t>(moved->x - 11, 0, 32)),
        .view_y = static_cast<std::int16_t>(std::clamp<std::int32_t>(moved->y - 11, 0, 32)),
        .moved = true,
        .render_required = true,
        .present_required = true,
        .complete = plan.complete,
    };
}

std::optional<BattleAiMovementPlan> BattleSetup::begin_ai_movement_plan(
    const std::size_t actor_slot,
    const std::int16_t target_slot,
    const BattlePathCoord requested_target,
    const std::int16_t mode,
    const std::int16_t range) const {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) || mode < 0 ||
        mode > 3 || requested_target.x < 0 ||
        requested_target.x >= static_cast<std::int16_t>(kBattleExtent) ||
        requested_target.y < 0 ||
        requested_target.y >= static_cast<std::int16_t>(kBattleExtent) ||
        ((mode == 1 || mode == 2) &&
         (target_slot < 0 || target_slot >= combatant_count_)) ||
        (target_slot >= combatant_count_)) {
        return std::nullopt;
    }
    if (mode == 3 && range < 0) {
        return std::nullopt;
    }

    const auto& actor = combatants_[actor_slot].words;
    const BattlePathCoord source{
        actor[combatant_word::x],
        actor[combatant_word::y],
    };
    BattleAiMovementPlan plan{data_};
    plan.actor_slot = actor_slot;
    plan.target_slot = target_slot;
    plan.requested_target = requested_target;
    plan.source = source;
    plan.destination = requested_target;
    plan.mode = mode;
    plan.range = range;

    plan.pathing.build(requested_target, BattlePathMode::targeting);
    plan.preliminary_target_distance = plan.pathing.value(source);
    plan.preliminary_within_turn_range =
        static_cast<std::int32_t>(plan.preliminary_target_distance) -
            actor[combatant_word::round_value] <=
        range;

    const auto select_range_layer = [&](const bool require_alignment) {
        plan.pathing.build(requested_target, BattlePathMode::movement);
        plan.movement_map_build_count = wrapping_i16(
            static_cast<std::int32_t>(plan.movement_map_build_count) + 1);
        auto layer = range;
        for (std::size_t iteration = 0U; iteration <=
                static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max());
             ++iteration) {
            bool found = false;
            std::int16_t best_distance = 1000;
            BattlePathCoord best{};
            for (std::int16_t x = 0; x < static_cast<std::int16_t>(kBattleExtent); ++x) {
                for (std::int16_t y = 0; y < static_cast<std::int16_t>(kBattleExtent); ++y) {
                    const BattlePathCoord candidate{x, y};
                    if (plan.pathing.value(candidate) != layer ||
                        (require_alignment && candidate.x != requested_target.x &&
                         candidate.y != requested_target.y)) {
                        continue;
                    }
                    found = true;
                    const auto distance = static_cast<std::int16_t>(
                        std::abs(static_cast<std::int32_t>(candidate.x) - source.x) +
                        std::abs(static_cast<std::int32_t>(candidate.y) - source.y));
                    if (distance < best_distance) {
                        best = candidate;
                        best_distance = distance;
                    }
                }
            }
            if (found) {
                plan.destination = best;
                plan.selected_distance_layer = layer;
                return true;
            }
            layer = wrapping_i16(static_cast<std::int32_t>(layer) - 1);
            if (layer == 0) {
                return false;
            }
        }
        return false;
    };

    const auto use_aligned_layer = mode == 2 && plan.preliminary_within_turn_range;
    const auto use_range_layer = mode == 3;
    if (use_aligned_layer) {
        if (range < 0) {
            return std::nullopt;
        }
        plan.selection = BattleAiMovementSelection::aligned_range_layer;
        static_cast<void>(select_range_layer(true));
    } else if (use_range_layer) {
        plan.selection = BattleAiMovementSelection::range_layer;
        static_cast<void>(select_range_layer(false));
    } else {
        plan.selection = BattleAiMovementSelection::generic_reachable_neighbor;
        plan.pathing.build(source, BattlePathMode::movement);
        plan.movement_map_build_count = wrapping_i16(
            static_cast<std::int32_t>(plan.movement_map_build_count) + 1);
        auto cursor = requested_target;
        for (std::size_t iteration = 0U; iteration < kBattleOccupancyCells; ++iteration) {
            bool found = false;
            for (const auto direction : kLegacyPathDirections) {
                const BattlePathCoord candidate{
                    static_cast<std::int16_t>(cursor.x + direction.x),
                    static_cast<std::int16_t>(cursor.y + direction.y),
                };
                plan.pathing.build(source, BattlePathMode::movement);
                plan.movement_map_build_count = wrapping_i16(
                    static_cast<std::int32_t>(plan.movement_map_build_count) + 1);
                if (plan.pathing.value(candidate) < 128 &&
                    (candidate.x == source.x || candidate.y == source.y)) {
                    cursor = candidate;
                    found = true;
                    break;
                }
            }
            if (!found) {
                for (const auto direction : kLegacyPathDirections) {
                    const BattlePathCoord candidate{
                        static_cast<std::int16_t>(cursor.x + direction.x),
                        static_cast<std::int16_t>(cursor.y + direction.y),
                    };
                    plan.pathing.build(source, BattlePathMode::movement);
                    plan.movement_map_build_count = wrapping_i16(
                        static_cast<std::int32_t>(plan.movement_map_build_count) + 1);
                    if (plan.pathing.value(candidate) < 128) {
                        cursor = candidate;
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                break;
            }
            if (cursor.x > source.x && cursor.x > 0) {
                cursor.x = static_cast<std::int16_t>(cursor.x - 1);
            } else if (cursor.x < source.x && cursor.x < 63) {
                cursor.x = static_cast<std::int16_t>(cursor.x + 1);
            } else if (cursor.y > source.y && cursor.y > 0) {
                cursor.y = static_cast<std::int16_t>(cursor.y - 1);
            } else if (cursor.y < source.y && cursor.y < 63) {
                cursor.y = static_cast<std::int16_t>(cursor.y + 1);
            }
            if (cursor == source) {
                break;
            }
        }
        plan.destination = cursor;
    }

    if (plan.destination == source) {
        plan.complete = true;
        return plan;
    }
    plan.first_reachability_passed = plan.pathing.value(plan.destination) < 128;
    if (!plan.first_reachability_passed) {
        plan.complete = true;
        return plan;
    }
    plan.pathing.build(source, BattlePathMode::movement);
    plan.movement_map_build_count = wrapping_i16(
        static_cast<std::int32_t>(plan.movement_map_build_count) + 1);
    plan.second_reachability_passed = plan.pathing.value(plan.destination) < 128;
    if (!plan.second_reachability_passed) {
        plan.complete = true;
        return plan;
    }
    plan.path_marked = plan.pathing.mark_shortest_path(source, plan.destination);
    plan.complete = !plan.path_marked;
    return plan;
}

std::optional<BattleAiMovementStep> BattleSetup::advance_ai_movement(
    BattleAiMovementPlan& plan) {
    if (!valid() || plan.complete || !plan.path_marked ||
        plan.actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        plan.mode < 0 || plan.mode > 3) {
        return std::nullopt;
    }
    auto& actor = combatants_[plan.actor_slot].words;
    const BattlePathCoord from{
        actor[combatant_word::x],
        actor[combatant_word::y],
    };
    const auto moved = move_one_marked_step(plan.pathing, plan.actor_slot);
    if (!moved) {
        plan.complete = true;
        return std::nullopt;
    }
    plan.step_count = wrapping_i16(static_cast<std::int32_t>(plan.step_count) + 1);

    auto complete = *moved == plan.destination ||
        actor[combatant_word::round_value] <= 0;
    if (!complete && (plan.mode == 1 || plan.mode == 2)) {
        if (plan.target_slot < 0 || plan.target_slot >= combatant_count_) {
            plan.complete = true;
            return std::nullopt;
        }
        const auto& target = combatants_[static_cast<std::size_t>(plan.target_slot)].words;
        const auto distance =
            std::abs(static_cast<std::int32_t>(target[combatant_word::x]) -
                     actor[combatant_word::x]) +
            std::abs(static_cast<std::int32_t>(target[combatant_word::y]) -
                     actor[combatant_word::y]);
        if (distance <= plan.range) {
            complete = plan.mode == 1 ||
                actor[combatant_word::x] == target[combatant_word::x] ||
                actor[combatant_word::y] == target[combatant_word::y];
        }
    }
    plan.complete = complete;

    const auto role_id = actor[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    return BattleAiMovementStep{
        .from = from,
        .to = *moved,
        .remaining_round_value = actor[combatant_word::round_value],
        .physical_power = ranger_.roles[static_cast<std::size_t>(role_id)].word(
            model::role_word::physical_power),
        .view_center_x = moved->x,
        .view_center_y = moved->y,
        .view_x = static_cast<std::int16_t>(std::clamp<std::int32_t>(moved->x - 11, 0, 32)),
        .view_y = static_cast<std::int16_t>(std::clamp<std::int32_t>(moved->y - 11, 0, 32)),
        .moved = true,
        .render_required = true,
        .present_required = true,
        .complete = complete,
    };
}

std::optional<std::size_t> BattleSetup::defer_turn_to_end(const std::size_t actor_slot) {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_)) {
        error_ = "battle wait actor is outside combatant slots";
        return std::nullopt;
    }
    auto slot = actor_slot;
    while (slot + 1U < static_cast<std::size_t>(combatant_count_)) {
        swap_combatants(slot, slot + 1U);
        ++slot;
    }
    return slot;
}

std::optional<BattleRenderPlan> BattleSetup::battle_render_plan(
    const BattleRenderState& state,
    const std::span<const std::int16_t> path_values) const {
    if (!valid() || state.view_x < 0 || state.view_x > 32 || state.view_y < 0 ||
        state.view_y > 32 ||
        (state.path_limit > 0 && path_values.size() != kBattleOccupancyCells)) {
        return std::nullopt;
    }

    BattleRenderPlan plan{};
    plan.commands.reserve(4'096U);
    const auto append_sprite = [&plan](
                                   const BattleRenderCommandKind kind,
                                   const std::int16_t map_x,
                                   const std::int16_t map_y,
                                   const std::int32_t screen_x,
                                   const std::int32_t screen_y,
                                   const std::int32_t sprite_id,
                                   const std::int16_t overlay_variant = 0,
                                   const std::int16_t style = 0,
                                   const std::int16_t value = 0) {
        plan.commands.push_back(BattleRenderCommand{
            kind,
            map_x,
            map_y,
            screen_x,
            screen_y,
            sprite_id,
            overlay_variant,
            style,
            value});
    };

    const auto field = data_.battlefield();
    for (std::int16_t local_x = 0; local_x < 32; ++local_x) {
        for (std::int16_t local_y = 0; local_y < 32; ++local_y) {
            const auto map_x = static_cast<std::int16_t>(local_x + state.view_x);
            const auto map_y = static_cast<std::int16_t>(local_y + state.view_y);
            const auto cell = static_cast<std::size_t>(map_y) * kBattleExtent +
                static_cast<std::size_t>(map_x);
            append_sprite(
                BattleRenderCommandKind::legacy_sprite,
                map_x,
                map_y,
                18 * static_cast<std::int32_t>(local_x) -
                    18 * static_cast<std::int32_t>(local_y) + 145,
                9 * static_cast<std::int32_t>(local_x) +
                    9 * static_cast<std::int32_t>(local_y) - 81,
                field[cell]);
        }
    }

    for (std::int16_t local_x = 0; local_x < 32; ++local_x) {
        for (std::int16_t local_y = 0; local_y < 32; ++local_y) {
            const auto map_x = static_cast<std::int16_t>(local_x + state.view_x);
            const auto map_y = static_cast<std::int16_t>(local_y + state.view_y);
            const auto cell = static_cast<std::size_t>(map_y) * kBattleExtent +
                static_cast<std::size_t>(map_x);
            const auto sprite_x = 18 * static_cast<std::int32_t>(local_x) -
                18 * static_cast<std::int32_t>(local_y) + 145;
            const auto overlay_x = sprite_x - 18;
            const auto screen_y = 9 * static_cast<std::int32_t>(local_x) +
                9 * static_cast<std::int32_t>(local_y) - 81;

            if (state.path_limit > 0) {
                if (path_values[cell] > state.path_limit &&
                    path_values[cell] != kBattlePathBlocked) {
                    append_sprite(
                        BattleRenderCommandKind::cursor_overlay,
                        map_x,
                        map_y,
                        overlay_x,
                        screen_y,
                        0,
                        0,
                        3);
                }
                if (map_x == state.primary_cursor.x && map_y == state.primary_cursor.y) {
                    append_sprite(
                        BattleRenderCommandKind::cursor_overlay,
                        map_x,
                        map_y,
                        overlay_x,
                        screen_y,
                        0,
                        state.primary_cursor_alternate ? 1 : 0,
                        state.primary_cursor_alternate ? 3 : 2);
                }
            }
            if (state.secondary_cursor_visible && map_x == state.secondary_cursor.x &&
                map_y == state.secondary_cursor.y) {
                append_sprite(
                    BattleRenderCommandKind::cursor_overlay,
                    map_x,
                    map_y,
                    overlay_x,
                    screen_y,
                    0,
                    1,
                    3);
            }

            const auto object_sprite = field[kBattleOccupancyCells + cell];
            if (object_sprite != 0 && object_sprite != 15'000) {
                append_sprite(
                    BattleRenderCommandKind::legacy_sprite,
                    map_x,
                    map_y,
                    sprite_x,
                    screen_y,
                    object_sprite);
            }

            const auto occupant = data_.occupancy()[cell];
            if (occupant >= 0) {
                if (occupant >= combatant_count_) {
                    return std::nullopt;
                }
                const auto combatant = static_cast<std::size_t>(occupant);
                const auto& words = combatants_[combatant].words;
                const auto role_id = words[combatant_word::role_id];
                if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
                    return std::nullopt;
                }
                const auto highlighted = state.highlight_enabled && attack_effects_[cell] == 1 &&
                    ranger_.roles[static_cast<std::size_t>(role_id)].word(model::role_word::hp) >= 0;
                if (highlighted) {
                    std::int16_t color = 0;
                    if (state.highlight_mode == 1) {
                        color = 255;
                    } else if (state.highlight_mode == 2) {
                        color = 47;
                    } else if (state.highlight_mode == 3) {
                        color = 78;
                    }
                    if (color != 0) {
                        append_sprite(
                            BattleRenderCommandKind::highlighted_sprite,
                            map_x,
                            map_y,
                            sprite_x,
                            screen_y,
                            words[combatant_word::sprite],
                            0,
                            color);
                    }
                } else {
                    append_sprite(
                        BattleRenderCommandKind::legacy_sprite,
                        map_x,
                        map_y,
                        sprite_x,
                        screen_y,
                        words[combatant_word::sprite]);
                }
            }

            if (state.effect_visible && attack_effects_[cell] == 1) {
                append_sprite(
                    BattleRenderCommandKind::legacy_sprite,
                    map_x,
                    map_y,
                    sprite_x,
                    screen_y,
                    2 * static_cast<std::int32_t>(state.effect_id) +
                        state.effect_frame_offset);
            }

            if (state.damage_kind > 0 && occupant >= 0 && attack_effects_[cell] == 1) {
                static constexpr std::array<std::uint16_t, 6> kDamageColors{
                    0x0000U, 0x1014U, 0x3032U, 0x9193U, 0x0705U, 0x5053U};
                static constexpr std::array<std::int16_t, 6> kDamageSigns{0, -1, -1, 1, 1, -1};
                if (state.damage_kind <= 5) {
                    const auto combatant = static_cast<std::size_t>(occupant);
                    append_sprite(
                        BattleRenderCommandKind::damage_text,
                        map_x,
                        map_y,
                        overlay_x,
                        9 * static_cast<std::int32_t>(local_x) +
                            9 * static_cast<std::int32_t>(local_y) - 141 -
                            2 * static_cast<std::int32_t>(state.damage_text_offset),
                        0,
                        kDamageSigns[static_cast<std::size_t>(state.damage_kind)],
                        wrapping_i16(kDamageColors[static_cast<std::size_t>(state.damage_kind)]),
                        combatants_[combatant].words[combatant_word::damage_value]);
                }
            }
        }
    }
    return plan;
}

void BattleSetup::clear_attack_effects() noexcept {
    std::ranges::fill(attack_effects_, static_cast<std::int16_t>(0));
}

std::optional<BattleAreaResult> BattleSetup::apply_attack_area(
    const std::size_t actor_slot,
    const std::int16_t magic_slot,
    const BattlePathCoord target,
    const std::int16_t special_attack_bonus,
    random::LegacyRandom& random,
    const BattleAttackProfile* const cached_area_profile) {
    const auto current_profile = attack_profile(actor_slot, magic_slot);
    if (!current_profile) {
        error_ = "battle attack area profile is outside ranger records";
        return std::nullopt;
    }
    const auto& area_profile =
        cached_area_profile == nullptr ? *current_profile : *cached_area_profile;
    if (area_profile.area_type != 0 && area_profile.area_type != 2 &&
        area_profile.area_type != 3) {
        error_ = "battle line attack area requires its direction handler";
        return std::nullopt;
    }
    if (area_profile.select_distance < 0 || area_profile.attack_distance < 0) {
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
                             hurt_type = area_profile.hurt_type,
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

    if (area_profile.area_type == 2) {
        const auto actor_x = actor_words[combatant_word::x];
        const auto actor_y = actor_words[combatant_word::y];
        for (std::int32_t distance = 1; distance <= area_profile.select_distance; ++distance) {
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

    const auto radius = static_cast<std::int32_t>(area_profile.attack_distance);
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
    random::LegacyRandom& random,
    const BattleAttackProfile* const cached_area_profile) {
    const auto current_profile = attack_profile(actor_slot, magic_slot);
    if (!current_profile) {
        error_ = "battle line attack profile is outside ranger records";
        return std::nullopt;
    }
    const auto& area_profile =
        cached_area_profile == nullptr ? *current_profile : *cached_area_profile;
    BattleAreaResult result{};
    if (direction < 0 || direction > 3 || area_profile.select_distance < 1) {
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
    for (std::int32_t distance = 1; distance <= area_profile.select_distance; ++distance) {
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

std::optional<BattleMagicAnimationPlan> BattleSetup::magic_animation_plan(
    const std::size_t actor_slot,
    const std::int16_t magic_slot,
    const std::int16_t fight_pointer_base) const {
    const auto profile = attack_profile(actor_slot, magic_slot);
    if (!profile) {
        return std::nullopt;
    }
    const auto& magic = ranger_.magics[static_cast<std::size_t>(profile->magic_id)];
    return magic_animation_plan(
        actor_slot,
        magic_slot,
        magic.word(model::magic_word::magic_type),
        magic.word(model::magic_word::effect_id),
        fight_pointer_base);
}

std::optional<BattleMagicAnimationPlan> BattleSetup::magic_animation_plan(
    const std::size_t actor_slot,
    const std::int16_t magic_slot,
    const std::int16_t magic_type,
    const std::int16_t effect_id,
    const std::int16_t fight_pointer_base) const {
    if (!valid() || actor_slot >= static_cast<std::size_t>(combatant_count_) ||
        magic_slot < 0 || static_cast<std::size_t>(magic_slot) >= model::role_word::magic_count ||
        magic_type < 0 || magic_type > 4 || effect_id < 0 ||
        static_cast<std::size_t>(effect_id) >= kBattleEffectFrameCounts.size() ||
        fight_pointer_base < 0) {
        return std::nullopt;
    }
    const auto role_id = combatants_[actor_slot].words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    const auto magic_id = role.word(
        model::role_word::magic_id_begin + static_cast<std::size_t>(magic_slot));
    if (magic_id < 0 || static_cast<std::size_t>(magic_id) >= ranger_.magics.size()) {
        return std::nullopt;
    }
    const auto& magic = ranger_.magics[static_cast<std::size_t>(magic_id)];
    const auto type_index = static_cast<std::size_t>(magic_type);
    const auto effect_index = static_cast<std::size_t>(effect_id);
    const auto actor_frame_count = role.word(model::role_word::frame_begin + type_index);
    const auto effect_start = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::frame_begin + 5U + type_index)) -
        1);
    const auto magic_dispatch_frame = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::frame_begin + 10U + type_index)) -
        1);
    const auto total_frames = wrapping_i16(
        static_cast<std::int32_t>(role.word(model::role_word::frame_begin + 5U + type_index)) +
        kBattleEffectFrameCounts[effect_index] - 1);

    auto sprite_base = static_cast<std::int32_t>(fight_pointer_base);
    for (std::int16_t index = 0; index < magic_type; ++index) {
        sprite_base += 4 * static_cast<std::int32_t>(
            role.word(model::role_word::frame_begin + static_cast<std::size_t>(index)));
    }
    auto effect_frame = static_cast<std::int16_t>(-2);
    for (std::int16_t index = 0; index < effect_id; ++index) {
        effect_frame = wrapping_i16(
            static_cast<std::int32_t>(effect_frame) +
            2 * kBattleEffectFrameCounts[static_cast<std::size_t>(index)]);
    }

    BattleMagicAnimationPlan plan{
        role.word(model::role_word::head_id),
        magic.word(model::magic_word::sound_id),
        effect_id,
        true,
        {}};
    if (total_frames > 0) {
        plan.frames.reserve(static_cast<std::size_t>(total_frames));
    }
    const auto direction = combatants_[actor_slot].words[combatant_word::initial_mode];
    auto current_sprite = combatants_[actor_slot].words[combatant_word::sprite];
    bool effect_visible = false;
    bool effect_dispatched = false;
    bool magic_dispatched = false;
    for (std::int32_t frame_index = 0;
         static_cast<std::int16_t>(frame_index) < total_frames;
         ++frame_index) {
        const auto legacy_frame = static_cast<std::int16_t>(frame_index);
        bool actor_sprite_updated = false;
        if (legacy_frame < actor_frame_count) {
            current_sprite = wrapping_i16(
                2 * static_cast<std::int32_t>(actor_frame_count) * direction +
                2 * sprite_base + 2 * frame_index);
            actor_sprite_updated = true;
        }
        bool dispatch_effect_sample = false;
        if (legacy_frame >= effect_start) {
            effect_visible = true;
            effect_frame = wrapping_i16(static_cast<std::int32_t>(effect_frame) + 2);
            if (!effect_dispatched) {
                effect_dispatched = true;
                dispatch_effect_sample = true;
            }
        }
        bool dispatch_magic_sample = false;
        if (legacy_frame >= magic_dispatch_frame && !magic_dispatched) {
            magic_dispatched = true;
            dispatch_magic_sample = true;
        }
        plan.frames.push_back(BattleMagicAnimationFrame{
            current_sprite,
            effect_frame,
            17,
            actor_sprite_updated,
            effect_visible,
            dispatch_magic_sample,
            dispatch_effect_sample});
    }
    return plan;
}

std::optional<BattleEffectAnimationPlan> BattleSetup::effect_animation_plan(
    const std::int16_t effect_id) {
    if (effect_id < 0 || static_cast<std::size_t>(effect_id) >= kBattleEffectFrameCounts.size()) {
        return std::nullopt;
    }
    const auto effect_index = static_cast<std::size_t>(effect_id);
    auto effect_frame = static_cast<std::int16_t>(0);
    for (std::size_t index = 0; index < effect_index; ++index) {
        effect_frame = wrapping_i16(
            static_cast<std::int32_t>(effect_frame) + 2 * kBattleEffectFrameCounts[index]);
    }
    BattleEffectAnimationPlan plan{13, effect_id, 100, true, true, true, {}};
    const auto frame_count = kBattleEffectFrameCounts[effect_index];
    plan.frames.reserve(static_cast<std::size_t>(frame_count));
    for (std::int16_t frame = 0; frame < frame_count; ++frame) {
        plan.frames.push_back(BattleMagicAnimationFrame{
            0,
            effect_frame,
            17,
            false,
            true,
            false,
            false});
        effect_frame = wrapping_i16(static_cast<std::int32_t>(effect_frame) + 2);
    }
    return plan;
}

std::array<BattleDamageAnimationFrame, 10> BattleSetup::damage_animation_frames(
    const bool suppress_flash) noexcept {
    std::array<BattleDamageAnimationFrame, 10> frames{};
    for (std::size_t index = 0; index < frames.size(); ++index) {
        frames[index] = BattleDamageAnimationFrame{
            static_cast<std::int16_t>(index),
            1,
            index < 4U && !suppress_flash};
    }
    return frames;
}

bool BattleSetup::refresh_combatant_sprites() noexcept {
    if (!valid()) {
        return false;
    }
    for (std::int16_t slot = 0; slot < combatant_count_; ++slot) {
        auto& words = combatants_[static_cast<std::size_t>(slot)].words;
        words[combatant_word::sprite] =
            sprite_word(words[combatant_word::role_id], words[combatant_word::initial_mode]);
    }
    return true;
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
