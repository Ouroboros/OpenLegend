#include "openlegend/input/key_repeat.hpp"

namespace openlegend::input {
namespace {

[[nodiscard]] bool is_save_list_page_key(const compat::HostKey key) noexcept {
    return key == compat::HostKey::page_up || key == compat::HostKey::page_down;
}

[[nodiscard]] bool is_movement_direction_key(const compat::HostKey key) noexcept {
    using compat::HostKey;
    switch (key) {
    case HostKey::left:
    case HostKey::up:
    case HostKey::down:
    case HostKey::right:
    case HostKey::keypad_4:
    case HostKey::keypad_8:
    case HostKey::keypad_2:
    case HostKey::keypad_6:
        return true;
    default:
        return false;
    }
}

}  // namespace

KeyRepeatController::KeyRepeatController(
    const std::chrono::milliseconds initial_delay,
    const std::chrono::milliseconds save_list_page_interval) noexcept
    : initial_delay_(initial_delay),
      save_list_page_interval_(save_list_page_interval) {}

void KeyRepeatController::begin_frame() noexcept {
    movement_direction_pressed_ = false;
    save_list_page_key_pressed_ = false;
}

bool KeyRepeatController::handle_key_down(
    const compat::HostKey key,
    const bool host_repeat,
    const bool movement_repeat_active,
    const bool save_list_page_repeat_active,
    const TimePoint now) noexcept {
    const bool movement_direction = is_movement_direction_key(key);
    const bool controlled_direction = movement_direction && movement_repeat_active;
    const bool held_controlled_direction = movement_direction &&
        held_movement_direction_ == key;
    const bool controlled_save_list_page =
        is_save_list_page_key(key) && save_list_page_repeat_active;

    if (controlled_direction && !host_repeat) {
        held_movement_direction_ = key;
        movement_repeat_at_ = now + initial_delay_;
        movement_direction_pressed_ = true;
    }
    if (controlled_save_list_page && !host_repeat) {
        held_save_list_page_key_ = key;
        save_list_page_repeat_at_ = now + initial_delay_;
        save_list_page_key_pressed_ = true;
    }

    return (!controlled_direction && !held_controlled_direction &&
            !controlled_save_list_page) ||
        !host_repeat;
}

void KeyRepeatController::handle_key_up(const compat::HostKey key) noexcept {
    if (held_movement_direction_ == key) {
        held_movement_direction_.reset();
    }
    if (held_save_list_page_key_ == key) {
        held_save_list_page_key_.reset();
    }
}

std::optional<compat::HostKey> KeyRepeatController::take_movement_repeat(
    const bool active,
    const TimePoint now) noexcept {
    if (!active) {
        defer_movement_repeat(now);
        return std::nullopt;
    }
    if (movement_direction_pressed_ || !held_movement_direction_.has_value() ||
        now < movement_repeat_at_) {
        return std::nullopt;
    }
    return held_movement_direction_;
}

std::optional<compat::HostKey> KeyRepeatController::take_save_list_page_repeat(
    const bool active,
    const TimePoint now) noexcept {
    if (!active) {
        held_save_list_page_key_.reset();
        return std::nullopt;
    }
    if (save_list_page_key_pressed_ || !held_save_list_page_key_.has_value() ||
        now < save_list_page_repeat_at_) {
        return std::nullopt;
    }
    save_list_page_repeat_at_ = now + save_list_page_interval_;
    return held_save_list_page_key_;
}

void KeyRepeatController::defer_movement_repeat(const TimePoint now) noexcept {
    if (held_movement_direction_.has_value()) {
        movement_repeat_at_ = now + initial_delay_;
    }
}

}  // namespace openlegend::input
