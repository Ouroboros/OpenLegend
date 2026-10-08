#include "openlegend/model/checked_arithmetic.hpp"

#include <limits>

namespace openlegend::model {

std::optional<std::int64_t> checked_add(
    const std::int64_t left, const std::int64_t right) noexcept {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left > maximum - right) ||
        (right < 0 && left < minimum - right)) {
        return std::nullopt;
    }
    return left + right;
}

std::optional<std::int64_t> checked_subtract(
    const std::int64_t left, const std::int64_t right) noexcept {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((right > 0 && left < minimum + right) ||
        (right < 0 && left > maximum + right)) {
        return std::nullopt;
    }
    return left - right;
}

std::optional<std::int64_t> checked_multiply(
    const std::int64_t left, const std::int64_t right) noexcept {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if (left > 0) {
        if ((right > 0 && left > maximum / right) ||
            (right < 0 && right < minimum / left)) {
            return std::nullopt;
        }
    } else if (left < 0) {
        if ((right > 0 && left < minimum / right) ||
            (right < 0 && right < maximum / left)) {
            return std::nullopt;
        }
    }
    return left * right;
}

std::optional<std::int64_t> checked_divide(
    const std::int64_t numerator, const std::int64_t denominator) noexcept {
    if (denominator == 0 ||
        (numerator == std::numeric_limits<std::int64_t>::min() && denominator == -1)) {
        return std::nullopt;
    }
    return numerator / denominator;
}

}
