#pragma once

#include "openlegend/attributes.hpp"
#include "openlegend/app/runtime_configuration.hpp"

namespace openlegend::app {

struct DisplayResolutionResult {
    DisplayConfigurationStatus status{DisplayConfigurationStatus::ready};
    GameResolution resolution;
};

NODISCARD DisplayResolutionResult resolve_display_game_resolution(
    const DisplayConfigurationLoadResult& configuration,
    GameResolution fallback) noexcept;

}  // namespace openlegend::app
