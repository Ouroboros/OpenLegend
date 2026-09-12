#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::render {

struct IndexedViewport {
    int x{};
    int y{};
    int width{};
    int height{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return width > 0 && height > 0;
    }
};

class IndexedFramebuffer {
public:
    static constexpr int width = static_cast<int>(openlegend::compat::kLegacyWidth);
    static constexpr int height = static_cast<int>(openlegend::compat::kLegacyHeight);

    class CoordinateSpaceGuard;

    explicit IndexedFramebuffer(
        int pixel_width = width,
        int pixel_height = height);

    [[nodiscard]] bool resize(int pixel_width, int pixel_height);

    [[nodiscard]] int pixel_width() const noexcept { return pixel_width_; }

    [[nodiscard]] int pixel_height() const noexcept { return pixel_height_; }

    [[nodiscard]] int coordinate_width() const noexcept {
        return coordinate_space_.source_width;
    }

    [[nodiscard]] int coordinate_height() const noexcept {
        return coordinate_space_.source_height;
    }

    [[nodiscard]] bool legacy_size() const noexcept {
        return pixel_width_ == width && pixel_height_ == height;
    }

    [[nodiscard]] IndexedViewport legacy_ui_viewport() const noexcept;

    [[nodiscard]] CoordinateSpaceGuard use_native_coordinates() noexcept;

    [[nodiscard]] CoordinateSpaceGuard use_legacy_ui_coordinates() noexcept;

    void clear(std::uint8_t color) noexcept;

    [[nodiscard]] bool fill_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        std::uint8_t color) noexcept;

    [[nodiscard]] bool outline_rectangle(
        int x,
        int y,
        std::uint16_t rectangle_width,
        std::uint16_t rectangle_height,
        std::uint8_t color) noexcept;

    void draw_pixel(int x, int y, std::uint8_t color) noexcept;

    [[nodiscard]] bool blit(
        std::span<const std::uint8_t> source,
        int source_width,
        int source_height) noexcept;

    [[nodiscard]] bool copy_from(
        const IndexedFramebuffer& source) noexcept;

    template <typename Operation>
    [[nodiscard]] bool transform_rectangle(
        const int x,
        const int y,
        const int rectangle_width,
        const int rectangle_height,
        Operation operation) noexcept(noexcept(operation(std::declval<std::uint8_t&>()))) {
        const auto destination = mapped_rectangle(
            x, y, rectangle_width, rectangle_height);
        if (!destination.valid()) {
            return false;
        }
        for (int destination_y = destination.y;
             destination_y < destination.y + destination.height;
             ++destination_y) {
            auto* destination_row = row(destination_y);
            for (int destination_x = destination.x;
                 destination_x < destination.x + destination.width;
                 ++destination_x) {
                operation(destination_row[destination_x]);
            }
        }
        return true;
    }

    void set_palette(const openlegend::compat::LegacyPalette& palette) noexcept;

    [[nodiscard]] std::uint8_t* row(int y) noexcept;

    [[nodiscard]] const std::uint8_t* row(int y) const noexcept;

    [[nodiscard]] std::span<std::uint8_t> pixels() noexcept { return pixels_; }

    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return pixels_; }

    [[nodiscard]] const openlegend::compat::LegacyPalette& palette() const noexcept {
        return palette_;
    }

private:
    struct CoordinateSpace {
        int source_width{};
        int source_height{};
        IndexedViewport destination;
    };

    [[nodiscard]] CoordinateSpace native_coordinate_space() const noexcept;

    [[nodiscard]] CoordinateSpace legacy_ui_coordinate_space() const noexcept;

    [[nodiscard]] IndexedViewport mapped_rectangle(
        int x,
        int y,
        int rectangle_width,
        int rectangle_height) const noexcept;

    void restore_coordinate_space(CoordinateSpace coordinate_space) noexcept {
        coordinate_space_ = coordinate_space;
    }

    std::vector<std::uint8_t> pixels_;
    openlegend::compat::LegacyPalette palette_{};
    int pixel_width_{width};
    int pixel_height_{height};
    CoordinateSpace coordinate_space_{};
};

class IndexedFramebuffer::CoordinateSpaceGuard {
public:
    CoordinateSpaceGuard(const CoordinateSpaceGuard&) = delete;

    CoordinateSpaceGuard& operator=(const CoordinateSpaceGuard&) = delete;

    CoordinateSpaceGuard(CoordinateSpaceGuard&& other) noexcept
        : framebuffer_(std::exchange(other.framebuffer_, nullptr)),
          previous_(other.previous_) {}

    CoordinateSpaceGuard& operator=(CoordinateSpaceGuard&&) = delete;

    ~CoordinateSpaceGuard() {
        if (framebuffer_ != nullptr) {
            framebuffer_->restore_coordinate_space(previous_);
        }
    }

private:
    friend class IndexedFramebuffer;

    CoordinateSpaceGuard(
        IndexedFramebuffer& framebuffer,
        const CoordinateSpace replacement) noexcept
        : framebuffer_(&framebuffer),
          previous_(framebuffer.coordinate_space_) {
        framebuffer.coordinate_space_ = replacement;
    }

    IndexedFramebuffer* framebuffer_{};
    CoordinateSpace previous_{};
};

}  // namespace openlegend::render
