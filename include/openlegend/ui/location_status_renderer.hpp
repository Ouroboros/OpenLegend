#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <utility>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"

namespace openlegend::ui {

class ModernUiRenderer;

class LocationStatusRenderer {
public:
    NODISCARD bool render(
        std::span<const std::uint8_t> legacy_name,
        int location_x,
        int location_y,
        const compat::LegacyPalette& palette,
        ModernUiRenderer& ui_renderer,
        render::RgbaFramebuffer& framebuffer,
        std::optional<std::pair<int, int>> ship_position = std::nullopt) const;
};

}  // namespace openlegend::ui
