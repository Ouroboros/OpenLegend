#include "openlegend/battle/battle_data.hpp"

#include <cstddef>
#include <cstdint>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/model/experience.hpp"
#include "openlegend/resource/packed_archive.hpp"

namespace openlegend::battle {

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
    constexpr std::size_t experience_table_offset = 0x4DF8EU;
    const auto executable = data_root.read("Z.DAT");
    if (!executable) {
        error_ = executable.error;
        return;
    }
    if (executable.bytes.size() < experience_table_offset + experience_thresholds_.size() * 2U) {
        error_ = "Z.DAT is shorter than the original experience table";
        return;
    }
    for (std::size_t index = 0U; index < experience_thresholds_.size(); ++index) {
        experience_thresholds_[index] = compat::read_u16le(
            executable.bytes, experience_table_offset + index * 2U);
    }
    if (!model::experience_thresholds_valid(experience_thresholds_)) {
        error_ = "Z.DAT experience thresholds are invalid";
        return;
    }
    occupancy_.fill(-1);
}

}  // namespace openlegend::battle
