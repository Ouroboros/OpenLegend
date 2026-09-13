#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/resource/legacy_sprite.hpp"

namespace openlegend::render {

class IndexedFramebuffer;

struct IndexedSpriteImage {
    std::uint16_t width{};
    std::uint16_t height{};
    std::int16_t x_offset{};
    std::int16_t y_offset{};
    std::vector<std::uint8_t> indices;
    std::vector<std::uint8_t> coverage;

    NODISCARD bool valid() const noexcept {
        const auto pixel_count =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
        return width != 0U && height != 0U && indices.size() == pixel_count &&
            coverage.size() == pixel_count;
    }
};

struct IndexedSpriteDecodeResult {
    IndexedSpriteImage image;
    std::string error;

    NODISCARD explicit operator bool() const noexcept {
        return error.empty() && image.valid();
    }
};

NODISCARD IndexedSpriteDecodeResult decode_indexed_sprite(
    const resource::SpriteFrameView& frame);

NODISCARD bool composite_indexed_sprite(
    IndexedFramebuffer& destination,
    const IndexedSpriteImage& image,
    int anchor_x,
    int anchor_y) noexcept;

NODISCARD bool convert_indexed_sprite_to_rgba(
    const IndexedSpriteImage& image,
    const compat::LegacyPalette& palette,
    std::span<std::uint8_t> rgba) noexcept;

}  // namespace openlegend::render
