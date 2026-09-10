#pragma once

#include <cstdint>

namespace openlegend::compat {

struct Rgba8 {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{};
};

inline constexpr Rgba8 kRuntimeClearColor{0U, 0U, 0U, 0xFFU};
inline constexpr std::uint8_t kOpaqueAlpha = kRuntimeClearColor.alpha;

}  // namespace openlegend::compat
