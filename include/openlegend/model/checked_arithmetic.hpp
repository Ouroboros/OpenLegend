#pragma once

#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"

namespace openlegend::model {

NODISCARD std::optional<std::int64_t> checked_add(
    std::int64_t left, std::int64_t right) noexcept;

NODISCARD std::optional<std::int64_t> checked_subtract(
    std::int64_t left, std::int64_t right) noexcept;

NODISCARD std::optional<std::int64_t> checked_multiply(
    std::int64_t left, std::int64_t right) noexcept;

NODISCARD std::optional<std::int64_t> checked_divide(
    std::int64_t numerator, std::int64_t denominator) noexcept;

}
