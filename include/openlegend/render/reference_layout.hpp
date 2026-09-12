#pragma once

#include <algorithm>
#include <cstdint>

#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::render {

struct ReferenceViewport {
    int x{};
    int y{};
    int width{};
    int height{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return width > 0 && height > 0;
    }
};

[[nodiscard]] constexpr ReferenceViewport fit_legacy_reference_viewport(
    const int target_width,
    const int target_height) noexcept {
    constexpr auto reference_width = static_cast<int>(compat::kLegacyWidth);
    constexpr auto reference_height = static_cast<int>(compat::kLegacyHeight);
    if (target_width < reference_width || target_height < reference_height) {
        return {};
    }
    const auto width_limited =
        static_cast<std::int64_t>(target_width) * reference_height <=
        static_cast<std::int64_t>(target_height) * reference_width;
    int viewport_width = 0;
    int viewport_height = 0;
    if (width_limited) {
        viewport_width = target_width;
        viewport_height = static_cast<int>(
            static_cast<std::int64_t>(target_width) * reference_height /
            reference_width);
    } else {
        viewport_height = target_height;
        viewport_width = static_cast<int>(
            static_cast<std::int64_t>(target_height) * reference_width /
            reference_height);
    }
    return {
        (target_width - viewport_width) / 2,
        (target_height - viewport_height) / 2,
        viewport_width,
        viewport_height};
}

[[nodiscard]] constexpr int scale_legacy_reference_length(
    const int length,
    const int target_width,
    const int target_height) noexcept {
    if (length <= 0) {
        return 0;
    }
    constexpr auto reference_width = static_cast<int>(compat::kLegacyWidth);
    constexpr auto reference_height = static_cast<int>(compat::kLegacyHeight);
    const auto width_limited =
        static_cast<std::int64_t>(target_width) * reference_height <=
        static_cast<std::int64_t>(target_height) * reference_width;
    const auto numerator = width_limited ? target_width : target_height;
    const auto denominator = width_limited ? reference_width : reference_height;
    return std::max(
        1,
        static_cast<int>(
            (static_cast<std::int64_t>(length) * numerator + denominator / 2) /
            denominator));
}

}  // namespace openlegend::render
