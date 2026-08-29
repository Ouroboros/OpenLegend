#pragma once

namespace openlegend::render {

struct ScreenPoint {
    int x{};
    int y{};

    friend bool operator==(const ScreenPoint&, const ScreenPoint&) = default;
};

[[nodiscard]] constexpr ScreenPoint project_isometric(
    const int relative_x,
    const int relative_y,
    const int origin_x,
    const int origin_y) noexcept {
    return ScreenPoint{
        origin_x + (relative_x - relative_y) * 18,
        origin_y + (relative_x + relative_y) * 9};
}

[[nodiscard]] constexpr ScreenPoint legacy_world_tile_screen(
    const int cache_x,
    const int cache_y,
    const int view_cache_x,
    const int view_cache_y) noexcept {
    return project_isometric(
        cache_x - (view_cache_x - 11),
        cache_y - (view_cache_y - 11),
        145,
        -81);
}

}  // namespace openlegend::render
