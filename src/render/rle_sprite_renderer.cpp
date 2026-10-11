#include "openlegend/render/rle_sprite_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace openlegend::render {
namespace {

struct ClippedRun {
    int destination_x{};
    std::size_t source_offset{};
    std::size_t count{};
};

NODISCARD ClippedRun clip_run(
    const int destination_x, const std::size_t pixel_count, const int width) noexcept {
    const auto begin = std::max(destination_x, 0);
    const auto end = std::min(destination_x + static_cast<int>(pixel_count), width);
    if (begin >= end) {
        return {};
    }
    return {begin, static_cast<std::size_t>(begin - destination_x),
            static_cast<std::size_t>(end - begin)};
}

}  // namespace

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
    if (framebuffer.native_coordinates_active()) {
        for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
            const auto destination_y = top + static_cast<int>(row_index);
            if (destination_y < 0 || destination_y >= framebuffer.pixel_height()) {
                continue;
            }
            auto* destination_row = framebuffer.row(destination_y);
            auto destination_x = left;
            for (const auto& run : frame.rows()[row_index].runs) {
                destination_x += static_cast<int>(run.skip);
                const auto clipped =
                    clip_run(destination_x, run.pixels.size(), framebuffer.pixel_width());
                if (clipped.count != 0U) {
                    std::memcpy(destination_row + clipped.destination_x,
                        run.pixels.data() + clipped.source_offset, clipped.count);
                }
                destination_x += static_cast<int>(run.pixels.size());
            }
        }
        return;
    }
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

void draw_rle_sprite(
    IndexedLayer& layer,
    const openlegend::resource::SpriteFrameView& frame,
    const int anchor_x,
    const int anchor_y) noexcept {
    if (!layer.valid() || !frame.valid()) {
        return;
    }

    const auto left = anchor_x - static_cast<int>(frame.x_offset());
    const auto top = anchor_y - static_cast<int>(frame.y_offset());
    const auto layer_width = static_cast<std::size_t>(layer.width());
    for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
        const auto destination_y = top + static_cast<int>(row_index);
        if (destination_y < 0 || destination_y >= layer.height()) {
            continue;
        }
        const auto row_offset = static_cast<std::size_t>(destination_y) * layer_width;
        auto destination_x = left;
        for (const auto& run : frame.rows()[row_index].runs) {
            destination_x += static_cast<int>(run.skip);
            const auto clipped = clip_run(destination_x, run.pixels.size(), layer.width());
            if (clipped.count != 0U) {
                const auto offset = row_offset + static_cast<std::size_t>(clipped.destination_x);
                std::memcpy(layer.indices().data() + offset,
                    run.pixels.data() + clipped.source_offset, clipped.count);
                std::memset(layer.coverage().data() + offset, 255, clipped.count);
            }
            destination_x += static_cast<int>(run.pixels.size());
        }
    }
}

}  // namespace openlegend::render
