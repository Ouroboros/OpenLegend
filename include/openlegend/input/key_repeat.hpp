#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "openlegend/compat/runtime_platform.hpp"

namespace openlegend::input {

enum class DirectionRepeatContext : std::uint8_t {
    none,
    movement,
    battle_cursor,
    menu,
    save_list,
};

class KeyRepeatController {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    KeyRepeatController(
        std::chrono::milliseconds movement_initial_delay,
        std::chrono::milliseconds menu_initial_delay,
        std::chrono::milliseconds menu_interval) noexcept;

    void begin_frame() noexcept;
    void set_context(DirectionRepeatContext context, TimePoint now) noexcept;

    [[nodiscard]] bool handle_key_down(
        compat::HostKey key,
        bool host_repeat,
        TimePoint now) noexcept;
    void handle_key_up(compat::HostKey key, TimePoint now) noexcept;

    [[nodiscard]] std::optional<compat::HostKey> take_movement_repeat(
        TimePoint now) noexcept;
    [[nodiscard]] std::optional<compat::HostKey> take_menu_repeat(
        TimePoint now) noexcept;
    [[nodiscard]] std::optional<std::chrono::nanoseconds> time_until_menu_repeat(
        TimePoint now) const noexcept;

    void defer_movement_repeat(TimePoint now) noexcept;

private:
    static constexpr std::size_t kHeldKeyCapacity = 16U;
    static constexpr std::size_t kBlockedKeyCapacity = 32U;

    [[nodiscard]] bool controls_key(compat::HostKey key) const noexcept;
    void block_movement_keys() noexcept;
    void block_menu_keys() noexcept;

    std::chrono::milliseconds movement_initial_delay_{};
    std::chrono::milliseconds menu_initial_delay_{};
    std::chrono::milliseconds menu_interval_{};
    DirectionRepeatContext context_{DirectionRepeatContext::none};
    std::array<compat::HostKey, kHeldKeyCapacity> held_movement_keys_{};
    std::size_t held_movement_key_count_{};
    std::array<compat::HostKey, kHeldKeyCapacity> held_menu_keys_{};
    std::size_t held_menu_key_count_{};
    std::array<compat::HostKey, kBlockedKeyCapacity> blocked_keys_{};
    std::size_t blocked_key_count_{};
    TimePoint movement_repeat_at_{};
    TimePoint menu_repeat_at_{};
    bool movement_key_pressed_{};
    bool menu_key_pressed_{};
};

}  // namespace openlegend::input
