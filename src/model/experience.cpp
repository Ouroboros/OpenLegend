#include "openlegend/model/experience.hpp"

#include <cstddef>
#include <limits>

#include "openlegend/attributes.hpp"
#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {
namespace {

NODISCARD std::optional<std::int64_t> requirement(
    const std::span<const std::uint16_t> thresholds,
    const std::int64_t level) noexcept {
    const auto original_levels = static_cast<std::int64_t>(thresholds.size());
    if (level <= original_levels) {
        return thresholds[static_cast<std::size_t>(level - 1)];
    }
    const auto last_index = thresholds.size() - 1U;
    const auto last_cost = std::int64_t{thresholds[last_index]} - thresholds[last_index - 1U];
    const auto previous_cost = std::int64_t{thresholds[last_index - 1U]} - thresholds[last_index - 2U];
    const auto step = last_cost - previous_cost;
    const auto extra_levels = level - original_levels;
    const auto base_cost = checked_multiply(extra_levels, last_cost);
    if (!base_cost.has_value()) {
        return std::nullopt;
    }
    std::int64_t growing_cost = 0;
    if (step != 0) {
        auto left_factor = extra_levels;
        auto right_factor = extra_levels + 1;
        if (left_factor % 2 == 0) {
            left_factor /= 2;
        } else {
            right_factor /= 2;
        }
        const auto triangular = checked_multiply(left_factor, right_factor);
        if (!triangular.has_value()) {
            return std::nullopt;
        }
        const auto scaled = checked_multiply(*triangular, step);
        if (!scaled.has_value()) {
            return std::nullopt;
        }
        growing_cost = *scaled;
    }
    const auto extra_cost = checked_add(*base_cost, growing_cost);
    return extra_cost.has_value()
        ? checked_add(thresholds.back(), *extra_cost) : std::nullopt;
}

}

bool experience_thresholds_valid(const std::span<const std::uint16_t> thresholds) noexcept {
    if (thresholds.size() < 3U || thresholds.front() != 0) {
        return false;
    }
    for (std::size_t index = 1U; index < thresholds.size(); ++index) {
        if (thresholds[index] <= thresholds[index - 1U]) {
            return false;
        }
    }
    const auto last_index = thresholds.size() - 1U;
    const auto last_cost = std::int64_t{thresholds[last_index]} - thresholds[last_index - 1U];
    const auto previous_cost = std::int64_t{thresholds[last_index - 1U]} - thresholds[last_index - 2U];
    return last_cost >= previous_cost;
}

std::optional<std::int64_t> level_experience_requirement(
    const std::span<const std::uint16_t> thresholds,
    const std::int64_t level) noexcept {
    if (level < 1 || !experience_thresholds_valid(thresholds)) {
        return std::nullopt;
    }
    return requirement(thresholds, level);
}

std::optional<std::int64_t> level_for_experience(
    const std::span<const std::uint16_t> thresholds,
    const std::int64_t current_level,
    const std::int64_t experience) noexcept {
    if (current_level < 1 || experience < 0 || !experience_thresholds_valid(thresholds)) {
        return std::nullopt;
    }
    auto lower = current_level;
    auto upper = std::numeric_limits<std::int64_t>::max();
    while (lower < upper) {
        const auto distance = upper - lower;
        const auto middle = lower + distance / 2 + distance % 2;
        const auto needed = requirement(thresholds, middle);
        if (needed.has_value() && *needed <= experience) {
            lower = middle;
        } else {
            upper = middle - 1;
        }
    }
    return lower;
}

}
