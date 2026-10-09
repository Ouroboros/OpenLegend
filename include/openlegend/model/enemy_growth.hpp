#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"
#include "openlegend/model/new_game_plus_configuration.hpp"
#include "openlegend/model/role_state.hpp"

namespace openlegend::model {

struct EnemyAttributes {
    std::int64_t maximum_hp{};
    std::int64_t maximum_mp{};
    std::int64_t attack{};
    std::int64_t defence{};
    std::int64_t use_poison{};
    std::int64_t anti_poison{};
    std::int64_t hidden_weapon{};
    std::int64_t level{};
    std::array<std::int64_t, role_word::magic_level_count> magic_levels{};

    NODISCARD bool operator==(const EnemyAttributes&) const = default;
};

NODISCARD std::optional<EnemyAttributes> calculate_enemy_attributes(
    const RoleState& baseline,
    const NewGamePlusConfiguration& configuration,
    std::int64_t playthrough) noexcept;

}
