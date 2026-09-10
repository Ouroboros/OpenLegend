#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <optional>

#include "openlegend/compat/runtime_platform.hpp"

namespace openlegend::input {

class KeyRepeatController {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    KeyRepeatController(
        std::chrono::milliseconds initial_delay,
        std::chrono::milliseconds save_list_page_interval) noexcept;

    void begin_frame() noexcept;

    [[nodiscard]] bool handle_key_down(
        compat::HostKey key,
        bool host_repeat,
        bool movement_repeat_active,
        bool save_list_page_repeat_active,
        TimePoint now) noexcept;
    void handle_key_up(compat::HostKey key) noexcept;

    [[nodiscard]] std::optional<compat::HostKey> take_movement_repeat(
        bool active,
        TimePoint now) noexcept;
    [[nodiscard]] std::optional<compat::HostKey> take_save_list_page_repeat(
        bool active,
        TimePoint now) noexcept;

    void defer_movement_repeat(TimePoint now) noexcept;

private:
    [[nodiscard]] std::optional<compat::HostKey> active_movement_direction() const noexcept;
    [[nodiscard]] bool movement_key_held(compat::HostKey key) const noexcept;
    void remember_movement_key(compat::HostKey key) noexcept;
    [[nodiscard]] bool forget_movement_key(compat::HostKey key) noexcept;

    std::chrono::milliseconds initial_delay_{};
    std::chrono::milliseconds save_list_page_interval_{};
    std::array<compat::HostKey, 8> held_movement_keys_{};
    std::size_t held_movement_key_count_{};
    std::optional<compat::HostKey> held_save_list_page_key_;
    TimePoint movement_repeat_at_{};
    TimePoint save_list_page_repeat_at_{};
    bool movement_direction_pressed_{};
    bool movement_repeat_was_active_{};
    bool save_list_page_key_pressed_{};
};

}  // namespace openlegend::input
