#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>

#include <SDL3/SDL.h>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/runtime_platform.hpp"

namespace openlegend::platform::sdl3 {

enum class NativeLayerSlot : std::uint8_t {
    map_underlay,
    actor,
    map_overlay,
    screen_effect,
    count,
};

struct NativeRgbaLayer {
    NativeLayerSlot slot{NativeLayerSlot::map_underlay};
    compat::RgbaFrameView frame;
    std::int64_t logical_x{};
    std::int64_t logical_y{};
    bool refresh_texture{true};
};

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

    NODISCARD bool set_vsync_enabled(bool enabled) noexcept;

    NODISCARD bool vsync_enabled() const noexcept { return vsync_enabled_; }

    NODISCARD bool poll_event(compat::HostEvent& event) override;

    NODISCARD bool synchronize_name_text_input(
        bool enabled, std::size_t cursor_bytes) noexcept;

    void wait_for_event_or_timeout(std::chrono::nanoseconds timeout) noexcept;

    NODISCARD bool present(
        compat::RgbaFrameView frame,
        compat::RgbaFrameView modern_ui) override;

    NODISCARD bool present(
        compat::RgbaFrameView frame,
        compat::RgbaFrameView modern_ui,
        bool refresh_textures,
        std::uint8_t fade_alpha);

    NODISCARD bool present_native_layers(
        std::span<const NativeRgbaLayer> layers,
        compat::RgbaFrameView modern_ui,
        bool refresh_modern_ui,
        std::uint8_t fade_alpha);

    void delay(std::chrono::milliseconds duration) override;

private:
    NODISCARD bool ensure_frame_texture(int width, int height) noexcept;

    struct NativeLayerTexture {
        SDL_Texture* texture{};
        int width{};
        int height{};
        bool initialized{};
    };

    NODISCARD bool ensure_modern_ui_texture(int width, int height) noexcept;

    NODISCARD bool ensure_native_layer_texture(
        NativeLayerSlot slot, int width, int height) noexcept;

    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    SDL_Texture* texture_{};
    SDL_Texture* modern_ui_texture_{};
    std::array<
        NativeLayerTexture,
        static_cast<std::size_t>(NativeLayerSlot::count)> native_layer_textures_{};
    int texture_width_{};
    int texture_height_{};
    int modern_ui_texture_width_{};
    int modern_ui_texture_height_{};
    int game_width_{};
    int game_height_{};
    bool textures_initialized_{};
    bool modern_ui_texture_initialized_{};
    bool vsync_enabled_{};
};

}  // namespace openlegend::platform::sdl3
