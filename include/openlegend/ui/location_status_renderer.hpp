#pragma once

#include <cstdint>
#include <span>

#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"

namespace openlegend::ui {

class ModernUiRenderer;

class LocationStatusRenderer {
public:
    [[nodiscard]] bool render(
        std::span<const std::uint8_t> legacy_name,
        int location_x,
        int location_y,
        const compat::LegacyPalette& palette,
        ModernUiRenderer& ui_renderer,
        render::RgbaFramebuffer& framebuffer) const;
};

}  // namespace openlegend::ui
