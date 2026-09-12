#pragma once

#include <chrono>
#include <cstdint>

#include <SDL3/SDL.h>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/runtime_platform.hpp"

namespace openlegend::platform::sdl3 {

class SdlRuntimePlatform final : public compat::RuntimePlatform {
public:
    SdlRuntimePlatform(
        int window_width,
        int window_height,
        bool maximized,
        int game_width,
        int game_height);

    ~SdlRuntimePlatform() override;

    SdlRuntimePlatform(const SdlRuntimePlatform&) = delete;

    SdlRuntimePlatform& operator=(const SdlRuntimePlatform&) = delete;

    NODISCARD bool valid() const noexcept;

    NODISCARD bool query_window_state(
        int& normal_width, int& normal_height, bool& maximized) const noexcept;

    NODISCARD int presentation_scale() const noexcept;

    NODISCARD bool poll_event(compat::HostEvent& event) override;

    void wait_for_event_or_timeout(std::chrono::nanoseconds timeout) noexcept;

    NODISCARD bool present(
        compat::RgbaFrameView frame,
        compat::RgbaFrameView modern_ui) override;

    NODISCARD bool present(
        compat::RgbaFrameView frame,
        compat::RgbaFrameView modern_ui,
        bool refresh_textures,
        std::uint8_t fade_alpha);

    void delay(std::chrono::milliseconds duration) override;

private:
    NODISCARD bool ensure_frame_texture(int width, int height) noexcept;

    NODISCARD bool ensure_modern_ui_texture(int width, int height) noexcept;

    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    SDL_Texture* modern_ui_texture_{};
    int texture_width_{};
    int texture_height_{};
    int modern_ui_texture_width_{};
    int modern_ui_texture_height_{};
    int game_width_{};
    int game_height_{};
    bool textures_initialized_{};
};

}  // namespace openlegend::platform::sdl3
