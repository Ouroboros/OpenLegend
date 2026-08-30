#include "openlegend/model/game_snapshot.hpp"

#include <utility>

namespace openlegend::model {

namespace {

void write_i16le(
    const std::span<std::uint8_t> bytes, const std::size_t offset, const std::int16_t value) noexcept {
    const auto bits = static_cast<std::uint16_t>(value);
    bytes[offset] = static_cast<std::uint8_t>(bits & 0xFFU);
    bytes[offset + 1U] = static_cast<std::uint8_t>(bits >> 8U);
}

[[nodiscard]] bool valid_ends(
    const std::array<std::uint32_t, kSceneCount>& ends,
    const std::size_t record_bytes,
    const std::size_t total_bytes) noexcept {
    if (total_bytes != record_bytes * kSceneCount) {
        return false;
    }
    for (std::size_t index = 0U; index < ends.size(); ++index) {
        if (ends[index] != static_cast<std::uint32_t>((index + 1U) * record_bytes)) {
            return false;
        }
    }
    return true;
}

}  // namespace

CharacterId RangerHeader::team_member(const std::size_t index) const noexcept {
    return CharacterId{word(header_word::team_begin + index)};
}

void RangerHeader::set_team_member(const std::size_t index, const CharacterId role_id) noexcept {
    set_word(header_word::team_begin + index, role_id.value);
}

ItemId RangerHeader::inventory_item(const std::size_t index) const noexcept {
    return ItemId{word(header_word::inventory_begin + index * 2U)};
}

std::int16_t RangerHeader::inventory_count(const std::size_t index) const noexcept {
    return word(header_word::inventory_begin + index * 2U + 1U);
}

void RangerHeader::set_inventory(
    const std::size_t index, const ItemId item_id, const std::int16_t count) noexcept {
    set_word(header_word::inventory_begin + index * 2U, item_id.value);
    set_word(header_word::inventory_begin + index * 2U + 1U, count);
}

bool RangerState::valid() const noexcept {
    return roles.size() == kRoleCount && items.size() == kItemCount &&
        scenes.size() == kSceneMetadataCount && magics.size() == kMagicCount &&
        shops.size() == kShopCount;
}

bool GameSnapshot::valid() const noexcept {
    return ranger.valid() &&
        valid_ends(scene_map_ends, kSceneMapBytesPerScene, scene_maps.size()) &&
        valid_ends(scene_event_ends, kSceneEventBytesPerScene, scene_events.size());
}

std::optional<std::int16_t> GameSnapshot::scene_value(
    const std::size_t scene,
    const SceneLayer layer,
    const std::size_t linear_tile) const noexcept {
    const auto layer_index = static_cast<std::size_t>(layer);
    if (scene >= kSceneCount || layer_index >= kSceneLayerCount || linear_tile >= kSceneTileCount ||
        scene_maps.size() != kSceneCount * kSceneMapBytesPerScene) {
        return std::nullopt;
    }
    const std::size_t word_index =
        scene * kSceneLayerCount * kSceneTileCount + layer_index * kSceneTileCount + linear_tile;
    return compat::read_i16le(
        std::span<const std::uint8_t>{scene_maps}, word_index * 2U);
}

bool GameSnapshot::set_scene_value(
    const std::size_t scene,
    const SceneLayer layer,
    const std::size_t linear_tile,
    const std::int16_t value) noexcept {
    const auto layer_index = static_cast<std::size_t>(layer);
    if (scene >= kSceneCount || layer_index >= kSceneLayerCount || linear_tile >= kSceneTileCount ||
        scene_maps.size() != kSceneCount * kSceneMapBytesPerScene) {
        return false;
    }
    const std::size_t word_index =
        scene * kSceneLayerCount * kSceneTileCount + layer_index * kSceneTileCount + linear_tile;
    write_i16le(std::span<std::uint8_t>{scene_maps}, word_index * 2U, value);
    return true;
}

std::optional<std::int16_t> GameSnapshot::event_value(
    const std::size_t scene,
    const std::size_t event,
    const SceneEventField field) const noexcept {
    const auto field_index = static_cast<std::size_t>(field);
    if (scene >= kSceneCount || event >= kSceneEventCount ||
        field_index >= kSceneEventWordCount ||
        scene_events.size() != kSceneCount * kSceneEventBytesPerScene) {
        return std::nullopt;
    }
    const std::size_t word_index =
        scene * kSceneEventCount * kSceneEventWordCount + event * kSceneEventWordCount + field_index;
    return compat::read_i16le(
        std::span<const std::uint8_t>{scene_events}, word_index * 2U);
}

bool GameSnapshot::set_event_value(
    const std::size_t scene,
    const std::size_t event,
    const SceneEventField field,
    const std::int16_t value) noexcept {
    const auto field_index = static_cast<std::size_t>(field);
    if (scene >= kSceneCount || event >= kSceneEventCount ||
        field_index >= kSceneEventWordCount ||
        scene_events.size() != kSceneCount * kSceneEventBytesPerScene) {
        return false;
    }
    const std::size_t word_index =
        scene * kSceneEventCount * kSceneEventWordCount + event * kSceneEventWordCount + field_index;
    write_i16le(std::span<std::uint8_t>{scene_events}, word_index * 2U, value);
    return true;
}

bool GameState::import_snapshot(GameSnapshot snapshot) {
    if (!snapshot.valid()) {
        return false;
    }
    snapshot_ = std::move(snapshot);
    return true;
}

const RangerState* GameState::ranger() const noexcept {
    return snapshot_.has_value() ? &snapshot_->ranger : nullptr;
}

RangerState* GameState::ranger() noexcept {
    return snapshot_.has_value() ? &snapshot_->ranger : nullptr;
}

const GameSnapshot* GameState::snapshot() const noexcept {
    return snapshot_.has_value() ? &*snapshot_ : nullptr;
}

GameSnapshot* GameState::snapshot() noexcept {
    return snapshot_.has_value() ? &*snapshot_ : nullptr;
}

std::optional<GameSnapshot> GameState::export_snapshot() const {
    return snapshot_;
}

}  // namespace openlegend::model
