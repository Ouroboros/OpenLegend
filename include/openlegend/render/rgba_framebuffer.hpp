#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "openlegend/compat/color.hpp"
#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::render {

class RgbaFramebuffer {
public:
    static constexpr int width = static_cast<int>(compat::kLegacyWidth);
    static constexpr int height = static_cast<int>(compat::kLegacyHeight);
    static constexpr int maximum_scale = 64;

    RgbaFramebuffer();

    [[nodiscard]] bool set_scale(int scale);
    [[nodiscard]] int scale() const noexcept { return scale_; }
    [[nodiscard]] int pixel_width() const noexcept { return width * scale_; }
    [[nodiscard]] int pixel_height() const noexcept { return height * scale_; }

    void clear(compat::Rgba8 color) noexcept;

    [[nodiscard]] bool fill_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        compat::Rgba8 color) noexcept;

    [[nodiscard]] bool blend_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        compat::Rgba8 color) noexcept;

    [[nodiscard]] bool outline_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        compat::Rgba8 color) noexcept;

    [[nodiscard]] bool blend_pixel(int x, int y, compat::Rgba8 color) noexcept;
    [[nodiscard]] bool blend_physical_pixel(
        int x, int y, compat::Rgba8 color) noexcept;

    [[nodiscard]] std::uint8_t* row(int y) noexcept;
    [[nodiscard]] const std::uint8_t* row(int y) const noexcept;
    [[nodiscard]] std::span<std::uint8_t> pixels() noexcept {
        return {pixels_.data(), pixels_.size()};
    }
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept {
        return {pixels_.data(), pixels_.size()};
    }

private:
    std::vector<std::uint8_t> pixels_;
    int scale_{1};
};

}  // namespace openlegend::render
