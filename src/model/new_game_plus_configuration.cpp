#include "openlegend/model/new_game_plus_configuration.hpp"

#include "openlegend/attributes.hpp"
#include "openlegend/model/checked_arithmetic.hpp"
#include "openlegend/model/hurt.hpp"

namespace openlegend::model {
namespace {

constexpr std::int64_t original_experience_percent = 100;

NODISCARD std::optional<std::int64_t> experience_percent(
    const NewGamePlusConfiguration& configuration, const std::int64_t playthrough) noexcept {
    if (playthrough == 1) {
        return original_experience_percent;
    }
    const auto increase = checked_multiply(
        playthrough - 2, configuration.battle_experience_percent_step);
    if (!increase.has_value()) {
        return std::nullopt;
    }
    const auto result = checked_add(configuration.battle_experience_percent_ng2, *increase);
    if (!result.has_value() || *result < 0) {
        return std::nullopt;
    }
    return result;
}

}

std::optional<PlaythroughLimits> calculate_playthrough_limits(
    const NewGamePlusConfiguration& configuration, const std::int64_t playthrough) noexcept {
    if (playthrough < 1 || playthrough > configuration.maximum_playthroughs) {
        return std::nullopt;
    }
    const auto multiplier = experience_percent(configuration, playthrough);
    if (!multiplier.has_value()) {
        return std::nullopt;
    }
    return PlaythroughLimits{kHurtMaximum, kHurtRatioDenominator, *multiplier};
}

}
