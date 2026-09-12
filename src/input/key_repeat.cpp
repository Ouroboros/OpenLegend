#include "openlegend/attributes.hpp"
#include "openlegend/input/key_repeat.hpp"

namespace openlegend::input {
namespace {

NODISCARD bool is_cardinal_direction_key(const compat::HostKey key) noexcept {
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

NODISCARD bool is_page_navigation_key(const compat::HostKey key) noexcept {
    using compat::HostKey;
    return key == HostKey::page_up || key == HostKey::page_down ||
        key == HostKey::keypad_9 || key == HostKey::keypad_3;
}

NODISCARD bool is_movement_direction_key(const compat::HostKey key) noexcept {
    using compat::HostKey;
    return is_cardinal_direction_key(key) || is_page_navigation_key(key) ||
        key == HostKey::home || key == HostKey::end ||
        key == HostKey::keypad_7 || key == HostKey::keypad_1;
}

NODISCARD bool is_menu_repeat_key(const compat::HostKey key) noexcept {
    return is_cardinal_direction_key(key) || is_page_navigation_key(key);
}

NODISCARD bool is_save_list_single_press_key(
    const compat::HostKey key) noexcept {
    using compat::HostKey;
    return key == HostKey::home || key == HostKey::end ||
        key == HostKey::keypad_7 || key == HostKey::keypad_1;
}

template <std::size_t Capacity>
NODISCARD bool key_held(
    const std::array<compat::HostKey, Capacity>& keys,
    const std::size_t count,
    const compat::HostKey key) noexcept {
    for (std::size_t index = 0U; index < count; ++index) {
        if (keys[index] == key) {
            return true;
        }
    }
    return false;
}

template <std::size_t Capacity>
NODISCARD bool forget_key(
    std::array<compat::HostKey, Capacity>& keys,
    std::size_t& count,
    const compat::HostKey key) noexcept {
    for (std::size_t index = 0U; index < count; ++index) {
        if (keys[index] != key) {
            continue;
        }
        const bool was_active = index + 1U == count;
        for (std::size_t next = index + 1U; next < count; ++next) {
            keys[next - 1U] = keys[next];
        }
        --count;
        return was_active;
    }
    return false;
}

template <std::size_t Capacity>
void remember_key(
    std::array<compat::HostKey, Capacity>& keys,
    std::size_t& count,
    const compat::HostKey key) noexcept {
    static_cast<void>(forget_key(keys, count, key));
    if (count < keys.size()) {
        keys[count] = key;
        ++count;
    }
}

template <std::size_t Capacity>
void remember_passive_key(
    std::array<compat::HostKey, Capacity>& keys,
    std::size_t& count,
    const compat::HostKey key) noexcept {
    static_cast<void>(forget_key(keys, count, key));
    if (count == 0U) {
        remember_key(keys, count, key);
        return;
    }
    if (count < keys.size()) {
        keys[count] = keys[count - 1U];
        keys[count - 1U] = key;
        ++count;
    }
}

template <std::size_t SourceCapacity, std::size_t BlockedCapacity>
void block_keys(
    std::array<compat::HostKey, SourceCapacity>& source,
    std::size_t& source_count,
    std::array<compat::HostKey, BlockedCapacity>& blocked,
    std::size_t& blocked_count) noexcept {
    for (std::size_t index = 0U; index < source_count; ++index) {
        remember_key(blocked, blocked_count, source[index]);
    }
    source_count = 0U;
}

template <std::size_t Capacity>
NODISCARD std::optional<compat::HostKey> active_key(
    const std::array<compat::HostKey, Capacity>& keys,
    const std::size_t count) noexcept {
    if (count == 0U) {
        return std::nullopt;
    }
    return keys[count - 1U];
}

NODISCARD bool is_movement_context(
    const DirectionRepeatContext context) noexcept {
    return context == DirectionRepeatContext::movement ||
        context == DirectionRepeatContext::battle_cursor;
}

NODISCARD bool is_menu_context(const DirectionRepeatContext context) noexcept {
    return context == DirectionRepeatContext::menu ||
        context == DirectionRepeatContext::save_list;
}

}  // namespace

KeyRepeatController::KeyRepeatController(
    const std::chrono::milliseconds movement_initial_delay,
    const std::chrono::milliseconds menu_initial_delay,
    const std::chrono::milliseconds menu_interval) noexcept
    : movement_initial_delay_(movement_initial_delay),
      menu_initial_delay_(menu_initial_delay),
      menu_interval_(menu_interval) {}

void KeyRepeatController::begin_frame() noexcept {
    movement_key_pressed_ = false;
    menu_key_pressed_ = false;
}

void KeyRepeatController::set_context(
    const DirectionRepeatContext context,
    const TimePoint now) noexcept {
    if (context_ == context) {
        return;
    }
    if (is_menu_context(context_) && is_menu_context(context)) {
        context_ = context;
        return;
    }
    if (is_movement_context(context_)) {
        if (context_ == DirectionRepeatContext::movement &&
            context == DirectionRepeatContext::none) {
            defer_movement_repeat(now);
        } else {
            block_movement_keys();
        }
    } else if (is_menu_context(context_)) {
        block_menu_keys();
    } else if (context_ == DirectionRepeatContext::none &&
               context != DirectionRepeatContext::movement &&
               held_movement_key_count_ != 0U) {
        block_movement_keys();
    }
    context_ = context;
    movement_key_pressed_ = false;
    menu_key_pressed_ = false;
}

bool KeyRepeatController::controls_key(const compat::HostKey key) const noexcept {
    if (is_movement_context(context_)) {
        return is_movement_direction_key(key);
    }
    if (is_menu_context(context_)) {
        return is_menu_repeat_key(key);
    }
    return false;
}

bool KeyRepeatController::handle_key_down(
    const compat::HostKey key,
    const bool host_repeat,
    const TimePoint now) noexcept {
    if (key_held(blocked_keys_, blocked_key_count_, key)) {
        return false;
    }
    if (host_repeat &&
        (key_held(held_movement_keys_, held_movement_key_count_, key) ||
         key_held(held_menu_keys_, held_menu_key_count_, key))) {
        return false;
    }
    if (host_repeat && context_ == DirectionRepeatContext::save_list &&
        is_save_list_single_press_key(key)) {
        return false;
    }
    if (!controls_key(key)) {
        return true;
    }
    if (host_repeat) {
        return false;
    }

    if (is_movement_context(context_)) {
        const bool starts_movement = held_movement_key_count_ == 0U;
        // A battle-grid chord is one immediate nudge; the established held
        // direction remains the repeat carrier until it is released.
        if (context_ == DirectionRepeatContext::battle_cursor &&
            !starts_movement) {
            remember_passive_key(
                held_movement_keys_, held_movement_key_count_, key);
        } else {
            remember_key(held_movement_keys_, held_movement_key_count_, key);
        }
        if (starts_movement) {
            movement_repeat_at_ = now + movement_initial_delay_;
        }
        movement_key_pressed_ = true;
    } else {
        remember_key(held_menu_keys_, held_menu_key_count_, key);
        menu_repeat_at_ = now + menu_initial_delay_;
        menu_key_pressed_ = true;
    }
    return true;
}

void KeyRepeatController::handle_key_up(
    const compat::HostKey key,
    const TimePoint now) noexcept {
    const bool movement_was_active =
        forget_key(held_movement_keys_, held_movement_key_count_, key);
    if (movement_was_active && held_movement_key_count_ != 0U &&
        is_movement_context(context_)) {
        movement_repeat_at_ = TimePoint::min();
    }

    const bool menu_was_active =
        forget_key(held_menu_keys_, held_menu_key_count_, key);
    if (menu_was_active && held_menu_key_count_ != 0U &&
        is_menu_context(context_)) {
        menu_repeat_at_ = now + menu_initial_delay_;
    }
    static_cast<void>(forget_key(blocked_keys_, blocked_key_count_, key));
}

std::optional<compat::HostKey> KeyRepeatController::take_movement_repeat(
    const TimePoint now) noexcept {
    if (!is_movement_context(context_) || movement_key_pressed_ ||
        now < movement_repeat_at_) {
        return std::nullopt;
    }
    return active_key(held_movement_keys_, held_movement_key_count_);
}

std::optional<compat::HostKey> KeyRepeatController::take_menu_repeat(
    const TimePoint now) noexcept {
    if (!is_menu_context(context_) || menu_key_pressed_ ||
        now < menu_repeat_at_) {
        return std::nullopt;
    }
    const auto key = active_key(held_menu_keys_, held_menu_key_count_);
    if (key.has_value()) {
        menu_repeat_at_ = now + menu_interval_;
    }
    return key;
}

std::optional<std::chrono::nanoseconds>
KeyRepeatController::time_until_menu_repeat(const TimePoint now) const noexcept {
    if (!is_menu_context(context_) || held_menu_key_count_ == 0U) {
        return std::nullopt;
    }
    if (menu_repeat_at_ <= now) {
        return std::chrono::nanoseconds::zero();
    }
    return std::chrono::ceil<std::chrono::nanoseconds>(menu_repeat_at_ - now);
}

void KeyRepeatController::defer_movement_repeat(const TimePoint now) noexcept {
    if (held_movement_key_count_ != 0U) {
        movement_repeat_at_ = now + movement_initial_delay_;
    }
}

void KeyRepeatController::block_movement_keys() noexcept {
    block_keys(
        held_movement_keys_,
        held_movement_key_count_,
        blocked_keys_,
        blocked_key_count_);
}

void KeyRepeatController::block_menu_keys() noexcept {
    block_keys(
        held_menu_keys_,
        held_menu_key_count_,
        blocked_keys_,
        blocked_key_count_);
}

}  // namespace openlegend::input
