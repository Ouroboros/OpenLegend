#pragma once

#include <array>
#include <chrono>
#include <cstdint>

#include <SDL3/SDL.h>

#include "openlegend/compat/runtime_platform.hpp"

namespace openlegend::platform::sdl3 {

class SdlRuntimePlatform final : public compat::RuntimePlatform {
public:
    SdlRuntimePlatform(int window_width, int window_height, bool maximized);
    ~SdlRuntimePlatform() override;

    SdlRuntimePlatform(const SdlRuntimePlatform&) = delete;
    SdlRuntimePlatform& operator=(const SdlRuntimePlatform&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] bool query_window_state(
        int& normal_width, int& normal_height, bool& maximized) const noexcept;
    [[nodiscard]] bool poll_event(compat::HostEvent& event) override;
    [[nodiscard]] bool present(compat::IndexedFrameView frame) override;
    void delay(std::chrono::milliseconds duration) override;

private:
    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    std::array<std::uint8_t, compat::kLegacyPixelCount * 4U> rgba_pixels_{};
};

}  // namespace openlegend::platform::sdl3
