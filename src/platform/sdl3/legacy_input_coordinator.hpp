#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "openlegend/compat/runtime_platform.hpp"
#include "openlegend/input/key_repeat.hpp"
#include "openlegend/input/legacy_keyboard.hpp"

namespace openlegend::app {
class LegacyGameRuntime;
}

namespace openlegend::platform::sdl3 {

class SdlRuntimePlatform;

class LegacyInputCoordinator {
public:
    explicit LegacyInputCoordinator(app::LegacyGameRuntime& game);

    void process_host_events(
        SdlRuntimePlatform& platform,
        input::KeyRepeatController& key_repeat,
        std::uint32_t frame_tick,
        std::chrono::steady_clock::time_point input_now,
        bool& running);

    void dispatch_repeats(
        input::KeyRepeatController& key_repeat, std::uint32_t frame_tick);

    void apply_game_input();

    void after_advance(input::KeyRepeatController& key_repeat);

    void after_present();

    [[nodiscard]] bool waits_for_menu_input() const noexcept;

private:
    [[nodiscard]] bool uses_shared_direction_repeat() const noexcept;

    void synchronize();

    void sync_battle_confirmation();

    void sync_scene_input_reset();

    void dispatch_key_down(
        compat::HostKey key, bool repeat, std::uint32_t frame_tick);

    app::LegacyGameRuntime& game_;
    input::LegacyKeyboard keyboard_;
    std::optional<std::uint32_t> last_direction_repeat_tick_;
};

}  // namespace openlegend::platform::sdl3
