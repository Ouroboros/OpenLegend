#include "legacy_input_coordinator.hpp"

#include <chrono>
#include <cstdint>
#include <string>

#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/scene/scene.hpp"
#include "sdl_runtime_platform.hpp"

namespace openlegend::platform::sdl3 {

LegacyInputCoordinator::LegacyInputCoordinator(app::LegacyGameRuntime& game)
    : game_(game) {}

bool LegacyInputCoordinator::accepts_movement_repeat() const noexcept {
    return game_.view() == app::LegacyGameView::world ||
        game_.scene_loop_uses_key_states();
}

void LegacyInputCoordinator::process_host_events(
    SdlRuntimePlatform& platform,
    input::KeyRepeatController& key_repeat,
    const std::uint32_t frame_tick,
    const std::chrono::steady_clock::time_point input_now,
    bool& running) {
    compat::HostEvent event{};
    while (platform.poll_event(event)) {
        if (event.type == compat::HostEventType::quit) {
            diagnostics::log_info("host quit event");
            running = false;
        } else if (event.type == compat::HostEventType::key_down) {
            if (key_repeat.handle_key_down(
                    event.key,
                    event.repeat,
                    accepts_movement_repeat(),
                    game_.save_list_active(),
                    input_now)) {
                dispatch_key_down(event.key, event.repeat, frame_tick);
            }
        } else if (event.type == compat::HostEventType::key_up) {
            key_repeat.handle_key_up(event.key);
            keyboard_.handle_host_key(event.key, false);
            diagnostics::log_debug(
                "host key_up key=" +
                std::to_string(static_cast<int>(event.key)));
        }
        synchronize();
    }
}

void LegacyInputCoordinator::dispatch_repeats(
    input::KeyRepeatController& key_repeat,
    const std::uint32_t frame_tick) {
    if (const auto repeated_key = key_repeat.take_movement_repeat(
            accepts_movement_repeat(), std::chrono::steady_clock::now())) {
        keyboard_.handle_host_key(*repeated_key, false);
        dispatch_key_down(*repeated_key, true, frame_tick);
    }
    if (const auto repeated_key = key_repeat.take_save_list_page_repeat(
            game_.save_list_active(), std::chrono::steady_clock::now())) {
        keyboard_.handle_host_key(*repeated_key, false);
        dispatch_key_down(*repeated_key, true, frame_tick);
    }
    synchronize();
}

void LegacyInputCoordinator::apply_game_input() {
    const auto world_direction = keyboard_.world_direction();
    using input::LegacyWorldDirectionInput;
    const bool directional_input_consumed = game_.handle_world_input(
        world_direction == LegacyWorldDirectionInput::left,
        world_direction == LegacyWorldDirectionInput::up,
        world_direction == LegacyWorldDirectionInput::down,
        world_direction == LegacyWorldDirectionInput::right,
        keyboard_.edge(input::legacy_key::escape));
    if (directional_input_consumed &&
        world_direction != LegacyWorldDirectionInput::none) {
        keyboard_.consume_world_direction(world_direction);
    } else if (directional_input_consumed &&
               keyboard_.edge(input::legacy_key::escape)) {
        keyboard_.consume_edge(input::legacy_key::escape);
    }
    game_.set_scene_input_states(
        keyboard_.down(input::legacy_key::enter) ||
            keyboard_.down(input::legacy_key::space) ||
            keyboard_.down(input::legacy_key::keypad_insert),
        keyboard_.edge(input::legacy_key::escape),
        keyboard_.edge(input::legacy_key::weather_toggle));
}

void LegacyInputCoordinator::after_advance(
    input::KeyRepeatController& key_repeat) {
    synchronize();
    if (game_.take_clear_scene_exit_key_states_request()) {
        keyboard_.clear_scene_exit_key_states();
        key_repeat.defer_movement_repeat(std::chrono::steady_clock::now());
    }
}

void LegacyInputCoordinator::after_present() {
    sync_scene_input_reset();
    if (game_.take_clear_scene_exit_key_states_request()) {
        keyboard_.clear_scene_exit_key_states();
    }
    sync_battle_confirmation();
}

void LegacyInputCoordinator::synchronize() {
    sync_scene_input_reset();
    sync_battle_confirmation();
}

void LegacyInputCoordinator::sync_battle_confirmation() {
    if (game_.take_clear_battle_confirmation_states_request()) {
        keyboard_.clear_confirmation_states();
    }
    const auto direction = game_.take_clear_battle_menu_direction_request();
    if (direction != 0U) {
        keyboard_.clear_state(direction);
    }
    const auto cursor_key = game_.take_clear_battle_cursor_key_request();
    if (cursor_key == input::legacy_key::down) {
        keyboard_.consume_world_direction(
            input::LegacyWorldDirectionInput::down);
    } else if (cursor_key == input::legacy_key::right) {
        keyboard_.consume_world_direction(
            input::LegacyWorldDirectionInput::right);
    } else if (cursor_key == input::legacy_key::left) {
        keyboard_.consume_world_direction(
            input::LegacyWorldDirectionInput::left);
    } else if (cursor_key == input::legacy_key::up) {
        keyboard_.consume_world_direction(
            input::LegacyWorldDirectionInput::up);
    } else if (cursor_key != 0U) {
        keyboard_.clear_state(cursor_key);
    }
    const auto any_down = [this](const auto& keys) {
        return keyboard_.down(keys[0]) || keyboard_.down(keys[1]);
    };
    game_.set_battle_confirmation_state(
        keyboard_.down(input::legacy_key::enter) ||
        keyboard_.down(input::legacy_key::space) ||
        keyboard_.down(input::legacy_key::keypad_insert));
    game_.set_battle_menu_direction_states(
        keyboard_.down(input::legacy_key::down),
        keyboard_.down(input::legacy_key::up));
    game_.set_battle_cursor_input_states(
        any_down(input::legacy_key::world_down),
        any_down(input::legacy_key::world_right),
        any_down(input::legacy_key::world_left),
        any_down(input::legacy_key::world_up),
        keyboard_.down(input::legacy_key::escape));
}

void LegacyInputCoordinator::sync_scene_input_reset() {
    switch (game_.take_scene_input_reset_request()) {
    case scene::SceneInputReset::confirmation_group:
        keyboard_.clear_confirmation_states();
        break;
    case scene::SceneInputReset::main_ui_edge:
        keyboard_.consume_edge(input::legacy_key::escape);
        break;
    case scene::SceneInputReset::weather_disable_edge:
        keyboard_.consume_edge(input::legacy_key::weather_toggle);
        break;
    case scene::SceneInputReset::none:
        break;
    }
}

void LegacyInputCoordinator::dispatch_key_down(
    const compat::HostKey key,
    const bool repeat,
    const std::uint32_t frame_tick) {
    keyboard_.handle_host_key(key, true);
    sync_battle_confirmation();
    const auto translated_key = keyboard_.last_key();
    diagnostics::log_debug(
        "host key_down key=" + std::to_string(static_cast<int>(key)) +
        " repeat=" + (repeat ? std::string{"true"} : std::string{"false"}) +
        " translated=" + std::to_string(translated_key));
    if (translated_key == 0U) {
        return;
    }

    const bool defer_world_menu =
        translated_key == input::legacy_key::escape &&
        game_.view() == app::LegacyGameView::world;
    const bool defer_scene_input = game_.scene_loop_uses_key_states() &&
        (translated_key == input::legacy_key::escape ||
         translated_key == input::legacy_key::weather_toggle ||
         translated_key == input::legacy_key::enter ||
         translated_key == input::legacy_key::space ||
         translated_key == input::legacy_key::keypad_insert);
    if (!defer_world_menu && !defer_scene_input &&
        !game_.battle_menu_uses_key_states()) {
        const auto key_state_reset = game_.handle_key(
            translated_key,
            keyboard_.down(input::legacy_key::left_control),
            keyboard_.down(input::legacy_key::left_shift) ||
                keyboard_.down(input::legacy_key::right_shift),
            frame_tick);
        if (key_state_reset == app::LegacyKeyStateReset::edge) {
            keyboard_.consume_edge(translated_key);
        } else if (key_state_reset == app::LegacyKeyStateReset::translated) {
            keyboard_.clear_state(translated_key);
        } else if (
            key_state_reset == app::LegacyKeyStateReset::down_translated) {
            keyboard_.clear_state(input::legacy_key::down);
        } else if (
            key_state_reset == app::LegacyKeyStateReset::confirmation_group) {
            keyboard_.clear_confirmation_states();
        } else if (translated_key == input::legacy_key::escape) {
            keyboard_.consume_edge(input::legacy_key::escape);
        }
    }
    keyboard_.clear_last_key();
}

}  // namespace openlegend::platform::sdl3
