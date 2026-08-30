#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"

namespace openlegend::model {

inline constexpr std::size_t kNewGameNameMaximumBytes = 6U;

[[nodiscard]] bool set_protagonist_name(
    RangerState& ranger, std::span<const std::uint8_t> legacy_name) noexcept;

void roll_protagonist_attributes(RoleRecord& protagonist, random::LegacyRandom& random) noexcept;
void apply_baberuth_attributes(RoleRecord& protagonist) noexcept;

}  // namespace openlegend::model
