#include "openlegend/render/rgba_framebuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace openlegend::render {
namespace {

[[nodiscard]] bool valid_rectangle(
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height) noexcept {
    return width > 0U && height > 0U && x >= 0 && y >= 0 &&
        x <= RgbaFramebuffer::width - static_cast<int>(width) &&
        y <= RgbaFramebuffer::height - static_cast<int>(height);
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

RgbaFramebuffer::RgbaFramebuffer()
    : pixels_(compat::kModernRgbaByteCount, 0U) {}

bool RgbaFramebuffer::set_scale(const int scale) {
    if (scale <= 0 || scale > maximum_scale ||
        scale > std::numeric_limits<int>::max() / width ||
        scale > std::numeric_limits<int>::max() / height) {
        return false;
    }
    if (scale == scale_) {
        return true;
    }

    const auto scaled_width = static_cast<std::size_t>(width) *
        static_cast<std::size_t>(scale);
    const auto scaled_height = static_cast<std::size_t>(height) *
        static_cast<std::size_t>(scale);
    if (scaled_height > std::numeric_limits<std::size_t>::max() / scaled_width ||
        scaled_width * scaled_height >
            std::numeric_limits<std::size_t>::max() /
                compat::kModernRgbaBytesPerPixel) {
        return false;
    }
    const auto byte_count = scaled_width * scaled_height *
        compat::kModernRgbaBytesPerPixel;
    if (byte_count > pixels_.max_size()) {
        return false;
    }

    try {
        std::vector<std::uint8_t> replacement(byte_count, 0U);
        pixels_.swap(replacement);
    } catch (const std::bad_alloc&) {
        return false;
    }
    scale_ = scale;
    return true;
}

void RgbaFramebuffer::clear(const compat::Rgba8 color) noexcept {
    const auto pixel_count = pixels_.size() / compat::kModernRgbaBytesPerPixel;
    for (std::size_t pixel = 0U; pixel < pixel_count; ++pixel) {
        write_color(
            pixels_.data() + pixel * compat::kModernRgbaBytesPerPixel, color);
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

    const auto physical_x = x * scale_;
    const auto physical_y = y * scale_;
    const auto physical_width = static_cast<int>(rectangle_width) * scale_;
    const auto physical_height = static_cast<int>(rectangle_height) * scale_;
    for (int destination_y = physical_y;
         destination_y < physical_y + physical_height;
         ++destination_y) {
        auto* destination = row(destination_y) +
            static_cast<std::size_t>(physical_x) *
                compat::kModernRgbaBytesPerPixel;
        for (int destination_x = 0;
             destination_x < physical_width;
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

    const auto physical_x = x * scale_;
    const auto physical_y = y * scale_;
    const auto physical_width = static_cast<int>(rectangle_width) * scale_;
    const auto physical_height = static_cast<int>(rectangle_height) * scale_;
    for (int destination_y = physical_y;
         destination_y < physical_y + physical_height;
         ++destination_y) {
        auto* destination = row(destination_y) +
            static_cast<std::size_t>(physical_x) *
                compat::kModernRgbaBytesPerPixel;
        for (int destination_x = 0;
             destination_x < physical_width;
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

    const auto physical_x = x * scale_;
    const auto physical_y = y * scale_;
    for (int offset_y = 0; offset_y < scale_; ++offset_y) {
        auto* destination = row(physical_y + offset_y) +
            static_cast<std::size_t>(physical_x) *
                compat::kModernRgbaBytesPerPixel;
        for (int offset_x = 0; offset_x < scale_; ++offset_x) {
            blend_color(destination, color);
            destination += compat::kModernRgbaBytesPerPixel;
        }
    }
    return true;
}

bool RgbaFramebuffer::blend_physical_pixel(
    const int x,
    const int y,
    const compat::Rgba8 color) noexcept {
    if (x < 0 || y < 0 || x >= pixel_width() || y >= pixel_height()) {
        return false;
    }
    auto* destination = row(y) +
        static_cast<std::size_t>(x) * compat::kModernRgbaBytesPerPixel;
    blend_color(destination, color);
    return true;
}

std::uint8_t* RgbaFramebuffer::row(const int y) noexcept {
    return pixels_.data() +
        static_cast<std::size_t>(y) * static_cast<std::size_t>(pixel_width()) *
            compat::kModernRgbaBytesPerPixel;
}

const std::uint8_t* RgbaFramebuffer::row(const int y) const noexcept {
    return pixels_.data() +
        static_cast<std::size_t>(y) * static_cast<std::size_t>(pixel_width()) *
            compat::kModernRgbaBytesPerPixel;
}

}  // namespace openlegend::render
