#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace openlegend::render {

inline constexpr std::size_t kFadeToBlackFrameCount = 64U;
inline constexpr std::size_t kFadeFromBlackFrameCount = 65U;

[[nodiscard]] constexpr std::uint8_t
fade_to_black_alpha(const std::size_t frame) noexcept {
  const auto bounded = std::min(frame, kFadeToBlackFrameCount - 1U);
  return static_cast<std::uint8_t>((255U * (bounded + 1U) + 32U) / 64U);
}

[[nodiscard]] constexpr std::uint8_t
fade_from_black_alpha(const std::size_t frame) noexcept {
  const auto bounded = std::min(frame, kFadeFromBlackFrameCount - 1U);
  return static_cast<std::uint8_t>((255U * (64U - bounded) + 32U) / 64U);
}

} // namespace openlegend::render
