#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/model/legacy_types.hpp"

namespace openlegend::model {

inline constexpr std::size_t kRangerHeaderBytes = 836U;
inline constexpr std::size_t kRoleRecordBytes = 182U;
inline constexpr std::size_t kItemRecordBytes = 190U;
inline constexpr std::size_t kSceneMetadataRecordBytes = 52U;
inline constexpr std::size_t kMagicRecordBytes = 136U;
inline constexpr std::size_t kShopRecordBytes = 30U;

inline constexpr std::size_t kRoleCount = 320U;
inline constexpr std::size_t kItemCount = 200U;
inline constexpr std::size_t kSceneMetadataCount = 84U;
inline constexpr std::size_t kMagicCount = 93U;
inline constexpr std::size_t kShopCount = 5U;
inline constexpr std::size_t kTeamMemberCount = 6U;
inline constexpr std::size_t kInventoryCount = 200U;

inline constexpr std::array<std::uint32_t, 6> kRangerCumulativeEnds{
    836U, 59'076U, 97'076U, 101'444U, 114'092U, 114'242U};

inline constexpr std::size_t kSceneCount = 100U;
inline constexpr std::size_t kSceneLayerCount = 6U;
inline constexpr std::size_t kSceneCoordinateCount = 64U;
inline constexpr std::size_t kSceneTileCount = 4'096U;
inline constexpr std::size_t kSceneMapBytesPerScene = 49'152U;
inline constexpr std::size_t kSceneEventCount = 200U;
inline constexpr std::size_t kSceneEventWordCount = 11U;
inline constexpr std::size_t kSceneEventBytes = 22U;
inline constexpr std::size_t kSceneEventBytesPerScene = 4'400U;

namespace header_word {
inline constexpr std::size_t in_ship = 0U;
inline constexpr std::size_t in_sub_map = 1U;
inline constexpr std::size_t main_map_x = 2U;
inline constexpr std::size_t main_map_y = 3U;
inline constexpr std::size_t sub_map_x = 4U;
inline constexpr std::size_t sub_map_y = 5U;
inline constexpr std::size_t face_towards = 6U;
inline constexpr std::size_t ship_x = 7U;
inline constexpr std::size_t ship_y = 8U;
inline constexpr std::size_t ship_x_1 = 9U;
inline constexpr std::size_t ship_y_1 = 10U;
inline constexpr std::size_t encode = 11U;
inline constexpr std::size_t team_begin = 12U;
inline constexpr std::size_t inventory_begin = 18U;
}  // namespace header_word

namespace role_word {
inline constexpr std::size_t id = 0U;
inline constexpr std::size_t head_id = 1U;
inline constexpr std::size_t increased_life = 2U;
inline constexpr std::size_t unused = 3U;
inline constexpr std::size_t name_byte = 8U;
inline constexpr std::size_t name_bytes = 10U;
inline constexpr std::size_t nickname_byte = 18U;
inline constexpr std::size_t nickname_bytes = 10U;
inline constexpr std::size_t sexual = 14U;
inline constexpr std::size_t level = 15U;
inline constexpr std::size_t experience = 16U;
inline constexpr std::size_t hp = 17U;
inline constexpr std::size_t maximum_hp = 18U;
inline constexpr std::size_t hurt = 19U;
inline constexpr std::size_t poison = 20U;
inline constexpr std::size_t physical_power = 21U;
inline constexpr std::size_t make_item_experience = 22U;
inline constexpr std::size_t equipment_begin = 23U;
inline constexpr std::size_t equipment_count = 2U;
inline constexpr std::size_t frame_begin = 25U;
inline constexpr std::size_t frame_count = 15U;
inline constexpr std::size_t mp_type = 40U;
inline constexpr std::size_t mp = 41U;
inline constexpr std::size_t maximum_mp = 42U;
inline constexpr std::size_t attack = 43U;
inline constexpr std::size_t speed = 44U;
inline constexpr std::size_t defence = 45U;
inline constexpr std::size_t medicine = 46U;
inline constexpr std::size_t use_poison = 47U;
inline constexpr std::size_t detoxification = 48U;
inline constexpr std::size_t anti_poison = 49U;
inline constexpr std::size_t fist = 50U;
inline constexpr std::size_t sword = 51U;
inline constexpr std::size_t knife = 52U;
inline constexpr std::size_t unusual = 53U;
inline constexpr std::size_t hidden_weapon = 54U;
inline constexpr std::size_t knowledge = 55U;
inline constexpr std::size_t morality = 56U;
inline constexpr std::size_t attack_with_poison = 57U;
inline constexpr std::size_t attack_twice = 58U;
inline constexpr std::size_t fame = 59U;
inline constexpr std::size_t iq = 60U;
inline constexpr std::size_t practice_item = 61U;
inline constexpr std::size_t item_experience = 62U;
inline constexpr std::size_t magic_id_begin = 63U;
inline constexpr std::size_t magic_count = 10U;
inline constexpr std::size_t magic_level_begin = 73U;
inline constexpr std::size_t magic_level_count = 10U;
inline constexpr std::size_t taking_item_begin = 83U;
inline constexpr std::size_t taking_item_count = 4U;
inline constexpr std::size_t taking_item_count_begin = 87U;
}  // namespace role_word

namespace item_word {
inline constexpr std::size_t id = 0U;
inline constexpr std::size_t name_byte = 2U;
inline constexpr std::size_t name_bytes = 20U;
inline constexpr std::size_t secondary_name_begin = 11U;
inline constexpr std::size_t secondary_name_count = 10U;
inline constexpr std::size_t introduction_byte = 42U;
inline constexpr std::size_t introduction_bytes = 30U;
inline constexpr std::size_t magic_id = 36U;
inline constexpr std::size_t hidden_weapon_effect_id = 37U;
inline constexpr std::size_t user = 38U;
inline constexpr std::size_t equipment_type = 39U;
inline constexpr std::size_t show_introduction = 40U;
inline constexpr std::size_t item_type = 41U;
inline constexpr std::size_t unknown_begin = 42U;
inline constexpr std::size_t unknown_count = 3U;
inline constexpr std::size_t add_hp = 45U;
inline constexpr std::size_t add_maximum_hp = 46U;
inline constexpr std::size_t add_poison = 47U;
inline constexpr std::size_t add_physical_power = 48U;
inline constexpr std::size_t change_mp_type = 49U;
inline constexpr std::size_t add_mp = 50U;
inline constexpr std::size_t add_maximum_mp = 51U;
inline constexpr std::size_t add_attack = 52U;
inline constexpr std::size_t add_speed = 53U;
inline constexpr std::size_t add_defence = 54U;
inline constexpr std::size_t add_medicine = 55U;
inline constexpr std::size_t add_use_poison = 56U;
inline constexpr std::size_t add_detoxification = 57U;
inline constexpr std::size_t add_anti_poison = 58U;
inline constexpr std::size_t add_fist = 59U;
inline constexpr std::size_t add_sword = 60U;
inline constexpr std::size_t add_knife = 61U;
inline constexpr std::size_t add_unusual = 62U;
inline constexpr std::size_t add_hidden_weapon = 63U;
inline constexpr std::size_t add_knowledge = 64U;
inline constexpr std::size_t add_morality = 65U;
inline constexpr std::size_t add_attack_twice = 66U;
inline constexpr std::size_t add_attack_with_poison = 67U;
inline constexpr std::size_t only_suitable_role = 68U;
inline constexpr std::size_t need_mp_type = 69U;
inline constexpr std::size_t need_mp = 70U;
inline constexpr std::size_t need_attack = 71U;
inline constexpr std::size_t need_speed = 72U;
inline constexpr std::size_t need_use_poison = 73U;
inline constexpr std::size_t need_medicine = 74U;
inline constexpr std::size_t need_detoxification = 75U;
inline constexpr std::size_t need_fist = 76U;
inline constexpr std::size_t need_sword = 77U;
inline constexpr std::size_t need_knife = 78U;
inline constexpr std::size_t need_unusual = 79U;
inline constexpr std::size_t need_hidden_weapon = 80U;
inline constexpr std::size_t need_iq = 81U;
inline constexpr std::size_t need_experience = 82U;
inline constexpr std::size_t need_make_item_experience = 83U;
inline constexpr std::size_t need_material = 84U;
inline constexpr std::size_t make_item_begin = 85U;
inline constexpr std::size_t make_item_count = 5U;
inline constexpr std::size_t make_item_count_begin = 90U;
}  // namespace item_word

namespace scene_metadata_word {
inline constexpr std::size_t id = 0U;
inline constexpr std::size_t name_byte = 2U;
inline constexpr std::size_t name_bytes = 10U;
inline constexpr std::size_t exit_music = 6U;
inline constexpr std::size_t entrance_music = 7U;
inline constexpr std::size_t jump_scene = 8U;
inline constexpr std::size_t entrance_condition = 9U;
inline constexpr std::size_t main_entrance_x_1 = 10U;
inline constexpr std::size_t main_entrance_y_1 = 11U;
inline constexpr std::size_t main_entrance_x_2 = 12U;
inline constexpr std::size_t main_entrance_y_2 = 13U;
inline constexpr std::size_t entrance_x = 14U;
inline constexpr std::size_t entrance_y = 15U;
inline constexpr std::size_t exit_x_begin = 16U;
inline constexpr std::size_t exit_count = 3U;
inline constexpr std::size_t exit_y_begin = 19U;
inline constexpr std::size_t jump_x = 22U;
inline constexpr std::size_t jump_y = 23U;
inline constexpr std::size_t jump_return_x = 24U;
inline constexpr std::size_t jump_return_y = 25U;
}  // namespace scene_metadata_word

namespace magic_word {
inline constexpr std::size_t id = 0U;
inline constexpr std::size_t name_byte = 2U;
inline constexpr std::size_t name_bytes = 10U;
inline constexpr std::size_t unknown_begin = 6U;
inline constexpr std::size_t unknown_count = 5U;
inline constexpr std::size_t sound_id = 11U;
inline constexpr std::size_t magic_type = 12U;
inline constexpr std::size_t effect_id = 13U;
inline constexpr std::size_t hurt_type = 14U;
inline constexpr std::size_t attack_area_type = 15U;
inline constexpr std::size_t need_mp = 16U;
inline constexpr std::size_t with_poison = 17U;
inline constexpr std::size_t attack_begin = 18U;
inline constexpr std::size_t level_value_count = 10U;
inline constexpr std::size_t select_distance_begin = 28U;
inline constexpr std::size_t attack_distance_begin = 38U;
inline constexpr std::size_t add_mp_begin = 48U;
inline constexpr std::size_t hurt_mp_begin = 58U;
}  // namespace magic_word

namespace shop_word {
inline constexpr std::size_t item_id_begin = 0U;
inline constexpr std::size_t item_count = 5U;
inline constexpr std::size_t total_begin = 5U;
inline constexpr std::size_t price_begin = 10U;
}  // namespace shop_word

enum class SceneLayer : std::size_t {
    earth = 0U,
    building = 1U,
    decoration = 2U,
    event_index = 3U,
    building_height = 4U,
    decoration_height = 5U,
};

enum class SceneEventField : std::size_t {
    cannot_walk = 0U,
    index = 1U,
    event_1 = 2U,
    event_2 = 3U,
    event_3 = 4U,
    current_picture = 5U,  // Legacy first-picture/condition word; historical API name retained.
    end_picture = 6U,
    begin_picture = 7U,  // Mutable picture rendered and advanced by the scene loop.
    picture_delay = 8U,
    x = 9U,
    y = 10U,
};

template <std::size_t ByteCount>
struct LegacyRecord {
    static_assert(ByteCount % 2U == 0U);
    static constexpr std::size_t byte_count = ByteCount;
    static constexpr std::size_t word_count = ByteCount / 2U;

    std::array<std::uint8_t, ByteCount> bytes{};

    [[nodiscard]] std::int16_t word(const std::size_t index) const noexcept {
        return compat::read_i16le(std::span<const std::uint8_t>{bytes}, index * 2U);
    }

    [[nodiscard]] std::uint16_t unsigned_word(const std::size_t index) const noexcept {
        return compat::read_u16le(std::span<const std::uint8_t>{bytes}, index * 2U);
    }

    void set_word(const std::size_t index, const std::int16_t value) noexcept {
        const auto bits = static_cast<std::uint16_t>(value);
        bytes[index * 2U] = static_cast<std::uint8_t>(bits & 0xFFU);
        bytes[index * 2U + 1U] = static_cast<std::uint8_t>(bits >> 8U);
    }

    [[nodiscard]] bool operator==(const LegacyRecord&) const = default;
};

struct RangerHeader : LegacyRecord<kRangerHeaderBytes> {
    [[nodiscard]] CharacterId team_member(std::size_t index) const noexcept;
    void set_team_member(std::size_t index, CharacterId role_id) noexcept;
    [[nodiscard]] ItemId inventory_item(std::size_t index) const noexcept;
    [[nodiscard]] std::int16_t inventory_count(std::size_t index) const noexcept;
    void set_inventory(std::size_t index, ItemId item_id, std::int16_t count) noexcept;
};

struct RoleRecord : LegacyRecord<kRoleRecordBytes> {
    [[nodiscard]] CharacterId id() const noexcept { return CharacterId{word(role_word::id)}; }
};

struct ItemRecord : LegacyRecord<kItemRecordBytes> {
    [[nodiscard]] ItemId id() const noexcept { return ItemId{word(item_word::id)}; }
};

struct SceneMetadataRecord : LegacyRecord<kSceneMetadataRecordBytes> {
    [[nodiscard]] SceneId id() const noexcept {
        return SceneId{word(scene_metadata_word::id)};
    }
};

struct MagicRecord : LegacyRecord<kMagicRecordBytes> {
    [[nodiscard]] MagicId id() const noexcept { return MagicId{word(magic_word::id)}; }
};

struct ShopRecord : LegacyRecord<kShopRecordBytes> {
    [[nodiscard]] ItemId item(std::size_t index) const noexcept {
        return ItemId{word(shop_word::item_id_begin + index)};
    }
};

struct RangerState {
    RangerHeader header;
    std::vector<RoleRecord> roles = std::vector<RoleRecord>(kRoleCount);
    std::vector<ItemRecord> items = std::vector<ItemRecord>(kItemCount);
    std::vector<SceneMetadataRecord> scenes =
        std::vector<SceneMetadataRecord>(kSceneMetadataCount);
    std::vector<MagicRecord> magics = std::vector<MagicRecord>(kMagicCount);
    std::vector<ShopRecord> shops = std::vector<ShopRecord>(kShopCount);

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool operator==(const RangerState&) const = default;
};

struct GameSnapshot {
    RangerState ranger;
    std::array<std::uint32_t, kSceneCount> scene_map_ends{};
    std::vector<std::uint8_t> scene_maps;
    std::array<std::uint32_t, kSceneCount> scene_event_ends{};
    std::vector<std::uint8_t> scene_events;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] std::optional<std::int16_t> scene_value(
        std::size_t scene, SceneLayer layer, std::size_t linear_tile) const noexcept;
    [[nodiscard]] bool set_scene_value(
        std::size_t scene,
        SceneLayer layer,
        std::size_t linear_tile,
        std::int16_t value) noexcept;
    [[nodiscard]] std::optional<std::int16_t> event_value(
        std::size_t scene, std::size_t event, SceneEventField field) const noexcept;
    [[nodiscard]] bool set_event_value(
        std::size_t scene,
        std::size_t event,
        SceneEventField field,
        std::int16_t value) noexcept;

    [[nodiscard]] bool operator==(const GameSnapshot&) const = default;
};

class GameState {
public:
    [[nodiscard]] bool import_snapshot(GameSnapshot snapshot);
    [[nodiscard]] bool loaded() const noexcept { return snapshot_.has_value(); }
    [[nodiscard]] const RangerState* ranger() const noexcept;
    [[nodiscard]] RangerState* ranger() noexcept;
    [[nodiscard]] const GameSnapshot* snapshot() const noexcept;
    [[nodiscard]] GameSnapshot* snapshot() noexcept;
    [[nodiscard]] std::optional<GameSnapshot> export_snapshot() const;

private:
    std::optional<GameSnapshot> snapshot_;
};

}  // namespace openlegend::model
