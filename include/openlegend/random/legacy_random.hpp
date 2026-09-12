#pragma once

#include <cstdint>

#include "openlegend/attributes.hpp"

namespace openlegend::random {

class LegacyRandom {
public:
    explicit constexpr LegacyRandom(const std::uint32_t seed = 1U) noexcept : state_(seed) {}

    constexpr void seed(const std::uint32_t value) noexcept { state_ = value; }

    NODISCARD constexpr std::uint32_t state() const noexcept { return state_; }

    NODISCARD std::uint16_t next() noexcept;

    NODISCARD std::int32_t bounded(std::int32_t upper_bound) noexcept;

    NODISCARD static constexpr std::uint32_t dos_time_seed(
        const std::uint8_t second, const std::uint8_t hundredth) noexcept {
        return static_cast<std::uint32_t>(second) * 100U +
               static_cast<std::uint32_t>(hundredth);
    }

private:
    std::uint32_t state_;
};

}  // namespace openlegend::random
