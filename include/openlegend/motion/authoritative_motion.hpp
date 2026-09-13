#pragma once

#include <chrono>
#include <cstdint>

#include "openlegend/attributes.hpp"

namespace openlegend::motion {

inline constexpr std::int64_t kFixedUnitsPerGridUnit = 1LL << 16;

struct GridPosition {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t height{};

    friend bool operator==(const GridPosition&, const GridPosition&) = default;
};

struct FixedPosition {
    std::int64_t x{};
    std::int64_t y{};
    std::int64_t height{};

    friend bool operator==(const FixedPosition&, const FixedPosition&) = default;
};

struct MotionAdvanceResult {
    bool reached_destination{};
    std::chrono::nanoseconds remaining{};

    friend bool operator==(const MotionAdvanceResult&, const MotionAdvanceResult&) = default;
};

class AuthoritativeMotion {
public:
    NODISCARD bool begin(
        GridPosition source,
        GridPosition destination,
        std::chrono::nanoseconds duration) noexcept;

    NODISCARD MotionAdvanceResult advance(std::chrono::nanoseconds elapsed) noexcept;

    void clear() noexcept;

    NODISCARD bool active() const noexcept { return active_; }

    NODISCARD const GridPosition& source() const noexcept { return source_; }

    NODISCARD const GridPosition& destination() const noexcept { return destination_; }

    NODISCARD const FixedPosition& position() const noexcept { return position_; }

    NODISCARD std::chrono::nanoseconds elapsed() const noexcept { return elapsed_; }

    NODISCARD std::chrono::nanoseconds duration() const noexcept { return duration_; }

private:
    NODISCARD static bool supported_path(
        GridPosition source, GridPosition destination) noexcept;

    NODISCARD static FixedPosition fixed_position(GridPosition position) noexcept;

    NODISCARD static std::int64_t evaluate_axis(
        std::int32_t source,
        std::int32_t destination,
        std::chrono::nanoseconds elapsed,
        std::chrono::nanoseconds duration) noexcept;

    void update_position() noexcept;

    GridPosition source_{};
    GridPosition destination_{};
    FixedPosition position_{};
    std::chrono::nanoseconds elapsed_{};
    std::chrono::nanoseconds duration_{};
    bool active_{};
};

}  // namespace openlegend::motion
