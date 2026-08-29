#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace openlegend::compat {

inline constexpr std::size_t kLegacyWidth = 320;
inline constexpr std::size_t kLegacyHeight = 200;
inline constexpr std::size_t kLegacyPixelCount = kLegacyWidth * kLegacyHeight;
inline constexpr std::size_t kLegacyPaletteSize = 256;

struct Rgb6 {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return red <= 63U && green <= 63U && blue <= 63U;
    }
};

struct IndexedFrameView {
    std::span<const std::uint8_t> pixels;
    std::span<const Rgb6> palette;

    [[nodiscard]] constexpr bool valid() const noexcept {
        if (pixels.size() != kLegacyPixelCount || palette.size() != kLegacyPaletteSize) {
            return false;
        }
        for (const auto color : palette) {
            if (!color.valid()) {
                return false;
            }
        }
        return true;
    }
};

[[nodiscard]] constexpr std::uint8_t expand_rgb6(const std::uint8_t value) noexcept {
    const auto six_bit = static_cast<std::uint8_t>(value & 0x3FU);
    return static_cast<std::uint8_t>((six_bit << 2U) | (six_bit >> 4U));
}

using LegacyPixels = std::array<std::uint8_t, kLegacyPixelCount>;
using LegacyPalette = std::array<Rgb6, kLegacyPaletteSize>;

}  // namespace openlegend::compat
