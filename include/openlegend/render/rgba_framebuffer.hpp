#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/color.hpp"
#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::render {

class RgbaFramebuffer {
public:
    static constexpr int width = static_cast<int>(compat::kLegacyWidth);
    static constexpr int height = static_cast<int>(compat::kLegacyHeight);
    static constexpr int maximum_scale = 64;

    RgbaFramebuffer();

    RgbaFramebuffer(int logical_width, int logical_height);

    NODISCARD bool set_dimensions(
        int logical_width, int logical_height, int scale);

    NODISCARD bool set_scale(int scale);

    NODISCARD int scale() const noexcept { return scale_; }

    NODISCARD int logical_width() const noexcept { return logical_width_; }

    NODISCARD int logical_height() const noexcept { return logical_height_; }

    NODISCARD int pixel_width() const noexcept { return logical_width_ * scale_; }

    NODISCARD int pixel_height() const noexcept { return logical_height_ * scale_; }

    void clear(compat::Rgba8 color) noexcept;

    NODISCARD bool fill_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        compat::Rgba8 color) noexcept;

    NODISCARD bool blend_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        compat::Rgba8 color) noexcept;

    NODISCARD bool outline_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        compat::Rgba8 color) noexcept;

    NODISCARD bool blend_pixel(int x, int y, compat::Rgba8 color) noexcept;

    NODISCARD bool blend_physical_pixel(
        int x, int y, compat::Rgba8 color) noexcept;

    NODISCARD std::uint8_t* row(int y) noexcept;

    NODISCARD const std::uint8_t* row(int y) const noexcept;

    NODISCARD std::span<std::uint8_t> pixels() noexcept {
        return {pixels_.data(), pixels_.size()};
    }

    NODISCARD std::span<const std::uint8_t> pixels() const noexcept {
        return {pixels_.data(), pixels_.size()};
    }

private:
    std::vector<std::uint8_t> pixels_;
    int logical_width_{width};
    int logical_height_{height};
    int scale_{1};
};

}  // namespace openlegend::render
