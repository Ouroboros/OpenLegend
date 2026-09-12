#include "openlegend/render/rgba_framebuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace openlegend::render {
namespace {

[[nodiscard]] bool valid_rectangle(
    const RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height) noexcept {
    return width > 0U && height > 0U && x >= 0 && y >= 0 &&
        x <= framebuffer.logical_width() - static_cast<int>(width) &&
        y <= framebuffer.logical_height() - static_cast<int>(height);
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
    : RgbaFramebuffer(width, height) {}

RgbaFramebuffer::RgbaFramebuffer(
    const int requested_width,
    const int requested_height) {
    if (!set_dimensions(requested_width, requested_height, 1)) {
        throw std::bad_alloc{};
    }
}

bool RgbaFramebuffer::set_dimensions(
    const int requested_width,
    const int requested_height,
    const int requested_scale) {
    if (requested_width <= 0 || requested_height <= 0 ||
        requested_scale <= 0 || requested_scale > maximum_scale ||
        requested_width > std::numeric_limits<int>::max() / requested_scale ||
        requested_height > std::numeric_limits<int>::max() / requested_scale) {
        return false;
    }
    if (requested_width == logical_width_ &&
        requested_height == logical_height_ && requested_scale == scale_ &&
        !pixels_.empty()) {
        return true;
    }

    const auto scaled_width = static_cast<std::size_t>(requested_width) *
        static_cast<std::size_t>(requested_scale);
    const auto scaled_height = static_cast<std::size_t>(requested_height) *
        static_cast<std::size_t>(requested_scale);
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
    logical_width_ = requested_width;
    logical_height_ = requested_height;
    scale_ = requested_scale;
    return true;
}

bool RgbaFramebuffer::set_scale(const int requested_scale) {
    return set_dimensions(logical_width_, logical_height_, requested_scale);
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
    if (!valid_rectangle(*this, x, y, rectangle_width, rectangle_height)) {
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
    if (!valid_rectangle(*this, x, y, rectangle_width, rectangle_height)) {
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
    if (!valid_rectangle(*this, x, y, rectangle_width, rectangle_height)) {
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
    if (x < 0 || y < 0 || x >= logical_width_ || y >= logical_height_) {
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
