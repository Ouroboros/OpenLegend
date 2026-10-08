#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "openlegend/attributes.hpp"
#include "openlegend/model/runtime_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"

namespace openlegend::model {

inline constexpr std::size_t kNewGameNameMaximumBytes = 6U;

NODISCARD bool set_protagonist_name(
    RangerState& ranger, std::span<const std::uint8_t> legacy_name) noexcept;

void roll_protagonist_attributes(RoleRecord& protagonist, random::LegacyRandom& random) noexcept;

void apply_baberuth_attributes(RoleRecord& protagonist) noexcept;

NODISCARD bool set_protagonist_name(
    RuntimeRangerState& ranger, std::span<const std::uint8_t> legacy_name);

void roll_protagonist_attributes(RoleState& protagonist, random::LegacyRandom& random);

void apply_baberuth_attributes(RoleState& protagonist);

}  // namespace openlegend::model
