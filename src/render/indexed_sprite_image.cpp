#include "openlegend/render/indexed_sprite_image.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>

#include "openlegend/render/indexed_framebuffer.hpp"

namespace openlegend::render {

IndexedSpriteDecodeResult decode_indexed_sprite(
    const resource::SpriteFrameView& frame) {
    IndexedSpriteDecodeResult result;
    if (!frame.valid()) {
        result.error = frame.error();
        return result;
    }
    if (frame.width() == 0U || frame.height() == 0U) {
        result.error = "sprite frame dimensions must be positive";
        return result;
    }

    const auto pixel_count = static_cast<std::size_t>(frame.width()) *
        static_cast<std::size_t>(frame.height());
    try {
        result.image.width = frame.width();
        result.image.height = frame.height();
        result.image.x_offset = frame.x_offset();
        result.image.y_offset = frame.y_offset();
        result.image.indices.resize(pixel_count, 0U);
        result.image.coverage.resize(pixel_count, 0U);
    } catch (const std::bad_alloc&) {
        result.image = {};
        result.error = "unable to allocate indexed sprite image";
        return result;
    }

    for (std::size_t row = 0U; row < frame.rows().size(); ++row) {
        auto x = std::size_t{};
        for (const auto& run : frame.rows()[row].runs) {
            x += run.skip;
            const auto destination = row * static_cast<std::size_t>(frame.width()) + x;
            std::copy(
                run.pixels.begin(),
                run.pixels.end(),
                result.image.indices.begin() +
                    static_cast<std::ptrdiff_t>(destination));
            std::fill_n(
                result.image.coverage.begin() +
                    static_cast<std::ptrdiff_t>(destination),
                run.pixels.size(),
                std::uint8_t{255U});
            x += run.pixels.size();
        }
    }
    return result;
}

bool composite_indexed_sprite(
    IndexedFramebuffer& destination,
    const IndexedSpriteImage& image,
    const int anchor_x,
    const int anchor_y) noexcept {
    if (!image.valid()) {
        return false;
    }
    const auto left = anchor_x - static_cast<int>(image.x_offset);
    const auto top = anchor_y - static_cast<int>(image.y_offset);
    for (std::size_t y = 0U; y < image.height; ++y) {
        for (std::size_t x = 0U; x < image.width; ++x) {
            const auto index = y * static_cast<std::size_t>(image.width) + x;
            if (image.coverage[index] != 0U) {
                destination.draw_pixel(
                    left + static_cast<int>(x),
                    top + static_cast<int>(y),
                    image.indices[index]);
            }
        }
    }
    return true;
}

bool convert_indexed_sprite_to_rgba(
    const IndexedSpriteImage& image,
    const compat::LegacyPalette& palette,
    const std::span<std::uint8_t> rgba) noexcept {
    if (!image.valid() ||
        rgba.size() != image.indices.size() *
            compat::kModernRgbaBytesPerPixel) {
        return false;
    }
    for (const auto color : palette) {
        if (!color.valid()) {
            return false;
        }
    }

    for (std::size_t index = 0U; index < image.indices.size(); ++index) {
        const auto color = palette[image.indices[index]];
        const auto destination = index * compat::kModernRgbaBytesPerPixel;
        rgba[destination] = compat::expand_rgb6(color.red);
        rgba[destination + 1U] = compat::expand_rgb6(color.green);
        rgba[destination + 2U] = compat::expand_rgb6(color.blue);
        rgba[destination + 3U] = image.coverage[index];
    }
    return true;
}

}  // namespace openlegend::render
