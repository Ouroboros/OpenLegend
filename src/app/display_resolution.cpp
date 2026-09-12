#include "openlegend/app/display_resolution.hpp"

#include <cmath>

namespace openlegend::app {

DisplayResolutionResult resolve_display_game_resolution(
    const DisplayConfigurationLoadResult& configuration,
    const GameResolution fallback) noexcept {
    if (configuration.status != DisplayConfigurationStatus::ready) {
        return DisplayResolutionResult{configuration.status, fallback};
    }
    if (configuration.scale_status != DisplayConfigurationStatus::ready) {
        return DisplayResolutionResult{configuration.scale_status, fallback};
    }
    if (configuration.scale.has_value()) {
        return DisplayResolutionResult{
            DisplayConfigurationStatus::ready,
            GameResolution{
                static_cast<int>(std::round(
                    static_cast<double>(kMinimumGameWidth) * *configuration.scale)),
                static_cast<int>(std::round(
                    static_cast<double>(kMinimumGameHeight) * *configuration.scale))}};
    }
    if (configuration.resolution_status != DisplayConfigurationStatus::ready) {
        return DisplayResolutionResult{configuration.resolution_status, fallback};
    }
    if (configuration.width.has_value() && configuration.height.has_value()) {
        return DisplayResolutionResult{
            DisplayConfigurationStatus::ready,
            GameResolution{*configuration.width, *configuration.height}};
    }
    return DisplayResolutionResult{DisplayConfigurationStatus::ready, fallback};
}

}  // namespace openlegend::app
