#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::render {

class IndexedFramebuffer {
public:
    static constexpr int width = static_cast<int>(openlegend::compat::kLegacyWidth);
    static constexpr int height = static_cast<int>(openlegend::compat::kLegacyHeight);

    void clear(std::uint8_t color) noexcept;
    [[nodiscard]] bool fill_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        std::uint8_t color) noexcept;

    void set_palette(const openlegend::compat::LegacyPalette& palette) noexcept;

    [[nodiscard]] std::uint8_t* row(int y) noexcept;
    [[nodiscard]] const std::uint8_t* row(int y) const noexcept;
    [[nodiscard]] std::span<std::uint8_t> pixels() noexcept { return pixels_; }
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return pixels_; }
    [[nodiscard]] const openlegend::compat::LegacyPalette& palette() const noexcept { return palette_; }

private:
    std::array<std::uint8_t, openlegend::compat::kLegacyPixelCount> pixels_{};
    openlegend::compat::LegacyPalette palette_{};
};

}  // namespace openlegend::render
