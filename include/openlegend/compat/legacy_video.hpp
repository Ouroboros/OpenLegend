#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

#include "openlegend/compat/color.hpp"

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
    int width{static_cast<int>(kLegacyWidth)};
    int height{static_cast<int>(kLegacyHeight)};

    [[nodiscard]] constexpr bool valid() const noexcept {
        if (width <= 0 || height <= 0 || palette.size() != kLegacyPaletteSize) {
            return false;
        }
        const auto unsigned_width = static_cast<std::size_t>(width);
        const auto unsigned_height = static_cast<std::size_t>(height);
        if (unsigned_height >
            std::numeric_limits<std::size_t>::max() / unsigned_width ||
            pixels.size() != unsigned_width * unsigned_height) {
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

struct RgbaFrameView {
    std::span<const std::uint8_t> pixels;
    int width{static_cast<int>(kLegacyWidth)};
    int height{static_cast<int>(kLegacyHeight)};

    [[nodiscard]] constexpr bool valid() const noexcept {
        if (width <= 0 || height <= 0) {
            return false;
        }
        const auto unsigned_width = static_cast<std::size_t>(width);
        const auto unsigned_height = static_cast<std::size_t>(height);
        if (unsigned_width > std::numeric_limits<std::size_t>::max() /
                kModernRgbaBytesPerPixel) {
            return false;
        }
        const auto row_bytes = unsigned_width * kModernRgbaBytesPerPixel;
        return unsigned_height <=
                std::numeric_limits<std::size_t>::max() / row_bytes &&
            pixels.size() == row_bytes * unsigned_height;
    }
};

[[nodiscard]] constexpr std::uint8_t expand_rgb6(const std::uint8_t value) noexcept {
    const auto six_bit = static_cast<std::uint8_t>(value & 0x3FU);
    return static_cast<std::uint8_t>((six_bit << 2U) | (six_bit >> 4U));
}

struct ProportionalViewport {
    float x{};
    float y{};
    float width{};
    float height{};
    float scale{};
    int presentation_scale{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return width > 0.0F && height > 0.0F && scale > 0.0F &&
            presentation_scale > 0;
    }
};

[[nodiscard]] constexpr ProportionalViewport proportional_viewport(
    const int output_width,
    const int output_height,
    const int source_width,
    const int source_height) noexcept {
    if (source_width <= 0 || source_height <= 0 ||
        output_width <= 0 || output_height <= 0) {
        return {};
    }
    const auto scale_x = static_cast<float>(output_width) /
        static_cast<float>(source_width);
    const auto scale_y = static_cast<float>(output_height) /
        static_cast<float>(source_height);
    const auto scale = scale_x < scale_y ? scale_x : scale_y;
    const auto width = static_cast<float>(source_width) * scale;
    const auto height = static_cast<float>(source_height) * scale;
    const auto rounded_scale = static_cast<int>(scale + 0.5F);
    const auto presentation_scale = rounded_scale > 0 ? rounded_scale : 1;
    return {
        (static_cast<float>(output_width) - width) / 2.0F,
        (static_cast<float>(output_height) - height) / 2.0F,
        width,
        height,
        scale,
        presentation_scale};
}

[[nodiscard]] constexpr ProportionalViewport proportional_viewport(
    const int output_width, const int output_height) noexcept {
    return proportional_viewport(
        output_width,
        output_height,
        static_cast<int>(kLegacyWidth),
        static_cast<int>(kLegacyHeight));
}

using LegacyPixels = std::array<std::uint8_t, kLegacyPixelCount>;
using LegacyPalette = std::array<Rgb6, kLegacyPaletteSize>;
using ModernRgbaPixels = std::array<std::uint8_t, kModernRgbaByteCount>;

[[nodiscard]] inline bool convert_indexed_frame_to_rgba(
    const IndexedFrameView frame, std::span<std::uint8_t> rgba) noexcept {
    if (!frame.valid() ||
        frame.pixels.size() >
            std::numeric_limits<std::size_t>::max() / kModernRgbaBytesPerPixel ||
        rgba.size() != frame.pixels.size() * kModernRgbaBytesPerPixel) {
        return false;
    }
    std::array<std::uint32_t, kLegacyPaletteSize> expanded_palette{};
    for (std::size_t index = 0U; index < frame.palette.size(); ++index) {
        const auto color = frame.palette[index];
        const auto red = static_cast<std::uint32_t>(expand_rgb6(color.red));
        const auto green = static_cast<std::uint32_t>(expand_rgb6(color.green));
        const auto blue = static_cast<std::uint32_t>(expand_rgb6(color.blue));
        if constexpr (std::endian::native == std::endian::little) {
            expanded_palette[index] = red | (green << 8U) | (blue << 16U) |
                (static_cast<std::uint32_t>(kOpaqueAlpha) << 24U);
        } else {
            expanded_palette[index] = (red << 24U) | (green << 16U) |
                (blue << 8U) | static_cast<std::uint32_t>(kOpaqueAlpha);
        }
    }
    for (std::size_t index = 0U; index < frame.pixels.size(); ++index) {
        const auto color = expanded_palette[frame.pixels[index]];
        std::memcpy(
            rgba.data() + index * kModernRgbaBytesPerPixel,
            &color,
            sizeof(color));
    }
    return true;
}

}  // namespace openlegend::compat
