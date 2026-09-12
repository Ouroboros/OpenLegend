#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "openlegend/attributes.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/legacy_sprite.hpp"

namespace openlegend::render {

NODISCARD constexpr std::optional<std::size_t> legacy_sprite_index(
    const std::uint32_t legacy_id) noexcept {
    if (legacy_id > 0x7FFEU) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(legacy_id / 2U);
}

NODISCARD constexpr std::optional<std::size_t> legacy_item_sprite_index(
    const std::int16_t item_id) noexcept {
    if (item_id < 0) {
        return std::nullopt;
    }
    constexpr std::uint32_t item_sprite_base_id = 7'002U;
    return legacy_sprite_index(
        item_sprite_base_id + 2U * static_cast<std::uint32_t>(item_id));
}

void draw_rle_sprite(
    IndexedFramebuffer& framebuffer,
    const openlegend::resource::SpriteFrameView& frame,
    int anchor_x,
    int anchor_y) noexcept;

}  // namespace openlegend::render
