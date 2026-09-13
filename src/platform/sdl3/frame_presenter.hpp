#pragma once

#include <cstdint>

#include "openlegend/attributes.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/indexed_layer.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"

namespace openlegend::app {
class LegacyGameRuntime;
struct NativeMotionPresentation;
}

namespace openlegend::platform::sdl3 {

class SdlRuntimePlatform;

class FramePresenter {
public:
    NODISCARD int present(
        app::LegacyGameRuntime& game,
        SdlRuntimePlatform& platform,
        bool refresh_legacy_frame = true,
        std::int64_t weather_offset_x = 0);

private:
    NODISCARD int present_native_motion(
        app::LegacyGameRuntime& game,
        SdlRuntimePlatform& platform,
        const app::NativeMotionPresentation& presentation,
        int game_width,
        int game_height,
        bool refresh_modern_ui,
        std::uint8_t fade_alpha,
        std::int64_t weather_offset_x);

    NODISCARD int present_native_weather(
        app::LegacyGameRuntime& game,
        SdlRuntimePlatform& platform,
        int game_width,
        int game_height,
        bool refresh_base_frame,
        bool refresh_modern_ui,
        std::uint8_t fade_alpha,
        std::int64_t weather_offset_x);

    render::RgbaFramebuffer rgba_framebuffer_;
    render::RgbaFramebuffer modern_ui_framebuffer_;
    render::IndexedFramebuffer motion_underlay_;
    render::IndexedLayer motion_overlay_;
    render::IndexedLayer motion_screen_effect_;
    render::RgbaFramebuffer motion_underlay_rgba_;
    render::RgbaFramebuffer motion_overlay_rgba_;
    render::RgbaFramebuffer motion_screen_effect_rgba_;
    render::RgbaFramebuffer motion_actor_rgba_;
    std::uint64_t motion_sequence_{};
    std::uint64_t motion_palette_revision_{};
    bool motion_palette_cycle_preview_{};
    bool motion_destination_depth_phase_{};
    bool motion_layers_ready_{};
    bool weather_layer_ready_{};
    bool base_frame_ready_{};
};

}  // namespace openlegend::platform::sdl3
