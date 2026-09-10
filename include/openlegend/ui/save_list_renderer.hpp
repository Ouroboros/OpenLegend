#pragma once

#include <cstdint>
#include <span>

#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/ui/save_list.hpp"

namespace openlegend::ui {

class ModernUiRenderer;

class SaveListRenderer {
public:
    [[nodiscard]] bool render(
        SaveListMode mode,
        std::uint16_t selection,
        std::span<const SaveListEntry> entries,
        const compat::LegacyPalette& palette,
        ModernUiRenderer& ui_renderer,
        render::RgbaFramebuffer& framebuffer) const;

    [[nodiscard]] bool render_delete_confirmation(
        std::uint16_t selection,
        const compat::LegacyPalette& palette,
        ModernUiRenderer& ui_renderer,
        render::RgbaFramebuffer& framebuffer) const;

    [[nodiscard]] bool render_io_wait(
        const compat::LegacyPalette& palette,
        ModernUiRenderer& ui_renderer,
        render::RgbaFramebuffer& framebuffer) const;
};

}  // namespace openlegend::ui
