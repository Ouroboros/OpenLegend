#include "sdl_runtime_platform.hpp"

#include <algorithm>
#include <limits>

namespace openlegend::platform::sdl3 {

SdlRuntimePlatform::SdlRuntimePlatform() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return;
    }

    window_ = SDL_CreateWindow("OpenLegend", 960, 600, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
        return;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr) {
        return;
    }

    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        static_cast<int>(compat::kLegacyWidth),
        static_cast<int>(compat::kLegacyHeight));
    if (texture_ != nullptr) {
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
    }
}

SdlRuntimePlatform::~SdlRuntimePlatform() {
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool SdlRuntimePlatform::valid() const noexcept {
    return window_ != nullptr && renderer_ != nullptr && texture_ != nullptr;
}

bool SdlRuntimePlatform::poll_event(compat::HostEvent& event) {
    SDL_Event sdl_event{};
    if (!SDL_PollEvent(&sdl_event)) {
        return false;
    }

    event.type = compat::HostEventType::none;
    if (sdl_event.type == SDL_EVENT_QUIT || sdl_event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        event.type = compat::HostEventType::quit;
    }
    return true;
}

bool SdlRuntimePlatform::present(const compat::IndexedFrameView frame) {
    if (!valid() || !frame.valid()) {
        return false;
    }

    for (std::size_t index = 0; index < frame.pixels.size(); ++index) {
        const auto palette_index = frame.pixels[index];
        const auto color = frame.palette[palette_index];
        const auto target = index * 4U;
        rgba_pixels_[target] = compat::expand_rgb6(color.red);
        rgba_pixels_[target + 1U] = compat::expand_rgb6(color.green);
        rgba_pixels_[target + 2U] = compat::expand_rgb6(color.blue);
        rgba_pixels_[target + 3U] = std::numeric_limits<std::uint8_t>::max();
    }

    if (!SDL_UpdateTexture(
            texture_,
            nullptr,
            rgba_pixels_.data(),
            static_cast<int>(compat::kLegacyWidth * 4U))) {
        return false;
    }

    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &output_width, &output_height)) {
        return false;
    }

    const auto scale_x = static_cast<float>(output_width) / static_cast<float>(compat::kLegacyWidth);
    const auto scale_y = static_cast<float>(output_height) / static_cast<float>(compat::kLegacyHeight);
    const auto scale = std::max(0.0F, std::min(scale_x, scale_y));
    const auto target_width = static_cast<float>(compat::kLegacyWidth) * scale;
    const auto target_height = static_cast<float>(compat::kLegacyHeight) * scale;
    const SDL_FRect destination{
        (static_cast<float>(output_width) - target_width) * 0.5F,
        (static_cast<float>(output_height) - target_height) * 0.5F,
        target_width,
        target_height};

    if (!SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255) || !SDL_RenderClear(renderer_) ||
        !SDL_RenderTexture(renderer_, texture_, nullptr, &destination)) {
        return false;
    }
    return SDL_RenderPresent(renderer_);
}

void SdlRuntimePlatform::delay(const std::chrono::milliseconds duration) {
    SDL_Delay(static_cast<std::uint32_t>(std::max(duration.count(), std::int64_t{0})));
}

}  // namespace openlegend::platform::sdl3
