#pragma once

#include <chrono>
#include <cstdint>

#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::compat {

enum class HostEventType {
    none,
    quit,
    key_down,
    key_up,
};

enum class HostKey : std::uint8_t {
    unknown = 0x00,
    escape = 0x01,
    digit_1 = 0x02,
    digit_2 = 0x03,
    digit_3 = 0x04,
    digit_4 = 0x05,
    digit_5 = 0x06,
    digit_6 = 0x07,
    digit_7 = 0x08,
    digit_8 = 0x09,
    digit_9 = 0x0A,
    digit_0 = 0x0B,
    minus = 0x0C,
    equals = 0x0D,
    backspace = 0x0E,
    tab = 0x0F,
    q = 0x10,
    w = 0x11,
    e = 0x12,
    r = 0x13,
    t = 0x14,
    y = 0x15,
    u = 0x16,
    i = 0x17,
    o = 0x18,
    p = 0x19,
    left_bracket = 0x1A,
    right_bracket = 0x1B,
    enter = 0x1C,
    left_control = 0x1D,
    a = 0x1E,
    s = 0x1F,
    d = 0x20,
    f = 0x21,
    g = 0x22,
    h = 0x23,
    j = 0x24,
    k = 0x25,
    l = 0x26,
    semicolon = 0x27,
    apostrophe = 0x28,
    grave = 0x29,
    left_shift = 0x2A,
    backslash = 0x2B,
    z = 0x2C,
    x = 0x2D,
    c = 0x2E,
    v = 0x2F,
    b = 0x30,
    n = 0x31,
    m = 0x32,
    comma = 0x33,
    period = 0x34,
    slash = 0x35,
    right_shift = 0x36,
    keypad_multiply = 0x37,
    left_alt = 0x38,
    space = 0x39,
    caps_lock = 0x3A,
    f1 = 0x3B,
    f2 = 0x3C,
    f3 = 0x3D,
    f4 = 0x3E,
    f5 = 0x3F,
    f6 = 0x40,
    f7 = 0x41,
    f8 = 0x42,
    f9 = 0x43,
    f10 = 0x44,
    num_lock = 0x45,
    scroll_lock = 0x46,
    keypad_7 = 0x47,
    keypad_8 = 0x48,
    keypad_9 = 0x49,
    keypad_minus = 0x4A,
    keypad_4 = 0x4B,
    keypad_5 = 0x4C,
    keypad_6 = 0x4D,
    keypad_plus = 0x4E,
    keypad_1 = 0x4F,
    keypad_2 = 0x50,
    keypad_3 = 0x51,
    keypad_0 = 0x52,
    keypad_decimal = 0x53,
    sysrq = 0x54,
    non_us_backslash = 0x56,
    f11 = 0x57,
    f12 = 0x58,
    keypad_equals = 0x59,
    keypad_enter = 0xE0,
    right_control = 0xE1,
    keypad_divide = 0xE2,
    right_alt = 0xE3,
    home = 0xE4,
    up = 0xE5,
    page_up = 0xE6,
    left = 0xE7,
    right = 0xE8,
    end = 0xE9,
    down = 0xEA,
    page_down = 0xEB,
    insert = 0xEC,
    delete_key = 0xED,
    left_gui = 0xEE,
    right_gui = 0xEF,
    application = 0xF0,
    print_screen = 0xF1,
    pause = 0xF2,
};

struct HostEvent {
    HostEventType type{HostEventType::none};
    HostKey key{HostKey::unknown};
    bool repeat{};
};

class RuntimePlatform {
public:
    virtual ~RuntimePlatform() = default;

    [[nodiscard]] virtual bool poll_event(HostEvent& event) = 0;
    [[nodiscard]] virtual bool present(IndexedFrameView frame) = 0;
    virtual void delay(std::chrono::milliseconds duration) = 0;
};

}  // namespace openlegend::compat
