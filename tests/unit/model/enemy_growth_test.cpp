#include <array>
#include <cstdint>
#include <limits>

#include "openlegend/attributes.hpp"
#include "openlegend/model/enemy_growth.hpp"
#include "test_support.hpp"

namespace {

NODISCARD openlegend::model::RoleState baseline_role() {
    using namespace openlegend::model;
    RoleState baseline;
    baseline.maximum_hp = 600;
    baseline.maximum_mp = 500;
    baseline.attack = 70;
    baseline.defence = 90;
    baseline.use_poison = 60;
    baseline.anti_poison = 33;
    baseline.hidden_weapon = 45;
    baseline.level = 10;
    baseline.hp = 600;
    baseline.mp = 500;
    baseline.speed = 80;
    baseline.medicine = 50;
    baseline.magic_ids[0U] = MagicId{57};
    baseline.magic_ids[2U] = MagicId{49};
    baseline.magic_ids[3U] = MagicId{57};
    baseline.magic_ids[4U] = MagicId{-1};
    baseline.magic_levels = {99, 347, 1234, 1'000'099, 11, 0, 0, 0, 0, 0};
    return baseline;
}

void check_growth() {
    using namespace openlegend::model;
    const auto baseline = baseline_role();
    const auto original = baseline;
    const NewGamePlusConfiguration configuration;
    struct GrowthCase {
        std::int64_t playthrough;
        EnemyAttributes expected;
    };
    constexpr std::array cases{
        GrowthCase{1, {600, 500, 70, 90, 60, 33, 45, 10,
            {99, 347, 1234, 1'000'099, 11, 0, 0, 0, 0, 0}}},
        GrowthCase{2, {1599, 1499, 170, 190, 160, 133, 145, 40,
            {1099, 347, 2234, 1'001'099, 11, 0, 0, 0, 0, 0}}},
        GrowthCase{3, {2598, 2498, 270, 290, 260, 233, 245, 70,
            {2099, 347, 3234, 1'002'099, 11, 0, 0, 0, 0, 0}}},
        GrowthCase{999, {997'602, 997'502, 99'870, 99'890, 99'860, 99'833, 99'845, 29'950,
            {998'099, 347, 999'234, 1'998'099, 11, 0, 0, 0, 0, 0}}},
    };
    for (const auto& fixture : cases) {
        OL_CHECK(calculate_enemy_attributes(baseline, configuration, fixture.playthrough) == fixture.expected);
        OL_CHECK(baseline == original);
    }
    auto distinct = configuration;
    distinct.hp_cap_step = 7;
    distinct.mp_cap_step = 11;
    distinct.attack_cap_step = 13;
    distinct.defence_cap_step = 17;
    distinct.use_poison_cap_step = 19;
    distinct.anti_poison_cap_step = 23;
    distinct.hidden_weapon_cap_step = 29;
    distinct.role_level_step = 31;
    distinct.martial_level_step = 37;
    constexpr EnemyAttributes distinct_expected{
        614, 522, 96, 124, 98, 79, 103, 72,
        {7499, 347, 8634, 1'007'499, 11, 0, 0, 0, 0, 0}};
    OL_CHECK(calculate_enemy_attributes(baseline, distinct, 3) == distinct_expected);
    auto wide = configuration;
    wide.attack_cap_step = 5'000'000'000'000;
    wide.martial_level_step = 50'000'000'000;
    const auto result = calculate_enemy_attributes(baseline, wide, 3);
    OL_CHECK(result.has_value());
    if (result) {
        OL_CHECK(result->attack == 10'000'000'000'070);
        OL_CHECK(result->magic_levels[0U] == 10'000'000'000'099);
        OL_CHECK(result->magic_levels[2U] == 10'000'000'001'234);
    }
}

void check_negative_steps() {
    using namespace openlegend::model;
    auto baseline = baseline_role();
    baseline.magic_levels[0U] = 200;
    NewGamePlusConfiguration configuration;
    configuration.hp_cap_step = -300;
    configuration.mp_cap_step = -250;
    configuration.attack_cap_step = -35;
    configuration.defence_cap_step = -45;
    configuration.use_poison_cap_step = -30;
    configuration.anti_poison_cap_step = -16;
    configuration.hidden_weapon_cap_step = -22;
    configuration.role_level_step = -4;
    configuration.martial_level_step = -1;
    constexpr EnemyAttributes expected{
        0, 0, 0, 0, 0, 1, 1, 2,
        {0, 347, 1034, 999'899, 11, 0, 0, 0, 0, 0}};
    OL_CHECK(calculate_enemy_attributes(baseline, configuration, 3) == expected);
    OL_CHECK(!calculate_enemy_attributes(baseline, configuration, 4));
    auto invalid = configuration;
    invalid.role_level_step = -5;
    OL_CHECK(!calculate_enemy_attributes(baseline, invalid, 3));
    invalid = configuration;
    invalid.anti_poison_cap_step = -17;
    OL_CHECK(!calculate_enemy_attributes(baseline, invalid, 3));
    invalid = configuration;
    invalid.martial_level_step = -2;
    OL_CHECK(!calculate_enemy_attributes(baseline, invalid, 3));
}

void check_boundaries() {
    using namespace openlegend::model;
    const auto baseline = baseline_role();
    const auto original = baseline;
    NewGamePlusConfiguration configuration;
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr std::array steps{
        &NewGamePlusConfiguration::hp_cap_step, &NewGamePlusConfiguration::mp_cap_step,
        &NewGamePlusConfiguration::attack_cap_step, &NewGamePlusConfiguration::defence_cap_step,
        &NewGamePlusConfiguration::use_poison_cap_step, &NewGamePlusConfiguration::anti_poison_cap_step,
        &NewGamePlusConfiguration::hidden_weapon_cap_step, &NewGamePlusConfiguration::role_level_step,
    };
    for (const auto field : steps) {
        auto candidate = configuration;
        candidate.*field = maximum;
        OL_CHECK(calculate_enemy_attributes(baseline, candidate, 1).has_value());
        OL_CHECK(!calculate_enemy_attributes(baseline, candidate, 2));
        OL_CHECK(!calculate_enemy_attributes(baseline, candidate, 3));
        candidate.*field = minimum;
        OL_CHECK(!calculate_enemy_attributes(baseline, candidate, 2));
        OL_CHECK(!calculate_enemy_attributes(baseline, candidate, 3));
        OL_CHECK(baseline == original);
    }
    constexpr std::array attributes{
        &RoleState::maximum_hp, &RoleState::maximum_mp, &RoleState::attack,
        &RoleState::defence, &RoleState::use_poison, &RoleState::anti_poison,
        &RoleState::hidden_weapon, &RoleState::level,
    };
    for (const auto field : attributes) {
        auto invalid = baseline;
        invalid.*field = -1;
        OL_CHECK(!calculate_enemy_attributes(invalid, configuration, 1));
    }
    auto invalid = baseline;
    invalid.level = 0;
    OL_CHECK(!calculate_enemy_attributes(invalid, configuration, 2));
    invalid = baseline;
    invalid.magic_levels[9U] = -1;
    OL_CHECK(!calculate_enemy_attributes(invalid, configuration, 2));
    for (const auto invalid_id : {-2, 93}) {
        invalid = baseline;
        invalid.magic_ids[9U] = MagicId{static_cast<std::int16_t>(invalid_id)};
        OL_CHECK(!calculate_enemy_attributes(invalid, configuration, 2));
    }
    configuration.martial_level_step = maximum;
    OL_CHECK(calculate_enemy_attributes(baseline, configuration, 1).has_value());
    OL_CHECK(!calculate_enemy_attributes(baseline, configuration, 2));
    auto empty = baseline;
    empty.magic_ids.fill(MagicId{0});
    empty.magic_levels[9U] = maximum;
    const auto empty_result = calculate_enemy_attributes(empty, configuration, 999);
    OL_CHECK(empty_result.has_value());
    if (empty_result) {
        OL_CHECK(empty_result->magic_levels == empty.magic_levels);
    }
    configuration.martial_level_step = 1;
    empty.magic_ids[9U] = MagicId{57};
    OL_CHECK(!calculate_enemy_attributes(empty, configuration, 2));
    OL_CHECK(!calculate_enemy_attributes(baseline, configuration, 0));
    OL_CHECK(!calculate_enemy_attributes(baseline, configuration, 1000));
    configuration.hurt_cap_step = -100;
    OL_CHECK(!calculate_enemy_attributes(baseline, configuration, 2));
}

}

int main() {
    check_growth();
    check_negative_steps();
    check_boundaries();
    return openlegend::test::failures == 0 ? 0 : 1;
}
