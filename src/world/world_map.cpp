#include "openlegend/world/world_map.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/render/world_depth_order.hpp"
#include "openlegend/render/world_projection.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"

namespace openlegend::world {
namespace {

constexpr std::array<std::string_view, 5> kLayerFiles{
    "EARTH.002", "SURFACE.002", "BUILDING.002", "BUILDX.002", "BUILDY.002"};
constexpr std::array<std::int16_t, 4> kPlayerFrameBase{5002, 5016, 5030, 5044};
constexpr std::array<std::int16_t, 4> kIdleFrameOffset{54, 52, 50, 48};
constexpr std::array<std::int16_t, 4> kShipFrameBase{7430, 7438, 7446, 7454};
constexpr std::array<std::int16_t, 12> kBlockedWalkingRanges{
    358, 362, 374, 380, 458, 464, 506, 670, 818, 824, 838, 838};
constexpr std::array<std::int16_t, 4> kBlockedWalkingRangesTail{934, 936, 1016, 1022};
constexpr std::array<std::int16_t, 12> kLandRanges{
    4, 356, 364, 372, 382, 456, 672, 954, 466, 504, 1000, 1014};
constexpr std::array<std::int16_t, 12> kShipCoastRanges{
    358, 362, 374, 380, 458, 464, 458, 464, 506, 610, 1016, 1022};

[[nodiscard]] constexpr std::size_t layer_index(const WorldLayer layer) noexcept {
    return static_cast<std::size_t>(layer);
}

[[nodiscard]] constexpr std::size_t world_index(const int x, const int y) noexcept {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(kWorldExtent) +
           static_cast<std::size_t>(x);
}

[[nodiscard]] constexpr std::size_t cache_index(const int x, const int y) noexcept {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(kWorldCacheExtent) +
           static_cast<std::size_t>(x);
}

[[nodiscard]] constexpr int clamped_origin(const int coordinate) noexcept {
    return std::clamp(coordinate - 64, 0, kWorldCacheMaximumOrigin);
}

[[nodiscard]] bool in_ranges(
    const std::int16_t value, const std::span<const std::int16_t> pairs) noexcept {
    for (std::size_t index = 0U; index + 1U < pairs.size(); index += 2U) {
        if (value >= pairs[index] && value <= pairs[index + 1U]) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr std::string_view direction_name(
    const WorldDirection direction) noexcept {
    switch (direction) {
    case WorldDirection::up: return "up";
    case WorldDirection::right: return "right";
    case WorldDirection::left: return "left";
    case WorldDirection::down: return "down";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::pair<int, int> direction_delta(
    const WorldDirection direction) noexcept {
    switch (direction) {
    case WorldDirection::up: return {0, -1};
    case WorldDirection::right: return {1, 0};
    case WorldDirection::left: return {-1, 0};
    case WorldDirection::down: return {0, 1};
    }
    return {0, 0};
}

}  // namespace

WorldMapData::WorldMapData(const resource::DataRoot& data_root) {
    for (std::size_t layer = 0U; layer < kLayerFiles.size(); ++layer) {
        const auto file = data_root.read(kLayerFiles[layer]);
        if (!file) {
            error_ = file.error;
            return;
        }
        if (file.bytes.size() != kWorldLayerBytes) {
            error_ = std::string{kLayerFiles[layer]} + " is not a 480x480 int16le layer";
            return;
        }
        auto& values = layers_[layer];
        values.resize(kWorldCellCount);
        for (std::size_t index = 0U; index < values.size(); ++index) {
            values[index] = compat::read_i16le(file.bytes, index * 2U);
        }
    }
}

std::int16_t WorldMapData::at(const WorldLayer layer, const int x, const int y) const noexcept {
    if (!valid() || x < 0 || x >= kWorldExtent || y < 0 || y >= kWorldExtent) {
        return 0;
    }
    return layers_[layer_index(layer)][world_index(x, y)];
}

std::span<const std::int16_t> WorldMapData::layer(const WorldLayer layer) const noexcept {
    return layers_[layer_index(layer)];
}

WorldCache::WorldCache() {
    for (auto& layer : layers_) {
        layer.resize(kWorldCacheCellCount);
    }
}

bool WorldCache::reload(
    const WorldMapData& map, const int origin_x, const int origin_y) noexcept {
    if (!map.valid() || origin_x < 0 || origin_x > kWorldCacheMaximumOrigin || origin_y < 0 ||
        origin_y > kWorldCacheMaximumOrigin) {
        return false;
    }
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    for (std::size_t layer = 0U; layer < layers_.size(); ++layer) {
        const auto source = map.layer(static_cast<WorldLayer>(layer));
        auto& destination = layers_[layer];
        for (int row = 0; row < kWorldCacheExtent; ++row) {
            const auto source_begin = world_index(origin_x, origin_y + row);
            const auto destination_begin = cache_index(0, row);
            std::copy_n(
                source.begin() + static_cast<std::ptrdiff_t>(source_begin),
                kWorldCacheExtent,
                destination.begin() + static_cast<std::ptrdiff_t>(destination_begin));
        }
    }
    return true;
}

std::int16_t WorldCache::at(
    const WorldLayer layer, const int cache_x, const int cache_y) const noexcept {
    if (cache_x < 0 || cache_x >= kWorldCacheExtent || cache_y < 0 ||
        cache_y >= kWorldCacheExtent) {
        return 0;
    }
    return layers_[layer_index(layer)][cache_index(cache_x, cache_y)];
}

std::span<const std::int16_t> WorldCache::layer(const WorldLayer layer) const noexcept {
    return layers_[layer_index(layer)];
}

WorldSession::WorldSession(
    const resource::DataRoot& data_root,
    const WorldMapData& map,
    model::RangerState& ranger,
    random::LegacyRandom& random)
    : map_(map),
      ranger_(ranger),
      random_(random),
      sprites_(resource::PackedArchive::open(
          data_root.path() / "MMAP.IDX", data_root.path() / "MMAP.GRP")),
      weather_sprites_(resource::PackedArchive::open(
          data_root.path() / "CLOUD.IDX", data_root.path() / "CLOUD.GRP")) {
    if (!map_.valid()) {
        error_ = map_.error();
        return;
    }
    if (!sprites_.valid()) {
        error_ = sprites_.error();
        return;
    }
    if (!weather_sprites_.valid()) {
        error_ = weather_sprites_.error();
        return;
    }
    const auto palette_file = data_root.read("MMAP.COL");
    if (!palette_file) {
        error_ = palette_file.error;
        return;
    }
    const auto palette = resource::parse_vga_palette(palette_file.bytes);
    if (!palette) {
        error_ = palette.error;
        return;
    }
    palette_ = palette.palette;
    for (int red = 0; red < 16; ++red) {
        for (int green = 0; green < 16; ++green) {
            for (int blue = 0; blue < 16; ++blue) {
                auto best_distance = 30'000;
                std::uint8_t best_index{};
                for (std::size_t palette_index = 0U; palette_index < palette_.size();
                     ++palette_index) {
                    const auto target_red = red * 4 + 2;
                    const auto target_green = green * 4 + 2;
                    const auto target_blue = blue * 4 + 2;
                    const auto red_delta = target_red - palette_[palette_index].red;
                    const auto green_delta = target_green - palette_[palette_index].green;
                    const auto blue_delta = target_blue - palette_[palette_index].blue;
                    const auto distance = red_delta * red_delta + green_delta * green_delta +
                                          blue_delta * blue_delta;
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = static_cast<std::uint8_t>(palette_index);
                    }
                }
                const auto lookup_index = static_cast<std::size_t>(red * 256 + green * 16 + blue);
                rgb4_lookup_[lookup_index] = best_index;
            }
        }
    }

    world_x_ = ranger_.header.word(model::header_word::main_map_x);
    world_y_ = ranger_.header.word(model::header_word::main_map_y);
    ship_x_ = ranger_.header.word(model::header_word::ship_x);
    ship_y_ = ranger_.header.word(model::header_word::ship_y);
    ship_next_x_ = ranger_.header.word(model::header_word::ship_x_1);
    ship_next_y_ = ranger_.header.word(model::header_word::ship_y_1);
    direction_ = static_cast<WorldDirection>(
        std::clamp<std::int16_t>(ranger_.header.word(model::header_word::face_towards), 0, 3));
    ship_direction_ = static_cast<WorldDirection>(
        std::clamp<std::int16_t>(ranger_.header.word(model::header_word::encode), 0, 3));
    in_ship_ = ranger_.header.word(model::header_word::in_ship) != 0;
    if (!cache_.reload(map_, clamped_origin(world_x_), clamped_origin(world_y_))) {
        error_ = "unable to initialize 128x128 world cache";
    }
}

WorldStepResult WorldSession::move(const WorldDirection direction) {
    if (!valid()) {
        diagnostics::log_error("world move rejected: invalid session");
        return {};
    }
    const auto source_x = world_x_;
    const auto source_y = world_y_;
    if (in_ship_) {
        ship_direction_ = direction;
        ship_frame_offset_ = static_cast<std::int16_t>(ship_frame_offset_ + 2);
        if (ship_frame_offset_ > 6) {
            ship_frame_offset_ = 2;
        }
    } else {
        direction_ = direction;
        idle_animation_ = false;
        idle_animation_counter_ = 0;
        idle_animation_delay_ = 0;
        idle_animation_step_ = 0;
        walk_frame_offset_ = static_cast<std::int16_t>(walk_frame_offset_ + 2);
        if (walk_frame_offset_ > 12) {
            walk_frame_offset_ = 2;
        }
    }

    ++role_recovery_counter_;
    if (role_recovery_counter_ == 50) {
        for (std::size_t index = 0U; index < model::kTeamMemberCount; ++index) {
            const auto role_id = ranger_.header.team_member(index).value;
            if ((index != 0U && role_id <= 0) || role_id < 0 ||
                static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
                continue;
            }
            auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
            const auto bug_compatible_poison = ranger_.roles[index].word(model::role_word::poison);
            if (role.word(model::role_word::hurt) <= 50 && bug_compatible_poison <= 50) {
                continue;
            }
            for (const auto word : {model::role_word::hp, model::role_word::mp,
                                    model::role_word::physical_power}) {
                if (role.word(word) > 1) {
                    role.set_word(word, static_cast<std::int16_t>(role.word(word) - 1));
                }
            }
        }
        role_recovery_counter_ = 0;
    }

    const auto [delta_x, delta_y] = direction_delta(direction);
    const auto target_x = std::clamp(world_x_ + delta_x, 0, kWorldExtent - 1);
    const auto target_y = std::clamp(world_y_ + delta_y, 0, kWorldExtent - 1);
    const auto moved_coordinate = delta_x != 0 ? target_x : target_y;
    const auto cache_target_x = target_x - cache_.origin_x();
    const auto cache_target_y = target_y - cache_.origin_y();
    const auto target_earth = cache_.at(WorldLayer::earth, cache_target_x, cache_target_y);
    const auto target_surface = cache_.at(WorldLayer::surface, cache_target_x, cache_target_y);
    const auto target_building = cache_.at(WorldLayer::building, cache_target_x, cache_target_y);
    const auto target_build_x = cache_.at(WorldLayer::build_x, cache_target_x, cache_target_y);
    const auto target_build_y = cache_.at(WorldLayer::build_y, cache_target_x, cache_target_y);
    if (const auto scene = entrance_at(target_x, target_y); scene.has_value()) {
        diagnostics::log_info(
            "world entrance direction=" + std::string{direction_name(direction)} +
            " from=" + std::to_string(source_x) + "," + std::to_string(source_y) +
            " target=" + std::to_string(target_x) + "," + std::to_string(target_y) +
            " scene=" + std::to_string(*scene) +
            " frame=" + std::to_string(player_frame()));
        commit_header();
        return {WorldStepKind::enter_scene, *scene, static_cast<std::int16_t>(world_x_),
                static_cast<std::int16_t>(world_y_)};
    }

    bool can_move = false;
    if (!in_ship_) {
        const auto boards_ship =
            (target_x == ship_x_ && target_y == ship_y_) ||
            (target_x == ship_next_x_ && target_y == ship_next_y_);
        if (boards_ship) {
            in_ship_ = true;
            ship_direction_ = direction;
            can_move = true;
        } else {
            can_move = target_is_walkable(target_x, target_y, moved_coordinate);
        }
    } else {
        const auto target_clear =
            cache_.at(WorldLayer::building, cache_target_x, cache_target_y) == 0 &&
            cache_.at(WorldLayer::build_x, cache_target_x, cache_target_y) == 0 &&
            cache_.at(WorldLayer::build_y, cache_target_x, cache_target_y) == 0;
        if (target_is_ship_water(target_x, target_y, moved_coordinate)) {
            can_move = true;
        } else if (target_clear && moved_coordinate > 10 && moved_coordinate < 459 &&
                   in_ranges(
                       cache_.at(WorldLayer::earth, cache_target_x, cache_target_y),
                       kLandRanges) &&
                   cache_.at(WorldLayer::surface, cache_target_x, cache_target_y) == 0) {
            in_ship_ = false;
            direction_ = direction;
            idle_animation_ = false;
            walk_frame_offset_ = 0;
            idle_counter_ = 0;
            can_move = true;
        }
    }
    if (!can_move || (target_x == world_x_ && target_y == world_y_)) {
        if (in_ship_ && !can_move) {
            ship_x_ = world_x_;
            ship_y_ = world_y_;
            ship_next_x_ = target_x;
            ship_next_y_ = target_y;
            ship_direction_ = direction;
        }
        diagnostics::log_info(
            "world blocked direction=" + std::string{direction_name(direction)} +
            " from=" + std::to_string(source_x) + "," + std::to_string(source_y) +
            " target=" + std::to_string(target_x) + "," + std::to_string(target_y) +
            " in_ship=" + (in_ship_ ? std::string{"true"} : std::string{"false"}) +
            " frame=" + std::to_string(player_frame()) +
            " earth=" + std::to_string(target_earth) +
            " surface=" + std::to_string(target_surface) +
            " building=" + std::to_string(target_building) +
            " build_x=" + std::to_string(target_build_x) +
            " build_y=" + std::to_string(target_build_y));
        commit_header();
        return {WorldStepKind::stay, -1, static_cast<std::int16_t>(world_x_),
                static_cast<std::int16_t>(world_y_)};
    }

    world_x_ = target_x;
    world_y_ = target_y;
    if (in_ship_) {
        ship_x_ = world_x_;
        ship_y_ = world_y_;
        ship_next_x_ = world_x_ + delta_x;
        ship_next_y_ = world_y_ + delta_y;
        ship_direction_ = direction;
    }
    if (weather_active_) {
        for (auto& particle : weather_) {
            if (delta_y != 0) {
                particle.x = static_cast<std::int16_t>(particle.x + 18 * delta_y);
                particle.y = static_cast<std::int16_t>(particle.y - 9 * delta_y);
            } else {
                particle.x = static_cast<std::int16_t>(particle.x - 18 * delta_x);
                particle.y = static_cast<std::int16_t>(particle.y - 9 * delta_x);
            }
        }
    }
    reload_cache_if_needed(delta_y != 0);
    commit_header();
    diagnostics::log_info(
        "world moved direction=" + std::string{direction_name(direction)} +
        " from=" + std::to_string(source_x) + "," + std::to_string(source_y) +
        " to=" + std::to_string(world_x_) + "," + std::to_string(world_y_) +
        " in_ship=" + (in_ship_ ? std::string{"true"} : std::string{"false"}) +
        " frame=" + std::to_string(player_frame()) +
        " cache_origin=" + std::to_string(cache_.origin_x()) + "," +
        std::to_string(cache_.origin_y()));
    return {WorldStepKind::moved, -1, static_cast<std::int16_t>(world_x_),
            static_cast<std::int16_t>(world_y_)};
}

void WorldSession::idle_tick() {
    if (!valid()) {
        return;
    }
    ++idle_counter_;
    if (!in_ship_ && idle_counter_ > 20) {
        walk_frame_offset_ = 0;
        idle_counter_ = 0;
    }
    if (walk_frame_offset_ != 0) {
        idle_animation_counter_ = 0;
        idle_animation_delay_ = 0;
    } else {
        ++idle_animation_counter_;
    }
    if (idle_animation_counter_ > 50 && !idle_animation_) {
        if (random_.bounded(10) == 0) {
            idle_animation_ = true;
            walk_frame_offset_ = 0;
            idle_counter_ = 0;
            idle_animation_step_ = static_cast<std::int16_t>(idle_animation_step_ + 2);
        }
    }
    if (idle_animation_) {
        ++idle_animation_delay_;
        if (idle_animation_delay_ > 3) {
            idle_animation_delay_ = 0;
            idle_animation_step_ = static_cast<std::int16_t>(idle_animation_step_ + 2);
        }
        if (idle_animation_step_ > 12) {
            idle_animation_ = false;
            idle_animation_counter_ = 0;
            idle_animation_delay_ = 0;
            idle_animation_step_ = 0;
            walk_frame_offset_ = 0;
        }
    }
    ++physical_power_counter_;
    if (physical_power_counter_ == 200) {
        for (std::size_t index = 0U; index < model::kTeamMemberCount; ++index) {
            const auto role_id = ranger_.header.team_member(index).value;
            if ((index != 0U && role_id <= 0) || role_id < 0 ||
                static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
                continue;
            }
            auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
            if (role.word(model::role_word::physical_power) < 100) {
                role.set_word(
                    model::role_word::physical_power,
                    static_cast<std::int16_t>(
                        role.word(model::role_word::physical_power) + 1));
            }
        }
        physical_power_counter_ = 0;
    }
}

void WorldSession::periodic_tick() {
    if (valid()) {
        update_weather();
    }
}

bool WorldSession::render(render::IndexedFramebuffer& framebuffer) const {
    if (!valid()) {
        return false;
    }
    framebuffer.clear(0U);
    framebuffer.set_palette(palette_);
    const auto view_x = cache_x();
    const auto view_y = cache_y();
    for (int cache_tile_x = view_x - 11; cache_tile_x < view_x + 21; ++cache_tile_x) {
        for (int cache_tile_y = view_y - 11; cache_tile_y < view_y + 21; ++cache_tile_y) {
            const auto point = render::legacy_world_tile_screen(
                cache_tile_x, cache_tile_y, view_x, view_y);
            if (!draw_sprite(
                    framebuffer,
                    cache_.at(WorldLayer::earth, cache_tile_x, cache_tile_y),
                    point.x,
                    point.y)) {
                return false;
            }
        }
    }
    for (int cache_tile_x = view_x - 11; cache_tile_x < view_x + 21; ++cache_tile_x) {
        for (int cache_tile_y = view_y - 11; cache_tile_y < view_y + 21; ++cache_tile_y) {
            const auto sprite = cache_.at(WorldLayer::surface, cache_tile_x, cache_tile_y);
            if (sprite == 0) {
                continue;
            }
            const auto point = render::legacy_world_tile_screen(
                cache_tile_x, cache_tile_y, view_x, view_y);
            if (!draw_sprite(framebuffer, sprite, point.x, point.y)) {
                return false;
            }
        }
    }

    const render::LegacyWorldDepthInput input{
        cache_.layer(WorldLayer::build_x),
        cache_.layer(WorldLayer::build_y),
        cache_.layer(WorldLayer::building),
        view_x,
        view_y,
        cache_.origin_x(),
        cache_.origin_y(),
        {static_cast<std::int16_t>(world_x_), static_cast<std::int16_t>(world_y_), view_x,
         view_y, 5000},
        render::LegacyDepthActor{
            static_cast<std::int16_t>(ship_x_),
            static_cast<std::int16_t>(ship_y_),
            ship_x_ - cache_.origin_x(),
            ship_y_ - cache_.origin_y(),
            6000}};
    const auto depth = render::build_legacy_world_depth_list(input);
    if (!depth) {
        diagnostics::log_error(
            "world render depth list failed x=" + std::to_string(world_x_) +
            " y=" + std::to_string(world_y_));
        return false;
    }
    bool player_drawn = false;
    for (const auto& entry : depth.entries) {
        if (entry.sprite_id == 5000) {
            const auto sprite = in_ship_
                                    ? static_cast<std::int16_t>(
                                          kShipFrameBase[static_cast<std::size_t>(ship_direction_)] +
                                          ship_frame_offset_)
                                    : player_frame();
            if (!draw_sprite(framebuffer, sprite, 145, 117)) {
                diagnostics::log_error(
                    "world player sprite draw failed sprite=" + std::to_string(sprite) +
                    " x=" + std::to_string(world_x_) +
                    " y=" + std::to_string(world_y_));
                return false;
            }
            player_drawn = true;
            continue;
        }
        const auto relative_x = static_cast<int>(entry.world_x) - cache_.origin_x() -
                                (view_x - 10);
        const auto relative_y = static_cast<int>(entry.world_y) - cache_.origin_y() -
                                (view_y - 10);
        const auto point = render::project_isometric(relative_x, relative_y, 145, -63);
        const auto sprite = entry.sprite_id == 6000
                                ? static_cast<std::int16_t>(
                                      kShipFrameBase[static_cast<std::size_t>(ship_direction_)] +
                                      ship_frame_offset_)
                                : entry.sprite_id;
        if (sprite != 0 && !draw_sprite(framebuffer, sprite, point.x, point.y)) {
            return false;
        }
    }
    if (weather_active_) {
        for (const auto& particle : weather_) {
            if (particle.y > -1000 && !draw_weather_particle(framebuffer, particle)) {
                return false;
            }
        }
    }
    const auto frame = in_ship_
                           ? static_cast<std::int16_t>(
                                 kShipFrameBase[static_cast<std::size_t>(ship_direction_)] +
                                 ship_frame_offset_)
                           : player_frame();
    if (!player_drawn) {
        diagnostics::log_warning(
            "world player missing from depth output x=" + std::to_string(world_x_) +
            " y=" + std::to_string(world_y_) +
            " frame=" + std::to_string(frame) +
            " entries=" + std::to_string(depth.entries.size()) +
            " cache_origin=" + std::to_string(cache_.origin_x()) + "," +
            std::to_string(cache_.origin_y()));
    } else {
        diagnostics::log_trace(
            "world player rendered x=" + std::to_string(world_x_) +
            " y=" + std::to_string(world_y_) +
            " frame=" + std::to_string(frame) +
            " entries=" + std::to_string(depth.entries.size()));
    }
    return true;
}

std::int16_t WorldSession::player_frame() const noexcept {
    const auto direction_index = static_cast<std::size_t>(direction_);
    if (idle_animation_) {
        return static_cast<std::int16_t>(
            kPlayerFrameBase[direction_index] + kIdleFrameOffset[direction_index] +
            idle_animation_step_);
    }
    return static_cast<std::int16_t>(kPlayerFrameBase[direction_index] + walk_frame_offset_);
}

bool WorldSession::target_is_walkable(
    const int world_x, const int world_y, const int moved_coordinate) const noexcept {
    const auto target_x = world_x - cache_.origin_x();
    const auto target_y = world_y - cache_.origin_y();
    if (moved_coordinate <= 10 || moved_coordinate >= 459 ||
        cache_.at(WorldLayer::building, target_x, target_y) != 0 ||
        cache_.at(WorldLayer::build_x, target_x, target_y) != 0 ||
        cache_.at(WorldLayer::build_y, target_x, target_y) != 0) {
        return false;
    }
    const auto earth = cache_.at(WorldLayer::earth, target_x, target_y);
    return !in_ranges(earth, kBlockedWalkingRanges) &&
           !in_ranges(earth, kBlockedWalkingRangesTail);
}

bool WorldSession::target_is_ship_water(
    const int world_x, const int world_y, const int moved_coordinate) const noexcept {
    const auto target_x = world_x - cache_.origin_x();
    const auto target_y = world_y - cache_.origin_y();
    const auto current_x = world_x_ - cache_.origin_x();
    const auto current_y = world_y_ - cache_.origin_y();
    const auto one_building_cell_is_empty =
        cache_.at(WorldLayer::building, current_x, current_y) == 0 ||
        cache_.at(WorldLayer::building, target_x, target_y) == 0;
    const auto one_owner_cell_is_empty =
        (cache_.at(WorldLayer::build_x, current_x, current_y) == 0 &&
         cache_.at(WorldLayer::build_y, current_x, current_y) == 0) ||
        (cache_.at(WorldLayer::build_x, target_x, target_y) == 0 &&
         cache_.at(WorldLayer::build_y, target_x, target_y) == 0);
    if (moved_coordinate <= 10 || moved_coordinate >= 459 ||
        !one_building_cell_is_empty || !one_owner_cell_is_empty) {
        return false;
    }
    return in_ranges(cache_.at(WorldLayer::earth, current_x, current_y), kShipCoastRanges) ||
           in_ranges(cache_.at(WorldLayer::earth, target_x, target_y), kShipCoastRanges);
}

std::optional<std::int16_t> WorldSession::entrance_at(
    const int world_x, const int world_y) const noexcept {
    const auto count = std::min<std::size_t>(84U, ranger_.scenes.size());
    for (std::size_t index = 0U; index < count; ++index) {
        const auto& scene = ranger_.scenes[index];
        const auto at_first = scene.word(model::scene_metadata_word::main_entrance_x_1) == world_x &&
                              scene.word(model::scene_metadata_word::main_entrance_y_1) == world_y;
        const auto at_second = scene.word(model::scene_metadata_word::main_entrance_x_2) == world_x &&
                               scene.word(model::scene_metadata_word::main_entrance_y_2) == world_y;
        if (!at_first && !at_second) {
            continue;
        }
        const auto condition = scene.word(model::scene_metadata_word::entrance_condition);
        if (condition == 0) {
            return static_cast<std::int16_t>(index);
        }
        if (condition == 2) {
            for (std::size_t party = 0U; party < model::kTeamMemberCount; ++party) {
                const auto role_id = ranger_.header.team_member(party).value;
                if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
                    continue;
                }
                if (ranger_.roles[static_cast<std::size_t>(role_id)].word(model::role_word::iq) >=
                    70) {
                    return static_cast<std::int16_t>(index);
                }
            }
        }
    }
    return std::nullopt;
}

void WorldSession::reload_cache_if_needed(const bool vertical_move) {
    const auto local_x = cache_x();
    const auto local_y = cache_y();
    const auto desired_x = clamped_origin(world_x_);
    const auto desired_y = clamped_origin(world_y_);
    const auto crossed = vertical_move
                             ? ((local_y > 98 || local_y < 11) &&
                                desired_y != cache_.origin_y())
                             : ((local_x > 98 || local_x < 11) &&
                                desired_x != cache_.origin_x());
    if (crossed) {
        static_cast<void>(cache_.reload(map_, desired_x, desired_y));
    }
}

void WorldSession::commit_header() noexcept {
    ranger_.header.set_word(model::header_word::in_ship, in_ship_ ? 1 : 0);
    ranger_.header.set_word(model::header_word::main_map_x, static_cast<std::int16_t>(world_x_));
    ranger_.header.set_word(model::header_word::main_map_y, static_cast<std::int16_t>(world_y_));
    ranger_.header.set_word(
        model::header_word::face_towards, static_cast<std::int16_t>(direction_));
    ranger_.header.set_word(model::header_word::ship_x, static_cast<std::int16_t>(ship_x_));
    ranger_.header.set_word(model::header_word::ship_y, static_cast<std::int16_t>(ship_y_));
    ranger_.header.set_word(model::header_word::ship_x_1, static_cast<std::int16_t>(ship_next_x_));
    ranger_.header.set_word(model::header_word::ship_y_1, static_cast<std::int16_t>(ship_next_y_));
    ranger_.header.set_word(
        model::header_word::encode, static_cast<std::int16_t>(ship_direction_));
}

void WorldSession::update_weather() {
    if (weather_active_) {
        bool all_done = true;
        for (auto& particle : weather_) {
            particle.x = static_cast<std::int16_t>(particle.x + 1);
            if (particle.x <= 500) {
                all_done = false;
            }
        }
        if (!all_done) {
            return;
        }
        weather_active_ = false;
    }
    if (random_.bounded(1) != 0) {
        return;
    }
    weather_active_ = true;
    for (auto& particle : weather_) {
        particle.kind = static_cast<std::int16_t>(random_.bounded(4));
    }
    for (auto& particle : weather_) {
        particle.speed = static_cast<std::int16_t>(random_.bounded(3) + 6);
    }
    for (auto& particle : weather_) {
        particle.x = static_cast<std::int16_t>(random_.bounded(100) - 300);
    }
    for (std::size_t index = 0U; index < weather_.size(); ++index) {
        if (random_.bounded(2) != 0) {
            weather_[index].y = static_cast<std::int16_t>(
                -3000 - 1000 * static_cast<std::int32_t>(index));
        } else {
            weather_[index].y = static_cast<std::int16_t>(
                random_.bounded(50) + static_cast<std::int32_t>(index) * 75);
        }
    }
}

bool WorldSession::draw_sprite(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t legacy_id,
    const int anchor_x,
    const int anchor_y) const {
    if (legacy_id < 0) {
        return false;
    }
    const auto index = render::legacy_sprite_index(static_cast<std::uint16_t>(legacy_id));
    if (!index.has_value() || *index >= sprites_.entry_count()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(sprites_.entry(*index));
    if (!frame.valid()) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, frame, anchor_x, anchor_y);
    return true;
}

bool WorldSession::draw_weather_particle(
    render::IndexedFramebuffer& framebuffer, const WeatherParticle& particle) const {
    if (particle.kind < 0 || static_cast<std::size_t>(particle.kind) >= weather_sprites_.entry_count() ||
        particle.speed < 0 || particle.speed > 8) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(
        weather_sprites_.entry(static_cast<std::size_t>(particle.kind)));
    if (!frame.valid()) {
        return false;
    }
    const auto source_weight = static_cast<int>(particle.speed);
    const auto destination_weight = 8 - source_weight;
    const auto left = static_cast<int>(particle.x) - static_cast<int>(frame.x_offset());
    const auto top = static_cast<int>(particle.y) - static_cast<int>(frame.y_offset());
    for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
        const auto destination_y = top + static_cast<int>(row_index);
        auto destination_x = left;
        for (const auto& run : frame.rows()[row_index].runs) {
            destination_x += static_cast<int>(run.skip);
            for (const auto source_index : run.pixels) {
                if (destination_y >= 0 && destination_y < render::IndexedFramebuffer::height &&
                    destination_x >= 0 && destination_x < render::IndexedFramebuffer::width) {
                    auto& destination_index = framebuffer.row(destination_y)[destination_x];
                    const auto source = palette_[source_index];
                    const auto destination = palette_[destination_index];
                    const auto red = (static_cast<int>(source.red) * source_weight) / 32 +
                                     (static_cast<int>(destination.red) * destination_weight) / 32;
                    const auto green = (static_cast<int>(source.green) * source_weight) / 32 +
                                       (static_cast<int>(destination.green) * destination_weight) / 32;
                    const auto blue = (static_cast<int>(source.blue) * source_weight) / 32 +
                                      (static_cast<int>(destination.blue) * destination_weight) / 32;
                    const auto lookup_index = static_cast<std::size_t>(red * 256 + green * 16 + blue);
                    destination_index = rgb4_lookup_[lookup_index];
                }
                ++destination_x;
            }
        }
    }
    return true;
}

}  // namespace openlegend::world
