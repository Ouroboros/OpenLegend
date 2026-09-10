#include "frame_presenter.hpp"

#include <iostream>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>

#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/diagnostics/log.hpp"
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

int FramePresenter::present(
    app::LegacyGameRuntime& game, SdlRuntimePlatform& platform) {
    if (!game.render()) {
        diagnostics::log_critical(
            "render failed view=" +
            std::to_string(static_cast<int>(game.view())));
        report_presentation_error(
            "render", "unable to render legacy game state");
        return 6;
    }

    const auto& framebuffer = game.framebuffer();
    const compat::IndexedFrameView indexed_frame{
        framebuffer.pixels(), framebuffer.palette()};
    if (!compat::convert_indexed_frame_to_rgba(
            indexed_frame, rgba_framebuffer_.pixels())) {
        diagnostics::log_critical("indexed framebuffer conversion failed");
        report_presentation_error(
            "render", "unable to convert indexed framebuffer to RGBA");
        return 7;
    }

    const auto presentation_scale = platform.presentation_scale();
    if (!modern_ui_framebuffer_.set_scale(presentation_scale)) {
        diagnostics::log_critical(
            "modern RGBA UI framebuffer resize failed scale=" +
            std::to_string(presentation_scale));
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

    const compat::RgbaFrameView frame{rgba_framebuffer_.pixels()};
    const compat::RgbaFrameView modern_ui{
        modern_ui_framebuffer_.pixels(),
        modern_ui_framebuffer_.pixel_width(),
        modern_ui_framebuffer_.pixel_height()};
    if (!platform.present(frame, modern_ui)) {
        diagnostics::log_critical(
            std::string{"RGBA framebuffer present failed: "} + SDL_GetError());
        report_presentation_error(
            "present", "unable to present RGBA framebuffer", SDL_GetError());
        return 7;
    }
    return 0;
}

}  // namespace openlegend::platform::sdl3
