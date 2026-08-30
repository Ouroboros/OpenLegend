#include "openlegend/random/legacy_random.hpp"

namespace openlegend::random {

std::uint16_t LegacyRandom::next() noexcept {
    state_ = state_ * 0x41C64E6DU + 0x3039U;
    return static_cast<std::uint16_t>((state_ >> 16U) & 0x7FFFU);
}

std::int32_t LegacyRandom::bounded(const std::int32_t upper_bound) noexcept {
    if (upper_bound <= 1 || upper_bound > 30'000) {
        return 0;
    }
    return static_cast<std::int32_t>(next()) % upper_bound;
}

}  // namespace openlegend::random
