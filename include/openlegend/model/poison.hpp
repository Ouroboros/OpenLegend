#pragma once

#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"
#include "openlegend/random/legacy_random.hpp"

namespace openlegend::model {

struct PoisonApplication {
    std::int64_t maximum_depth{};
    std::int64_t theoretical_amount{};
    std::int64_t applied_amount{};
    std::int64_t overflow{};
    std::int64_t hp_damage{};
};

NODISCARD std::optional<PoisonApplication> poison_application(
    std::int64_t power, std::int64_t resistance, std::int64_t divisor,
    std::int64_t current_poison, std::int64_t maximum_hp) noexcept;

NODISCARD std::optional<std::int64_t> poison_round_damage(
    std::int64_t poison, std::int64_t maximum_hp) noexcept;

NODISCARD std::optional<std::int64_t> detoxification_amount(
    std::int64_t ability, std::int64_t poison, random::LegacyRandom& random) noexcept;

}
