#include "openlegend/render/world_depth_order.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace openlegend::render {
namespace {

constexpr std::size_t cache_cell_count =
    static_cast<std::size_t>(legacy_world_cache_extent) *
    static_cast<std::size_t>(legacy_world_cache_extent);
constexpr std::uint16_t maximum_legacy_sprite_id = 0x2064U;

[[nodiscard]] std::size_t cache_index(const int x, const int y) noexcept {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(legacy_world_cache_extent) +
           static_cast<std::size_t>(x);
}

[[nodiscard]] bool same_owner(
    const LegacyDepthEntry& entry,
    const std::int16_t owner_x,
    const std::int16_t owner_y) noexcept {
    return entry.world_x == owner_x && entry.world_y == owner_y;
}

}  // namespace

LegacyDepthResult build_legacy_world_depth_list(const LegacyWorldDepthInput& input) {
    LegacyDepthResult result;
    if (input.owner_x.size() < cache_cell_count || input.owner_y.size() < cache_cell_count ||
        input.building_sprite.size() < cache_cell_count) {
        result.error = "legacy world depth grids must contain 128x128 cells";
        return result;
    }

    const auto start_x = input.view_cache_x - 11;
    const auto start_y = input.view_cache_y - 11;
    const auto end_x = input.view_cache_x + 21;
    const auto end_y = input.view_cache_y + 21;
    if (start_x < 0 || start_y < 0 || end_x > legacy_world_cache_extent ||
        end_y > legacy_world_cache_extent) {
        result.error = "legacy 32x32 view lies outside the 128x128 cache";
        return result;
    }

    auto owner_x = std::vector<std::int16_t>{input.owner_x.begin(), input.owner_x.end()};
    auto owner_y = std::vector<std::int16_t>{input.owner_y.begin(), input.owner_y.end()};
    const auto inject_actor = [&](const LegacyDepthActor& actor) {
        if (actor.cache_x >= 0 && actor.cache_x < legacy_world_cache_extent && actor.cache_y >= 0 &&
            actor.cache_y < legacy_world_cache_extent) {
            const auto index = cache_index(actor.cache_x, actor.cache_y);
            owner_x[index] = actor.world_x;
            owner_y[index] = actor.world_y;
        }
    };
    inject_actor(input.primary_actor);
    if (input.secondary_actor) {
        inject_actor(*input.secondary_actor);
    }

    result.entries.reserve(cache_cell_count);
    for (auto cache_x = start_x; cache_x < end_x; ++cache_x) {
        auto scan_floor_y = start_y;
        for (auto cache_y = start_y; cache_y < end_y; ++cache_y) {
            const auto index = cache_index(cache_x, cache_y);
            const auto occupied = owner_x[index] != 0 || owner_y[index] != 0;
            const auto primary_here = cache_x == input.primary_actor.cache_x &&
                                      cache_y == input.primary_actor.cache_y;
            const auto secondary_here = input.secondary_actor &&
                                        cache_x == input.secondary_actor->cache_x &&
                                        cache_y == input.secondary_actor->cache_y;
            if (!occupied && !primary_here && !secondary_here) {
                continue;
            }

            if (primary_here) {
                result.entries.push_back(LegacyDepthEntry{
                    input.primary_actor.world_x,
                    input.primary_actor.world_y,
                    input.primary_actor.sprite_id});
                continue;
            }
            if (secondary_here) {
                result.entries.push_back(LegacyDepthEntry{
                    input.secondary_actor->world_x,
                    input.secondary_actor->world_y,
                    input.secondary_actor->sprite_id});
                continue;
            }

            const auto current_owner_x = owner_x[index];
            const auto current_owner_y = owner_y[index];
            const auto found = std::find_if(
                result.entries.begin(), result.entries.end(), [&](const LegacyDepthEntry& entry) {
                    return same_owner(entry, current_owner_x, current_owner_y);
                });
            if (found == result.entries.end()) {
                const auto local_owner_x = static_cast<int>(current_owner_x) - input.cache_origin_x;
                const auto local_owner_y = static_cast<int>(current_owner_y) - input.cache_origin_y;
                if (local_owner_x < 0 || local_owner_x >= legacy_world_cache_extent ||
                    local_owner_y < 0 || local_owner_y >= legacy_world_cache_extent) {
                    result.error = "building owner lies outside the 128x128 cache";
                    return result;
                }
                const auto sprite_id = input.building_sprite[cache_index(local_owner_x, local_owner_y)];
                if (static_cast<std::uint16_t>(sprite_id) > maximum_legacy_sprite_id) {
                    result.error = "building sprite id exceeds the original 0x2064 limit";
                    return result;
                }
                result.entries.push_back(LegacyDepthEntry{current_owner_x, current_owner_y, sprite_id});
                continue;
            }

            const auto found_index = static_cast<std::size_t>(found - result.entries.begin());
            if (found_index == result.entries.size() - 1U) {
                continue;
            }

            const auto saved_last = result.entries.back();
            for (auto scan_y = cache_y - 1; scan_y >= scan_floor_y; --scan_y) {
                const auto scan_index = cache_index(cache_x, scan_y);
                if (owner_x[scan_index] == 0 && owner_y[scan_index] == 0) {
                    continue;
                }
                const auto above_is_current = owner_x[scan_index] == current_owner_x &&
                                              owner_y[scan_index] == current_owner_y;
                const auto above_is_found = same_owner(
                    result.entries[found_index], owner_x[scan_index], owner_y[scan_index]);
                if (!above_is_current && !above_is_found) {
                    for (auto move = result.entries.size() - 1U; move > found_index; --move) {
                        result.entries[move] = result.entries[move - 1U];
                    }
                    result.entries[found_index] = saved_last;
                }
            }
            scan_floor_y = cache_y + 1;
        }
    }
    return result;
}

}  // namespace openlegend::render
