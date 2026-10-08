#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "openlegend/model/new_game.hpp"
#include "openlegend/text/big5.hpp"
#include "test_support.hpp"

namespace {

void check_seed_zero_roll() {
    using namespace openlegend;

    model::RoleRecord role;
    role.set_word(model::role_word::level, 1);
    random::LegacyRandom random{0U};
    model::roll_protagonist_attributes(role, random);

    OL_CHECK(role.word(model::role_word::mp_type) == 0);
    OL_CHECK(role.word(model::role_word::maximum_mp) == 29);
    OL_CHECK(role.word(model::role_word::attack) == 29);
    OL_CHECK(role.word(model::role_word::speed) == 28);
    OL_CHECK(role.word(model::role_word::defence) == 29);
    OL_CHECK(role.word(model::role_word::medicine) == 28);
    OL_CHECK(role.word(model::role_word::use_poison) == 26);
    OL_CHECK(role.word(model::role_word::detoxification) == 22);
    OL_CHECK(role.word(model::role_word::anti_poison) == 23);
    OL_CHECK(role.word(model::role_word::fist) == 21);
    OL_CHECK(role.word(model::role_word::sword) == 22);
    OL_CHECK(role.word(model::role_word::knife) == 23);
    OL_CHECK(role.word(model::role_word::unusual) == 30);
    OL_CHECK(role.word(model::role_word::hidden_weapon) == 29);
    OL_CHECK(role.word(model::role_word::increased_life) == 5);
    OL_CHECK(role.word(model::role_word::maximum_hp) == 44);
    OL_CHECK(role.word(model::role_word::hp) == 44);
    OL_CHECK(role.word(model::role_word::mp) == 29);
    OL_CHECK(role.word(model::role_word::iq) == 68);
    OL_CHECK(random.next() == 26'233U);
}

void check_level_multiplier() {
    using namespace openlegend;

    model::RoleRecord role;
    role.set_word(model::role_word::level, 2);
    random::LegacyRandom random{0U};
    model::roll_protagonist_attributes(role, random);
    OL_CHECK(role.word(model::role_word::increased_life) == 5);
    OL_CHECK(role.word(model::role_word::maximum_hp) == 59);
    OL_CHECK(role.word(model::role_word::hp) == 59);
}

void check_cheat() {
    using namespace openlegend::model;

    RoleRecord role;
    apply_baberuth_attributes(role);
    OL_CHECK(role.word(role_word::mp_type) == 2);
    OL_CHECK(role.word(role_word::maximum_mp) == 40);
    OL_CHECK(role.word(role_word::attack) == 30);
    OL_CHECK(role.word(role_word::speed) == 30);
    OL_CHECK(role.word(role_word::defence) == 30);
    OL_CHECK(role.word(role_word::medicine) == 30);
    OL_CHECK(role.word(role_word::use_poison) == 30);
    OL_CHECK(role.word(role_word::detoxification) == 30);
    OL_CHECK(role.word(role_word::anti_poison) == 30);
    OL_CHECK(role.word(role_word::fist) == 30);
    OL_CHECK(role.word(role_word::sword) == 30);
    OL_CHECK(role.word(role_word::knife) == 30);
    OL_CHECK(role.word(role_word::unusual) == 30);
    OL_CHECK(role.word(role_word::hidden_weapon) == 30);
    OL_CHECK(role.word(role_word::increased_life) == 10);
    OL_CHECK(role.word(role_word::maximum_hp) == 50);
    OL_CHECK(role.word(role_word::hp) == 50);
    OL_CHECK(role.word(role_word::mp) == 40);
    OL_CHECK(role.word(role_word::iq) == 100);
}

void check_name_transport() {
    using namespace openlegend::model;

    RangerState ranger;
    ranger.roles[0].bytes.fill(0xA5U);
    constexpr std::array<std::uint8_t, 4> name{0xA5U, 0x44U, 0xA4U, 0x6AU};
    OL_CHECK(set_protagonist_name(ranger, name));
    for (std::size_t index = 0U; index < name.size(); ++index) {
        OL_CHECK(ranger.roles[0].bytes[role_word::name_byte + index] == name[index]);
    }
    for (std::size_t index = name.size(); index < role_word::name_bytes; ++index) {
        OL_CHECK(ranger.roles[0].bytes[role_word::name_byte + index] == 0U);
    }
    OL_CHECK(ranger.roles[0].bytes[role_word::name_byte - 1U] == 0xA5U);
    OL_CHECK(ranger.roles[0].bytes[role_word::name_byte + role_word::name_bytes] == 0xA5U);

    constexpr std::array<std::uint8_t, 7> too_long{};
    constexpr std::array<std::uint8_t, 2> embedded_zero{'A', 0U};
    OL_CHECK(!set_protagonist_name(ranger, {}));
    OL_CHECK(!set_protagonist_name(ranger, too_long));
    OL_CHECK(!set_protagonist_name(ranger, embedded_zero));
}

void check_logical_attribute_generation() {
    using namespace openlegend;

    constexpr std::array<std::uint32_t, 4> seeds{0U, 1U, 3U, 0xFFFFFFFFU};
    for (const auto seed : seeds) {
        model::RoleRecord record;
        record.set_word(model::role_word::level, 1);
        auto role = model::decode_legacy_role(record, model::kItemCount).value();
        random::LegacyRandom legacy_random{seed};
        random::LegacyRandom logical_random{seed};
        model::roll_protagonist_attributes(record, legacy_random);
        model::roll_protagonist_attributes(role, logical_random);
        OL_CHECK(model::encode_legacy_role(role) == record);
        OL_CHECK(logical_random.state() == legacy_random.state());
        model::apply_baberuth_attributes(record);
        model::apply_baberuth_attributes(role);
        OL_CHECK(model::encode_legacy_role(role) == record);
    }

    model::RoleState role;
    role.level = 1'000'000'000;
    random::LegacyRandom random{0U};
    model::roll_protagonist_attributes(role, random);
    OL_CHECK(role.maximum_hp == 15'000'000'029);
    OL_CHECK(role.hp == role.maximum_hp);
    role.level = std::numeric_limits<std::int64_t>::max();
    const auto before = role;
    const auto before_random = random.state();
    bool rejected = false;
    try {
        model::roll_protagonist_attributes(role, random);
    } catch (const std::overflow_error&) {
        rejected = true;
    }
    OL_CHECK(rejected);
    OL_CHECK(role == before);
    OL_CHECK(random.state() == before_random);
}

void check_logical_name_transport() {
    using namespace openlegend;

    model::RuntimeRangerState ranger;
    constexpr std::array<std::uint8_t, 4> name{0xA5U, 0x44U, 0xA4U, 0x6AU};
    OL_CHECK(model::set_protagonist_name(ranger, name));
    const auto expected = text::decode_big5(text::Big5TextView{name}).value();
    OL_CHECK(ranger.roles[0].name == expected);
    constexpr std::array<std::uint8_t, 2> invalid{0xA5U, 0U};
    OL_CHECK(!model::set_protagonist_name(ranger, invalid));
    OL_CHECK(ranger.roles[0].name == expected);
}

}  // namespace

int main() {
    check_seed_zero_roll();
    check_level_multiplier();
    check_cheat();
    check_name_transport();
    check_logical_attribute_generation();
    check_logical_name_transport();
    return openlegend::test::failures == 0 ? 0 : 1;
}
