#pragma once

#include <chrono>
#include <cstdint>

namespace openlegend::timing {

inline constexpr std::uint32_t kPitInputFrequency = 1'193'182U;
inline constexpr std::uint32_t kPitDivisor = 65'536U;
inline constexpr std::uint32_t kBiosTicksPerDay = 0x1800B0U;

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

[[nodiscard]] std::int32_t legacy_delay_tick_count(std::int32_t argument) noexcept;
[[nodiscard]] std::uint32_t wait_for_tick_change(
    TickSource& source, std::uint32_t captured_tick) noexcept;
void wait_for_next_tick(TickSource& source) noexcept;
void legacy_delay(TickSource& source, std::int32_t argument) noexcept;

}  // namespace openlegend::timing
