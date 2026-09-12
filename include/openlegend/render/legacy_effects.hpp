#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "openlegend/render/indexed_framebuffer.hpp"

namespace openlegend::render {

[[nodiscard]] std::optional<std::vector<std::uint16_t>> parse_legacy_shadow_mask(
    std::span<const std::uint8_t> bytes);

[[nodiscard]] bool apply_legacy_shadow_mask(
    IndexedFramebuffer& framebuffer,
    std::span<const std::uint16_t> alternating_zero_skip_runs,
    int byte_offset) noexcept;

}  // namespace openlegend::render
