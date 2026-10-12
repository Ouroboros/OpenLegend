#include "openlegend/model/experience.hpp"

#include <cmath>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {
namespace {

// 原版每级升级费用D(L)=E0(L+1)−E0(L)近似按固定倍率增长；对原表全部等级的ln D(L)
// 与L做最小二乘，求出每级增长倍率r。超出原表的等级从原表末级费用起按r逐级增长，
// 在原表末级处连续。
NODISCARD std::optional<double> fitted_cost_ratio(
    const std::span<const std::uint16_t> thresholds) noexcept {
    const auto count = thresholds.size() - 1U;
    if (count < 2U) {
        return std::nullopt;
    }
    double sum_x = 0.0;
    double sum_y = 0.0;
    for (std::size_t level = 1U; level <= count; ++level) {
        sum_x += static_cast<double>(level);
        sum_y += std::log(static_cast<double>(thresholds[level] - thresholds[level - 1U]));
    }
    const auto mean_x = sum_x / static_cast<double>(count);
    const auto mean_y = sum_y / static_cast<double>(count);
    double covariance = 0.0;
    double variance = 0.0;
    for (std::size_t level = 1U; level <= count; ++level) {
        const auto dx = static_cast<double>(level) - mean_x;
        const auto cost = static_cast<double>(thresholds[level] - thresholds[level - 1U]);
        covariance += dx * (std::log(cost) - mean_y);
        variance += dx * dx;
    }
    const auto ratio = std::exp(covariance / variance);
    // 倍率须大于1，才能保证每级费用随等级递增。
    return std::isfinite(ratio) && ratio > 1.0 ? std::optional<double>{ratio} : std::nullopt;
}

// 由倍率推算的累计门槛；超出signed64可表示范围视为不可达。
NODISCARD std::optional<std::int64_t> extended_requirement(
    const std::span<const std::uint16_t> thresholds, const double ratio,
    const std::int64_t level) noexcept {
    // E(L)=E0(L0)+Dlast×r×(r^(L−L0)−1)/(r−1)，即L0起每级费用Dlast×r^(k−L0+1)之和。
    const auto original_levels = static_cast<std::int64_t>(thresholds.size());
    const auto last_cost =
        static_cast<double>(thresholds.back() - thresholds[thresholds.size() - 2U]);
    const auto extra_levels = static_cast<double>(level - original_levels);
    const auto value = std::round(static_cast<double>(thresholds.back()) +
        last_cost * ratio * (std::pow(ratio, extra_levels) - 1.0) / (ratio - 1.0));
    if (!std::isfinite(value) || value >= 9.2e18) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(value);
}

// 1～100级累计门槛与倍率按原表内容缓存，原表不变时直接查表。
constexpr std::int64_t kCachedLevels = 100;

struct RequirementCache {
    std::vector<std::uint16_t> source;
    std::optional<double> ratio;
    std::vector<std::optional<std::int64_t>> levels;
};

NODISCARD const RequirementCache& requirement_cache(
    const std::span<const std::uint16_t> thresholds) {
    thread_local RequirementCache cache;
    if (!std::ranges::equal(cache.source, thresholds)) {
        cache.source.assign(thresholds.begin(), thresholds.end());
        cache.ratio = fitted_cost_ratio(thresholds);
        cache.levels.assign(static_cast<std::size_t>(kCachedLevels), std::nullopt);
        for (std::int64_t level = 1; level <= kCachedLevels; ++level) {
            auto& entry = cache.levels[static_cast<std::size_t>(level - 1)];
            if (level <= static_cast<std::int64_t>(thresholds.size())) {
                entry = thresholds[static_cast<std::size_t>(level - 1)];
            } else if (cache.ratio.has_value()) {
                entry = extended_requirement(thresholds, *cache.ratio, level);
            }
        }
    }
    return cache;
}

NODISCARD std::optional<std::int64_t> requirement(
    const std::span<const std::uint16_t> thresholds,
    const std::int64_t level) {
    const auto& cache = requirement_cache(thresholds);
    if (level <= kCachedLevels) {
        return cache.levels[static_cast<std::size_t>(level - 1)];
    }
    return cache.ratio.has_value()
        ? extended_requirement(thresholds, *cache.ratio, level) : std::nullopt;
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
    return fitted_cost_ratio(thresholds).has_value();
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
