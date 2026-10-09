#include "openlegend/model/poison.hpp"

#include <algorithm>

#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {

std::optional<PoisonApplication> poison_application(
    const std::int64_t power, const std::int64_t resistance, const std::int64_t divisor,
    const std::int64_t current_poison, const std::int64_t maximum_hp) noexcept {
    if (resistance < 0 || divisor <= 0 || current_poison < 0 || current_poison > 99 || maximum_hp < 0) {
        return std::nullopt;
    }
    PoisonApplication result;
    if (power <= resistance) {
        return result;
    }
    const auto difference = power - resistance;
    const auto scaled_depth = checked_multiply(99, difference);
    if (!scaled_depth.has_value()) {
        return std::nullopt;
    }
    result.maximum_depth = *scaled_depth / power;
    result.theoretical_amount = difference / divisor;
    result.applied_amount = std::max<std::int64_t>(
        0, std::min(result.theoretical_amount, result.maximum_depth - current_poison));
    result.overflow = result.theoretical_amount - result.applied_amount;
    const auto proportional_damage = checked_multiply(maximum_hp, result.overflow);
    const auto proportional_limit = checked_multiply(maximum_hp, result.maximum_depth);
    if (!proportional_damage.has_value() || !proportional_limit.has_value()) {
        return std::nullopt;
    }
    result.hp_damage = std::min(
        std::max(result.overflow / 10, *proportional_damage / 1000),
        std::max(result.maximum_depth / 10, *proportional_limit / 1000));
    return result;
}

std::optional<std::int64_t> poison_round_damage(
    const std::int64_t poison, const std::int64_t maximum_hp) noexcept {
    if (poison < 0 || poison > 99 || maximum_hp < 0) {
        return std::nullopt;
    }
    const auto proportional = checked_multiply(maximum_hp, poison);
    return proportional.has_value()
        ? std::optional<std::int64_t>{std::max(poison / 10, *proportional / 1000)}
        : std::nullopt;
}

std::optional<std::int64_t> detoxification_amount(
    const std::int64_t ability, const std::int64_t poison, random::LegacyRandom& random) noexcept {
    if (poison < 0 || poison > 99) {
        return std::nullopt;
    }
    const auto nonnegative_ability = std::max<std::int64_t>(ability, 0);
    const auto scaled_limit = checked_multiply(99, nonnegative_ability);
    const auto denominator = checked_add(nonnegative_ability, 100);
    if (!scaled_limit.has_value() || !denominator.has_value()) {
        return std::nullopt;
    }
    auto candidate_random = random;
    const auto first_roll = candidate_random.bounded(10);
    const auto second_roll = candidate_random.bounded(10);
    const auto theoretical = std::clamp<std::int64_t>(
        nonnegative_ability / 3 + first_roll - second_roll, 0, 99);
    const auto result = std::min({theoretical, *scaled_limit / *denominator, poison});
    random = candidate_random;
    return result;
}

}
