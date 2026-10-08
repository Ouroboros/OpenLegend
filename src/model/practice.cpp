#include "openlegend/model/practice.hpp"

#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {

bool practice_rules_valid(const PracticeRules& rules) noexcept {
    return rules.aptitude_step > 0 && rules.aptitude_base > 100 / rules.aptitude_step &&
        rules.unlearned_level > 0 && rules.unassociated_first_level > 0 &&
        rules.reward_numerator >= 0 && rules.reward_denominator > 0;
}

std::optional<std::int64_t> practice_experience_requirement(
    const std::int64_t need, const std::int64_t iq, const std::int64_t tier,
    const PracticeRules& rules) noexcept {
    if (!practice_rules_valid(rules) || need < 0 || iq < 0 || iq > 100 || tier < 1) {
        return std::nullopt;
    }
    const auto factored = checked_multiply(need, rules.aptitude_base - iq / rules.aptitude_step);
    return factored.has_value() ? checked_multiply(*factored, tier) : std::nullopt;
}

std::optional<PracticeCost> manual_experience_requirement(
    const RoleState& role, const ItemRecord& item, const std::size_t item_id,
    const PracticeRules& rules) noexcept {
    if (item_id >= role.no_magic_count.size()) {
        return std::nullopt;
    }
    const auto magic_id = item.word(item_word::magic_id);
    std::optional<std::int64_t> tier = rules.unlearned_level;
    std::int16_t magic_slot = -1;
    if (magic_id == -1) {
        if (role.no_magic_count[item_id] < 0) {
            return std::nullopt;
        }
        tier = checked_add(role.no_magic_count[item_id], rules.unassociated_first_level);
    } else {
        for (std::size_t slot = 0U; slot < role.magic_ids.size(); ++slot) {
            if (role.magic_ids[slot].value == magic_id) {
                if (role.magic_levels[slot] < 0) {
                    return std::nullopt;
                }
                tier = checked_add(role.magic_levels[slot] / 100, 1);
                magic_slot = static_cast<std::int16_t>(slot);
                break;
            }
        }
    }
    if (!tier.has_value()) {
        return std::nullopt;
    }
    const auto cost = practice_experience_requirement(
        item.word(item_word::need_experience), role.iq, *tier, rules);
    if (!cost.has_value()) {
        return std::nullopt;
    }
    return PracticeCost{*cost, magic_slot};
}

std::optional<std::int64_t> training_experience_reward(
    const std::int64_t experience, const PracticeRules& rules) noexcept {
    if (!practice_rules_valid(rules) || experience < 0) {
        return std::nullopt;
    }
    const auto numerator = checked_multiply(experience, rules.reward_numerator);
    return numerator.has_value()
        ? checked_divide(*numerator, rules.reward_denominator) : std::nullopt;
}

}
