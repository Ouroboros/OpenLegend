#pragma once

#include <chrono>
#include <cstdint>

namespace openlegend::timing {

inline constexpr std::uint32_t kPitInputFrequency = 1'193'182U;
inline constexpr std::uint32_t kPitDivisor = 65'536U;
inline constexpr std::uint32_t kBiosTicksPerDay = 0x1800B0U;
// Nominal VGA mode 13h timing; independent of the PIT/BIOS clock.
inline constexpr std::uint32_t kVgaPixelClock = 25'175'000U;
inline constexpr std::uint32_t kVgaClocksPerFrame = 800U * 449U;

class TickSource {
public:
    virtual ~TickSource() = default;

    [[nodiscard]] virtual std::uint32_t tick() const noexcept = 0;
    virtual void idle() noexcept = 0;
};

class SteadyBiosTickSource final : public TickSource {
public:
    SteadyBiosTickSource() noexcept;

    [[nodiscard]] std::uint32_t tick() const noexcept override;
    void idle() noexcept override;

private:
    std::chrono::steady_clock::time_point origin_;
};

class SteadyVgaRetraceSource final : public TickSource {
public:
    SteadyVgaRetraceSource() noexcept;
    explicit SteadyVgaRetraceSource(std::chrono::nanoseconds frame_period) noexcept;

    [[nodiscard]] std::uint32_t tick() const noexcept override;
    void idle() noexcept override;

private:
    std::chrono::steady_clock::time_point origin_;
    std::chrono::nanoseconds frame_period_;
};

[[nodiscard]] std::int32_t legacy_delay_tick_count(std::int32_t argument) noexcept;
[[nodiscard]] std::uint32_t wait_for_tick_change(
    TickSource& source, std::uint32_t captured_tick) noexcept;
void wait_for_next_tick(TickSource& source) noexcept;
void legacy_delay(TickSource& source, std::int32_t argument) noexcept;

}  // namespace openlegend::timing
