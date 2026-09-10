#include "openlegend/render/rgba_framebuffer.hpp"

#include <cstddef>

namespace openlegend::render {
namespace {

[[nodiscard]] constexpr bool valid_rectangle(
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height) noexcept {
    return width > 0U && height > 0U && x >= 0 && y >= 0 &&
        x + static_cast<int>(width) <= RgbaFramebuffer::width &&
        y + static_cast<int>(height) <= RgbaFramebuffer::height;
}

void write_color(std::uint8_t* destination, const compat::Rgba8 color) noexcept {
    destination[0] = color.red;
    destination[1] = color.green;
    destination[2] = color.blue;
    destination[3] = color.alpha;
}

void blend_color(std::uint8_t* destination, const compat::Rgba8 color) noexcept {
    if (color.alpha == 0U) {
        return;
    }
    if (color.alpha == compat::kOpaqueAlpha) {
        write_color(destination, color);
        return;
    }

    const auto alpha = static_cast<std::uint32_t>(color.alpha);
    const auto inverse_alpha = 255U - alpha;
    destination[0] = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(color.red) * alpha +
         static_cast<std::uint32_t>(destination[0]) * inverse_alpha + 127U) /
        255U);
    destination[1] = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(color.green) * alpha +
         static_cast<std::uint32_t>(destination[1]) * inverse_alpha + 127U) /
        255U);
    destination[2] = static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(color.blue) * alpha +
         static_cast<std::uint32_t>(destination[2]) * inverse_alpha + 127U) /
        255U);
    destination[3] = static_cast<std::uint8_t>(
        alpha +
        (static_cast<std::uint32_t>(destination[3]) * inverse_alpha + 127U) /
            255U);
}

}  // namespace

void RgbaFramebuffer::clear(const compat::Rgba8 color) noexcept {
    for (std::size_t pixel = 0U; pixel < compat::kLegacyPixelCount; ++pixel) {
        write_color(pixels_.data() + pixel * compat::kModernRgbaBytesPerPixel, color);
    }
}

bool RgbaFramebuffer::fill_rectangle(
    const int x,
    const int y,
    const std::uint16_t rectangle_width,
    const std::uint16_t rectangle_height,
    const compat::Rgba8 color) noexcept {
    if (!valid_rectangle(x, y, rectangle_width, rectangle_height)) {
        return false;
    }
    for (int destination_y = y;
         destination_y < y + static_cast<int>(rectangle_height);
         ++destination_y) {
        auto* destination = row(destination_y) +
            static_cast<std::size_t>(x) * compat::kModernRgbaBytesPerPixel;
        for (int destination_x = 0;
             destination_x < static_cast<int>(rectangle_width);
             ++destination_x) {
            write_color(destination, color);
            destination += compat::kModernRgbaBytesPerPixel;
        }
    }
    return true;
}

bool RgbaFramebuffer::blend_rectangle(
    const int x,
    const int y,
    const std::uint16_t rectangle_width,
    const std::uint16_t rectangle_height,
    const compat::Rgba8 color) noexcept {
    if (!valid_rectangle(x, y, rectangle_width, rectangle_height)) {
        return false;
    }
    for (int destination_y = y;
         destination_y < y + static_cast<int>(rectangle_height);
         ++destination_y) {
        auto* destination = row(destination_y) +
            static_cast<std::size_t>(x) * compat::kModernRgbaBytesPerPixel;
        for (int destination_x = 0;
             destination_x < static_cast<int>(rectangle_width);
             ++destination_x) {
            blend_color(destination, color);
            destination += compat::kModernRgbaBytesPerPixel;
        }
    }
    return true;
}

bool RgbaFramebuffer::outline_rectangle(
    const int x,
    const int y,
    const std::uint16_t rectangle_width,
    const std::uint16_t rectangle_height,
    const compat::Rgba8 color) noexcept {
    if (!valid_rectangle(x, y, rectangle_width, rectangle_height)) {
        return false;
    }
    return fill_rectangle(x, y, rectangle_width, 1U, color) &&
        fill_rectangle(
            x,
            y + static_cast<int>(rectangle_height) - 1,
            rectangle_width,
            1U,
            color) &&
        fill_rectangle(x, y, 1U, rectangle_height, color) &&
        fill_rectangle(
            x + static_cast<int>(rectangle_width) - 1,
            y,
            1U,
            rectangle_height,
            color);
}

bool RgbaFramebuffer::blend_pixel(
    const int x,
    const int y,
    const compat::Rgba8 color) noexcept {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    auto* destination = row(y) +
        static_cast<std::size_t>(x) * compat::kModernRgbaBytesPerPixel;
    blend_color(destination, color);
    return true;
}

std::uint8_t* RgbaFramebuffer::row(const int y) noexcept {
    return pixels_.data() +
        static_cast<std::size_t>(y) * compat::kLegacyWidth *
            compat::kModernRgbaBytesPerPixel;
}

const std::uint8_t* RgbaFramebuffer::row(const int y) const noexcept {
    return pixels_.data() +
        static_cast<std::size_t>(y) * compat::kLegacyWidth *
            compat::kModernRgbaBytesPerPixel;
}

}  // namespace openlegend::render
