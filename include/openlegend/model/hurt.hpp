#pragma once

#include <compare>
#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"

namespace openlegend::model {

inline constexpr std::int64_t kHurtMaximum = 99;
inline constexpr std::int64_t kHurtRatioDenominator = 100;
inline constexpr std::int64_t kMagicHurtDivisor = 10;
inline constexpr std::int64_t kThrowingHurtDivisor = 4;

enum class HurtBand : std::uint8_t { low, moderate, severe };

NODISCARD std::optional<std::strong_ordering> compare_hurt_percentage(
    std::int64_t hurt, std::int64_t denominator, std::int64_t percentage) noexcept;

NODISCARD std::optional<HurtBand> hurt_band(
    std::int64_t hurt, std::int64_t denominator) noexcept;

NODISCARD std::optional<std::int64_t> hurt_action_penalty(
    std::int64_t hurt, std::int64_t denominator) noexcept;

NODISCARD std::optional<std::int64_t> hurt_gain(
    std::int64_t damage, std::int64_t maximum_hp, std::int64_t maximum_mp,
    std::int64_t original_divisor) noexcept;

NODISCARD std::optional<std::int64_t> hurt_hp_loss(
    std::int64_t maximum_hp, std::int64_t hurt) noexcept;

}
