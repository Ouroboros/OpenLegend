#pragma once

#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"

namespace openlegend::model {

struct NewGamePlusConfiguration {
    bool enabled{};
    std::int64_t maximum_playthroughs{999};
    std::int64_t hurt_cap_step{100};
    std::int64_t hp_cap_step{999};
    std::int64_t mp_cap_step{999};
    std::int64_t attack_cap_step{100};
    std::int64_t defence_cap_step{100};
    std::int64_t use_poison_cap_step{100};
    std::int64_t anti_poison_cap_step{100};
    std::int64_t hidden_weapon_cap_step{100};
    std::int64_t role_level_step{30};
    std::int64_t martial_level_step{10};
    std::int64_t battle_experience_percent_ng2{150};
    std::int64_t battle_experience_percent_step{100};

    NODISCARD bool operator==(const NewGamePlusConfiguration&) const = default;
};

struct PlaythroughLimits {
    std::int64_t hurt_maximum{};
    std::int64_t hurt_ratio_denominator{};
    std::int64_t battle_experience_percent{};

    NODISCARD bool operator==(const PlaythroughLimits&) const = default;
};

NODISCARD std::optional<PlaythroughLimits> calculate_playthrough_limits(
    const NewGamePlusConfiguration& configuration, std::int64_t playthrough) noexcept;

}
