#include "openlegend/battle/battle_data.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "openlegend/battle/battle_pathing.hpp"
#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/model/experience.hpp"
#include "openlegend/resource/packed_archive.hpp"

namespace openlegend::battle {

ProgressionData load_progression_data(const resource::DataRoot& data_root) {
    ProgressionData result;
    constexpr std::size_t experience_table_offset = 0x4DF8EU;
    const auto executable = data_root.read("Z.DAT");
    if (!executable) {
        result.error = executable.error;
        return result;
    }
    if (executable.bytes.size() < experience_table_offset + result.thresholds.size() * 2U) {
        result.error = "Z.DAT is shorter than the original experience table";
        return result;
    }
    for (std::size_t index = 0U; index < result.thresholds.size(); ++index) {
        result.thresholds[index] = compat::read_u16le(
            executable.bytes, experience_table_offset + index * 2U);
    }
    if (!model::experience_thresholds_valid(result.thresholds)) {
        result.error = "Z.DAT experience thresholds are invalid";
        return result;
    }
    const auto& bytes = executable.bytes;
    if (bytes[0x250EFU] != 0x66U || bytes[0x250F0U] != 0x81U ||
        bytes[0x250F1U] != 0xB8U || compat::read_u32le(bytes, 0x250F2U) != 0x70170U ||
        bytes[0x250FAU] != 0x66U || bytes[0x250FBU] != 0xC7U ||
        bytes[0x250FCU] != 0x80U || compat::read_u32le(bytes, 0x250FDU) != 0x70170U ||
        compat::read_i16le(bytes, 0x250F6U) != compat::read_i16le(bytes, 0x25101U)) {
        result.error = "Z.DAT maximum HP rule instruction layout is invalid";
        return result;
    }
    result.original_maximum_hp = compat::read_i16le(bytes, 0x250F6U);
    if (result.original_maximum_hp <= 0) {
        result.error = "Z.DAT original maximum HP is not positive";
        return result;
    }
    // 同一物品效果函数中最大内力的上限比较与写回（角色最大内力字段0x701A0）。
    if (bytes[0x253B7U] != 0x66U || bytes[0x253B8U] != 0x81U ||
        bytes[0x253B9U] != 0xB8U || compat::read_u32le(bytes, 0x253BAU) != 0x701A0U ||
        bytes[0x253C2U] != 0x66U || bytes[0x253C3U] != 0xC7U ||
        bytes[0x253C4U] != 0x80U || compat::read_u32le(bytes, 0x253C5U) != 0x701A0U ||
        compat::read_i16le(bytes, 0x253BEU) != compat::read_i16le(bytes, 0x253C9U)) {
        result.error = "Z.DAT maximum MP rule instruction layout is invalid";
        return result;
    }
    result.original_maximum_mp = compat::read_i16le(bytes, 0x253BEU);
    if (result.original_maximum_mp <= 0) {
        result.error = "Z.DAT original maximum MP is not positive";
        return result;
    }
    if (bytes[0x35521U] != 0xBBU || bytes[0x35531U] != 0xBAU ||
        bytes[0x35556U] != 0xBBU || bytes[0x35566U] != 0xBAU ||
        bytes[0x3553BU] != 0x8DU || bytes[0x3553CU] != 0x56U ||
        bytes[0x35570U] != 0x01U || bytes[0x35571U] != 0xD2U ||
        bytes[0x34F1AU] != 0xC1U || bytes[0x34F1BU] != 0xE0U ||
        bytes[0x34F1DU] != 0xBDU || bytes[0x34F1CU] >= 31U ||
        compat::read_u32le(bytes, 0x35522U) != compat::read_u32le(bytes, 0x35557U) ||
        compat::read_u32le(bytes, 0x35532U) != compat::read_u32le(bytes, 0x35567U)) {
        result.error = "Z.DAT practice rule instruction layout is invalid";
        return result;
    }
    result.practice_rules = {
        .aptitude_base = std::bit_cast<std::int32_t>(compat::read_u32le(bytes, 0x35532U)),
        .aptitude_step = std::bit_cast<std::int32_t>(compat::read_u32le(bytes, 0x35522U)),
        .unlearned_level = std::bit_cast<std::int8_t>(bytes[0x3553DU]),
        .unassociated_first_level = 2,
        .reward_numerator = std::int64_t{1} << bytes[0x34F1CU],
        .reward_denominator = std::bit_cast<std::int32_t>(compat::read_u32le(bytes, 0x34F1EU)),
    };
    if (!model::practice_rules_valid(result.practice_rules)) {
        result.error = "Z.DAT practice rule parameters are invalid";
    }
    return result;
}

BattleData::BattleData(
    const resource::DataRoot& data_root, const std::int16_t battle_id,
    const std::int16_t merged_battle_id)
    : battle_id_(battle_id) {
    const auto war = data_root.read("WAR.STA");
    if (!war) {
        error_ = war.error;
        return;
    }
    if (war.bytes.size() % kBattleDefinitionBytes != 0U) {
        error_ = "WAR.STA size is not a multiple of 186 bytes";
        return;
    }
    if (battle_id < 0) {
        error_ = "battle id is outside WAR.STA";
        return;
    }
    const auto definition_index = static_cast<std::size_t>(battle_id);
    const auto definition_count = war.bytes.size() / kBattleDefinitionBytes;
    if (definition_index >= definition_count) {
        error_ = "battle id is outside WAR.STA";
        return;
    }
    const auto definition_offset = definition_index * kBattleDefinitionBytes;
    for (std::size_t word = 0U; word < definition_.size(); ++word) {
        definition_[word] = compat::read_i16le(war.bytes, definition_offset + word * 2U);
    }

    const auto archive = resource::PackedArchive::open(
        data_root.path() / "WARFLD.IDX", data_root.path() / "WARFLD.GRP");
    if (!archive.valid()) {
        error_ = archive.error();
        return;
    }
    const auto field_id = battlefield_id();
    if (field_id < 0 || static_cast<std::size_t>(field_id) >= archive.entry_count()) {
        error_ = "WAR battlefield id is outside WARFLD archive";
        return;
    }
    const auto field = archive.entry(static_cast<std::size_t>(field_id));
    if (field.size() < kBattlefieldBytes) {
        error_ = "WARFLD entry is shorter than 16384 bytes";
        return;
    }
    for (std::size_t word = 0U; word < battlefield_.size(); ++word) {
        battlefield_[word] = compat::read_i16le(field, word * 2U);
    }
    const auto experience_data = load_progression_data(data_root);
    if (!experience_data.error.empty()) {
        error_ = experience_data.error;
        return;
    }
    experience_thresholds_ = experience_data.thresholds;
    practice_rules_ = experience_data.practice_rules;
    original_maximum_hp_ = experience_data.original_maximum_hp;
    original_maximum_mp_ = experience_data.original_maximum_mp;
    occupancy_.fill(-1);
    // 并入失败（敌方槽位不足或找不到站位）时还原为原版战斗。
    if (merged_battle_id >= 0) {
        const auto original = definition_;
        if (!merge_enemies(war.bytes, merged_battle_id)) {
            definition_ = original;
            diagnostics::log_warning(
                "battle merge skipped id=" + std::to_string(battle_id) +
                " merged=" + std::to_string(merged_battle_id) +
                " reason=enemies do not fit this battlefield");
        }
    }
}

bool BattleData::merge_enemies(
    const std::span<const std::uint8_t> war, const std::int16_t merged_battle_id) {
    constexpr std::size_t kExperienceWord = 7U;
    constexpr std::size_t kPartyXBegin = 21U;
    constexpr std::size_t kPartyYBegin = 27U;
    constexpr std::size_t kPartySlots = 6U;
    constexpr std::size_t kEnemyBegin = 33U;
    constexpr std::size_t kEnemyXBegin = 53U;
    constexpr std::size_t kEnemyYBegin = 73U;
    constexpr std::size_t kEnemySlots = 20U;
    const auto offset = static_cast<std::size_t>(merged_battle_id) * kBattleDefinitionBytes;
    if (offset + kBattleDefinitionBytes > war.size()) {
        return false;
    }
    const auto source_word = [&war, offset](const std::size_t word) {
        return compat::read_i16le(war, offset + word * 2U);
    };
    // 已占用格：本战我方与敌方的预设站位。
    std::array<bool, kBattleOccupancyCells> used{};
    const auto mark = [&used](const std::int16_t x, const std::int16_t y) {
        if (x >= 0 && y >= 0 && x < static_cast<std::int16_t>(kBattleExtent) &&
            y < static_cast<std::int16_t>(kBattleExtent)) {
            used[static_cast<std::size_t>(y) * kBattleExtent + static_cast<std::size_t>(x)] = true;
        }
    };
    for (std::size_t slot = 0U; slot < kPartySlots; ++slot) {
        mark(definition_[kPartyXBegin + slot], definition_[kPartyYBegin + slot]);
    }
    for (std::size_t slot = 0U; slot < kEnemySlots; ++slot) {
        if (definition_[kEnemyBegin + slot] >= 0) {
            mark(definition_[kEnemyXBegin + slot], definition_[kEnemyYBegin + slot]);
        }
    }
    const auto standable = [this, &used](const int x, const int y) {
        if (x < 0 || y < 0 || x >= static_cast<int>(kBattleExtent) ||
            y >= static_cast<int>(kBattleExtent)) {
            return false;
        }
        const auto cell = static_cast<std::size_t>(y) * kBattleExtent + static_cast<std::size_t>(x);
        return !used[cell] && battlefield_[kBattleOccupancyCells + cell] == 0 &&
            !battle_ground_blocked(battlefield_[cell]);
    };
    std::size_t free_slot = 0U;
    for (std::size_t slot = 0U; slot < kEnemySlots; ++slot) {
        const auto role_id = source_word(kEnemyBegin + slot);
        if (role_id < 0) {
            continue;
        }
        while (free_slot < kEnemySlots && definition_[kEnemyBegin + free_slot] >= 0) {
            ++free_slot;
        }
        if (free_slot >= kEnemySlots) {
            return false;
        }
        // 以对方原站位外移两格为起点（上半区往上、下半区往下），由近及远找可站立的空格。
        const auto origin_x = static_cast<int>(source_word(kEnemyXBegin + slot));
        const auto origin_y = static_cast<int>(source_word(kEnemyYBegin + slot));
        const auto start_y = origin_y + (origin_y <= 26 ? -2 : 2);
        std::optional<std::pair<int, int>> found;
        for (int radius = 0; radius <= 8 && !found.has_value(); ++radius) {
            for (int dy = -radius; dy <= radius && !found.has_value(); ++dy) {
                for (int dx = -radius; dx <= radius && !found.has_value(); ++dx) {
                    if (std::abs(dx) + std::abs(dy) == radius &&
                        standable(origin_x + dx, start_y + dy)) {
                        found = std::pair{origin_x + dx, start_y + dy};
                    }
                }
            }
        }
        if (!found.has_value()) {
            return false;
        }
        definition_[kEnemyBegin + free_slot] = role_id;
        definition_[kEnemyXBegin + free_slot] = static_cast<std::int16_t>(found->first);
        definition_[kEnemyYBegin + free_slot] = static_cast<std::int16_t>(found->second);
        mark(definition_[kEnemyXBegin + free_slot], definition_[kEnemyYBegin + free_slot]);
    }
    definition_[kExperienceWord] = static_cast<std::int16_t>(std::min<std::int32_t>(
        static_cast<std::int32_t>(definition_[kExperienceWord]) + source_word(kExperienceWord),
        std::numeric_limits<std::int16_t>::max()));
    return true;
}

}  // namespace openlegend::battle
