#include "openlegend/model/runtime_snapshot.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

#include "openlegend/attributes.hpp"
#include "openlegend/text/big5.hpp"

namespace openlegend::model {
namespace {

NODISCARD bool reference_valid(
    const std::int16_t reference, const std::size_t count) noexcept {
    return reference == -1 ||
        (reference >= 0 && static_cast<std::size_t>(reference) < count);
}

NODISCARD bool header_valid(const RuntimeRangerState& ranger) noexcept {
    const auto& header = ranger.header;
    if (header.word(header_word::in_ship) < 0 || header.word(header_word::in_ship) > 1 ||
        header.word(header_word::in_sub_map) < 0 ||
        header.word(header_word::in_sub_map) > 1 ||
        header.word(header_word::face_towards) < 0 ||
        header.word(header_word::face_towards) > 3 ||
        header.word(header_word::encode) < 0 || header.word(header_word::encode) > 3) {
        return false;
    }
    std::array<bool, kRoleCount> members{};
    for (std::size_t slot = 0U; slot < kTeamMemberCount; ++slot) {
        const auto role_id = header.team_member(slot).value;
        if (!reference_valid(role_id, ranger.roles.size())) {
            return false;
        }
        if (role_id >= 0) {
            const auto index = static_cast<std::size_t>(role_id);
            if (members[index]) {
                return false;
            }
            members[index] = true;
        }
    }
    auto empty_tail = false;
    for (std::size_t slot = 0U; slot < kInventoryCount; ++slot) {
        const auto item_id = header.inventory_item(slot).value;
        const auto count = header.inventory_count(slot);
        if (!reference_valid(item_id, ranger.items.size())) {
            return false;
        }
        if (item_id == -1) {
            if (count != 0) {
                return false;
            }
            empty_tail = true;
        } else if (empty_tail || count <= 0) {
            return false;
        }
    }
    return true;
}

NODISCARD bool role_values_valid(
    const RoleState& role, const std::int64_t hurt_maximum,
    const std::int64_t poison_maximum) noexcept {
    constexpr auto nonnegative_fields = std::array{
        &RoleState::experience, &RoleState::hp, &RoleState::maximum_hp,
        &RoleState::hurt, &RoleState::poison, &RoleState::physical_power,
        &RoleState::make_item_experience, &RoleState::mp, &RoleState::maximum_mp,
        &RoleState::attack, &RoleState::speed, &RoleState::defence,
        &RoleState::medicine, &RoleState::use_poison, &RoleState::detoxification,
        &RoleState::anti_poison, &RoleState::fist, &RoleState::sword,
        &RoleState::knife, &RoleState::unusual, &RoleState::hidden_weapon,
        &RoleState::knowledge, &RoleState::morality, &RoleState::fame,
        &RoleState::attack_with_poison, &RoleState::item_experience};
    if (!std::ranges::all_of(nonnegative_fields, [&role](const auto field) {
            return role.*field >= 0;
        })) {
        return false;
    }
    return role.increased_life >= 1 && role.increased_life <= 10 && role.level >= 1 &&
        role.sex >= std::numeric_limits<std::int16_t>::min() &&
        role.sex <= std::numeric_limits<std::int16_t>::max() &&
        role.mp_type >= 0 && role.mp_type <= 2 &&
        role.attack_twice >= 0 && role.attack_twice <= 1 &&
        role.physical_power <= 100 && role.iq >= 0 && role.iq <= 100 &&
        role.hp <= role.maximum_hp && role.mp <= role.maximum_mp &&
        role.hurt <= hurt_maximum && role.poison <= poison_maximum &&
        std::ranges::all_of(role.magic_levels, [](const auto value) { return value >= 0; }) &&
        std::ranges::all_of(role.taking_counts, [](const auto value) { return value >= 0; }) &&
        std::ranges::all_of(role.no_magic_count, [](const auto value) { return value >= 0; });
}

NODISCARD bool role_references_valid(
    const RoleState& role, const RuntimeRangerState& ranger) noexcept {
    if (role.id.value < 0 || !reference_valid(role.id.value, ranger.roles.size())) {
        return false;
    }
    for (std::size_t slot = 0U; slot < role.equipment.size(); ++slot) {
        const auto item_id = role.equipment[slot].value;
        if (!reference_valid(item_id, ranger.items.size())) {
            return false;
        }
        if (item_id >= 0) {
            const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
            if (item.word(item_word::item_type) != 1 ||
                item.word(item_word::equipment_type) != static_cast<std::int16_t>(slot)) {
                return false;
            }
        }
    }
    const auto practice_item = role.practice_item.value;
    if (!reference_valid(practice_item, ranger.items.size()) ||
        (practice_item >= 0 &&
         ranger.items[static_cast<std::size_t>(practice_item)].word(item_word::item_type) != 2)) {
        return false;
    }
    for (std::size_t slot = 0U; slot < role.magic_ids.size(); ++slot) {
        const auto magic_id = role.magic_ids[slot].value;
        if (!reference_valid(magic_id, ranger.magics.size()) ||
            (magic_id <= 0 && role.magic_levels[slot] != 0)) {
            return false;
        }
    }
    for (std::size_t slot = 0U; slot < role.taking_items.size(); ++slot) {
        const auto item_id = role.taking_items[slot].value;
        if (!reference_valid(item_id, ranger.items.size()) ||
            (item_id == -1 && role.taking_counts[slot] != 0)) {
            return false;
        }
    }
    for (std::size_t item_id = 0U; item_id < role.no_magic_count.size(); ++item_id) {
        if (role.no_magic_count[item_id] == 0) {
            continue;
        }
        const auto& item = ranger.items[item_id];
        if (item.word(item_word::item_type) != 2 || item.word(item_word::magic_id) != -1 ||
            item.word(item_word::need_experience) <= 0) {
            return false;
        }
    }
    return true;
}

template <std::size_t ByteCount>
NODISCARD bool definitions_match_except_word(
    const LegacyRecord<ByteCount>& current, const LegacyRecord<ByteCount>& baseline,
    const std::size_t mutable_word) noexcept {
    for (std::size_t byte = 0U; byte < ByteCount; ++byte) {
        if (byte / 2U != mutable_word && current.bytes[byte] != baseline.bytes[byte]) {
            return false;
        }
    }
    return true;
}

NODISCARD bool persistent_values_valid(
    const RuntimeRangerState& ranger, const PlaythroughLimits& limits,
    const std::int64_t poison_maximum) noexcept {
    if (!header_valid(ranger)) {
        return false;
    }
    for (std::size_t index = 0U; index < ranger.roles.size(); ++index) {
        const auto& role = ranger.roles[index];
        const auto in_party = [&ranger, index] {
            for (std::size_t slot = 0U; slot < kTeamMemberCount; ++slot) {
                if (ranger.header.team_member(slot).value == static_cast<std::int16_t>(index)) {
                    return true;
                }
            }
            return false;
        }();
        if ((role.ever_joined || in_party) &&
            (!role_values_valid(role, limits.hurt_maximum, poison_maximum) ||
             !role_references_valid(role, ranger))) {
            return false;
        }
    }
    return std::ranges::all_of(ranger.items, [&ranger](const ItemRecord& item) {
        return reference_valid(item.word(item_word::user), ranger.roles.size());
    });
}

}

std::int16_t RuntimeRangerHeader::word(const std::size_t index) const {
    return words_.at(index);
}

void RuntimeRangerHeader::set_word(const std::size_t index, const std::int64_t value) {
    if (value < std::numeric_limits<std::int16_t>::min() ||
        value > std::numeric_limits<std::int16_t>::max()) {
        throw std::out_of_range("native header field");
    }
    words_.at(index) = static_cast<std::int16_t>(value);
}

CharacterId RuntimeRangerHeader::team_member(const std::size_t index) const noexcept {
    return index < kTeamMemberCount
        ? CharacterId{words_[header_word::team_begin + index]} : CharacterId{};
}

void RuntimeRangerHeader::set_team_member(
    const std::size_t index, const CharacterId role_id) noexcept {
    if (index < kTeamMemberCount) {
        words_[header_word::team_begin + index] = role_id.value;
    }
}

ItemId RuntimeRangerHeader::inventory_item(const std::size_t index) const noexcept {
    return index < inventory_.size() ? inventory_[index].item_id : ItemId{};
}

std::int64_t RuntimeRangerHeader::inventory_count(const std::size_t index) const noexcept {
    return index < inventory_.size() ? inventory_[index].count : 0;
}

void RuntimeRangerHeader::set_inventory(
    const std::size_t index, const ItemId item_id, const std::int64_t count) noexcept {
    if (index < inventory_.size()) {
        inventory_[index] = InventoryEntry{item_id, count};
    }
}

RuntimeRangerHeader decode_legacy_header(const RangerHeader& header) {
    RuntimeRangerHeader result;
    for (std::size_t index = 0U; index < header_word::inventory_begin; ++index) {
        result.set_word(index, header.word(index));
    }
    for (std::size_t slot = 0U; slot < kInventoryCount; ++slot) {
        result.set_inventory(slot, header.inventory_item(slot), header.inventory_count(slot));
    }
    return result;
}

std::optional<RangerHeader> encode_legacy_header(const RuntimeRangerHeader& header) {
    RangerHeader result;
    for (std::size_t index = 0U; index < header_word::inventory_begin; ++index) {
        result.set_word(index, header.word(index));
    }
    for (std::size_t slot = 0U; slot < kInventoryCount; ++slot) {
        const auto count = header.inventory_count(slot);
        if (count < std::numeric_limits<std::int16_t>::min() ||
            count > std::numeric_limits<std::int16_t>::max()) {
            return std::nullopt;
        }
        result.set_inventory(
            slot, header.inventory_item(slot), static_cast<std::int16_t>(count));
    }
    return result;
}

bool RuntimeRangerState::valid() const {
    return roles.size() == kRoleCount && items.size() == kItemCount &&
        scenes.size() == kSceneMetadataCount && magics.size() == kMagicCount &&
        shops.size() == kShopCount && std::all_of(
            roles.begin(), roles.end(), [this](const RoleState& role) {
                const auto name = text::encode_big5(role.name);
                const auto nickname = text::encode_big5(role.nickname);
                return role.no_magic_count.size() == items.size() &&
                    name.has_value() && name->size() <= role_word::name_bytes &&
                    nickname.has_value() && nickname->size() <= role_word::name_bytes;
            });
}

bool RuntimeRangerState::matches_legacy_definitions(const RangerState& baseline) const {
    if (!valid() || !baseline.valid()) {
        return false;
    }
    for (std::size_t index = 0U; index < roles.size(); ++index) {
        if (roles[index].id != baseline.roles[index].id() ||
            roles[index].head_id != baseline.roles[index].word(role_word::head_id)) {
            return false;
        }
    }
    for (std::size_t index = 0U; index < items.size(); ++index) {
        if (!definitions_match_except_word(
                items[index], baseline.items[index], item_word::user)) {
            return false;
        }
    }
    for (std::size_t index = 0U; index < scenes.size(); ++index) {
        if (!definitions_match_except_word(
                scenes[index], baseline.scenes[index], scene_metadata_word::entrance_condition)) {
            return false;
        }
    }
    if (magics != baseline.magics) {
        return false;
    }
    for (std::size_t index = 0U; index < shops.size(); ++index) {
        for (std::size_t slot = 0U; slot < shop_word::item_count; ++slot) {
            if (shops[index].word(shop_word::item_id_begin + slot) !=
                    baseline.shops[index].word(shop_word::item_id_begin + slot) ||
                shops[index].word(shop_word::price_begin + slot) !=
                    baseline.shops[index].word(shop_word::price_begin + slot)) {
                return false;
            }
        }
    }
    return true;
}

bool RuntimeGameSnapshot::valid() const {
    if ((origin != SnapshotOrigin::legacy && origin != SnapshotOrigin::new_game_plus) ||
        (origin == SnapshotOrigin::legacy && playthrough != 1) ||
        (origin == SnapshotOrigin::new_game_plus && !configuration.enabled)) {
        return false;
    }
    const auto limits = calculate_playthrough_limits(configuration, playthrough);
    const auto final_limits = calculate_playthrough_limits(
        configuration, configuration.maximum_playthroughs);
    return ranger.valid() && SceneArchives::valid() && limits.has_value() &&
        final_limits.has_value();
}

bool RuntimeGameSnapshot::valid_for_persistence() const {
    if (!valid()) {
        return false;
    }
    const auto limits = calculate_playthrough_limits(configuration, playthrough);
    return limits.has_value() && persistent_values_valid(
        ranger, *limits, configuration.enabled ? 99 : 100);
}

std::optional<RuntimeRangerState> decode_legacy_ranger(RangerState ranger) {
    if (!ranger.valid()) {
        return std::nullopt;
    }
    RuntimeRangerState result;
    for (std::size_t index = 0U; index < ranger.roles.size(); ++index) {
        auto role = decode_legacy_role(ranger.roles[index], ranger.items.size());
        if (!role.has_value()) {
            return std::nullopt;
        }
        result.roles[index] = std::move(*role);
    }
    result.header = decode_legacy_header(ranger.header);
    result.items = std::move(ranger.items);
    result.scenes = std::move(ranger.scenes);
    result.magics = std::move(ranger.magics);
    result.shops = std::move(ranger.shops);
    return result;
}

std::optional<RuntimeGameSnapshot> decode_legacy_snapshot(
    GameSnapshot snapshot, const NewGamePlusConfiguration& configuration,
    const RangerState* const baseline) {
    if (!snapshot.valid() || !calculate_playthrough_limits(configuration, 1).has_value()) {
        return std::nullopt;
    }
    auto ranger = decode_legacy_ranger(std::move(snapshot.ranger));
    if (!ranger.has_value() ||
        (baseline != nullptr && !ranger->matches_legacy_definitions(*baseline))) {
        return std::nullopt;
    }
    RuntimeGameSnapshot result;
    result.configuration = configuration;
    result.ranger = std::move(*ranger);
    static_cast<SceneArchives&>(result) = std::move(static_cast<SceneArchives&>(snapshot));
    if (!result.valid_for_persistence()) {
        return std::nullopt;
    }
    return result;
}

std::optional<GameSnapshot> encode_legacy_snapshot(const RuntimeGameSnapshot& snapshot) {
    if (!snapshot.valid_for_persistence() || snapshot.origin != SnapshotOrigin::legacy ||
        snapshot.configuration.enabled || snapshot.playthrough != 1) {
        return std::nullopt;
    }
    const auto header = encode_legacy_header(snapshot.ranger.header);
    if (!header.has_value()) {
        return std::nullopt;
    }
    GameSnapshot result;
    for (std::size_t index = 0U; index < snapshot.ranger.roles.size(); ++index) {
        auto record = encode_legacy_role(snapshot.ranger.roles[index]);
        if (!record.has_value()) {
            return std::nullopt;
        }
        result.ranger.roles[index] = std::move(*record);
    }
    result.ranger.header = *header;
    result.ranger.items = snapshot.ranger.items;
    result.ranger.scenes = snapshot.ranger.scenes;
    result.ranger.magics = snapshot.ranger.magics;
    result.ranger.shops = snapshot.ranger.shops;
    static_cast<SceneArchives&>(result) = static_cast<const SceneArchives&>(snapshot);
    return result;
}

bool RuntimeGameState::import_snapshot(
    GameSnapshot snapshot, const NewGamePlusConfiguration& configuration,
    const RangerState* const baseline) {
    auto decoded = decode_legacy_snapshot(std::move(snapshot), configuration, baseline);
    if (!decoded.has_value()) {
        return false;
    }
    snapshot_ = std::move(*decoded);
    return true;
}

bool RuntimeGameState::import_snapshot(RuntimeGameSnapshot snapshot) {
    if (!snapshot.valid_for_persistence()) {
        return false;
    }
    snapshot_ = std::move(snapshot);
    return true;
}

const RuntimeRangerState* RuntimeGameState::ranger() const noexcept {
    return snapshot_.has_value() ? &snapshot_->ranger : nullptr;
}

RuntimeRangerState* RuntimeGameState::ranger() noexcept {
    return snapshot_.has_value() ? &snapshot_->ranger : nullptr;
}

const RuntimeGameSnapshot* RuntimeGameState::snapshot() const noexcept {
    return snapshot_.has_value() ? &*snapshot_ : nullptr;
}

RuntimeGameSnapshot* RuntimeGameState::snapshot() noexcept {
    return snapshot_.has_value() ? &*snapshot_ : nullptr;
}

std::optional<RuntimeGameSnapshot> RuntimeGameState::export_snapshot() const {
    return snapshot_;
}

}
