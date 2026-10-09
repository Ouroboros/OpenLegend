#include "openlegend/battle/battle_data.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

#include "openlegend/compat/byte_reader.hpp"
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

BattleData::BattleData(const resource::DataRoot& data_root, const std::int16_t battle_id)
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
    occupancy_.fill(-1);
}

}  // namespace openlegend::battle
