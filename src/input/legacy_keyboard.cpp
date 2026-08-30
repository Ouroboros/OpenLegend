#include "openlegend/input/legacy_keyboard.hpp"

#include <algorithm>
#include <limits>

namespace openlegend::input {
namespace {

constexpr std::array<std::uint8_t, kLegacyTranslationSize> kTranslationTable{
    0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
    0x2D, 0x3D, 0x08, 0x09, 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49,
    0x4F, 0x50, 0x5B, 0x5D, 0x0D, 0x82, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48,
    0x4A, 0x4B, 0x4C, 0x3B, 0x27, 0x60, 0x83, 0x5C, 0x5A, 0x58, 0x43, 0x56,
    0x42, 0x4E, 0x4D, 0x2C, 0x2E, 0x2F, 0x84, 0x2A, 0x85, 0x20, 0x86, 0xC9,
    0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0x87, 0x88, 0x9D,
    0x9E, 0x9F, 0x2D, 0x9A, 0x9B, 0x9C, 0x2B, 0x97, 0x98, 0x99, 0x96, 0x89,
};

}  // namespace

LegacyKeyboard::LegacyKeyboard() {
    std::copy(kTranslationTable.begin(), kTranslationTable.end(), memory_.begin());
}

void LegacyKeyboard::handle_host_key(const compat::HostKey key, const bool pressed) noexcept {
    if (key == compat::HostKey::unknown) {
        return;
    }

    auto emit = [this, pressed](const std::uint8_t make_code) {
        handle_scan_code(pressed ? make_code : static_cast<std::uint8_t>(make_code | 0x80U));
    };
    switch (key) {
    case compat::HostKey::keypad_enter:
    case compat::HostKey::right_control:
    case compat::HostKey::keypad_divide:
    case compat::HostKey::right_alt:
    case compat::HostKey::home:
    case compat::HostKey::up:
    case compat::HostKey::page_up:
    case compat::HostKey::left:
    case compat::HostKey::right:
    case compat::HostKey::end:
    case compat::HostKey::down:
    case compat::HostKey::page_down:
    case compat::HostKey::insert:
    case compat::HostKey::delete_key:
    case compat::HostKey::left_gui:
    case compat::HostKey::right_gui:
    case compat::HostKey::application:
        break;
    case compat::HostKey::print_screen:
        if (pressed) {
            handle_scan_code(0xE0U);
            handle_scan_code(0x2AU);
            handle_scan_code(0xE0U);
            handle_scan_code(0x37U);
        } else {
            handle_scan_code(0xE0U);
            handle_scan_code(0xB7U);
            handle_scan_code(0xE0U);
            handle_scan_code(0xAAU);
        }
        return;
    case compat::HostKey::pause:
        if (pressed) {
            constexpr std::array<std::uint8_t, 6> sequence{
                0xE1U, 0x1DU, 0x45U, 0xE1U, 0x9DU, 0xC5U};
            for (const auto scan : sequence) {
                handle_scan_code(scan);
            }
        }
        return;
    default:
        emit(static_cast<std::uint8_t>(key));
        return;
    }

    std::uint8_t make_code{};
    switch (key) {
    case compat::HostKey::keypad_enter: make_code = 0x1CU; break;
    case compat::HostKey::right_control: make_code = 0x1DU; break;
    case compat::HostKey::keypad_divide: make_code = 0x35U; break;
    case compat::HostKey::right_alt: make_code = 0x38U; break;
    case compat::HostKey::home: make_code = 0x47U; break;
    case compat::HostKey::up: make_code = 0x48U; break;
    case compat::HostKey::page_up: make_code = 0x49U; break;
    case compat::HostKey::left: make_code = 0x4BU; break;
    case compat::HostKey::right: make_code = 0x4DU; break;
    case compat::HostKey::end: make_code = 0x4FU; break;
    case compat::HostKey::down: make_code = 0x50U; break;
    case compat::HostKey::page_down: make_code = 0x51U; break;
    case compat::HostKey::insert: make_code = 0x52U; break;
    case compat::HostKey::delete_key: make_code = 0x53U; break;
    case compat::HostKey::left_gui: make_code = 0x5BU; break;
    case compat::HostKey::right_gui: make_code = 0x5CU; break;
    case compat::HostKey::application: make_code = 0x5DU; break;
    default: return;
    }
    handle_scan_code(0xE0U);
    emit(make_code);
}

void LegacyKeyboard::handle_scan_code(const std::uint8_t raw_scan_code) noexcept {
    memory_[kLastRawOffset] = raw_scan_code;
    const auto translated_key = memory_[static_cast<std::size_t>(raw_scan_code & 0x7FU)];
    auto& translated_state = state_byte(translated_key);

    if (raw_scan_code < 0x80U) {
        if (translated_state == 0U) {
            memory_[kLastKeyOffset] = translated_key;
            ++translated_state;
        }
        const auto before_add = translated_state;
        translated_state = static_cast<std::uint8_t>(translated_state + 2U);
        if (before_add > std::numeric_limits<std::uint8_t>::max() - 2U) {
            --translated_state;
            --translated_state;
        }
        return;
    }

    translated_state = 0U;
    memory_[kLastKeyOffset] = 0U;
}

std::uint8_t LegacyKeyboard::last_raw_scan_code() const noexcept {
    return memory_[kLastRawOffset];
}

std::uint8_t LegacyKeyboard::last_key() const noexcept {
    return memory_[kLastKeyOffset];
}

void LegacyKeyboard::clear_last_key() noexcept {
    memory_[kLastKeyOffset] = 0U;
}

std::uint8_t LegacyKeyboard::state(const std::uint8_t translated_key) const noexcept {
    return state_byte(translated_key);
}

bool LegacyKeyboard::down(const std::uint8_t translated_key) const noexcept {
    return state(translated_key) != 0U;
}

bool LegacyKeyboard::edge(const std::uint8_t translated_key) const noexcept {
    return (state(translated_key) & 1U) != 0U;
}

void LegacyKeyboard::clear_state(const std::uint8_t translated_key) noexcept {
    state_byte(translated_key) = 0U;
}

void LegacyKeyboard::consume_edge(const std::uint8_t translated_key) noexcept {
    state_byte(translated_key) &= 0xFEU;
}

const std::array<std::uint8_t, kLegacyTranslationSize>&
LegacyKeyboard::translation_table() noexcept {
    return kTranslationTable;
}

std::uint8_t& LegacyKeyboard::state_byte(const std::uint8_t translated_key) noexcept {
    return memory_[kStateOffset + static_cast<std::size_t>(translated_key)];
}

const std::uint8_t& LegacyKeyboard::state_byte(const std::uint8_t translated_key) const noexcept {
    return memory_[kStateOffset + static_cast<std::size_t>(translated_key)];
}

}  // namespace openlegend::input
