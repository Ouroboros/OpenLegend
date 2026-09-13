#include "openlegend/render/native_motion_projection.hpp"

#include <algorithm>
#include <cstdint>

namespace openlegend::render {
namespace {

inline constexpr auto kSceneCameraMargin =
    11 * motion::kFixedUnitsPerGridUnit;
inline constexpr auto kSceneMaximumViewOrigin =
    36 * motion::kFixedUnitsPerGridUnit;

NODISCARD std::int64_t rounded_divide(
    const std::int64_t numerator,
    const std::int64_t denominator) noexcept {
    if (numerator >= 0) {
        return (numerator + denominator / 2) / denominator;
    }
    return -((-numerator + denominator / 2) / denominator);
}

NODISCARD std::int64_t scene_camera_axis(
    const std::int64_t actor_axis) noexcept {
    const auto view_origin = std::clamp(
        actor_axis - kSceneCameraMargin,
        std::int64_t{0},
        kSceneMaximumViewOrigin);
    return view_origin + kSceneCameraMargin;
}

}  // namespace

motion::FixedPosition world_camera_position(
    const motion::FixedPosition actor) noexcept {
    return motion::FixedPosition{actor.x, actor.y, 0};
}

motion::FixedPosition scene_camera_position(
    const motion::FixedPosition actor) noexcept {
    return motion::FixedPosition{
        scene_camera_axis(actor.x),
        scene_camera_axis(actor.y),
        0,
    };
}

std::int64_t fixed_presentation_progress(
    const std::chrono::nanoseconds elapsed,
    const std::chrono::nanoseconds duration) noexcept {
    if (elapsed <= std::chrono::nanoseconds::zero() ||
        duration <= std::chrono::nanoseconds::zero()) {
        return 0;
    }
    if (elapsed >= duration) {
        return motion::kFixedUnitsPerGridUnit;
    }
    return rounded_divide(
        motion::kFixedUnitsPerGridUnit * elapsed.count(),
        duration.count());
}

int quantize_output_delta(
    const std::int64_t fixed_logical_delta,
    const int viewport_extent,
    const int source_extent) noexcept {
    if (viewport_extent <= 0 || source_extent <= 0) {
        return 0;
    }
    const auto numerator = fixed_logical_delta * viewport_extent;
    const auto denominator =
        static_cast<std::int64_t>(source_extent) *
        motion::kFixedUnitsPerGridUnit;
    return static_cast<int>(rounded_divide(numerator, denominator));
}

}  // namespace openlegend::render
