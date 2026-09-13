#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::render {

class IndexedFramebuffer;

class IndexedLayer {
public:
    IndexedLayer() = default;

    NODISCARD bool set_dimensions(int width, int height);

    NODISCARD bool valid() const noexcept;

    NODISCARD int width() const noexcept { return width_; }

    NODISCARD int height() const noexcept { return height_; }

    void clear(std::uint8_t color = 0U) noexcept;

    void set_palette(const compat::LegacyPalette& palette) noexcept {
        palette_ = palette;
    }

    void draw_pixel(int x, int y, std::uint8_t color) noexcept;

    void draw_pixel(
        int x,
        int y,
        std::uint8_t color,
        std::uint8_t coverage) noexcept;

    NODISCARD std::span<std::uint8_t> indices() noexcept { return indices_; }

    NODISCARD std::span<const std::uint8_t> indices() const noexcept {
        return indices_;
    }

    NODISCARD std::span<std::uint8_t> coverage() noexcept { return coverage_; }

    NODISCARD std::span<const std::uint8_t> coverage() const noexcept {
        return coverage_;
    }

    NODISCARD const compat::LegacyPalette& palette() const noexcept {
        return palette_;
    }

private:
    std::vector<std::uint8_t> indices_;
    std::vector<std::uint8_t> coverage_;
    compat::LegacyPalette palette_{};
    int width_{};
    int height_{};
};

NODISCARD bool composite_indexed_layer(
    IndexedFramebuffer& destination,
    const IndexedLayer& source) noexcept;

NODISCARD bool convert_indexed_layer_to_rgba(
    const IndexedLayer& layer,
    std::span<std::uint8_t> rgba) noexcept;

}  // namespace openlegend::render
