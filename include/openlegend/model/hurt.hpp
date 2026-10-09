#pragma once

#include <compare>
#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"

namespace openlegend::model {

enum class HurtBand : std::uint8_t { low, moderate, severe };

NODISCARD std::optional<std::strong_ordering> compare_hurt_percentage(
    std::int64_t hurt, std::int64_t denominator, std::int64_t percentage) noexcept;

NODISCARD std::optional<HurtBand> hurt_band(
    std::int64_t hurt, std::int64_t denominator) noexcept;

}
