#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "openlegend/attributes.hpp"

namespace openlegend::render {

class Big5GlyphCache {
public:
    explicit Big5GlyphCache(std::span<const std::uint8_t> font_bytes) noexcept;

    NODISCARD std::optional<std::span<const std::uint8_t, 32>> resolve(
        std::uint16_t big5_code) noexcept;

    NODISCARD std::size_t next_replacement_slot() const noexcept {
        return next_slot_;
    }

private:
    std::span<const std::uint8_t> font_bytes_;
    std::array<std::uint16_t, 64> codes_{};
    std::array<bool, 64> valid_{};
    std::array<std::array<std::uint8_t, 32>, 64> glyphs_{};
    std::size_t next_slot_{0U};
};

}  // namespace openlegend::render
