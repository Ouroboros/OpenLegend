#include <array>
#include <cstdint>

#include "openlegend/model/new_game.hpp"
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
    OL_CHECK(role.word(model::role_word::maximum_hp) == 34);
    OL_CHECK(role.word(model::role_word::hp) == 34);
    OL_CHECK(role.word(model::role_word::mp) == 29);
    OL_CHECK(role.word(model::role_word::iq) == 68);
    OL_CHECK(random.next() == 26'233U);
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

}  // namespace

int main() {
    check_seed_zero_roll();
    check_cheat();
    check_name_transport();
    return openlegend::test::failures == 0 ? 0 : 1;
}
