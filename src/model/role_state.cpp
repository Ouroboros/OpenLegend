#include "openlegend/model/role_state.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <utility>

#include "openlegend/attributes.hpp"
#include "openlegend/text/big5.hpp"

namespace openlegend::model {
namespace {

struct ScalarField {
    std::size_t legacy_index;
    std::int64_t RoleState::* value;
    bool unsigned_legacy{};
};

constexpr auto scalar_fields = std::to_array<ScalarField>({
    {role_word::increased_life, &RoleState::increased_life},
    {role_word::unused, &RoleState::unused},
    {role_word::sexual, &RoleState::sex},
    {role_word::level, &RoleState::level},
    {role_word::experience, &RoleState::experience, true},
    {role_word::hp, &RoleState::hp},
    {role_word::maximum_hp, &RoleState::maximum_hp},
    {role_word::hurt, &RoleState::hurt},
    {role_word::poison, &RoleState::poison},
    {role_word::physical_power, &RoleState::physical_power},
    {role_word::make_item_experience, &RoleState::make_item_experience, true},
    {role_word::mp_type, &RoleState::mp_type},
    {role_word::mp, &RoleState::mp},
    {role_word::maximum_mp, &RoleState::maximum_mp},
    {role_word::attack, &RoleState::attack},
    {role_word::speed, &RoleState::speed},
    {role_word::defence, &RoleState::defence},
    {role_word::medicine, &RoleState::medicine},
    {role_word::use_poison, &RoleState::use_poison},
    {role_word::detoxification, &RoleState::detoxification},
    {role_word::anti_poison, &RoleState::anti_poison},
    {role_word::fist, &RoleState::fist},
    {role_word::sword, &RoleState::sword},
    {role_word::knife, &RoleState::knife},
    {role_word::unusual, &RoleState::unusual},
    {role_word::hidden_weapon, &RoleState::hidden_weapon},
    {role_word::knowledge, &RoleState::knowledge},
    {role_word::morality, &RoleState::morality},
    {role_word::attack_with_poison, &RoleState::attack_with_poison},
    {role_word::attack_twice, &RoleState::attack_twice},
    {role_word::fame, &RoleState::fame},
    {role_word::iq, &RoleState::iq},
    {role_word::item_experience, &RoleState::item_experience, true},
});

NODISCARD std::optional<std::u8string> decode_name(
    const RoleRecord& record, const std::size_t byte_offset) {
    const auto bytes = std::span<const std::uint8_t>{record.bytes}.subspan(
        byte_offset, role_word::name_bytes);
    const auto end = std::find(bytes.begin(), bytes.end(), 0U);
    const auto length = static_cast<std::size_t>(end - bytes.begin());
    return text::decode_big5(text::Big5TextView{bytes.first(length)});
}

NODISCARD bool encode_name(
    RoleRecord& record, const std::size_t byte_offset, const std::u8string& value) {
    const auto encoded = text::encode_big5(value);
    if (!encoded.has_value() || encoded->size() > role_word::name_bytes) {
        return false;
    }
    const auto destination = std::span<std::uint8_t>{record.bytes}.subspan(byte_offset);
    std::copy(encoded->begin(), encoded->end(), destination.begin());
    return true;
}

NODISCARD bool encode_value(
    RoleRecord& record,
    const std::size_t index,
    const std::int64_t value,
    const bool unsigned_legacy = false) noexcept {
    if (unsigned_legacy) {
        if (value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
            return false;
        }
        record.set_word(index, static_cast<std::int16_t>(static_cast<std::uint16_t>(value)));
    } else {
        if (value < std::numeric_limits<std::int16_t>::min() ||
            value > std::numeric_limits<std::int16_t>::max()) {
            return false;
        }
        record.set_word(index, static_cast<std::int16_t>(value));
    }
    return true;
}

}

std::optional<RoleState> decode_legacy_role(
    const RoleRecord& record, const std::size_t item_count) {
    auto name = decode_name(record, role_word::name_byte);
    auto nickname = decode_name(record, role_word::nickname_byte);
    if (!name.has_value() || !nickname.has_value()) {
        return std::nullopt;
    }
    RoleState state;
    state.id = record.id();
    state.head_id = record.word(role_word::head_id);
    state.name = std::move(*name);
    state.nickname = std::move(*nickname);
    for (const auto& field : scalar_fields) {
        state.*field.value = field.unsigned_legacy ?
            static_cast<std::int64_t>(record.unsigned_word(field.legacy_index)) :
            static_cast<std::int64_t>(record.word(field.legacy_index));
    }
    for (std::size_t index = 0U; index < state.equipment.size(); ++index) {
        state.equipment[index] = ItemId{record.word(role_word::equipment_begin + index)};
    }
    for (std::size_t index = 0U; index < state.frames.size(); ++index) {
        state.frames[index] = record.word(role_word::frame_begin + index);
    }
    state.practice_item = ItemId{record.word(role_word::practice_item)};
    for (std::size_t index = 0U; index < state.magic_ids.size(); ++index) {
        state.magic_ids[index] = MagicId{record.word(role_word::magic_id_begin + index)};
        state.magic_levels[index] = record.unsigned_word(role_word::magic_level_begin + index);
    }
    for (std::size_t index = 0U; index < state.taking_items.size(); ++index) {
        state.taking_items[index] = ItemId{record.word(role_word::taking_item_begin + index)};
        state.taking_counts[index] = record.word(role_word::taking_item_count_begin + index);
    }
    state.no_magic_count.assign(item_count, 0);
    return state;
}

std::optional<RoleRecord> encode_legacy_role(const RoleState& state) {
    if (state.ever_joined || std::any_of(
            state.no_magic_count.begin(), state.no_magic_count.end(),
            [](const std::int64_t count) { return count != 0; })) {
        return std::nullopt;
    }
    RoleRecord record;
    record.set_word(role_word::id, state.id.value);
    record.set_word(role_word::head_id, state.head_id);
    if (!encode_name(record, role_word::name_byte, state.name) ||
        !encode_name(record, role_word::nickname_byte, state.nickname)) {
        return std::nullopt;
    }
    for (const auto& field : scalar_fields) {
        if (!encode_value(record, field.legacy_index, state.*field.value, field.unsigned_legacy)) {
            return std::nullopt;
        }
    }
    for (std::size_t index = 0U; index < state.equipment.size(); ++index) {
        record.set_word(role_word::equipment_begin + index, state.equipment[index].value);
    }
    for (std::size_t index = 0U; index < state.frames.size(); ++index) {
        record.set_word(role_word::frame_begin + index, state.frames[index]);
    }
    record.set_word(role_word::practice_item, state.practice_item.value);
    for (std::size_t index = 0U; index < state.magic_ids.size(); ++index) {
        record.set_word(role_word::magic_id_begin + index, state.magic_ids[index].value);
        if (!encode_value(record, role_word::magic_level_begin + index, state.magic_levels[index], true)) {
            return std::nullopt;
        }
    }
    for (std::size_t index = 0U; index < state.taking_items.size(); ++index) {
        record.set_word(role_word::taking_item_begin + index, state.taking_items[index].value);
        if (!encode_value(record, role_word::taking_item_count_begin + index, state.taking_counts[index])) {
            return std::nullopt;
        }
    }
    return record;
}

}
