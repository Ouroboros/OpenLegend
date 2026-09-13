#pragma once

#include <chrono>
#include <cstdint>

#include "openlegend/attributes.hpp"
#include "openlegend/motion/authoritative_motion.hpp"

namespace openlegend::render {

inline constexpr int native_motion_overscan_x = 36;
inline constexpr int native_motion_overscan_y = 18;

struct FixedScreenPoint {
    std::int64_t x{};
    std::int64_t y{};

    friend bool operator==(const FixedScreenPoint&, const FixedScreenPoint&) = default;
};

NODISCARD constexpr FixedScreenPoint project_fixed_isometric(
    const motion::FixedPosition relative,
    const int origin_x,
    const int origin_y) noexcept {
    return FixedScreenPoint{
        static_cast<std::int64_t>(origin_x) *
                motion::kFixedUnitsPerGridUnit +
            (relative.x - relative.y) * 18,
        static_cast<std::int64_t>(origin_y) *
                motion::kFixedUnitsPerGridUnit +
            (relative.x + relative.y) * 9 - relative.height,
    };
}

NODISCARD motion::FixedPosition world_camera_position(
    motion::FixedPosition actor) noexcept;

NODISCARD motion::FixedPosition scene_camera_position(
    motion::FixedPosition actor) noexcept;

NODISCARD std::int64_t fixed_presentation_progress(
    std::chrono::nanoseconds elapsed,
    std::chrono::nanoseconds duration) noexcept;

NODISCARD int quantize_output_delta(
    std::int64_t fixed_logical_delta,
    int viewport_extent,
    int source_extent) noexcept;

}  // namespace openlegend::render
