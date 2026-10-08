#include <cstdint>
#include <limits>

#include "openlegend/model/checked_arithmetic.hpp"
#include "test_support.hpp"

namespace {

constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

void check_addition() {
    using openlegend::model::checked_add;

    OL_CHECK(checked_add(52'000, 20'000) == 72'000);
    OL_CHECK(checked_add(4'992'500, 55'938'400) == 60'930'900);
    OL_CHECK(checked_add(maximum, 0) == maximum);
    OL_CHECK(checked_add(minimum, 0) == minimum);
    OL_CHECK(checked_add(maximum - 1, 1) == maximum);
    OL_CHECK(checked_add(minimum + 1, -1) == minimum);
    OL_CHECK(checked_add(maximum, minimum) == -1);
    OL_CHECK(!checked_add(maximum, 1).has_value());
    OL_CHECK(!checked_add(minimum, -1).has_value());
    OL_CHECK(!checked_add(maximum, maximum).has_value());
    OL_CHECK(!checked_add(minimum, minimum).has_value());
}

void check_subtraction() {
    using openlegend::model::checked_subtract;

    OL_CHECK(checked_subtract(72'000, 52'000) == 20'000);
    OL_CHECK(checked_subtract(maximum, maximum) == 0);
    OL_CHECK(checked_subtract(minimum, minimum) == 0);
    OL_CHECK(checked_subtract(minimum, 0) == minimum);
    OL_CHECK(checked_subtract(maximum, 0) == maximum);
    OL_CHECK(checked_subtract(minimum, -1) == minimum + 1);
    OL_CHECK(checked_subtract(-1, minimum) == maximum);
    OL_CHECK(checked_subtract(maximum - 1, -1) == maximum);
    OL_CHECK(!checked_subtract(maximum, -1).has_value());
    OL_CHECK(!checked_subtract(minimum, 1).has_value());
    OL_CHECK(!checked_subtract(0, minimum).has_value());
    OL_CHECK(!checked_subtract(maximum, minimum).has_value());
}

void check_multiplication() {
    using openlegend::model::checked_multiply;

    OL_CHECK(checked_multiply(800, 7) == 5'600);
    OL_CHECK(checked_multiply(5'600, 9'989) == 55'938'400);
    OL_CHECK(checked_multiply(99, 3'000) == 297'000);
    OL_CHECK(checked_multiply(-70, 999) == -69'930);
    OL_CHECK(checked_multiply(999, -70) == -69'930);
    OL_CHECK(checked_multiply(-70, -999) == 69'930);
    OL_CHECK(checked_multiply(minimum, 1) == minimum);
    OL_CHECK(checked_multiply(1, minimum) == minimum);
    OL_CHECK(checked_multiply(maximum, 1) == maximum);
    OL_CHECK(checked_multiply(maximum, -1) == -maximum);
    OL_CHECK(checked_multiply(minimum, 0) == 0);
    OL_CHECK(checked_multiply(0, minimum) == 0);
    OL_CHECK(checked_multiply(maximum / 2, 2) == maximum - 1);
    OL_CHECK(checked_multiply(minimum / 2, 2) == minimum);
    OL_CHECK(!checked_multiply(maximum / 2 + 1, 2).has_value());
    OL_CHECK(!checked_multiply(minimum / 2 - 1, 2).has_value());
    OL_CHECK(!checked_multiply(minimum, -1).has_value());
    OL_CHECK(!checked_multiply(-1, minimum).has_value());
    OL_CHECK(!checked_multiply(minimum, minimum).has_value());
    OL_CHECK(!checked_multiply(maximum, maximum).has_value());
}

void check_division() {
    using openlegend::model::checked_divide;

    OL_CHECK(checked_divide(99, 20) == 4);
    OL_CHECK(checked_divide(-99, 20) == -4);
    OL_CHECK(checked_divide(99, -20) == -4);
    OL_CHECK(checked_divide(-99, -20) == 4);
    OL_CHECK(checked_divide(minimum, 1) == minimum);
    OL_CHECK(checked_divide(maximum, 1) == maximum);
    OL_CHECK(checked_divide(minimum, minimum) == 1);
    OL_CHECK(checked_divide(0, minimum) == 0);
    OL_CHECK(!checked_divide(1, 0).has_value());
    OL_CHECK(!checked_divide(0, 0).has_value());
    OL_CHECK(!checked_divide(minimum, -1).has_value());
}

}

int main() {
    check_addition();
    check_subtraction();
    check_multiplication();
    check_division();
    return openlegend::test::failures == 0 ? 0 : 1;
}
