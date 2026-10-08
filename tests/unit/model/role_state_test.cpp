#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "openlegend/model/role_state.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/text/big5.hpp"
#include "test_support.hpp"

namespace {

openlegend::model::RoleRecord make_record() {
    using namespace openlegend::model;

    RoleRecord record;
    record.set_word(role_word::id, 7);
    record.set_word(role_word::head_id, 11);
    record.set_word(role_word::increased_life, 6);
    record.set_word(role_word::level, 30);
    record.set_word(role_word::experience, static_cast<std::int16_t>(52'000U));
    record.set_word(role_word::hp, 600);
    record.set_word(role_word::maximum_hp, 999);
    record.set_word(role_word::physical_power, 100);
    record.set_word(role_word::anti_poison, 500);
    record.set_word(role_word::make_item_experience, static_cast<std::int16_t>(61'000U));
    record.set_word(role_word::item_experience, static_cast<std::int16_t>(55'000U));
    record.set_word(role_word::magic_id_begin, 22);
    record.set_word(role_word::magic_level_begin, -1);
    const auto encoded_name = openlegend::text::encode_big5(u8"張無忌");
    OL_CHECK(encoded_name.has_value());
    if (encoded_name.has_value()) {
        std::copy(encoded_name->begin(), encoded_name->end(),
            record.bytes.begin() + role_word::name_byte);
    }
    return record;
}

void check_wide_values_and_history_initialization() {
    using namespace openlegend::model;

    const auto record = make_record();
    auto state = decode_legacy_role(record, kItemCount);
    OL_CHECK(state.has_value());
    if (!state.has_value()) {
        return;
    }
    OL_CHECK(state->experience == 52'000);
    OL_CHECK(state->make_item_experience == 61'000);
    OL_CHECK(state->item_experience == 55'000);
    OL_CHECK(state->magic_levels[0] == 65'535);
    OL_CHECK(state->anti_poison == 500);
    OL_CHECK(state->name == u8"張無忌");
    OL_CHECK(state->nickname.empty());
    OL_CHECK(!state->ever_joined);
    OL_CHECK(state->no_magic_count.size() == kItemCount);
    OL_CHECK(std::all_of(state->no_magic_count.begin(), state->no_magic_count.end(),
        [](const std::int64_t count) { return count == 0; }));
    OL_CHECK(encode_legacy_role(*state) == record);

    state->experience = 1'000'000'000'000;
    state->hp = 5'000'000'000;
    state->maximum_hp = 5'000'000'000;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    OL_CHECK(state->experience == 1'000'000'000'000);
    OL_CHECK(state->hp == 5'000'000'000);
    OL_CHECK(state->maximum_hp == 5'000'000'000);
    OL_CHECK(record.unsigned_word(role_word::experience) == 52'000U);
}

void check_legacy_encoding_boundaries() {
    using namespace openlegend::model;

    auto state = decode_legacy_role(make_record(), kItemCount);
    OL_CHECK(state.has_value());
    if (!state.has_value()) {
        return;
    }
    state->attack = std::numeric_limits<std::int16_t>::max();
    OL_CHECK(encode_legacy_role(*state).has_value());
    ++state->attack;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    state->attack = 100;
    state->experience = std::numeric_limits<std::uint16_t>::max();
    OL_CHECK(encode_legacy_role(*state).has_value());
    ++state->experience;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    state->experience = -1;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    state->experience = 52'000;
    state->magic_levels[0] = 65'536;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    state->magic_levels[0] = -1;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    state->magic_levels[0] = 900;
    state->taking_counts[0] = 32'768;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    state->taking_counts[0] = 0;
    state->ever_joined = true;
    OL_CHECK(!encode_legacy_role(*state).has_value());
    state->ever_joined = false;
    state->no_magic_count[0] = 1;
    OL_CHECK(!encode_legacy_role(*state).has_value());
}

void check_name_validation() {
    using namespace openlegend;

    auto state = model::decode_legacy_role(make_record(), model::kItemCount);
    OL_CHECK(state.has_value());
    if (!state.has_value()) {
        return;
    }
    state->name = u8"甲乙丙丁戊";
    const auto full_name = model::encode_legacy_role(*state);
    OL_CHECK(full_name.has_value());
    if (full_name.has_value()) {
        const auto restored = model::decode_legacy_role(*full_name, model::kItemCount);
        OL_CHECK(restored.has_value());
        if (restored.has_value()) {
            OL_CHECK(restored->name == state->name);
        }
    }
    state->name = u8"甲乙丙丁戊己";
    OL_CHECK(!model::encode_legacy_role(*state).has_value());
    state->name = u8"🙂";
    OL_CHECK(!model::encode_legacy_role(*state).has_value());
    state->name = std::u8string{u8'A', u8'\0', u8'B'};
    OL_CHECK(!model::encode_legacy_role(*state).has_value());

    constexpr std::array<std::uint8_t, 1U> incomplete{0xA4U};
    constexpr std::array<std::uint8_t, 2U> invalid{0xA4U, 0x20U};
    OL_CHECK(!text::decode_big5(text::Big5TextView{incomplete}).has_value());
    OL_CHECK(!text::decode_big5(text::Big5TextView{invalid}).has_value());
}

void check_big5_decode_aliases() {
    using namespace openlegend;

    struct AliasCase {
        std::uint16_t code;
        std::u8string_view expected;
    };
    constexpr auto cases = std::to_array<AliasCase>({
        {0xF9F9U, u8"\u2550"}, {0xF9E9U, u8"\u255E"}, {0xF9EBU, u8"\u2561"},
        {0xF9EAU, u8"\u256A"}, {0xF9FAU, u8"\u256D"}, {0xF9FBU, u8"\u256E"},
        {0xF9FDU, u8"\u256F"}, {0xF9FCU, u8"\u2570"}, {0xA2CCU, u8"十"},
        {0xA2CEU, u8"卅"},
    });
    for (const auto& item : cases) {
        const std::array<std::uint8_t, 2U> bytes{
            static_cast<std::uint8_t>(item.code >> 8U),
            static_cast<std::uint8_t>(item.code & 0xFFU)};
        OL_CHECK(text::decode_big5(text::Big5TextView{bytes}) == std::u8string{item.expected});
    }
    auto record = make_record();
    const auto name_begin = record.bytes.begin() + model::role_word::name_byte;
    std::fill_n(name_begin, model::role_word::name_bytes, 0U);
    record.bytes[model::role_word::name_byte] = 0xA2U;
    record.bytes[model::role_word::name_byte + 1U] = 0xCCU;
    const auto state = model::decode_legacy_role(record, model::kItemCount);
    OL_CHECK(state.has_value());
    if (state.has_value()) {
        OL_CHECK(state->name == u8"十");
        const auto encoded = model::encode_legacy_role(*state);
        OL_CHECK(encoded.has_value());
        if (encoded.has_value()) {
            OL_CHECK(encoded->bytes[model::role_word::name_byte] == 0xA4U);
            OL_CHECK(encoded->bytes[model::role_word::name_byte + 1U] == 0x51U);
        }
    }
}

void check_current_asset_records() {
    using namespace openlegend;

    const auto loaded = persistence::load_baseline_ranger(test::game_data_root());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    for (const auto& record : loaded.ranger->roles) {
        const auto state = model::decode_legacy_role(record, loaded.ranger->items.size());
        OL_CHECK(state.has_value());
        if (!state.has_value()) {
            continue;
        }
        const auto encoded = model::encode_legacy_role(*state);
        OL_CHECK(encoded.has_value());
        if (!encoded.has_value()) {
            continue;
        }
        for (std::size_t index = 0U; index < model::RoleRecord::word_count; ++index) {
            const auto byte_offset = index * 2U;
            if (byte_offset >= model::role_word::name_byte &&
                byte_offset < model::role_word::nickname_byte + model::role_word::nickname_bytes) {
                continue;
            }
            OL_CHECK(encoded->word(index) == record.word(index));
        }
        OL_CHECK(model::decode_legacy_role(*encoded, loaded.ranger->items.size()) == state);
    }
}

}

int main() {
    check_wide_values_and_history_initialization();
    check_legacy_encoding_boundaries();
    check_name_validation();
    check_big5_decode_aliases();
    check_current_asset_records();
    return openlegend::test::failures == 0 ? 0 : 1;
}
