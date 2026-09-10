#include <array>
#include <cstddef>
#include <cstdint>

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
    OL_CHECK(!convert_indexed_frame_to_rgba(frame, std::span<std::uint8_t>{rgba}.first(4U)));

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

    constexpr auto exact = integer_viewport(960, 600);
    static_assert(exact.x == 0 && exact.y == 0);
    static_assert(exact.width == 960 && exact.height == 600 && exact.scale == 3);
    constexpr auto bordered = integer_viewport(1000, 700);
    static_assert(bordered.x == 20 && bordered.y == 50);
    static_assert(bordered.width == 960 && bordered.height == 600 && bordered.scale == 3);
    static_assert(!integer_viewport(319, 200).valid());
}
