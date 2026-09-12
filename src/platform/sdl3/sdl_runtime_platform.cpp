#include "sdl_runtime_platform.hpp"

#include <algorithm>

#include "openlegend/compat/color.hpp"

namespace openlegend::platform::sdl3 {
namespace {

[[nodiscard]] compat::HostKey host_key_from_scancode(const SDL_Scancode scancode) noexcept {
    using enum compat::HostKey;
    switch (scancode) {
    case SDL_SCANCODE_ESCAPE: return escape;
    case SDL_SCANCODE_1: return digit_1;
    case SDL_SCANCODE_2: return digit_2;
    case SDL_SCANCODE_3: return digit_3;
    case SDL_SCANCODE_4: return digit_4;
    case SDL_SCANCODE_5: return digit_5;
    case SDL_SCANCODE_6: return digit_6;
    case SDL_SCANCODE_7: return digit_7;
    case SDL_SCANCODE_8: return digit_8;
    case SDL_SCANCODE_9: return digit_9;
    case SDL_SCANCODE_0: return digit_0;
    case SDL_SCANCODE_MINUS: return minus;
    case SDL_SCANCODE_EQUALS: return equals;
    case SDL_SCANCODE_BACKSPACE: return backspace;
    case SDL_SCANCODE_TAB: return tab;
    case SDL_SCANCODE_Q: return q;
    case SDL_SCANCODE_W: return w;
    case SDL_SCANCODE_E: return e;
    case SDL_SCANCODE_R: return r;
    case SDL_SCANCODE_T: return t;
    case SDL_SCANCODE_Y: return y;
    case SDL_SCANCODE_U: return u;
    case SDL_SCANCODE_I: return i;
    case SDL_SCANCODE_O: return o;
    case SDL_SCANCODE_P: return p;
    case SDL_SCANCODE_LEFTBRACKET: return left_bracket;
    case SDL_SCANCODE_RIGHTBRACKET: return right_bracket;
    case SDL_SCANCODE_RETURN: return enter;
    case SDL_SCANCODE_KP_ENTER: return keypad_enter;
    case SDL_SCANCODE_LCTRL: return left_control;
    case SDL_SCANCODE_RCTRL: return right_control;
    case SDL_SCANCODE_A: return a;
    case SDL_SCANCODE_S: return s;
    case SDL_SCANCODE_D: return d;
    case SDL_SCANCODE_F: return f;
    case SDL_SCANCODE_G: return g;
    case SDL_SCANCODE_H: return h;
    case SDL_SCANCODE_J: return j;
    case SDL_SCANCODE_K: return k;
    case SDL_SCANCODE_L: return l;
    case SDL_SCANCODE_SEMICOLON: return semicolon;
    case SDL_SCANCODE_APOSTROPHE: return apostrophe;
    case SDL_SCANCODE_GRAVE: return grave;
    case SDL_SCANCODE_LSHIFT: return left_shift;
    case SDL_SCANCODE_BACKSLASH: return backslash;
    case SDL_SCANCODE_Z: return z;
    case SDL_SCANCODE_X: return x;
    case SDL_SCANCODE_C: return c;
    case SDL_SCANCODE_V: return v;
    case SDL_SCANCODE_B: return b;
    case SDL_SCANCODE_N: return n;
    case SDL_SCANCODE_M: return m;
    case SDL_SCANCODE_COMMA: return comma;
    case SDL_SCANCODE_PERIOD: return period;
    case SDL_SCANCODE_SLASH: return slash;
    case SDL_SCANCODE_RSHIFT: return right_shift;
    case SDL_SCANCODE_KP_MULTIPLY: return keypad_multiply;
    case SDL_SCANCODE_KP_DIVIDE: return keypad_divide;
    case SDL_SCANCODE_LALT: return left_alt;
    case SDL_SCANCODE_RALT: return right_alt;
    case SDL_SCANCODE_SPACE: return space;
    case SDL_SCANCODE_CAPSLOCK: return caps_lock;
    case SDL_SCANCODE_F1: return f1;
    case SDL_SCANCODE_F2: return f2;
    case SDL_SCANCODE_F3: return f3;
    case SDL_SCANCODE_F4: return f4;
    case SDL_SCANCODE_F5: return f5;
    case SDL_SCANCODE_F6: return f6;
    case SDL_SCANCODE_F7: return f7;
    case SDL_SCANCODE_F8: return f8;
    case SDL_SCANCODE_F9: return f9;
    case SDL_SCANCODE_F10: return f10;
    case SDL_SCANCODE_F11: return f11;
    case SDL_SCANCODE_F12: return f12;
    case SDL_SCANCODE_SYSREQ: return sysrq;
    case SDL_SCANCODE_NONUSBACKSLASH: return non_us_backslash;
    case SDL_SCANCODE_NUMLOCKCLEAR: return num_lock;
    case SDL_SCANCODE_SCROLLLOCK: return scroll_lock;
    case SDL_SCANCODE_KP_7: return keypad_7;
    case SDL_SCANCODE_HOME: return home;
    case SDL_SCANCODE_KP_8: return keypad_8;
    case SDL_SCANCODE_UP: return up;
    case SDL_SCANCODE_KP_9: return keypad_9;
    case SDL_SCANCODE_PAGEUP: return page_up;
    case SDL_SCANCODE_KP_MINUS: return keypad_minus;
    case SDL_SCANCODE_KP_4: return keypad_4;
    case SDL_SCANCODE_LEFT: return left;
    case SDL_SCANCODE_KP_5: return keypad_5;
    case SDL_SCANCODE_KP_6: return keypad_6;
    case SDL_SCANCODE_RIGHT: return right;
    case SDL_SCANCODE_KP_PLUS: return keypad_plus;
    case SDL_SCANCODE_KP_1: return keypad_1;
    case SDL_SCANCODE_END: return end;
    case SDL_SCANCODE_KP_2: return keypad_2;
    case SDL_SCANCODE_DOWN: return down;
    case SDL_SCANCODE_KP_3: return keypad_3;
    case SDL_SCANCODE_PAGEDOWN: return page_down;
    case SDL_SCANCODE_KP_0: return keypad_0;
    case SDL_SCANCODE_INSERT: return insert;
    case SDL_SCANCODE_KP_DECIMAL: return keypad_decimal;
    case SDL_SCANCODE_DELETE: return delete_key;
    case SDL_SCANCODE_KP_EQUALS: return keypad_equals;
    case SDL_SCANCODE_LGUI: return left_gui;
    case SDL_SCANCODE_RGUI: return right_gui;
    case SDL_SCANCODE_APPLICATION: return application;
    case SDL_SCANCODE_PRINTSCREEN: return print_screen;
    case SDL_SCANCODE_PAUSE: return pause;
    default: return unknown;
    }
}

}  // namespace

SdlRuntimePlatform::SdlRuntimePlatform(
    const int window_width,
    const int window_height,
    const bool maximized,
    const int game_width,
    const int game_height)
    : game_width_(game_width),
      game_height_(game_height) {
    if (game_width_ <= 0 || game_height_ <= 0 ||
        !SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return;
    }

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
    if (maximized) {
        flags |= SDL_WINDOW_MAXIMIZED;
    }
    window_ = SDL_CreateWindow(
        "OpenLegend",
        std::max(window_width, game_width_),
        std::max(window_height, game_height_),
        flags);
    if (window_ == nullptr ||
        !SDL_SetWindowMinimumSize(window_, game_width_, game_height_)) {
        return;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr ||
        !ensure_frame_texture(game_width_, game_height_)) {
        return;
    }
}

SdlRuntimePlatform::~SdlRuntimePlatform() {
    SDL_DestroyTexture(modern_ui_texture_);
    SDL_DestroyTexture(texture_);
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool SdlRuntimePlatform::valid() const noexcept {
    return window_ != nullptr && renderer_ != nullptr && texture_ != nullptr;
}

bool SdlRuntimePlatform::query_window_state(
    int& normal_width, int& normal_height, bool& maximized) const noexcept {
    if (window_ == nullptr) {
        return false;
    }
    maximized = (SDL_GetWindowFlags(window_) & SDL_WINDOW_MAXIMIZED) != 0U;
    return maximized || SDL_GetWindowSize(window_, &normal_width, &normal_height);
}

int SdlRuntimePlatform::presentation_scale() const noexcept {
    if (renderer_ == nullptr) {
        return 0;
    }
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(
            renderer_, &output_width, &output_height)) {
        return 0;
    }
    return compat::proportional_viewport(
        output_width,
        output_height,
        game_width_,
        game_height_).presentation_scale;
}

bool SdlRuntimePlatform::poll_event(compat::HostEvent& event) {
    SDL_Event sdl_event{};
    if (!SDL_PollEvent(&sdl_event)) {
        return false;
    }

    event = {};
    if (sdl_event.type == SDL_EVENT_QUIT || sdl_event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        event.type = compat::HostEventType::quit;
    } else if (sdl_event.type == SDL_EVENT_KEY_DOWN || sdl_event.type == SDL_EVENT_KEY_UP) {
        event.key = host_key_from_scancode(sdl_event.key.scancode);
        event.repeat = sdl_event.key.repeat;
        if (event.key != compat::HostKey::unknown) {
            event.type = sdl_event.type == SDL_EVENT_KEY_DOWN ? compat::HostEventType::key_down
                                                             : compat::HostEventType::key_up;
        }
    }
    return true;
}

void SdlRuntimePlatform::wait_for_event_or_timeout(
    const std::chrono::nanoseconds timeout) noexcept {
    const auto bounded = std::max(timeout, std::chrono::nanoseconds::zero());
    if (bounded > std::chrono::nanoseconds::zero()) {
        const auto timeout_ms =
            std::chrono::ceil<std::chrono::milliseconds>(bounded);
        static_cast<void>(SDL_WaitEventTimeout(
            nullptr, static_cast<Sint32>(timeout_ms.count())));
    }
}

bool SdlRuntimePlatform::ensure_frame_texture(
    const int width, const int height) noexcept {
    if (texture_ != nullptr && texture_width_ == width &&
        texture_height_ == height) {
        return true;
    }

    auto* replacement = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);
    if (replacement == nullptr ||
        !SDL_SetTextureScaleMode(replacement, SDL_SCALEMODE_NEAREST)) {
        SDL_DestroyTexture(replacement);
        return false;
    }

