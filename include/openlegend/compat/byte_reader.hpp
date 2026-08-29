#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace openlegend::compat {

[[nodiscard]] constexpr std::uint16_t read_u16le(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] constexpr std::int16_t read_i16le(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset) noexcept {
    return static_cast<std::int16_t>(read_u16le(bytes, offset));
}

[[nodiscard]] constexpr std::uint32_t read_u32le(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

}  // namespace openlegend::compat
