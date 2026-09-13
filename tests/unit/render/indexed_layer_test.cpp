#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/indexed_layer.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "test_support.hpp"

namespace {

openlegend::resource::SpriteFrameView sample_frame() {
    constexpr std::array<std::uint8_t, 18> encoded{
        3, 0,
        2, 0,
        1, 0,
        2, 0,
        5, 0, 3, 0, 5, 0,
        3, 1, 1, 7,
    };
    return openlegend::resource::SpriteFrameView::parse(encoded);
}

void run_composition_tests() {
    openlegend::render::IndexedLayer layer;
    OL_CHECK(layer.set_dimensions(3, 2));
    const auto frame = sample_frame();
    OL_CHECK(frame.valid());
    openlegend::render::draw_rle_sprite(layer, frame, 1, 2);

    openlegend::render::IndexedFramebuffer destination{3, 2};
    destination.clear(9U);
    OL_CHECK(openlegend::render::composite_indexed_layer(destination, layer));
    constexpr std::array<std::uint8_t, 6> expected{
        0U, 5U, 0U,
        9U, 7U, 9U,
    };
    OL_CHECK(std::ranges::equal(destination.pixels(), expected));
}

void run_rgba_tests() {
    openlegend::render::IndexedLayer layer;
    OL_CHECK(layer.set_dimensions(3, 2));
    openlegend::render::draw_rle_sprite(layer, sample_frame(), 1, 2);
    layer.draw_pixel(2, 1, 0U, 96U);
    openlegend::compat::LegacyPalette palette{};
    palette[0U] = {1U, 2U, 3U};
    palette[7U] = {7U, 8U, 9U};
    layer.set_palette(palette);

    std::vector<std::uint8_t> rgba(
        layer.indices().size() *
        openlegend::compat::kModernRgbaBytesPerPixel);
    OL_CHECK(openlegend::render::convert_indexed_layer_to_rgba(layer, rgba));
    OL_CHECK(rgba[3U] == 255U);
    OL_CHECK(rgba[12U + 3U] == 0U);
    OL_CHECK(rgba[16U] == openlegend::compat::expand_rgb6(7U));
    OL_CHECK(rgba[16U + 3U] == 255U);
    OL_CHECK(rgba[20U] == openlegend::compat::expand_rgb6(1U));
    OL_CHECK(rgba[20U + 3U] == 96U);
}

}  // namespace

int main() {
    run_composition_tests();
    run_rgba_tests();
    return openlegend::test::failures == 0 ? 0 : 1;
}
