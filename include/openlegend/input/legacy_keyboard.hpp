#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "openlegend/compat/runtime_platform.hpp"
#include "openlegend/input/legacy_key.hpp"

namespace openlegend::input {

inline constexpr std::size_t kLegacyTranslationSize = 84;
inline constexpr std::size_t kLegacyKeyStateCount = 256;

enum class LegacyWorldDirectionInput : std::uint8_t {
    none,
    left,
    up,
    down,
    right,
};

class LegacyKeyboard {
public:
    LegacyKeyboard();

    void handle_host_key(compat::HostKey key, bool pressed) noexcept;
    void handle_scan_code(std::uint8_t raw_scan_code) noexcept;

    [[nodiscard]] std::uint8_t last_raw_scan_code() const noexcept;
    [[nodiscard]] std::uint8_t last_key() const noexcept;
    void clear_last_key() noexcept;

    [[nodiscard]] std::uint8_t state(std::uint8_t translated_key) const noexcept;
    [[nodiscard]] bool down(std::uint8_t translated_key) const noexcept;
    [[nodiscard]] bool edge(std::uint8_t translated_key) const noexcept;
    void clear_state(std::uint8_t translated_key) noexcept;
    void consume_edge(std::uint8_t translated_key) noexcept;
    void clear_confirmation_states() noexcept;
    [[nodiscard]] LegacyWorldDirectionInput world_direction() const noexcept;
    void consume_world_direction(LegacyWorldDirectionInput direction) noexcept;
    void clear_world_direction_states() noexcept;
    void clear_scene_exit_key_states() noexcept;

    [[nodiscard]] static const std::array<std::uint8_t, kLegacyTranslationSize>&
    translation_table() noexcept;

private:
    static constexpr std::size_t kLastRawOffset = kLegacyTranslationSize;
    static constexpr std::size_t kLastKeyOffset = kLastRawOffset + 1U;
    static constexpr std::size_t kUnusedOffset = kLastKeyOffset + 1U;
    static constexpr std::size_t kStateOffset = kUnusedOffset + 1U;
    static constexpr std::size_t kMemorySize = kStateOffset + kLegacyKeyStateCount;

    [[nodiscard]] std::uint8_t& state_byte(std::uint8_t translated_key) noexcept;
    [[nodiscard]] const std::uint8_t& state_byte(std::uint8_t translated_key) const noexcept;

    std::array<std::uint8_t, kMemorySize> memory_{};
};

}  // namespace openlegend::input
