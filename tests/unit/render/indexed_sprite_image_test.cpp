#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/indexed_sprite_image.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "test_support.hpp"

namespace {

void run_decode_tests() {
    constexpr std::array<std::uint8_t, 18> encoded{
        3, 0,
        2, 0,
        1, 0,
        2, 0,
        5, 0, 3, 0, 5, 0,
        3, 1, 1, 7,
    };
    const auto frame = openlegend::resource::SpriteFrameView::parse(encoded);
    OL_CHECK(frame.valid());

    const auto decoded = openlegend::render::decode_indexed_sprite(frame);
    OL_CHECK(static_cast<bool>(decoded));
    OL_CHECK(decoded.image.width == 3U);
    OL_CHECK(decoded.image.height == 2U);
    OL_CHECK(decoded.image.x_offset == 1);
    OL_CHECK(decoded.image.y_offset == 2);
    OL_CHECK((decoded.image.indices == std::vector<std::uint8_t>{
        0U, 5U, 0U,
        0U, 7U, 0U,
    }));
    OL_CHECK((decoded.image.coverage == std::vector<std::uint8_t>{
        255U, 255U, 255U,
        0U, 255U, 0U,
    }));

    openlegend::render::IndexedFramebuffer destination{3, 2};
    destination.clear(9U);
    OL_CHECK(openlegend::render::composite_indexed_sprite(
        destination, decoded.image, 1, 2));
    constexpr std::array<std::uint8_t, 6> expected{
        0U, 5U, 0U,
        9U, 7U, 9U,
    };
    OL_CHECK(std::ranges::equal(destination.pixels(), expected));

    openlegend::compat::LegacyPalette palette{};
    palette[0U] = {1U, 2U, 3U};
    palette[5U] = {4U, 5U, 6U};
    palette[7U] = {7U, 8U, 9U};
    std::vector<std::uint8_t> rgba(
        decoded.image.indices.size() *
        openlegend::compat::kModernRgbaBytesPerPixel);
    OL_CHECK(openlegend::render::convert_indexed_sprite_to_rgba(
        decoded.image, palette, rgba));
    OL_CHECK(rgba[0U] == openlegend::compat::expand_rgb6(1U));
    OL_CHECK(rgba[1U] == openlegend::compat::expand_rgb6(2U));
    OL_CHECK(rgba[2U] == openlegend::compat::expand_rgb6(3U));
    OL_CHECK(rgba[3U] == 255U);
    OL_CHECK(rgba[12U + 3U] == 0U);
    OL_CHECK(rgba[16U] == openlegend::compat::expand_rgb6(7U));
    OL_CHECK(rgba[16U + 3U] == 255U);
}

void run_invalid_tests() {
    const auto invalid = openlegend::resource::SpriteFrameView::parse({});
    const auto decoded = openlegend::render::decode_indexed_sprite(invalid);
    OL_CHECK(!static_cast<bool>(decoded));
    OL_CHECK(!decoded.error.empty());
}

}  // namespace

int main() {
    run_decode_tests();
    run_invalid_tests();
    return openlegend::test::failures == 0 ? 0 : 1;
}
