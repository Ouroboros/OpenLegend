#include <array>
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
}
