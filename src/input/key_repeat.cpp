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

std::optional<compat::HostKey>
KeyRepeatController::active_movement_direction() const noexcept {
    if (held_movement_key_count_ == 0U) {
        return std::nullopt;
    }
    return held_movement_keys_[held_movement_key_count_ - 1U];
}

bool KeyRepeatController::movement_key_held(const compat::HostKey key) const noexcept {
    for (std::size_t index = 0U; index < held_movement_key_count_; ++index) {
        if (held_movement_keys_[index] == key) {
            return true;
        }
    }
    return false;
}

void KeyRepeatController::remember_movement_key(const compat::HostKey key) noexcept {
    static_cast<void>(forget_movement_key(key));
    if (held_movement_key_count_ < held_movement_keys_.size()) {
        held_movement_keys_[held_movement_key_count_] = key;
        ++held_movement_key_count_;
    }
}

bool KeyRepeatController::forget_movement_key(const compat::HostKey key) noexcept {
    for (std::size_t index = 0U; index < held_movement_key_count_; ++index) {
        if (held_movement_keys_[index] != key) {
            continue;
        }
        const bool was_active = index + 1U == held_movement_key_count_;
        for (std::size_t next = index + 1U; next < held_movement_key_count_; ++next) {
            held_movement_keys_[next - 1U] = held_movement_keys_[next];
        }
        --held_movement_key_count_;
        return was_active;
    }
    return false;
}

bool KeyRepeatController::handle_key_down(
    const compat::HostKey key,
    const bool host_repeat,
    const bool movement_repeat_active,
    const bool save_list_page_repeat_active,
    const TimePoint now) noexcept {
    const bool movement_direction = is_movement_direction_key(key);
    const bool controlled_direction = movement_direction && movement_repeat_active;
    const bool held_controlled_direction = movement_direction && movement_key_held(key);
    const bool controlled_save_list_page =
        is_save_list_page_key(key) && save_list_page_repeat_active;

    if (controlled_direction && !host_repeat) {
        remember_movement_key(key);
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
    if (forget_movement_key(key) && held_movement_key_count_ != 0U) {
        movement_repeat_at_ = TimePoint::min();
    }
    if (held_save_list_page_key_ == key) {
        held_save_list_page_key_.reset();
    }
}

std::optional<compat::HostKey> KeyRepeatController::take_movement_repeat(
    const bool active,
    const TimePoint now) noexcept {
    if (!active) {
        if (movement_repeat_was_active_) {
            defer_movement_repeat(now);
        }
        movement_repeat_was_active_ = false;
        return std::nullopt;
    }
    movement_repeat_was_active_ = true;
    const auto direction = active_movement_direction();
    if (movement_direction_pressed_ || !direction.has_value() ||
        now < movement_repeat_at_) {
        return std::nullopt;
    }
    return direction;
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
    if (held_movement_key_count_ != 0U) {
        movement_repeat_at_ = now + initial_delay_;
    }
}

}  // namespace openlegend::input
