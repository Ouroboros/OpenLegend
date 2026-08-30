#include "openlegend/render/indexed_framebuffer.hpp"

#include <algorithm>
#include <cstddef>

namespace openlegend::render {

void IndexedFramebuffer::clear(const std::uint8_t color) noexcept {
    pixels_.fill(color);
}

bool IndexedFramebuffer::fill_rectangle(
    const int x,
    const int y,
    const std::uint16_t rectangle_width,
    const std::uint16_t rectangle_height,
    const std::uint8_t color) noexcept {
    const auto right = x + static_cast<int>(rectangle_width);
    const auto bottom = y + static_cast<int>(rectangle_height);
    if (x < 0 || y < 0 || right > width || bottom > height) {
        return false;
    }
    for (auto row_index = y; row_index < bottom; ++row_index) {
        auto* destination = row(row_index) + x;
        std::fill(destination, destination + rectangle_width, color);
    }
    return true;
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
    const auto right = x + static_cast<int>(rectangle_width);
    const auto bottom = y + static_cast<int>(rectangle_height);
    if (x < 0 || y < 0 || right > width || bottom > height) {
        return false;
    }
    return fill_rectangle(x, y, rectangle_width, 1U, color) &&
           fill_rectangle(x, y, 1U, rectangle_height, color) &&
           fill_rectangle(right - 1, y, 1U, rectangle_height, color) &&
           fill_rectangle(x, bottom - 1, rectangle_width, 1U, color);
}

void IndexedFramebuffer::set_palette(const openlegend::compat::LegacyPalette& palette) noexcept {
    palette_ = palette;
}

std::uint8_t* IndexedFramebuffer::row(const int y) noexcept {
    return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
}

const std::uint8_t* IndexedFramebuffer::row(const int y) const noexcept {
    return pixels_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
}

}  // namespace openlegend::render
