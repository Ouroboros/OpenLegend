#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"
#include "openlegend/model/role_state.hpp"

namespace openlegend::model {

struct PracticeRules {
    std::int64_t aptitude_base{};
    std::int64_t aptitude_step{};
    std::int64_t unlearned_level{};
    std::int64_t unassociated_first_level{};
    std::int64_t reward_numerator{};
    std::int64_t reward_denominator{};
};

struct PracticeCost {
    std::int64_t experience{};
    std::int16_t magic_slot{-1};
};

NODISCARD bool practice_rules_valid(const PracticeRules& rules) noexcept;

NODISCARD std::optional<std::int64_t> practice_experience_requirement(
    std::int64_t need, std::int64_t iq, std::int64_t tier,
    const PracticeRules& rules) noexcept;

NODISCARD std::optional<PracticeCost> manual_experience_requirement(
    const RoleState& role, const ItemRecord& item, std::size_t item_id,
    const PracticeRules& rules) noexcept;

NODISCARD std::optional<std::int64_t> training_experience_reward(
    std::int64_t experience, const PracticeRules& rules) noexcept;

}
