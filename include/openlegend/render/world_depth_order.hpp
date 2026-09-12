#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace openlegend::render {

inline constexpr int legacy_world_cache_extent = 128;
inline constexpr int legacy_world_view_extent = 32;

struct LegacyDepthActor {
    std::int16_t world_x{};
    std::int16_t world_y{};
    int cache_x{};
    int cache_y{};
    std::int16_t sprite_id{};
};

struct WorldCacheBounds {
    int begin_x{};
    int begin_y{};
    int end_x{};
    int end_y{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return begin_x >= 0 && begin_y >= 0 && end_x > begin_x &&
            end_y > begin_y && end_x <= legacy_world_cache_extent &&
            end_y <= legacy_world_cache_extent;
    }
};

struct LegacyWorldDepthInput {
    std::span<const std::int16_t> owner_x;
    std::span<const std::int16_t> owner_y;
    std::span<const std::int16_t> building_sprite;
    int view_cache_x{};
    int view_cache_y{};
    int cache_origin_x{};
    int cache_origin_y{};
    LegacyDepthActor primary_actor{};
    std::optional<LegacyDepthActor> secondary_actor;
};

struct LegacyDepthEntry {
    std::int16_t world_x{};
    std::int16_t world_y{};
    std::int16_t sprite_id{};

    friend bool operator==(const LegacyDepthEntry&, const LegacyDepthEntry&) = default;
};

struct LegacyDepthResult {
    std::vector<LegacyDepthEntry> entries;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] LegacyDepthResult build_legacy_world_depth_list(
    const LegacyWorldDepthInput& input);

[[nodiscard]] LegacyDepthResult build_legacy_world_depth_list(
    const LegacyWorldDepthInput& input,
    WorldCacheBounds cache_bounds);

}  // namespace openlegend::render
