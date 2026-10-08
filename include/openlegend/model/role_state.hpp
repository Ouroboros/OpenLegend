#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/model/game_snapshot.hpp"

namespace openlegend::model {

struct RoleState {
    CharacterId id;
    std::int16_t head_id{};
    std::int64_t increased_life{};
    std::int64_t unused{};
    std::u8string name;
    std::u8string nickname;
    std::int64_t sex{};
    std::int64_t level{};
    std::int64_t experience{};
    std::int64_t hp{};
    std::int64_t maximum_hp{};
    std::int64_t hurt{};
    std::int64_t poison{};
    std::int64_t physical_power{};
    std::int64_t make_item_experience{};
    std::array<ItemId, role_word::equipment_count> equipment;
    std::array<std::int16_t, role_word::frame_count> frames{};
    std::int64_t mp_type{};
    std::int64_t mp{};
    std::int64_t maximum_mp{};
    std::int64_t attack{};
    std::int64_t speed{};
    std::int64_t defence{};
    std::int64_t medicine{};
    std::int64_t use_poison{};
    std::int64_t detoxification{};
    std::int64_t anti_poison{};
    std::int64_t fist{};
    std::int64_t sword{};
    std::int64_t knife{};
    std::int64_t unusual{};
    std::int64_t hidden_weapon{};
    std::int64_t knowledge{};
    std::int64_t morality{};
    std::int64_t attack_with_poison{};
    std::int64_t attack_twice{};
    std::int64_t fame{};
    std::int64_t iq{};
    ItemId practice_item;
    std::int64_t item_experience{};
    std::array<MagicId, role_word::magic_count> magic_ids;
    std::array<std::int64_t, role_word::magic_level_count> magic_levels{};
    std::array<ItemId, role_word::taking_item_count> taking_items;
    std::array<std::int64_t, role_word::taking_item_count> taking_counts{};
    bool ever_joined{};
    std::vector<std::int64_t> no_magic_count = std::vector<std::int64_t>(kItemCount);

    NODISCARD std::int64_t word(std::size_t index) const;

    NODISCARD std::int64_t unsigned_word(std::size_t index) const;

    void set_word(std::size_t index, std::int64_t value);

    NODISCARD std::array<std::uint8_t, role_word::name_bytes> legacy_name() const;

    NODISCARD bool operator==(const RoleState&) const = default;
};

NODISCARD std::optional<RoleState> decode_legacy_role(
    const RoleRecord& record, std::size_t item_count);

NODISCARD std::optional<RoleRecord> encode_legacy_role(const RoleState& state);

}
