#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/legacy_sprite.hpp"

namespace openlegend::render {

[[nodiscard]] constexpr std::optional<std::size_t> legacy_sprite_index(
    const std::uint32_t legacy_id) noexcept {
    if (legacy_id > 0x7FFEU) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(legacy_id / 2U);
}

void draw_rle_sprite(
    IndexedFramebuffer& framebuffer,
    const openlegend::resource::SpriteFrameView& frame,
    int anchor_x,
    int anchor_y) noexcept;

}  // namespace openlegend::render
