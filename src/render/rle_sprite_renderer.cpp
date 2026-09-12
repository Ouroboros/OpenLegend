#include "openlegend/render/rle_sprite_renderer.hpp"

#include <cstddef>

namespace openlegend::render {

void draw_rle_sprite(
    IndexedFramebuffer& framebuffer,
    const openlegend::resource::SpriteFrameView& frame,
    const int anchor_x,
    const int anchor_y) noexcept {
    if (!frame.valid()) {
        return;
    }

    const auto left = anchor_x - static_cast<int>(frame.x_offset());
    const auto top = anchor_y - static_cast<int>(frame.y_offset());
    for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
        const auto destination_y = top + static_cast<int>(row_index);
        auto destination_x = left;
        for (const auto& run : frame.rows()[row_index].runs) {
            destination_x += static_cast<int>(run.skip);
            for (const auto pixel : run.pixels) {
                if (destination_y >= 0 &&
                    destination_y < framebuffer.coordinate_height() &&
                    destination_x >= 0 &&
                    destination_x < framebuffer.coordinate_width()) {
                    framebuffer.draw_pixel(destination_x, destination_y, pixel);
                }
                ++destination_x;
            }
        }
    }
}

}  // namespace openlegend::render
