#include "openlegend/attributes.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

namespace openlegend::render {
namespace {

NODISCARD bool valid_dimensions(
    const int width, const int height) noexcept {
    if (width <= 0 || height <= 0) {
        return false;
    }
    const auto unsigned_width = static_cast<std::size_t>(width);
    const auto unsigned_height = static_cast<std::size_t>(height);
    return unsigned_height <=
        std::numeric_limits<std::size_t>::max() / unsigned_width;
}

NODISCARD int mapped_edge(
    const int coordinate,
    const int source_extent,
    const int destination_begin,
    const int destination_extent) noexcept {
    return destination_begin + static_cast<int>(
        static_cast<std::int64_t>(coordinate) * destination_extent /
        source_extent);
}

}  // namespace

IndexedFramebuffer::IndexedFramebuffer(
    const int requested_width,
    const int requested_height) {
    if (!resize(requested_width, requested_height)) {
        throw std::bad_alloc{};
    }
}

bool IndexedFramebuffer::resize(
    const int requested_width,
    const int requested_height) {
    if (!valid_dimensions(requested_width, requested_height)) {
        return false;
    }
    const auto byte_count = static_cast<std::size_t>(requested_width) *
        static_cast<std::size_t>(requested_height);
    if (byte_count > pixels_.max_size()) {
        return false;
    }
    try {
        std::vector<std::uint8_t> replacement(byte_count, 0U);
        pixels_.swap(replacement);
    } catch (const std::bad_alloc&) {
        return false;
    }
    pixel_width_ = requested_width;
    pixel_height_ = requested_height;
    coordinate_space_ = native_coordinate_space();
    return true;
}

IndexedViewport IndexedFramebuffer::legacy_ui_viewport() const noexcept {
    const auto width_limited =
        static_cast<std::int64_t>(pixel_width_) * height <=
        static_cast<std::int64_t>(pixel_height_) * width;
    int viewport_width = 0;
    int viewport_height = 0;
    if (width_limited) {
        viewport_width = pixel_width_;
        viewport_height = static_cast<int>(
            static_cast<std::int64_t>(pixel_width_) * height / width);
    } else {
        viewport_height = pixel_height_;
        viewport_width = static_cast<int>(
            static_cast<std::int64_t>(pixel_height_) * width / height);
    }
    return {
        (pixel_width_ - viewport_width) / 2,
        (pixel_height_ - viewport_height) / 2,
        viewport_width,
        viewport_height};
}

IndexedFramebuffer::CoordinateSpaceGuard
IndexedFramebuffer::use_native_coordinates() noexcept {
    return CoordinateSpaceGuard{*this, native_coordinate_space()};
}

IndexedFramebuffer::CoordinateSpaceGuard
IndexedFramebuffer::use_legacy_ui_coordinates() noexcept {
    return CoordinateSpaceGuard{*this, legacy_ui_coordinate_space()};
}

void IndexedFramebuffer::clear(const std::uint8_t color) noexcept {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

bool IndexedFramebuffer::fill_rectangle(
    const int x,
    const int y,
    const std::uint16_t rectangle_width,
    const std::uint16_t rectangle_height,
    const std::uint8_t color) noexcept {
    if (x < 0 || y < 0 || x > coordinate_width() || y > coordinate_height() ||
        static_cast<int>(rectangle_width) > coordinate_width() - x ||
        static_cast<int>(rectangle_height) > coordinate_height() - y) {
        return false;
    }
    if (rectangle_width == 0U || rectangle_height == 0U) {
        return true;
    }
    return transform_rectangle(
        x,
        y,
        static_cast<int>(rectangle_width),
        static_cast<int>(rectangle_height),
        [color](std::uint8_t& destination) { destination = color; });
}

bool IndexedFramebuffer::outline_rectangle(
    const int x,
    const int y,
    const std::uint16_t rectangle_width,
    const std::uint16_t rectangle_height,
    const std::uint8_t color) noexcept {
    if (rectangle_width == 0U || rectangle_height == 0U) {
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

void IndexedFramebuffer::draw_pixel(
    const int x,
    const int y,
    const std::uint8_t color) noexcept {
    const auto destination = mapped_rectangle(x, y, 1, 1);
    if (!destination.valid()) {
        return;
    }
    for (int destination_y = destination.y;
         destination_y < destination.y + destination.height;
         ++destination_y) {
        auto* destination_row = row(destination_y);
        std::fill(
            destination_row + destination.x,
            destination_row + destination.x + destination.width,
            color);
    }
}

bool IndexedFramebuffer::blit(
    const std::span<const std::uint8_t> source,
    const int source_width,
    const int source_height) noexcept {
    if (source_width != coordinate_width() ||
        source_height != coordinate_height() ||
        !valid_dimensions(source_width, source_height) ||
        source.size() != static_cast<std::size_t>(source_width) *
            static_cast<std::size_t>(source_height)) {
        return false;
    }
    const auto destination = coordinate_space_.destination;
    if (destination.x == 0 && destination.y == 0 &&
        destination.width == source_width &&
        destination.height == source_height) {
        std::copy(source.begin(), source.end(), pixels_.begin());
        return true;
    }
    for (int source_y = 0; source_y < source_height; ++source_y) {
        for (int source_x = 0; source_x < source_width; ++source_x) {
            draw_pixel(
                source_x,
                source_y,
                source[static_cast<std::size_t>(source_y) *
                           static_cast<std::size_t>(source_width) +
                       static_cast<std::size_t>(source_x)]);
        }
    }
    return true;
}

bool IndexedFramebuffer::copy_from(
    const IndexedFramebuffer& source) noexcept {
    if (pixel_width_ != source.pixel_width_ ||
        pixel_height_ != source.pixel_height_ ||
        pixels_.size() != source.pixels_.size()) {
        return false;
    }
    std::copy(source.pixels_.begin(), source.pixels_.end(), pixels_.begin());
    palette_ = source.palette_;
    return true;
}

void IndexedFramebuffer::set_palette(
    const openlegend::compat::LegacyPalette& palette) noexcept {
    palette_ = palette;
}

std::uint8_t* IndexedFramebuffer::row(const int y) noexcept {
    return pixels_.data() + static_cast<std::size_t>(y) *
        static_cast<std::size_t>(pixel_width_);
}

const std::uint8_t* IndexedFramebuffer::row(const int y) const noexcept {
    return pixels_.data() + static_cast<std::size_t>(y) *
        static_cast<std::size_t>(pixel_width_);
}

IndexedFramebuffer::CoordinateSpace
IndexedFramebuffer::native_coordinate_space() const noexcept {
    return {
        pixel_width_,
        pixel_height_,
        IndexedViewport{0, 0, pixel_width_, pixel_height_}};
}

IndexedFramebuffer::CoordinateSpace
IndexedFramebuffer::legacy_ui_coordinate_space() const noexcept {
    return {width, height, legacy_ui_viewport()};
}

IndexedViewport IndexedFramebuffer::mapped_rectangle(
    const int x,
    const int y,
    const int rectangle_width,
    const int rectangle_height) const noexcept {
    if (rectangle_width <= 0 || rectangle_height <= 0 || x < 0 || y < 0 ||
        x > coordinate_space_.source_width - rectangle_width ||
        y > coordinate_space_.source_height - rectangle_height ||
        !coordinate_space_.destination.valid()) {
        return {};
    }
    const auto& destination = coordinate_space_.destination;
    const auto left = mapped_edge(
        x,
        coordinate_space_.source_width,
        destination.x,
        destination.width);
    const auto right = mapped_edge(
        x + rectangle_width,
        coordinate_space_.source_width,
        destination.x,
        destination.width);
    const auto top = mapped_edge(
        y,
        coordinate_space_.source_height,
        destination.y,
        destination.height);
    const auto bottom = mapped_edge(
        y + rectangle_height,
        coordinate_space_.source_height,
        destination.y,
        destination.height);
    return {left, top, right - left, bottom - top};
}

}  // namespace openlegend::render
