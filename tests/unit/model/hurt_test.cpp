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

}

int main() {
    check_percentage_boundaries();
    check_color_bands();
    return openlegend::test::failures == 0 ? 0 : 1;
}
