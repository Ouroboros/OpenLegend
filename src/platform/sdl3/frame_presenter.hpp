#pragma once

#include "openlegend/render/rgba_framebuffer.hpp"

namespace openlegend::app {
class LegacyGameRuntime;
}

namespace openlegend::platform::sdl3 {

class SdlRuntimePlatform;

class FramePresenter {
public:
    [[nodiscard]] int present(
        app::LegacyGameRuntime& game, SdlRuntimePlatform& platform);

private:
    render::RgbaFramebuffer rgba_framebuffer_;
    render::RgbaFramebuffer modern_ui_framebuffer_;
};

}  // namespace openlegend::platform::sdl3
