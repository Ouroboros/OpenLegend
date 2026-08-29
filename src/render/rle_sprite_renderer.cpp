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
                if (destination_y >= 0 && destination_y < IndexedFramebuffer::height &&
                    destination_x >= 0 && destination_x < IndexedFramebuffer::width) {
                    framebuffer.row(destination_y)[destination_x] = pixel;
                }
                ++destination_x;
            }
        }
    }
}

}  // namespace openlegend::render
