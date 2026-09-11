#include "openlegend/time/legacy_clock.hpp"

#include <cmath>
#include <thread>

namespace openlegend::timing {

SteadyBiosTickSource::SteadyBiosTickSource() noexcept : origin_(std::chrono::steady_clock::now()) {}

std::uint32_t SteadyBiosTickSource::tick() const noexcept {
    const auto elapsed = std::chrono::duration<long double>(
        std::chrono::steady_clock::now() - origin_);
    const auto exact_ticks =
        elapsed.count() * static_cast<long double>(kPitInputFrequency) /
        static_cast<long double>(kPitDivisor);
    const auto whole_ticks = static_cast<std::uint64_t>(std::floor(exact_ticks));
    return static_cast<std::uint32_t>(whole_ticks % kBiosTicksPerDay);
}

std::chrono::steady_clock::time_point
SteadyBiosTickSource::next_tick_deadline() const noexcept {
    const auto elapsed = std::chrono::duration<long double>(
        std::chrono::steady_clock::now() - origin_);
    const auto exact_ticks =
        elapsed.count() * static_cast<long double>(kPitInputFrequency) /
        static_cast<long double>(kPitDivisor);
    const auto next_tick = std::floor(exact_ticks) + 1.0L;
    const auto next_tick_offset = std::chrono::duration<long double>{
        next_tick * static_cast<long double>(kPitDivisor) /
        static_cast<long double>(kPitInputFrequency)};
    return origin_ +
        std::chrono::ceil<std::chrono::steady_clock::duration>(next_tick_offset);
}

std::chrono::nanoseconds SteadyBiosTickSource::time_until_next_tick() const noexcept {
    const auto now = std::chrono::steady_clock::now();
    const auto deadline = next_tick_deadline();
    if (deadline <= now) {
        return std::chrono::nanoseconds::zero();
    }
    return std::chrono::ceil<std::chrono::nanoseconds>(deadline - now);
}

void SteadyBiosTickSource::idle() noexcept {
    std::this_thread::sleep_until(next_tick_deadline());
}

SteadyVgaRetraceSource::SteadyVgaRetraceSource() noexcept
    : origin_(std::chrono::steady_clock::now()) {}

std::uint32_t SteadyVgaRetraceSource::tick() const noexcept {
    const auto elapsed = std::chrono::duration<long double>(
        std::chrono::steady_clock::now() - origin_);
    const auto frames = elapsed.count() * static_cast<long double>(kVgaPixelClock) /
        static_cast<long double>(kVgaClocksPerFrame);
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(std::floor(frames)));
}

void SteadyVgaRetraceSource::idle() noexcept {
    const auto elapsed = std::chrono::duration<long double>(
        std::chrono::steady_clock::now() - origin_);
    const auto frames = elapsed.count() * static_cast<long double>(kVgaPixelClock) /
        static_cast<long double>(kVgaClocksPerFrame);
    const auto next_frame_offset = std::chrono::duration<long double>{
        (std::floor(frames) + 1.0L) * static_cast<long double>(kVgaClocksPerFrame) /
        static_cast<long double>(kVgaPixelClock)};
    std::this_thread::sleep_until(origin_ +
        std::chrono::ceil<std::chrono::steady_clock::duration>(next_frame_offset));
}

std::int32_t legacy_delay_tick_count(const std::int32_t argument) noexcept {
    return argument / 40 + 1;
}

std::uint32_t wait_for_tick_change(
    TickSource& source, const std::uint32_t captured_tick) noexcept {
    auto current = source.tick();
    while (current == captured_tick) {
        source.idle();
        current = source.tick();
    }
    return current;
}

void wait_for_next_tick(TickSource& source) noexcept {
    const auto captured_tick = source.tick();
    static_cast<void>(wait_for_tick_change(source, captured_tick));
}

void legacy_delay(TickSource& source, const std::int32_t argument) noexcept {
    const auto wait_count = legacy_delay_tick_count(argument);
    for (std::int32_t elapsed = 0; elapsed < wait_count; ++elapsed) {
        wait_for_next_tick(source);
    }
}

}  // namespace openlegend::timing
