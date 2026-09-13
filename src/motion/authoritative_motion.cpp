#include "openlegend/motion/authoritative_motion.hpp"

#include <chrono>
#include <cstdint>

#include "openlegend/attributes.hpp"

namespace openlegend::motion {
namespace {

inline constexpr std::int64_t kMaximumAxisDelta = 1'024;
inline constexpr auto kMaximumMotionDuration = std::chrono::seconds{60};

NODISCARD std::int64_t rounded_divide(
    const std::int64_t numerator,
    const std::int64_t denominator) noexcept {
    if (numerator >= 0) {
        return (numerator + denominator / 2) / denominator;
    }
    return -((-numerator + denominator / 2) / denominator);
}

NODISCARD bool supported_axis(
    const std::int32_t source,
    const std::int32_t destination) noexcept {
    const auto delta =
        static_cast<std::int64_t>(destination) - static_cast<std::int64_t>(source);
    return delta >= -kMaximumAxisDelta && delta <= kMaximumAxisDelta;
}

}  // namespace

bool AuthoritativeMotion::begin(
    const GridPosition source,
    const GridPosition destination,
    const std::chrono::nanoseconds duration) noexcept {
    if (active_ || source == destination ||
        duration <= std::chrono::nanoseconds::zero() ||
        duration > kMaximumMotionDuration ||
        !supported_path(source, destination)) {
        return false;
    }

    source_ = source;
    destination_ = destination;
    position_ = fixed_position(source);
    elapsed_ = std::chrono::nanoseconds::zero();
    duration_ = duration;
    active_ = true;
    return true;
}

MotionAdvanceResult AuthoritativeMotion::advance(
    const std::chrono::nanoseconds elapsed) noexcept {
    if (elapsed <= std::chrono::nanoseconds::zero()) {
        return {};
    }
    if (!active_) {
        return MotionAdvanceResult{false, elapsed};
    }

    const auto until_destination = duration_ - elapsed_;
    if (elapsed >= until_destination) {
        elapsed_ = duration_;
        position_ = fixed_position(destination_);
        active_ = false;
        return MotionAdvanceResult{true, elapsed - until_destination};
    }

    elapsed_ += elapsed;
    update_position();
    return {};
}

void AuthoritativeMotion::clear() noexcept {
    source_ = {};
    destination_ = {};
    position_ = {};
    elapsed_ = std::chrono::nanoseconds::zero();
    duration_ = std::chrono::nanoseconds::zero();
    active_ = false;
}

bool AuthoritativeMotion::supported_path(
    const GridPosition source,
    const GridPosition destination) noexcept {
    return supported_axis(source.x, destination.x) &&
        supported_axis(source.y, destination.y) &&
        supported_axis(source.height, destination.height);
}

FixedPosition AuthoritativeMotion::fixed_position(
    const GridPosition position) noexcept {
    return FixedPosition{
        static_cast<std::int64_t>(position.x) * kFixedUnitsPerGridUnit,
        static_cast<std::int64_t>(position.y) * kFixedUnitsPerGridUnit,
        static_cast<std::int64_t>(position.height) * kFixedUnitsPerGridUnit,
    };
}

std::int64_t AuthoritativeMotion::evaluate_axis(
    const std::int32_t source,
    const std::int32_t destination,
    const std::chrono::nanoseconds elapsed,
    const std::chrono::nanoseconds duration) noexcept {
    const auto source_fixed =
        static_cast<std::int64_t>(source) * kFixedUnitsPerGridUnit;
    const auto delta_fixed =
        (static_cast<std::int64_t>(destination) - static_cast<std::int64_t>(source)) *
        kFixedUnitsPerGridUnit;
    const auto offset = rounded_divide(
        delta_fixed * elapsed.count(), duration.count());
    return source_fixed + offset;
}

void AuthoritativeMotion::update_position() noexcept {
    position_.x = evaluate_axis(source_.x, destination_.x, elapsed_, duration_);
    position_.y = evaluate_axis(source_.y, destination_.y, elapsed_, duration_);
    position_.height = evaluate_axis(
        source_.height, destination_.height, elapsed_, duration_);
}

}  // namespace openlegend::motion
