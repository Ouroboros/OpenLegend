#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/resource/packed_archive.hpp"

namespace openlegend::world {

inline constexpr int kWorldExtent = 480;
inline constexpr int kWorldCacheExtent = 128;
inline constexpr int kWorldCacheMaximumOrigin = kWorldExtent - kWorldCacheExtent;
inline constexpr std::size_t kWorldCellCount =
    static_cast<std::size_t>(kWorldExtent) * static_cast<std::size_t>(kWorldExtent);
inline constexpr std::size_t kWorldLayerBytes = kWorldCellCount * 2U;
inline constexpr std::size_t kWorldCacheCellCount =
    static_cast<std::size_t>(kWorldCacheExtent) * static_cast<std::size_t>(kWorldCacheExtent);

enum class WorldLayer : std::size_t {
    earth,
    surface,
    building,
    build_x,
    build_y,
    count,
};

class WorldMapData {
public:
    explicit WorldMapData(const resource::DataRoot& data_root);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD std::int16_t at(WorldLayer layer, int x, int y) const noexcept;

    NODISCARD std::span<const std::int16_t> layer(WorldLayer layer) const noexcept;

private:
    std::array<std::vector<std::int16_t>, static_cast<std::size_t>(WorldLayer::count)> layers_;
    std::string error_;
};

class WorldCache {
public:
    WorldCache();

    NODISCARD bool reload(const WorldMapData& map, int origin_x, int origin_y) noexcept;

    NODISCARD std::int16_t at(WorldLayer layer, int cache_x, int cache_y) const noexcept;

    NODISCARD std::span<const std::int16_t> layer(WorldLayer layer) const noexcept;

    NODISCARD int origin_x() const noexcept { return origin_x_; }

    NODISCARD int origin_y() const noexcept { return origin_y_; }

private:
    std::array<std::vector<std::int16_t>, static_cast<std::size_t>(WorldLayer::count)> layers_;
    int origin_x_{};
    int origin_y_{};
};

enum class WorldDirection : std::uint8_t {
    up = 0U,
    right = 1U,
    left = 2U,
    down = 3U,
};

enum class WorldStepKind {
    stay,
    moved,
    enter_scene,
    open_ui,
};

struct WorldMoveContinuation {
    WorldDirection direction{WorldDirection::up};
    std::int16_t target_x{};
    std::int16_t target_y{};

    friend bool operator==(const WorldMoveContinuation&, const WorldMoveContinuation&) = default;
};

struct WorldStepResult {
    WorldStepKind kind{WorldStepKind::stay};
    std::int16_t scene_id{-1};
    std::int16_t world_x{};
    std::int16_t world_y{};
    std::optional<WorldMoveContinuation> continuation;

    friend bool operator==(const WorldStepResult&, const WorldStepResult&) = default;
};

class WorldSession {
public:
    WorldSession(
        const resource::DataRoot& data_root,
        const WorldMapData& map,
        model::RangerState& ranger,
        random::LegacyRandom& random);

    WorldSession(
        const resource::DataRoot& data_root,
        const WorldMapData& map,
        model::RangerState& ranger,
        random::LegacyRandom& random,
        const resource::PackedArchive& startup_weather_sprites,
        const compat::LegacyPalette& startup_palette);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD WorldStepResult move(WorldDirection direction);

    void restore_direction_after_scene(WorldDirection direction) noexcept;

    NODISCARD WorldStepResult resume_move_after_scene(
        const WorldMoveContinuation& continuation);

    void sync_persistent_state(bool include_direction) noexcept;

    void idle_tick();

    void periodic_tick();

    void idle_animation_tick();

    void cycle_palette();

    void prepare_game_menu_frame() noexcept;

    NODISCARD bool render(render::IndexedFramebuffer& framebuffer) const;

    NODISCARD std::int16_t physical_power_counter() const noexcept {
        return physical_power_counter_;
    }

    void set_physical_power_counter(const std::int16_t counter) noexcept {
        physical_power_counter_ = counter;
    }

    NODISCARD int world_x() const noexcept { return world_x_; }

    NODISCARD int world_y() const noexcept { return world_y_; }

    NODISCARD int cache_x() const noexcept { return world_x_ - cache_.origin_x(); }

    NODISCARD int cache_y() const noexcept { return world_y_ - cache_.origin_y(); }

    NODISCARD WorldDirection direction() const noexcept { return direction_; }

    NODISCARD std::int16_t player_frame() const noexcept;

    NODISCARD std::optional<std::int16_t> rendered_player_frame() const noexcept;

    NODISCARD const WorldCache& cache() const noexcept { return cache_; }

private:
    friend struct WorldSessionTestAccess;

    WorldSession(
        const resource::DataRoot& data_root,
        const WorldMapData& map,
        model::RangerState& ranger,
        random::LegacyRandom& random,
        const resource::PackedArchive* startup_weather_sprites,
        const compat::LegacyPalette* startup_palette);

    struct WeatherParticle {
        std::int16_t x{};
        std::int16_t y{};
        std::int16_t speed{};
        std::int16_t kind{};
    };

    NODISCARD bool target_is_walkable(
        int world_x, int world_y, int moved_coordinate) const noexcept;

    NODISCARD bool target_is_ship_water(
        int world_x,
        int world_y,
        int moved_coordinate,
        int delta_x,
        int delta_y) const noexcept;

    NODISCARD std::optional<std::int16_t> entrance_at(int world_x, int world_y) const noexcept;

    NODISCARD WorldStepResult complete_move(
        WorldDirection direction, int target_x, int target_y);

    void reload_cache_if_needed(bool vertical_move);

    void update_weather();

    NODISCARD bool draw_sprite(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t legacy_id,
        int anchor_x,
        int anchor_y) const;

    NODISCARD bool draw_weather_particle(
        render::IndexedFramebuffer& framebuffer,
        const WeatherParticle& particle) const;

    const WorldMapData& map_;
    model::RangerState& ranger_;
    random::LegacyRandom& random_;
    WorldCache cache_;
    mutable std::optional<WorldCache> expanded_render_cache_;
    // Cached pixel spans stay valid even when a WorldSession is copied.
    std::shared_ptr<const resource::PackedArchive> sprites_;
    mutable std::vector<std::optional<resource::SpriteFrameView>> sprite_frames_;
    resource::PackedArchive weather_sprites_;
    compat::LegacyPalette palette_{};
    std::array<std::uint8_t, 4096> rgb4_lookup_{};
    int world_x_{};
    int world_y_{};
    int ship_x_{};
    int ship_y_{};
    int ship_next_x_{};
    int ship_next_y_{};
    WorldDirection direction_{WorldDirection::up};
    WorldDirection ship_direction_{WorldDirection::up};
    std::int16_t walk_frame_offset_{};
    std::int16_t ship_frame_offset_{};
    std::int16_t idle_counter_{};
    std::int16_t idle_animation_counter_{};
    std::int16_t idle_animation_delay_{};
    std::int16_t idle_animation_step_{};
    std::optional<std::int16_t> player_frame_override_;
    std::int16_t physical_power_counter_{};
    std::int32_t role_recovery_counter_{};
    bool idle_animation_{};
    std::int16_t in_ship_{};
    bool weather_active_{};
    std::array<WeatherParticle, 3> weather_{};
    std::string error_;
};

}  // namespace openlegend::world
