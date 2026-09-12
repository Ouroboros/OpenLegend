#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "openlegend/compat/legacy_video.hpp"
#include "test_support.hpp"

void run_legacy_video_tests() {
    using namespace openlegend::compat;

    static_assert(kLegacyPixelCount == 64'000U);
    static_assert(expand_rgb6(0U) == 0U);
    static_assert(expand_rgb6(63U) == 255U);
    static_assert(expand_rgb6(31U) == 125U);

    LegacyPixels pixels{};
    LegacyPalette palette{};
    auto frame = IndexedFrameView{pixels, palette};
    OL_CHECK(frame.valid());

    palette[7].red = 64U;
    frame = IndexedFrameView{pixels, palette};
    OL_CHECK(!frame.valid());

    const std::array<std::uint8_t, 10> short_pixels{};
    palette[7].red = 0U;
    frame = IndexedFrameView{short_pixels, palette};
    OL_CHECK(!frame.valid());

    pixels[0] = 7U;
    palette[7] = {63U, 31U, 1U};
    frame = IndexedFrameView{pixels, palette};
    ModernRgbaPixels rgba{};
    OL_CHECK(convert_indexed_frame_to_rgba(frame, rgba));
    OL_CHECK(rgba[0] == 255U);
    OL_CHECK(rgba[1] == 125U);
    OL_CHECK(rgba[2] == 4U);
    OL_CHECK(rgba[3] == 255U);
    OL_CHECK(RgbaFrameView{rgba}.valid());
    OL_CHECK(!RgbaFrameView{std::span<const std::uint8_t>{rgba}.first(4U)}.valid());
    OL_CHECK((RgbaFrameView{
        std::span<const std::uint8_t>{rgba}.first(16U), 2, 2}.valid()));
    OL_CHECK((!RgbaFrameView{
        std::span<const std::uint8_t>{rgba}.first(16U), 0, 2}.valid()));
    OL_CHECK((!RgbaFrameView{
        std::span<const std::uint8_t>{rgba}.first(16U), 2, 3}.valid()));
    OL_CHECK(!convert_indexed_frame_to_rgba(frame, std::span<std::uint8_t>{rgba}.first(4U)));

    std::vector<std::uint8_t> dynamic_pixels(640U * 360U, 7U);
    std::vector<std::uint8_t> dynamic_rgba(
        dynamic_pixels.size() * kModernRgbaBytesPerPixel);
    const IndexedFrameView dynamic_frame{
        dynamic_pixels, palette, 640, 360};
    OL_CHECK(dynamic_frame.valid());
    OL_CHECK(convert_indexed_frame_to_rgba(dynamic_frame, dynamic_rgba));
    OL_CHECK(dynamic_rgba.front() == 255U);
    OL_CHECK(dynamic_rgba.back() == 255U);

    LegacyPixels full_pixels{};
    LegacyPalette full_palette{};
    for (std::size_t index = 0U; index < full_palette.size(); ++index) {
        full_palette[index] = {
            static_cast<std::uint8_t>(index & 0x3FU),
            static_cast<std::uint8_t>((index >> 2U) & 0x3FU),
            static_cast<std::uint8_t>((index * 5U) & 0x3FU)};
    }
    for (std::size_t index = 0U; index < full_pixels.size(); ++index) {
        full_pixels[index] = static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU);
    }
    const auto source_pixels = full_pixels;
    const auto full_frame = IndexedFrameView{full_pixels, full_palette};
    ModernRgbaPixels full_rgba{};
    OL_CHECK(convert_indexed_frame_to_rgba(full_frame, full_rgba));
    OL_CHECK(full_pixels == source_pixels);
    for (std::size_t index = 0U; index < full_pixels.size(); ++index) {
        const auto color = full_palette[full_pixels[index]];
        const auto target = index * kModernRgbaBytesPerPixel;
        OL_CHECK(full_rgba[target] == expand_rgb6(color.red));
        OL_CHECK(full_rgba[target + 1U] == expand_rgb6(color.green));
        OL_CHECK(full_rgba[target + 2U] == expand_rgb6(color.blue));
        OL_CHECK(full_rgba[target + 3U] == 0xFFU);
    }

    constexpr auto exact = proportional_viewport(960, 600);
    static_assert(exact.x == 0.0F && exact.y == 0.0F);
    static_assert(
        exact.width == 960.0F && exact.height == 600.0F &&
        exact.scale == 3.0F && exact.presentation_scale == 3);
    constexpr auto bordered = proportional_viewport(1000, 700);
    static_assert(bordered.x == 0.0F && bordered.y == 37.5F);
    static_assert(
        bordered.width == 1000.0F && bordered.height == 625.0F &&
        bordered.scale == 3.125F && bordered.presentation_scale == 3);
    constexpr auto downscaled = proportional_viewport(319, 200);
    static_assert(downscaled.valid());
    static_assert(
        downscaled.width == 319.0F && downscaled.height < 200.0F &&
        downscaled.presentation_scale == 1);
    static_assert(!proportional_viewport(0, 200).valid());
    constexpr auto widescreen = proportional_viewport(1920, 1080, 640, 360);
    static_assert(widescreen.x == 0.0F && widescreen.y == 0.0F);
    static_assert(
        widescreen.width == 1920.0F && widescreen.height == 1080.0F &&
        widescreen.scale == 3.0F && widescreen.presentation_scale == 3);
    constexpr auto dynamic_bordered =
        proportional_viewport(1000, 700, 640, 360);
    static_assert(dynamic_bordered.x == 0.0F && dynamic_bordered.y == 68.75F);
    static_assert(
        dynamic_bordered.width == 1000.0F &&
        dynamic_bordered.height == 562.5F &&
        dynamic_bordered.scale == 1.5625F &&
        dynamic_bordered.presentation_scale == 2);
    constexpr auto maximized_4k =
        proportional_viewport(3840, 2034, 1280, 720);
    static_assert(maximized_4k.x > 111.9F && maximized_4k.x < 112.1F);
    static_assert(maximized_4k.y == 0.0F);
    static_assert(
        maximized_4k.width > 3615.9F && maximized_4k.width < 3616.1F &&
        maximized_4k.height == 2034.0F &&
        maximized_4k.presentation_scale == 3);
}
