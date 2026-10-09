#include "openlegend/model/enemy_growth.hpp"

#include <array>
#include <cstddef>

#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {

std::optional<EnemyAttributes> calculate_enemy_attributes(
    const RoleState& baseline,
    const NewGamePlusConfiguration& configuration,
    const std::int64_t playthrough) noexcept {
    if (!calculate_playthrough_limits(configuration, playthrough).has_value()) {
        return std::nullopt;
    }
    const auto completed = playthrough - 1;
    EnemyAttributes result;
    struct GrowthField {
        std::int64_t baseline;
        std::int64_t step;
        std::int64_t minimum;
        std::int64_t* destination;
    };
    const std::array fields{
        GrowthField{baseline.maximum_hp, configuration.hp_cap_step, 0, &result.maximum_hp},
        GrowthField{baseline.maximum_mp, configuration.mp_cap_step, 0, &result.maximum_mp},
        GrowthField{baseline.attack, configuration.attack_cap_step, 0, &result.attack},
        GrowthField{baseline.defence, configuration.defence_cap_step, 0, &result.defence},
        GrowthField{baseline.use_poison, configuration.use_poison_cap_step, 0, &result.use_poison},
        GrowthField{baseline.anti_poison, configuration.anti_poison_cap_step, 0, &result.anti_poison},
        GrowthField{baseline.hidden_weapon, configuration.hidden_weapon_cap_step, 0, &result.hidden_weapon},
        GrowthField{baseline.level, configuration.role_level_step, 1, &result.level},
    };
    for (const auto& field : fields) {
        if (field.baseline < field.minimum) {
            return std::nullopt;
        }
        const auto increment = checked_multiply(completed, field.step);
        const auto value = increment.has_value()
            ? checked_add(field.baseline, *increment) : std::nullopt;
        if (!value.has_value() || *value < field.minimum) {
            return std::nullopt;
        }
        *field.destination = *value;
    }
    result.magic_levels = baseline.magic_levels;
    for (std::size_t slot = 0U; slot < baseline.magic_ids.size(); ++slot) {
        const auto magic_id = baseline.magic_ids[slot].value;
        if (magic_id < -1 || magic_id >= static_cast<std::int16_t>(kMagicCount) ||
            baseline.magic_levels[slot] < 0) {
            return std::nullopt;
        }
        if (magic_id <= 0) {
            continue;
        }
        const auto units = checked_multiply(100, completed);
        const auto increment = units.has_value()
            ? checked_multiply(*units, configuration.martial_level_step) : std::nullopt;
        const auto proficiency = increment.has_value()
            ? checked_add(baseline.magic_levels[slot], *increment) : std::nullopt;
        if (!proficiency.has_value() || *proficiency < 0) {
            return std::nullopt;
        }
        result.magic_levels[slot] = *proficiency;
    }
    return result;
}

}
