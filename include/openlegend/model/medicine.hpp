#pragma once

#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"
#include "openlegend/random/legacy_random.hpp"

namespace openlegend::model {

NODISCARD std::optional<bool> medicine_allowed(
    std::int64_t ability, std::int64_t hurt) noexcept;

NODISCARD std::optional<std::int64_t> medicine_amount(
    std::int64_t ability,
    std::int64_t hurt,
    std::int64_t hurt_maximum,
    random::LegacyRandom& random) noexcept;

}
