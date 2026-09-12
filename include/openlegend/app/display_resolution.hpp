#pragma once

#include "openlegend/app/runtime_configuration.hpp"

namespace openlegend::app {

struct DisplayResolutionResult {
    DisplayConfigurationStatus status{DisplayConfigurationStatus::ready};
    GameResolution resolution;
};

[[nodiscard]] DisplayResolutionResult resolve_display_game_resolution(
    const DisplayConfigurationLoadResult& configuration,
    GameResolution fallback) noexcept;

}  // namespace openlegend::app
