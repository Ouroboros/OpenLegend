#include <array>
#include <cstdint>
#include <limits>

#include "openlegend/model/medicine.hpp"
#include "test_support.hpp"

namespace {

void check_treatment() {
    using namespace openlegend;
    struct Case {
        std::int64_t ability;
        std::int64_t hurt;
        std::int64_t maximum;
        std::array<std::int64_t, 5> amounts;
    };
    constexpr std::array cases{
        Case{80, 0, 99, {64, 65, 66, 67, 68}},
        Case{80, 25, 99, {61, 62, 63, 64, 65}},
        Case{80, 26, 99, {61, 62, 63, 64, 65}},
        Case{80, 40, 99, {57, 58, 59, 60, 61}},
        Case{80, 50, 99, {53, 54, 55, 56, 57}},
        Case{80, 51, 99, {52, 53, 54, 55, 56}},
        Case{80, 75, 99, {35, 36, 37, 38, 39}},
        Case{80, 76, 99, {34, 35, 36, 37, 38}},
        Case{100, 90, 99, {24, 25, 26, 27, 28}},
        Case{100, 95, 99, {16, 17, 18, 19, 20}},
        Case{100, 99, 99, {10, 11, 12, 13, 14}},
        Case{81, 99, 99, {9, 9, 10, 11, 12}},
        Case{20, 40, 99, {14, 15, 16, 17, 18}},
        Case{20, 41, 99, {0, 0, 0, 0, 0}},
        Case{100, 120, 199, {59, 60, 61, 62, 63}},
        Case{100, 121, 199, {0, 0, 0, 0, 0}},
        Case{200, 199, 199, {20, 21, 22, 23, 24}},
        Case{300, 299, 299, {30, 31, 32, 33, 34}},
        Case{500, 499, 499, {50, 51, 52, 53, 54}},
        Case{5'000'000'000'000, 100, 199,
             {3'336'027'629'371, 3'336'027'629'372, 3'336'027'629'373,
              3'336'027'629'374, 3'336'027'629'375}},
        Case{5'000'000'000'000, 0, 0,
             {4'000'000'000'000, 4'000'000'000'001, 4'000'000'000'002,
              4'000'000'000'003, 4'000'000'000'004}},
        Case{0, 20, 99, {0, 1, 2, 3, 4}},
        Case{-1, 20, 99, {0, 1, 2, 3, 4}},
        Case{-1, 21, 99, {0, 0, 0, 0, 0}},
    };
    for (const auto& entry : cases) {
        std::array<bool, 5> seen{};
        for (std::uint32_t seed = 0; seed < 100U; ++seed) {
            random::LegacyRandom expected{seed};
            const auto variation = static_cast<std::size_t>(expected.bounded(5));
            seen[variation] = true;
            random::LegacyRandom actual{seed};
            OL_CHECK(model::medicine_amount(entry.ability, entry.hurt, entry.maximum, actual) ==
                     entry.amounts[variation]);
            OL_CHECK(actual.state() == expected.state());
        }
        for (const auto observed : seen) {
            OL_CHECK(observed);
        }
    }
    random::LegacyRandom random{1U};
    OL_CHECK(model::medicine_amount(7'000'000'000'000'000'000, 0, 99, random) ==
             5'600'000'000'000'000'003);
    OL_CHECK(random.state() == 1'103'527'590U);
}

void check_invalid_treatment() {
    using namespace openlegend;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    random::LegacyRandom random{1U};
    for (const auto values : std::array{
             std::array<std::int64_t, 3>{100, -1, 99},
             std::array<std::int64_t, 3>{100, 100, 99},
             std::array<std::int64_t, 3>{100, 0, -1},
             std::array<std::int64_t, 3>{100, 1, 0},
             std::array<std::int64_t, 3>{maximum, 0, 99}}) {
        OL_CHECK(!model::medicine_amount(values[0], values[1], values[2], random).has_value());
        OL_CHECK(random.state() == 1U);
    }
    OL_CHECK(model::medicine_allowed(20, 40) == true);
    OL_CHECK(model::medicine_allowed(20, 41) == false);
    OL_CHECK(model::medicine_allowed(100, 120) == true);
    OL_CHECK(model::medicine_allowed(100, 121) == false);
    OL_CHECK(model::medicine_allowed(1000, 1200) == true);
    OL_CHECK(model::medicine_allowed(1000, 1201) == false);
    OL_CHECK(!model::medicine_allowed(maximum, 0).has_value());
    OL_CHECK(!model::medicine_allowed(100, -1).has_value());
}

}

int main() {
    check_treatment();
    check_invalid_treatment();
    return openlegend::test::failures == 0 ? 0 : 1;
}