    SDL_DestroyTexture(texture_);
    texture_ = replacement;
    texture_width_ = width;
    texture_height_ = height;
    textures_initialized_ = false;
    return true;
}

bool SdlRuntimePlatform::ensure_modern_ui_texture(
    const int width, const int height) noexcept {
    if (modern_ui_texture_ != nullptr &&
        modern_ui_texture_width_ == width &&
        modern_ui_texture_height_ == height) {
        return true;
    }

    auto* replacement = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);
    if (replacement == nullptr ||
        !SDL_SetTextureBlendMode(
            replacement, SDL_BLENDMODE_BLEND_PREMULTIPLIED) ||
        !SDL_SetTextureScaleMode(replacement, SDL_SCALEMODE_NEAREST)) {
        SDL_DestroyTexture(replacement);
        return false;
    }

    SDL_DestroyTexture(modern_ui_texture_);
    modern_ui_texture_ = replacement;
    modern_ui_texture_width_ = width;
    modern_ui_texture_height_ = height;
    textures_initialized_ = false;
    return true;
}

bool SdlRuntimePlatform::present(
    const compat::RgbaFrameView frame,
    const compat::RgbaFrameView modern_ui) {
    return present(frame, modern_ui, true, 0U);
}

bool SdlRuntimePlatform::present(
    const compat::RgbaFrameView frame,
    const compat::RgbaFrameView modern_ui,
    const bool refresh_textures,
    const std::uint8_t fade_alpha) {
    if (!valid() || !frame.valid() || !modern_ui.valid() ||
        frame.width != game_width_ || frame.height != game_height_ ||
        !ensure_frame_texture(frame.width, frame.height) ||
        !ensure_modern_ui_texture(modern_ui.width, modern_ui.height)) {
        return false;
    }

    if ((refresh_textures || !textures_initialized_) &&
        (!SDL_UpdateTexture(
             texture_,
             nullptr,
             frame.pixels.data(),
             frame.width * static_cast<int>(compat::kModernRgbaBytesPerPixel)) ||
         !SDL_UpdateTexture(
             modern_ui_texture_,
             nullptr,
             modern_ui.pixels.data(),
             modern_ui.width *
                 static_cast<int>(compat::kModernRgbaBytesPerPixel)))) {
        return false;
    }
    textures_initialized_ = true;

    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetCurrentRenderOutputSize(renderer_, &output_width, &output_height)) {
        return false;
    }

    const auto viewport = compat::proportional_viewport(
        output_width, output_height, frame.width, frame.height);
    if (!viewport.valid()) {
        return false;
    }
    const SDL_FRect destination{
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height};

    constexpr auto clear_color = compat::kRuntimeClearColor;
    if (!SDL_SetRenderDrawColor(
            renderer_,
            clear_color.red,
            clear_color.green,
            clear_color.blue,
            clear_color.alpha) ||
        !SDL_RenderClear(renderer_) ||
        !SDL_RenderTexture(renderer_, texture_, nullptr, &destination) ||
        !SDL_RenderTexture(
            renderer_, modern_ui_texture_, nullptr, &destination)) {
        return false;
    }
    if (fade_alpha != 0U &&
        (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND) ||
         !SDL_SetRenderDrawColor(renderer_, 0U, 0U, 0U, fade_alpha) ||
         !SDL_RenderFillRect(renderer_, &destination))) {
        return false;
    }
    return SDL_RenderPresent(renderer_);
}

void SdlRuntimePlatform::delay(const std::chrono::milliseconds duration) {
    SDL_Delay(static_cast<std::uint32_t>(std::max(duration.count(), std::int64_t{0})));
}

}  // namespace openlegend::platform::sdl3
