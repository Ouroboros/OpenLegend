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

void SteadyBiosTickSource::idle() noexcept {
    std::this_thread::yield();
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
