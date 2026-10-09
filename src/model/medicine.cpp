#include "openlegend/model/medicine.hpp"

#include <algorithm>
#include <cmath>

#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {

std::optional<bool> medicine_allowed(
    const std::int64_t ability, const std::int64_t hurt) noexcept {
    if (hurt < 0) {
        return std::nullopt;
    }
    const auto medicine = std::max(ability, std::int64_t{0});
    const auto threshold = checked_add(medicine, std::max(std::int64_t{20}, medicine / 5));
    if (!threshold.has_value()) {
        return std::nullopt;
    }
    return hurt <= *threshold;
}

std::optional<std::int64_t> medicine_amount(
    const std::int64_t ability,
    const std::int64_t hurt,
    const std::int64_t hurt_maximum,
    random::LegacyRandom& random) noexcept {
    if (hurt_maximum < 0 || hurt < 0 || hurt > hurt_maximum) {
        return std::nullopt;
    }
    const auto allowed = medicine_allowed(ability, hurt);
    if (!allowed.has_value()) {
        return std::nullopt;
    }
    auto candidate_random = random;
    const auto variation = candidate_random.bounded(5);
    if (!*allowed) {
        random = candidate_random;
        return 0;
    }
    const auto medicine = std::max(ability, std::int64_t{0});
    const auto ratio = hurt_maximum == 0 ? 0.0L
        : static_cast<long double>(hurt) / static_cast<long double>(hurt_maximum);
    const auto multiplier = (16.0L - 7.0L * ratio * ratio * (1.0L + ratio)) / 20.0L;
    const auto base = std::trunc(static_cast<long double>(medicine) * multiplier);
    if (!std::isfinite(base) || base < 0 || base >= std::ldexp(1.0L, 63)) {
        return std::nullopt;
    }
    const auto theoretical = checked_add(static_cast<std::int64_t>(base), variation);
    if (!theoretical.has_value()) {
        return std::nullopt;
    }
    const auto minimum = medicine / 10 + (medicine % 10 != 0 ? 1 : 0);
    random = candidate_random;
    return std::max(*theoretical, minimum);
}

}
