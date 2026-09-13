#include "frame_presenter.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>

#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/motion/authoritative_motion.hpp"
#include "openlegend/render/indexed_sprite_image.hpp"
#include "openlegend/render/legacy_effects.hpp"
#include "openlegend/render/native_motion_projection.hpp"
#include "sdl_runtime_platform.hpp"

namespace openlegend::platform::sdl3 {
namespace {

void report_presentation_error(
    const std::string_view category,
    const std::string_view message,
    const std::string_view detail = {}) {
    std::string record{category};
    record += ": ";
    record += message;
    if (!detail.empty()) {
        record += ": ";
        record += detail;
    }
    std::cerr << record << '\n';
    if (diagnostics::logging_to_file()) {
        diagnostics::log_error(record);
    }
}

}  // namespace

int FramePresenter::present_native_motion(
    app::LegacyGameRuntime& game,
    SdlRuntimePlatform& platform,
    const app::NativeMotionPresentation& motion,
    const int game_width,
    const int game_height,
    const bool refresh_modern_ui,
    const std::uint8_t fade_alpha,
    const std::int64_t weather_offset_x) {
    const auto layer_width =
        game_width + 2 * render::native_motion_overscan_x;
    const auto layer_height =
        game_height + 2 * render::native_motion_overscan_y;
    const bool refresh_layers =
        !motion_layers_ready_ || motion.sequence != motion_sequence_ ||
        motion.destination_depth_phase != motion_destination_depth_phase_ ||
        motion_underlay_.pixel_width() != layer_width ||
        motion_underlay_.pixel_height() != layer_height;
    const bool refresh_palette =
        refresh_layers ||
        motion.palette_revision != motion_palette_revision_ ||
        motion.preview_palette_cycle != motion_palette_cycle_preview_;
    if (refresh_layers) {
        if (!motion_underlay_.resize(layer_width, layer_height) ||
            !motion_overlay_.set_dimensions(layer_width, layer_height) ||
            !motion_screen_effect_.set_dimensions(layer_width, layer_height) ||
            !motion_underlay_rgba_.set_dimensions(layer_width, layer_height, 1) ||
            !motion_overlay_rgba_.set_dimensions(layer_width, layer_height, 1) ||
            !motion_screen_effect_rgba_.set_dimensions(
                layer_width, layer_height, 1) ||
            !motion_actor_rgba_.set_dimensions(
                static_cast<int>(motion.actor_sprite->width),
                static_cast<int>(motion.actor_sprite->height),
                1) ||
            !game.render_motion_layers(
                motion_underlay_, motion_overlay_, motion_screen_effect_)) {
            report_presentation_error(
                "render", "unable to build native motion layers");
            return 7;
        }
        motion_sequence_ = motion.sequence;
        motion_destination_depth_phase_ = motion.destination_depth_phase;
        motion_layers_ready_ = true;
    }
    if (refresh_palette) {
        auto palette = *motion.palette;
        if (motion.preview_palette_cycle) {
            render::cycle_legacy_palette(palette);
        }
        motion_underlay_.set_palette(palette);
        motion_overlay_.set_palette(palette);
        motion_screen_effect_.set_palette(palette);
        const compat::IndexedFrameView underlay{
            motion_underlay_.pixels(),
            motion_underlay_.palette(),
            layer_width,
            layer_height};
        if (!compat::convert_indexed_frame_to_rgba(
                underlay, motion_underlay_rgba_.pixels()) ||
            !render::convert_indexed_layer_to_rgba(
                motion_overlay_, motion_overlay_rgba_.pixels()) ||
            !render::convert_indexed_layer_to_rgba(
                motion_screen_effect_, motion_screen_effect_rgba_.pixels()) ||
            !render::convert_indexed_sprite_to_rgba(
                *motion.actor_sprite,
                palette,
                motion_actor_rgba_.pixels()) ||
            !game.render_motion_weather(
                motion_screen_effect_rgba_, palette)) {
            report_presentation_error(
                "render", "unable to convert native motion layers to RGBA");
            return 7;
        }
        motion_palette_revision_ = motion.palette_revision;
        motion_palette_cycle_preview_ = motion.preview_palette_cycle;
        weather_layer_ready_ = false;
    }

    const auto camera_for = [&](const openlegend::motion::FixedPosition position) {
        return motion.domain == app::NativeMotionDomain::world
            ? render::world_camera_position(position)
            : render::scene_camera_position(position);
    };
    const auto source_camera = camera_for(motion.source);
    const auto current_camera = camera_for(motion.position);
    const auto map_offset = render::project_fixed_isometric(
        openlegend::motion::FixedPosition{
            source_camera.x - current_camera.x,
            source_camera.y - current_camera.y,
            0},
        0,
        0);
    const auto actor_anchor = render::project_fixed_isometric(
        openlegend::motion::FixedPosition{
            motion.position.x - current_camera.x,
            motion.position.y - current_camera.y,
            motion.position.height},
        game_width / 2 - 15,
        game_height / 2 + 17);
    const auto actor_left = actor_anchor.x -
        static_cast<std::int64_t>(motion.actor_sprite->x_offset) *
            openlegend::motion::kFixedUnitsPerGridUnit;
    const auto actor_top = actor_anchor.y -
        static_cast<std::int64_t>(motion.actor_sprite->y_offset) *
            openlegend::motion::kFixedUnitsPerGridUnit;
    const auto map_left = map_offset.x -
        static_cast<std::int64_t>(render::native_motion_overscan_x) *
            openlegend::motion::kFixedUnitsPerGridUnit;
    const auto map_top = map_offset.y -
        static_cast<std::int64_t>(render::native_motion_overscan_y) *
            openlegend::motion::kFixedUnitsPerGridUnit;
    const auto screen_effect_left =
        motion.domain == app::NativeMotionDomain::world
        ? map_left + weather_offset_x
        : -static_cast<std::int64_t>(render::native_motion_overscan_x) *
              openlegend::motion::kFixedUnitsPerGridUnit;
    const auto screen_effect_top =
        motion.domain == app::NativeMotionDomain::world
        ? map_top
        : -static_cast<std::int64_t>(render::native_motion_overscan_y) *
              openlegend::motion::kFixedUnitsPerGridUnit;
    if (motion.domain == app::NativeMotionDomain::world && refresh_layers) {
        diagnostics::log_debug(
            "weather_motion_layer sequence=" +
            std::to_string(motion.sequence) +
            " destination_phase=" +
            std::to_string(
                static_cast<int>(motion.destination_depth_phase)) +
            " source=" + std::to_string(motion.source.x) + "," +
            std::to_string(motion.source.y) +
            " current=" + std::to_string(motion.position.x) + "," +
            std::to_string(motion.position.y) +
            " map_offset=" + std::to_string(map_offset.x) + "," +
            std::to_string(map_offset.y) +
            " map_destination=" + std::to_string(map_left) + "," +
            std::to_string(map_top) +
            " weather_offset=" + std::to_string(weather_offset_x) +
            " screen_destination=" +
            std::to_string(screen_effect_left) + "," +
            std::to_string(screen_effect_top) +
            " refresh_palette=" +
            std::to_string(static_cast<int>(refresh_palette)));
    }

    const std::array<NativeRgbaLayer, 4> layers{
        NativeRgbaLayer{
            NativeLayerSlot::map_underlay,
            compat::RgbaFrameView{
                motion_underlay_rgba_.pixels(), layer_width, layer_height},
            map_left,
            map_top,
            refresh_palette},
        NativeRgbaLayer{
            NativeLayerSlot::actor,
            compat::RgbaFrameView{
                motion_actor_rgba_.pixels(),
                motion_actor_rgba_.pixel_width(),
                motion_actor_rgba_.pixel_height()},
            actor_left,
            actor_top,
            refresh_palette},
        NativeRgbaLayer{
            NativeLayerSlot::map_overlay,
            compat::RgbaFrameView{
                motion_overlay_rgba_.pixels(), layer_width, layer_height},
            map_left,
            map_top,
            refresh_palette},
        NativeRgbaLayer{
            NativeLayerSlot::screen_effect,
            compat::RgbaFrameView{
                motion_screen_effect_rgba_.pixels(), layer_width, layer_height},
            screen_effect_left,
            screen_effect_top,
            refresh_palette},
    };
    const compat::RgbaFrameView modern_ui{
        modern_ui_framebuffer_.pixels(),
        modern_ui_framebuffer_.pixel_width(),
        modern_ui_framebuffer_.pixel_height()};
    if (!platform.present_native_layers(
            layers, modern_ui, refresh_modern_ui, fade_alpha)) {
        diagnostics::log_critical(
            std::string{"native motion present failed: "} + SDL_GetError());
        report_presentation_error(
            "present", "unable to present native motion layers", SDL_GetError());
        return 7;
    }
    return 0;
}

int FramePresenter::present_native_weather(
    app::LegacyGameRuntime& game,
    SdlRuntimePlatform& platform,
    const int game_width,
    const int game_height,
    const bool refresh_base_frame,
    const bool refresh_modern_ui,
    const std::uint8_t fade_alpha,
    const std::int64_t weather_offset_x) {
    const auto layer_width =
        game_width + 2 * render::native_motion_overscan_x;
    const auto layer_height =
        game_height + 2 * render::native_motion_overscan_y;
    const bool refresh_weather =
        refresh_base_frame || !weather_layer_ready_ ||
        motion_screen_effect_rgba_.logical_width() != layer_width ||
        motion_screen_effect_rgba_.logical_height() != layer_height;
    if (refresh_weather) {
        if (!motion_screen_effect_rgba_.set_dimensions(
                layer_width, layer_height, 1)) {
            report_presentation_error(
                "render", "unable to resize native weather layer");
            return 7;
        }
        motion_screen_effect_rgba_.clear({0U, 0U, 0U, 0U});
        if (!game.render_motion_weather(
                motion_screen_effect_rgba_, game.framebuffer().palette())) {
            report_presentation_error(
                "render", "unable to render native weather layer");
            return 7;
        }
        weather_layer_ready_ = true;
    }

    const auto screen_effect_left =
        -static_cast<std::int64_t>(render::native_motion_overscan_x) *
            openlegend::motion::kFixedUnitsPerGridUnit +
        weather_offset_x;
    const auto screen_effect_top =
        -static_cast<std::int64_t>(render::native_motion_overscan_y) *
        openlegend::motion::kFixedUnitsPerGridUnit;
    if (refresh_weather) {
        diagnostics::log_debug(
            "weather_idle_layer refresh_base=" +
            std::to_string(static_cast<int>(refresh_base_frame)) +
            " weather_offset=" + std::to_string(weather_offset_x) +
            " screen_destination=" +
            std::to_string(screen_effect_left) + "," +
            std::to_string(screen_effect_top));
    }

    const std::array<NativeRgbaLayer, 2> layers{
        NativeRgbaLayer{
            NativeLayerSlot::map_underlay,
            compat::RgbaFrameView{
                rgba_framebuffer_.pixels(), game_width, game_height},
            0,
            0,
            refresh_base_frame},
        NativeRgbaLayer{
            NativeLayerSlot::screen_effect,
            compat::RgbaFrameView{
                motion_screen_effect_rgba_.pixels(), layer_width, layer_height},
            screen_effect_left,
            screen_effect_top,
            refresh_weather},
    };
    const compat::RgbaFrameView modern_ui{
        modern_ui_framebuffer_.pixels(),
        modern_ui_framebuffer_.pixel_width(),
        modern_ui_framebuffer_.pixel_height()};
    if (!platform.present_native_layers(
            layers, modern_ui, refresh_modern_ui, fade_alpha)) {
        diagnostics::log_critical(
            std::string{"native weather present failed: "} + SDL_GetError());
        report_presentation_error(
            "present", "unable to present native weather layer", SDL_GetError());
        return 7;
    }
    return 0;
}

int FramePresenter::present(
    app::LegacyGameRuntime& game,
    SdlRuntimePlatform& platform,
    const bool refresh_legacy_frame,
    const std::int64_t weather_offset_x) {
    const auto fade_overlay = game.rgba_fade_overlay();
    const auto motion_active = game.motion_active();
    const auto& framebuffer = game.framebuffer();
    const auto game_width = framebuffer.pixel_width();
    const auto game_height = framebuffer.pixel_height();
    const auto presentation_scale = platform.presentation_scale();
    const auto legacy_dimensions_changed =
        rgba_framebuffer_.logical_width() != game_width ||
        rgba_framebuffer_.logical_height() != game_height;
    const auto ui_dimensions_changed =
        modern_ui_framebuffer_.logical_width() != game_width ||
        modern_ui_framebuffer_.logical_height() != game_height ||
        modern_ui_framebuffer_.scale() != presentation_scale;
    const auto refresh_base_frame =
        (!motion_active && refresh_legacy_frame &&
         fade_overlay.refresh_base_frame) ||
        !base_frame_ready_ || legacy_dimensions_changed;
    const auto refresh_modern_ui = refresh_base_frame || ui_dimensions_changed;
    if (((!motion_active && refresh_legacy_frame) || refresh_base_frame) &&
        !game.render()) {
        diagnostics::log_critical(
            "render failed view=" +
            std::to_string(static_cast<int>(game.view())));
        report_presentation_error(
            "render", "unable to render legacy game state");
        return 6;
    }
    if (refresh_base_frame) {
        if (!rgba_framebuffer_.set_dimensions(game_width, game_height, 1)) {
            diagnostics::log_critical(
                "legacy RGBA framebuffer resize failed size=" +
                std::to_string(game_width) + "x" + std::to_string(game_height));
            report_presentation_error(
                "render", "unable to resize legacy RGBA framebuffer");
            return 7;
        }
        const compat::IndexedFrameView indexed_frame{
            framebuffer.pixels(), framebuffer.palette(), game_width, game_height};
        if (!compat::convert_indexed_frame_to_rgba(
                indexed_frame, rgba_framebuffer_.pixels())) {
            diagnostics::log_critical("indexed framebuffer conversion failed");
            report_presentation_error(
                "render", "unable to convert indexed framebuffer to RGBA");
            return 7;
        }

        base_frame_ready_ = true;
    }
    if (refresh_modern_ui) {
        if (!modern_ui_framebuffer_.set_dimensions(
                game_width, game_height, presentation_scale)) {
            diagnostics::log_critical(
                "modern RGBA UI framebuffer resize failed size=" +
                std::to_string(game_width) + "x" + std::to_string(game_height) +
                " scale=" + std::to_string(presentation_scale));
            report_presentation_error(
                "render", "unable to resize modern RGBA UI framebuffer");
            return 7;
        }
        modern_ui_framebuffer_.clear({0U, 0U, 0U, 0U});
        if (!game.render_modern_ui(modern_ui_framebuffer_)) {
            diagnostics::log_critical("modern RGBA UI render failed");
            report_presentation_error(
                "render", "unable to render modern RGBA UI");
            return 7;
        }
    }

    if (motion_active && game.motion_layers_available()) {
        const auto motion = game.motion_presentation();
        if (motion.renderable()) {
            return present_native_motion(
                game,
                platform,
                motion,
                game_width,
                game_height,
                refresh_modern_ui,
                fade_overlay.alpha,
                weather_offset_x);
        }
    }
    if (!motion_active && game.weather_presentation_active()) {
        return present_native_weather(
            game,
            platform,
            game_width,
            game_height,
            refresh_base_frame,
            refresh_modern_ui,
            fade_overlay.alpha,
            weather_offset_x);
    }

    const compat::RgbaFrameView frame{
        rgba_framebuffer_.pixels(), game_width, game_height};
    const compat::RgbaFrameView modern_ui{
        modern_ui_framebuffer_.pixels(),
        modern_ui_framebuffer_.pixel_width(),
        modern_ui_framebuffer_.pixel_height()};
    if (!platform.present(
            frame,
            modern_ui,
            refresh_modern_ui,
            fade_overlay.alpha)) {
        diagnostics::log_critical(
            std::string{"RGBA framebuffer present failed: "} + SDL_GetError());
        report_presentation_error(
            "present", "unable to present RGBA framebuffer", SDL_GetError());
        return 7;
    }
    return 0;
}

}  // namespace openlegend::platform::sdl3
