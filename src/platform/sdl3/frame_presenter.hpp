#pragma once

#include "openlegend/attributes.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"

namespace openlegend::app {
class LegacyGameRuntime;
}

namespace openlegend::platform::sdl3 {

class SdlRuntimePlatform;

class FramePresenter {
public:
    NODISCARD int present(
        app::LegacyGameRuntime& game, SdlRuntimePlatform& platform);

private:
    render::RgbaFramebuffer rgba_framebuffer_;
    render::RgbaFramebuffer modern_ui_framebuffer_;
    bool base_frame_ready_{};
};

}  // namespace openlegend::platform::sdl3
