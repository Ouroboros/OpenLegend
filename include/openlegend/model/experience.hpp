#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "openlegend/attributes.hpp"

namespace openlegend::model {

NODISCARD bool experience_thresholds_valid(std::span<const std::uint16_t> thresholds) noexcept;

NODISCARD std::optional<std::int64_t> level_experience_requirement(
    std::span<const std::uint16_t> thresholds, std::int64_t level) noexcept;

NODISCARD std::optional<std::int64_t> level_for_experience(
    std::span<const std::uint16_t> thresholds,
    std::int64_t current_level,
    std::int64_t experience) noexcept;

}
