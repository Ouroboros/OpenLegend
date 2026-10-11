#include <array>
#include <compare>
#include <cstdint>
#include <limits>

#include "openlegend/model/hurt.hpp"
#include "test_support.hpp"

namespace {

void check_percentage_boundaries() {
    using namespace openlegend::model;
    struct Case {
        std::int64_t hurt;
        std::int64_t denominator;
        std::int64_t percentage;
        std::strong_ordering expected;
    };
    using Order = std::strong_ordering;
    constexpr std::array cases{
        Case{32, 100, 33, Order::less}, Case{33, 100, 33, Order::equal},
        Case{34, 100, 33, Order::greater}, Case{65, 200, 33, Order::less},
        Case{66, 200, 33, Order::equal}, Case{67, 200, 33, Order::greater},
        Case{99, 200, 50, Order::less}, Case{100, 200, 50, Order::equal},
        Case{101, 200, 50, Order::greater}, Case{80, 200, 40, Order::equal},
        Case{81, 200, 40, Order::greater}, Case{55, 110, 50, Order::equal},
        Case{56, 110, 50, Order::greater}, Case{36, 110, 33, Order::less},
        Case{37, 110, 33, Order::greater}, Case{99, 300, 33, Order::equal},
        Case{100, 300, 33, Order::greater}, Case{0, 0, 33, Order::less},
        Case{0, 0, 50, Order::less}, Case{0, 0, 0, Order::equal},
        Case{2'500'000'000'000, 5'000'000'000'000, 50, Order::equal},
        Case{2'500'000'000'001, 5'000'000'000'000, 50, Order::greater},
    };
    for (const auto& entry : cases) {
        const auto actual = compare_hurt_percentage(
            entry.hurt, entry.denominator, entry.percentage);
        OL_CHECK(actual.has_value());
        if (actual.has_value()) {
            OL_CHECK(*actual == entry.expected);
        }
    }
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(!compare_hurt_percentage(-1, 100, 33).has_value());
    OL_CHECK(!compare_hurt_percentage(0, -1, 33).has_value());
    OL_CHECK(!compare_hurt_percentage(0, 100, -1).has_value());
    OL_CHECK(!compare_hurt_percentage(1, 0, 33).has_value());
    OL_CHECK(!compare_hurt_percentage(maximum / 100 + 1, 100, 33).has_value());
    OL_CHECK(!compare_hurt_percentage(0, maximum / 33 + 1, 33).has_value());
    OL_CHECK(compare_hurt_percentage(maximum / 100, 100, 33) == Order::greater);
}

void check_color_bands() {
    using namespace openlegend::model;
    struct Case {
        std::int64_t hurt;
        std::int64_t denominator;
        HurtBand band;
    };
    constexpr std::array cases{
        Case{0, 0, HurtBand::low}, Case{33, 100, HurtBand::low},
        Case{34, 100, HurtBand::moderate}, Case{66, 100, HurtBand::moderate},
        Case{67, 100, HurtBand::severe}, Case{66, 200, HurtBand::low},
        Case{67, 200, HurtBand::moderate}, Case{132, 200, HurtBand::moderate},
        Case{133, 200, HurtBand::severe}, Case{198, 300, HurtBand::moderate},
        Case{199, 300, HurtBand::severe}, Case{36, 110, HurtBand::low},
        Case{37, 110, HurtBand::moderate}, Case{72, 110, HurtBand::moderate},
        Case{73, 110, HurtBand::severe},
        Case{1'650'000'000'000, 5'000'000'000'000, HurtBand::low},
        Case{1'650'000'000'001, 5'000'000'000'000, HurtBand::moderate},
        Case{3'300'000'000'000, 5'000'000'000'000, HurtBand::moderate},
        Case{3'300'000'000'001, 5'000'000'000'000, HurtBand::severe},
    };
    for (const auto& entry : cases) {
        OL_CHECK(hurt_band(entry.hurt, entry.denominator) == entry.band);
    }
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(!hurt_band(0, maximum / 66 + 1).has_value());
    OL_CHECK(!hurt_band(-1, 100).has_value());
    OL_CHECK(!hurt_band(1, 0).has_value());
}

void check_action_penalty() {
    using namespace openlegend::model;
    struct Case {
        std::int64_t hurt;
        std::int64_t denominator;
        std::int64_t penalty;
    };
    constexpr std::array cases{
        Case{0, 100, 0}, Case{39, 100, 0}, Case{40, 100, 1},
        Case{79, 100, 1}, Case{80, 100, 2}, Case{99, 100, 2},
        Case{79, 200, 0}, Case{80, 200, 1}, Case{159, 200, 1},
        Case{160, 200, 2}, Case{199, 200, 2}, Case{299, 300, 2},
        Case{499, 500, 2}, Case{43, 110, 0}, Case{44, 110, 1},
        Case{87, 110, 1}, Case{88, 110, 2}, Case{0, 0, 0},
        Case{1'999'999'999'999, 5'000'000'000'000, 0},
        Case{2'000'000'000'000, 5'000'000'000'000, 1},
        Case{4'999'999'999'999, 5'000'000'000'000, 2},
    };
    for (const auto& entry : cases) {
        OL_CHECK(hurt_action_penalty(entry.hurt, entry.denominator) == entry.penalty);
    }
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(!hurt_action_penalty(-1, 100).has_value());
    OL_CHECK(!hurt_action_penalty(0, -1).has_value());
    OL_CHECK(!hurt_action_penalty(1, 0).has_value());
    OL_CHECK(!hurt_action_penalty(maximum / 100 + 1, 100).has_value());
    OL_CHECK(!hurt_action_penalty(0, maximum / 40 + 1).has_value());
}

void check_gain_and_hp_loss() {
    using namespace openlegend::model;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    struct GainCase {
        std::int64_t damage;
        std::int64_t maximum_hp;
        std::int64_t maximum_mp;
        std::int64_t divisor;
        std::int64_t gain;
    };
    constexpr std::array gains{
        GainCase{0, 29, 8, kMagicHurtDivisor, 0}, GainCase{8, 29, 8, kMagicHurtDivisor, 10},
        GainCase{14, 29, 8, kMagicHurtDivisor, 18}, GainCase{1000, 29, 8, kMagicHurtDivisor, 99},
        GainCase{8, 29, 8, kThrowingHurtDivisor, 26}, GainCase{100, 29, 8, kThrowingHurtDivisor, 99},
        GainCase{14, 1000, 1000, kThrowingHurtDivisor, 0}, GainCase{100, 992, 999, kMagicHurtDivisor, 2},
        GainCase{1, 0, 0, kMagicHurtDivisor, 99},
        GainCase{2'500'000'000'000, 5'000'000'000'000, 5'000'000'000'000, kMagicHurtDivisor, 12},
        GainCase{maximum, maximum / 2, maximum / 2, kMagicHurtDivisor, 49},
        GainCase{maximum, maximum, maximum, kMagicHurtDivisor, 24},
    };
    for (const auto& entry : gains) {
        OL_CHECK(hurt_gain(entry.damage, entry.maximum_hp, entry.maximum_mp, entry.divisor) == entry.gain);
    }
    OL_CHECK(!hurt_gain(-1, 29, 8, kMagicHurtDivisor).has_value());
    OL_CHECK(!hurt_gain(1, -1, 8, kMagicHurtDivisor).has_value());
    OL_CHECK(!hurt_gain(1, 29, -1, kMagicHurtDivisor).has_value());
    OL_CHECK(!hurt_gain(1, 29, 8, 0).has_value());

    struct LossCase {
        std::int64_t maximum_hp;
        std::int64_t hurt;
        std::int64_t loss;
    };
    constexpr std::array losses{
        LossCase{29, 99, 1}, LossCase{20, 99, 0}, LossCase{992, 99, 49},
        LossCase{4988, 99, 246}, LossCase{5'000'000'000'000, 99, 247'500'000'000},
        LossCase{maximum, 99, 456'556'915'824'311'402}, LossCase{0, 99, 0}, LossCase{992, 0, 0},
        LossCase{100, -20, -1}, LossCase{0, -1, 0},
    };
    for (const auto& entry : losses) {
        OL_CHECK(hurt_hp_loss(entry.maximum_hp, entry.hurt) == entry.loss);
    }
    OL_CHECK(!hurt_hp_loss(-1, 0).has_value());
    OL_CHECK(!hurt_hp_loss(maximum, maximum).has_value());
}

}

int main() {
    check_percentage_boundaries();
    check_color_bands();
    check_action_penalty();
    check_gain_and_hp_loss();
    return openlegend::test::failures == 0 ? 0 : 1;
}
