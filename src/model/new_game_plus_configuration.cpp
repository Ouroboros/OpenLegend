#include "openlegend/model/new_game_plus_configuration.hpp"

#include "openlegend/attributes.hpp"
#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {
namespace {

constexpr std::int64_t original_hurt_maximum = 99;
constexpr std::int64_t hurt_denominator_step = 10;
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
    const auto hurt_increase = checked_multiply(playthrough - 1, configuration.hurt_cap_step);
    if (!hurt_increase.has_value()) {
        return std::nullopt;
    }
    const auto hurt_maximum = checked_add(original_hurt_maximum, *hurt_increase);
    if (!hurt_maximum.has_value() || *hurt_maximum < 0) {
        return std::nullopt;
    }
    const auto denominator_blocks = checked_add(
        *hurt_maximum / hurt_denominator_step,
        *hurt_maximum % hurt_denominator_step == 0 ? 0 : 1);
    if (!denominator_blocks.has_value()) {
        return std::nullopt;
    }
    const auto denominator = checked_multiply(*denominator_blocks, hurt_denominator_step);
    const auto multiplier = experience_percent(configuration, playthrough);
    if (!denominator.has_value() || !multiplier.has_value()) {
        return std::nullopt;
    }
    return PlaythroughLimits{*hurt_maximum, *denominator, *multiplier};
}

}
