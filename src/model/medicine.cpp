#include "openlegend/model/medicine.hpp"

#include <algorithm>
#include <array>
#include <limits>

#include "openlegend/attributes.hpp"
#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {
namespace {

constexpr std::int64_t product_radix = 65'536;
using FractionProduct = std::array<std::int64_t, 17>;

NODISCARD bool multiply_product(
    FractionProduct& product, const std::int64_t factor) noexcept {
    if (factor < 0) {
        return false;
    }
    FractionProduct accumulated{};
    for (std::size_t left = 0U; left < product.size(); ++left) {
        auto remaining = factor;
        for (std::size_t right = 0U; right < 4U; ++right) {
            const auto digit = remaining % product_radix;
            remaining /= product_radix;
            if (product[left] == 0 || digit == 0) {
                continue;
            }
            const auto destination = left + right;
            if (destination >= accumulated.size()) {
                return false;
            }
            accumulated[destination] += product[left] * digit;
        }
    }
    std::int64_t carry = 0;
    for (std::size_t index = 0U; index < product.size(); ++index) {
        const auto value = accumulated[index] + carry;
        accumulated[index] = value % product_radix;
        carry = value / product_radix;
    }
    if (carry != 0) {
        return false;
    }
    product = accumulated;
    return true;
}

NODISCARD std::optional<FractionProduct> fraction_product(
    const std::int64_t coefficient,
    const std::array<std::int64_t, 4>& factors) noexcept {
    FractionProduct product{};
    product[0U] = coefficient;
    for (const auto factor : factors) {
        if (!multiply_product(product, factor)) {
            return std::nullopt;
        }
    }
    return product;
}

NODISCARD bool product_less(
    const FractionProduct& left, const FractionProduct& right) noexcept {
    for (std::size_t end = left.size(); end > 0U; --end) {
        if (left[end - 1U] != right[end - 1U]) {
            return left[end - 1U] < right[end - 1U];
        }
    }
    return false;
}

NODISCARD bool subtract_product(
    FractionProduct& left, const FractionProduct& right) noexcept {
    if (product_less(left, right)) {
        return false;
    }
    std::int64_t borrow = 0;
    for (std::size_t index = 0U; index < left.size(); ++index) {
        const auto value = right[index] + borrow;
        if (left[index] < value) {
            left[index] += product_radix - value;
            borrow = 1;
        } else {
            left[index] -= value;
            borrow = 0;
        }
    }
    return borrow == 0;
}

NODISCARD std::optional<std::int64_t> medicine_base(
    const std::int64_t ability, const std::int64_t hurt,
    const std::int64_t hurt_maximum) noexcept {
    const auto denominator = hurt_maximum == 0 ? 1 : hurt_maximum;
    auto numerator = fraction_product(16, {ability, denominator, denominator, denominator});
    const auto first_penalty = fraction_product(7, {ability, hurt, hurt, denominator});
    const auto second_penalty = fraction_product(7, {ability, hurt, hurt, hurt});
    const auto divisor = fraction_product(20, {1, denominator, denominator, denominator});
    if (!numerator || !first_penalty || !second_penalty || !divisor ||
        !subtract_product(*numerator, *first_penalty) ||
        !subtract_product(*numerator, *second_penalty)) {
        return std::nullopt;
    }
    std::int64_t result = 0;
    for (auto bit = std::numeric_limits<std::int64_t>::digits - 1; bit >= 0; --bit) {
        const auto increment = std::int64_t{1} << bit;
        auto scaled_divisor = *divisor;
        if (!multiply_product(scaled_divisor, increment)) {
            return std::nullopt;
        }
        if (product_less(*numerator, scaled_divisor)) {
            continue;
        }
        const auto changed = checked_add(result, increment);
        if (!changed || !subtract_product(*numerator, scaled_divisor)) {
            return std::nullopt;
        }
        result = *changed;
    }
    return result;
}

}

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
    const std::int64_t variation = candidate_random.bounded(5);
    if (!*allowed) {
        random = candidate_random;
        return 0;
    }
    const auto medicine = std::max(ability, std::int64_t{0});
    const auto base = medicine_base(medicine, hurt, hurt_maximum);
    if (!base.has_value()) {
        return std::nullopt;
    }
    const auto theoretical = checked_add(*base, variation);
    if (!theoretical.has_value()) {
        return std::nullopt;
    }
    const auto minimum = medicine / 10 + (medicine % 10 != 0 ? 1 : 0);
    random = candidate_random;
    return std::max(*theoretical, minimum);
}

}
