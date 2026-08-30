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
inline constexpr std::size_t kModernRgbaBytesPerPixel = 4;
inline constexpr std::size_t kModernRgbaByteCount =
    kLegacyPixelCount * kModernRgbaBytesPerPixel;

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

struct IntegerViewport {
    int x{};
    int y{};
    int width{};
    int height{};
    int scale{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return width > 0 && height > 0 && scale > 0;
    }
};

[[nodiscard]] constexpr IntegerViewport integer_viewport(
    const int output_width, const int output_height) noexcept {
    if (output_width < static_cast<int>(kLegacyWidth) ||
        output_height < static_cast<int>(kLegacyHeight)) {
        return {};
    }
    const auto scale_x = output_width / static_cast<int>(kLegacyWidth);
    const auto scale_y = output_height / static_cast<int>(kLegacyHeight);
    const auto scale = scale_x < scale_y ? scale_x : scale_y;
    const auto width = static_cast<int>(kLegacyWidth) * scale;
    const auto height = static_cast<int>(kLegacyHeight) * scale;
    return {(output_width - width) / 2, (output_height - height) / 2, width, height, scale};
}

using LegacyPixels = std::array<std::uint8_t, kLegacyPixelCount>;
using LegacyPalette = std::array<Rgb6, kLegacyPaletteSize>;
using ModernRgbaPixels = std::array<std::uint8_t, kModernRgbaByteCount>;

[[nodiscard]] inline bool convert_indexed_frame_to_rgba(
    const IndexedFrameView frame, std::span<std::uint8_t> rgba) noexcept {
    if (!frame.valid() || rgba.size() != kModernRgbaByteCount) {
        return false;
    }
    for (std::size_t index = 0U; index < frame.pixels.size(); ++index) {
        const auto color = frame.palette[frame.pixels[index]];
        const auto target = index * kModernRgbaBytesPerPixel;
        rgba[target] = expand_rgb6(color.red);
        rgba[target + 1U] = expand_rgb6(color.green);
        rgba[target + 2U] = expand_rgb6(color.blue);
        rgba[target + 3U] = 0xFFU;
    }
    return true;
}

}  // namespace openlegend::compat
