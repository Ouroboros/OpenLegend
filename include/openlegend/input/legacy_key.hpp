#pragma once

#include <array>
#include <cstdint>

namespace openlegend::input::legacy_key {

inline constexpr std::uint8_t backspace = 0x08U;
inline constexpr std::uint8_t enter = 0x0DU;
inline constexpr std::uint8_t escape = 0x1BU;
inline constexpr std::uint8_t space = 0x20U;
inline constexpr std::uint8_t comma = static_cast<std::uint8_t>(',');
inline constexpr std::uint8_t period = static_cast<std::uint8_t>('.');
inline constexpr std::uint8_t delete_save = static_cast<std::uint8_t>('D');
inline constexpr std::uint8_t weather_toggle = static_cast<std::uint8_t>('L');
inline constexpr std::uint8_t no = static_cast<std::uint8_t>('N');
inline constexpr std::uint8_t yes = static_cast<std::uint8_t>('Y');
inline constexpr std::uint8_t left_control = 0x82U;
inline constexpr std::uint8_t left_shift = 0x83U;
inline constexpr std::uint8_t right_shift = 0x84U;
inline constexpr std::uint8_t keypad_insert = 0x96U;
inline constexpr std::uint8_t end = 0x97U;
inline constexpr std::uint8_t down = 0x98U;
inline constexpr std::uint8_t page_down = 0x99U;
inline constexpr std::uint8_t left = 0x9AU;
inline constexpr std::uint8_t right = 0x9CU;
inline constexpr std::uint8_t home = 0x9DU;
inline constexpr std::uint8_t up = 0x9EU;
inline constexpr std::uint8_t page_up = 0x9FU;

inline constexpr std::array<std::uint8_t, 3> confirmation{
    enter,
    space,
    keypad_insert,
};
inline constexpr std::array<std::uint8_t, 2> world_left{
    left,
    home,
};
inline constexpr std::array<std::uint8_t, 2> world_up{
    up,
    page_up,
};
inline constexpr std::array<std::uint8_t, 2> world_down{
    end,
    down,
};
inline constexpr std::array<std::uint8_t, 2> world_right{
    page_down,
    right,
};

inline constexpr std::uint8_t kEnter = enter;
inline constexpr std::uint8_t kEscape = escape;
inline constexpr std::uint8_t kSpace = space;
inline constexpr std::uint8_t kKeypadInsert = keypad_insert;
inline constexpr std::uint8_t kDown = down;
inline constexpr std::uint8_t kPageDown = page_down;
inline constexpr std::uint8_t kLeft = left;
inline constexpr std::uint8_t kRight = right;
inline constexpr std::uint8_t kUp = up;
inline constexpr std::uint8_t kPageUp = page_up;

}  // namespace openlegend::input::legacy_key
