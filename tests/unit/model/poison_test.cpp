#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

#include "openlegend/model/poison.hpp"
#include "test_support.hpp"

namespace {

void check_application() {
    using namespace openlegend::model;
    struct Case {
        std::int64_t power;
        std::int64_t resistance;
        std::int64_t divisor;
        std::int64_t poison;
        std::int64_t maximum_hp;
        PoisonApplication expected;
    };
    constexpr std::array cases{
        Case{40, 0, 4, 0, 100, {99, 10, 10, 0, 0}},
        Case{400, 0, 4, 0, 1000, {99, 100, 99, 1, 1}},
        Case{400, 0, 4, 99, 1000, {99, 100, 0, 100, 99}},
        Case{100, 80, 4, 40, 5000, {19, 5, 0, 5, 25}},
        Case{100, 90, 4, 0, 1000, {9, 2, 2, 0, 0}},
        Case{120, 0, 15, 99, 5'000'000'000'000, {99, 8, 0, 8, 40'000'000'000}},
        Case{104, 0, 2, 98, 32, {99, 52, 1, 51, 5}},
        Case{400, 0, 4, 0, 32, {99, 100, 99, 1, 0}},
        Case{100, 99, 2, 0, 1000, {0, 0, 0, 0, 0}},
        Case{100, 100, 4, 99, 1000, {}},
        Case{-1, 0, 4, 0, 1000, {}},
    };
    for (const auto& entry : cases) {
        const auto actual = poison_application(
            entry.power, entry.resistance, entry.divisor, entry.poison, entry.maximum_hp);
        OL_CHECK(actual.has_value());
        if (!actual.has_value()) {
            continue;
        }
        OL_CHECK(actual->maximum_depth == entry.expected.maximum_depth);
        OL_CHECK(actual->theoretical_amount == entry.expected.theoretical_amount);
        OL_CHECK(actual->applied_amount == entry.expected.applied_amount);
        OL_CHECK(actual->overflow == entry.expected.overflow);
        OL_CHECK(actual->hp_damage == entry.expected.hp_damage);
        OL_CHECK(entry.poison + actual->applied_amount <= 99);
    }
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(!poison_application(maximum, 0, 4, 0, 100).has_value());
    OL_CHECK(!poison_application(400, 0, 4, 99, maximum).has_value());
    OL_CHECK(!poison_application(100, -1, 4, 0, 100).has_value());
    OL_CHECK(!poison_application(100, 0, 0, 0, 100).has_value());
    OL_CHECK(!poison_application(100, 0, 4, 100, 100).has_value());
    OL_CHECK(!poison_application(100, 0, 4, 0, -1).has_value());
    OL_CHECK(poison_application(maximum, maximum, 4, 99, maximum)->hp_damage == 0);
    OL_CHECK(poison_round_damage(99, 32) == 9);
    OL_CHECK(poison_round_damage(99, 716) == 70);
    OL_CHECK(poison_round_damage(9, 100) == 0);
    OL_CHECK(poison_round_damage(1, 5'000'000'000'000) == 5'000'000'000);
    OL_CHECK(poison_round_damage(0, maximum) == 0);
    OL_CHECK(!poison_round_damage(99, maximum).has_value());
    OL_CHECK(!poison_round_damage(100, 716).has_value());
    OL_CHECK(!poison_round_damage(-1, 716).has_value());
}

void check_detoxification() {
    using namespace openlegend;
    std::array<bool, 100> seen{};
    std::size_t combinations = 0U;
    std::size_t zero_results = 0U;
    std::int64_t largest_at_197 = 0;
    for (std::uint32_t seed = 0U; seed < 10'000U && combinations < seen.size(); ++seed) {
        random::LegacyRandom expected_random{seed};
        const auto first = expected_random.bounded(10);
        const auto second = expected_random.bounded(10);
        const auto index = static_cast<std::size_t>(first * 10 + second);
        if (seen[index]) {
            continue;
        }
        seen[index] = true;
        ++combinations;
        random::LegacyRandom actual_random{seed};
        const auto amount = model::detoxification_amount(22, 99, actual_random);
        OL_CHECK(amount.has_value());
        OL_CHECK(actual_random.state() == expected_random.state());
        if (amount == 0) {
            ++zero_results;
        }
        actual_random.seed(seed);
        OL_CHECK(model::detoxification_amount(300, 99, actual_random) == 74);
        OL_CHECK(actual_random.state() == expected_random.state());
        actual_random.seed(seed);
        OL_CHECK(model::detoxification_amount(1000, 99, actual_random) == 90);
        actual_random.seed(seed);
        OL_CHECK(model::detoxification_amount(1000, 3, actual_random) == 3);
        actual_random.seed(seed);
        OL_CHECK(model::detoxification_amount(-1, 99, actual_random) == 0);
        OL_CHECK(actual_random.state() == expected_random.state());
        actual_random.seed(seed);
        largest_at_197 = std::max(largest_at_197,
            *model::detoxification_amount(197, 99, actual_random));
    }
    OL_CHECK(combinations == 100U);
    OL_CHECK(zero_results == 6U);
    OL_CHECK(largest_at_197 == 65);

    random::LegacyRandom actual_random{1U};
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(!model::detoxification_amount(maximum, 99, actual_random).has_value());
    OL_CHECK(actual_random.state() == 1U);
    OL_CHECK(!model::detoxification_amount(100, 100, actual_random).has_value());
    OL_CHECK(actual_random.state() == 1U);
    OL_CHECK(model::detoxification_amount(maximum / 99, 99, actual_random) == 98);
    OL_CHECK(actual_random.state() == 2'524'885'223U);
}

}

int main() {
    check_application();
    check_detoxification();
    return openlegend::test::failures == 0 ? 0 : 1;
}
