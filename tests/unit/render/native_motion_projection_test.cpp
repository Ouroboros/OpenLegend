#include <chrono>
#include <cstdint>

#include "openlegend/motion/authoritative_motion.hpp"
#include "openlegend/render/native_motion_projection.hpp"
#include "test_support.hpp"

namespace {

using openlegend::motion::FixedPosition;
using openlegend::motion::kFixedUnitsPerGridUnit;
using openlegend::render::FixedScreenPoint;

void run_fixed_projection_tests() {
    OL_CHECK((openlegend::render::project_fixed_isometric(
        FixedPosition{kFixedUnitsPerGridUnit, 0, 0}, 100, 50) ==
        FixedScreenPoint{
            118 * kFixedUnitsPerGridUnit,
            59 * kFixedUnitsPerGridUnit}));
    OL_CHECK((openlegend::render::project_fixed_isometric(
        FixedPosition{0, kFixedUnitsPerGridUnit, 5 * kFixedUnitsPerGridUnit},
        100,
        50) ==
        FixedScreenPoint{
            82 * kFixedUnitsPerGridUnit,
            54 * kFixedUnitsPerGridUnit}));
}

void run_camera_tests() {
    const auto world = openlegend::render::world_camera_position(
        FixedPosition{20 * kFixedUnitsPerGridUnit,
                      30 * kFixedUnitsPerGridUnit,
                      7 * kFixedUnitsPerGridUnit});
    OL_CHECK((world == FixedPosition{
        20 * kFixedUnitsPerGridUnit,
        30 * kFixedUnitsPerGridUnit,
        0}));

    const auto low = openlegend::render::scene_camera_position(
        FixedPosition{0, 5 * kFixedUnitsPerGridUnit, 0});
    OL_CHECK((low == FixedPosition{
        11 * kFixedUnitsPerGridUnit,
        11 * kFixedUnitsPerGridUnit,
        0}));
    const auto middle = openlegend::render::scene_camera_position(
        FixedPosition{20 * kFixedUnitsPerGridUnit,
                      30 * kFixedUnitsPerGridUnit,
                      0});
    OL_CHECK((middle == FixedPosition{
        20 * kFixedUnitsPerGridUnit,
        30 * kFixedUnitsPerGridUnit,
        0}));
    const auto high = openlegend::render::scene_camera_position(
        FixedPosition{63 * kFixedUnitsPerGridUnit,
                      60 * kFixedUnitsPerGridUnit,
                      0});
    OL_CHECK((high == FixedPosition{
        47 * kFixedUnitsPerGridUnit,
        47 * kFixedUnitsPerGridUnit,
        0}));
}

void run_presentation_progress_tests() {
    using namespace std::chrono_literals;
    OL_CHECK(openlegend::render::fixed_presentation_progress(0ns, 100ns) == 0);
    OL_CHECK(openlegend::render::fixed_presentation_progress(50ns, 100ns) ==
        kFixedUnitsPerGridUnit / 2);
    OL_CHECK(openlegend::render::fixed_presentation_progress(100ns, 100ns) ==
        kFixedUnitsPerGridUnit);
    OL_CHECK(openlegend::render::fixed_presentation_progress(150ns, 100ns) ==
        kFixedUnitsPerGridUnit);
    OL_CHECK(openlegend::render::fixed_presentation_progress(50ns, 0ns) == 0);
}

void run_output_quantization_tests() {
    OL_CHECK(openlegend::render::quantize_output_delta(
        kFixedUnitsPerGridUnit, 3'456, 320) == 11);
    OL_CHECK(openlegend::render::quantize_output_delta(
        -kFixedUnitsPerGridUnit, 3'456, 320) == -11);
    OL_CHECK(openlegend::render::quantize_output_delta(
        kFixedUnitsPerGridUnit / 2, 3'456, 320) == 5);
    OL_CHECK(openlegend::render::quantize_output_delta(
        kFixedUnitsPerGridUnit, 0, 320) == 0);
}

}  // namespace

int main() {
    run_fixed_projection_tests();
    run_camera_tests();
    run_presentation_progress_tests();
    run_output_quantization_tests();
    return openlegend::test::failures == 0 ? 0 : 1;
}
