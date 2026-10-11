#include "openlegend/model/hurt.hpp"

#include <algorithm>
#include <bit>

#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {
namespace {

constexpr std::uint64_t kHurtGainScale = 990;
constexpr std::int64_t kHurtCapacityFactor = 2;
constexpr std::int64_t kHurtHpLossDivisor = 2000;

NODISCARD std::uint64_t scaled_fraction(
    const std::uint64_t remainder, const std::uint64_t capacity, const std::uint64_t scale) noexcept {
    std::uint64_t quotient = 0U;
    std::uint64_t accumulated = 0U;
    for (auto mask = std::bit_floor(scale); mask != 0U; mask >>= 1U) {
        quotient *= 2U;
        if (accumulated >= capacity - accumulated) {
            accumulated -= capacity - accumulated;
            ++quotient;
        } else {
            accumulated += accumulated;
        }
        if ((scale & mask) != 0U) {
            if (accumulated >= capacity - remainder) {
                accumulated -= capacity - remainder;
                ++quotient;
            } else {
                accumulated += remainder;
            }
        }
    }
    return quotient;
}

}

std::optional<std::strong_ordering> compare_hurt_percentage(
    const std::int64_t hurt,
    const std::int64_t denominator,
    const std::int64_t percentage) noexcept {
    if (hurt < 0 || denominator < 0 || percentage < 0) {
        return std::nullopt;
    }
    if (denominator == 0) {
        if (hurt != 0) {
            return std::nullopt;
        }
        return std::int64_t{0} <=> percentage;
    }
    const auto scaled_hurt = checked_multiply(100, hurt);
    const auto threshold = checked_multiply(percentage, denominator);
    if (!scaled_hurt.has_value() || !threshold.has_value()) {
        return std::nullopt;
    }
    return *scaled_hurt <=> *threshold;
}

std::optional<HurtBand> hurt_band(
    const std::int64_t hurt, const std::int64_t denominator) noexcept {
    const auto lower = compare_hurt_percentage(hurt, denominator, 33);
    const auto upper = compare_hurt_percentage(hurt, denominator, 66);
    if (!lower.has_value() || !upper.has_value()) {
        return std::nullopt;
    }
    return *upper > 0 ? HurtBand::severe : *lower > 0 ? HurtBand::moderate : HurtBand::low;
}

std::optional<std::int64_t> hurt_action_penalty(
    const std::int64_t hurt, const std::int64_t denominator) noexcept {
    if (hurt < 0 || denominator < 0) {
        return std::nullopt;
    }
    if (denominator == 0) {
        return hurt == 0 ? std::optional<std::int64_t>{0} : std::nullopt;
    }
    const auto scaled_hurt = checked_multiply(100, hurt);
    const auto scaled_denominator = checked_multiply(40, denominator);
    if (!scaled_hurt.has_value() || !scaled_denominator.has_value()) {
        return std::nullopt;
    }
    return checked_divide(*scaled_hurt, *scaled_denominator);
}

std::optional<std::int64_t> hurt_gain(
    const std::int64_t damage,
    const std::int64_t maximum_hp,
    const std::int64_t maximum_mp,
    const std::int64_t original_divisor) noexcept {
    if (damage < 0 || maximum_hp < 0 || maximum_mp < 0 || original_divisor <= 0) {
        return std::nullopt;
    }
    if (damage == 0) {
        return 0;
    }
    const auto capacity = static_cast<std::uint64_t>(maximum_hp) + static_cast<std::uint64_t>(maximum_mp);
    if (capacity == 0U) {
        return kHurtMaximum;
    }
    const auto divisor = checked_multiply(original_divisor, kHurtCapacityFactor);
    const auto saturation = divisor.has_value()
        ? checked_multiply(kHurtMaximum, *divisor) : std::nullopt;
    if (!saturation.has_value()) {
        return std::nullopt;
    }
    const auto value = static_cast<std::uint64_t>(damage);
    const auto whole = value / capacity;
    const auto saturated_whole =
        (static_cast<std::uint64_t>(*saturation) + kHurtGainScale - 1U) / kHurtGainScale;
    if (whole >= saturated_whole) {
        return kHurtMaximum;
    }
    const auto scaled = kHurtGainScale * whole + scaled_fraction(value % capacity, capacity, kHurtGainScale);
    return std::min(static_cast<std::int64_t>(scaled / static_cast<std::uint64_t>(*divisor)), kHurtMaximum);
}

std::optional<std::int64_t> hurt_hp_loss(
    const std::int64_t maximum_hp, const std::int64_t hurt) noexcept {
    if (maximum_hp < 0) {
        return std::nullopt;
    }
    const auto whole = checked_multiply(maximum_hp / kHurtHpLossDivisor, hurt);
    const auto part = checked_multiply(maximum_hp % kHurtHpLossDivisor, hurt);
    if (!whole.has_value() || !part.has_value()) {
        return std::nullopt;
    }
    return checked_add(*whole, *part / kHurtHpLossDivisor);
}

}
