#include "openlegend/render/indexed_layer.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <new>

#include "openlegend/render/indexed_framebuffer.hpp"

namespace openlegend::render {

bool IndexedLayer::set_dimensions(const int width, const int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }
    const auto unsigned_width = static_cast<std::size_t>(width);
    const auto unsigned_height = static_cast<std::size_t>(height);
    if (unsigned_height >
        std::numeric_limits<std::size_t>::max() / unsigned_width) {
        return false;
    }

    try {
        const auto pixel_count = unsigned_width * unsigned_height;
        indices_.assign(pixel_count, 0U);
        coverage_.assign(pixel_count, 0U);
    } catch (const std::bad_alloc&) {
        indices_.clear();
        coverage_.clear();
        width_ = 0;
        height_ = 0;
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

bool IndexedLayer::valid() const noexcept {
    if (width_ <= 0 || height_ <= 0) {
        return false;
    }
    const auto pixel_count =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    return indices_.size() == pixel_count && coverage_.size() == pixel_count;
}

void IndexedLayer::clear(const std::uint8_t color) noexcept {
    std::ranges::fill(indices_, color);
    std::ranges::fill(coverage_, std::uint8_t{0U});
}

void IndexedLayer::draw_pixel(
    const int x, const int y, const std::uint8_t color) noexcept {
    draw_pixel(x, y, color, 255U);
}

void IndexedLayer::draw_pixel(
    const int x,
    const int y,
    const std::uint8_t color,
    const std::uint8_t coverage) noexcept {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return;
    }
    const auto index = static_cast<std::size_t>(y) *
            static_cast<std::size_t>(width_) +
        static_cast<std::size_t>(x);
    indices_[index] = color;
    coverage_[index] = coverage;
}

bool composite_indexed_layer(
    IndexedFramebuffer& destination,
    const IndexedLayer& source) noexcept {
    if (!source.valid() || destination.pixel_width() != source.width() ||
        destination.pixel_height() != source.height()) {
        return false;
    }
    const auto source_indices = source.indices();
    const auto source_coverage = source.coverage();
    auto destination_pixels = destination.pixels();
    for (std::size_t index = 0U; index < source_indices.size(); ++index) {
        if (source_coverage[index] != 0U) {
            destination_pixels[index] = source_indices[index];
        }
    }
    return true;
}

bool convert_indexed_layer_to_rgba(
    const IndexedLayer& layer,
    const std::span<std::uint8_t> rgba) noexcept {
    if (!layer.valid() ||
        rgba.size() != layer.indices().size() *
            compat::kModernRgbaBytesPerPixel) {
        return false;
    }
    for (const auto color : layer.palette()) {
        if (!color.valid()) {
            return false;
        }
    }

    const auto indices = layer.indices();
    const auto coverage = layer.coverage();
    for (std::size_t index = 0U; index < indices.size(); ++index) {
        const auto color = layer.palette()[indices[index]];
        const auto destination = index * compat::kModernRgbaBytesPerPixel;
        rgba[destination] = compat::expand_rgb6(color.red);
        rgba[destination + 1U] = compat::expand_rgb6(color.green);
        rgba[destination + 2U] = compat::expand_rgb6(color.blue);
        rgba[destination + 3U] = coverage[index];
    }
    return true;
}

}  // namespace openlegend::render
