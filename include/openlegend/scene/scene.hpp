#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/packed_archive.hpp"

namespace openlegend::scene {

inline constexpr int kSceneExtent = 64;
inline constexpr int kSceneViewExtent = 28;
inline constexpr int kSceneMaximumViewOrigin = 36;
inline constexpr std::size_t kTalkCount = 2'977U;
inline constexpr std::size_t kEventScriptCount = 1'018U;

[[nodiscard]] std::vector<std::vector<std::uint8_t>> paginate_dialogue(
    std::span<const std::uint8_t> zero_terminated_text);

class SceneAssets {
public:
    explicit SceneAssets(const resource::DataRoot& data_root);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t talk_count() const noexcept { return talks_.entry_count(); }
    [[nodiscard]] std::size_t script_count() const noexcept { return scripts_.entry_count(); }
    [[nodiscard]] std::vector<std::uint8_t> talk(std::size_t talk_id) const;
    [[nodiscard]] std::vector<std::int16_t> script(std::size_t script_id) const;

private:
    resource::PackedArchive talks_;
    resource::PackedArchive scripts_;
    std::string error_;
};

enum class SceneDirection : std::uint8_t {
    up = 0U,
    right = 1U,
    left = 2U,
    down = 3U,
};

enum class SceneStepKind {
    stay,
    moved,
    present,
    fade_from_black,
    fade_to_black,
    scene_title,
    dialogue,
    notice,
    question,
    battle,
    shop,
    open_ui,
    return_world,
    quit,
};

enum class SceneQuestion {
    none,
    battle,
    join,
    rest,
};

enum class SceneResponse {
    acknowledge,
    yes,
    no,
    battle_victory,
    battle_defeat,
    cancel,
};

struct SceneStepResult {
    SceneStepKind kind{SceneStepKind::stay};
    std::int16_t scene_id{-1};
    std::int16_t scene_x{};
    std::int16_t scene_y{};
    std::int16_t talk_id{-1};
    std::int16_t head_id{-1};
    std::int16_t style{};
    std::int16_t battle_id{-1};
    std::int16_t shop_id{-1};
    std::uint16_t wait_ticks{1U};
    SceneQuestion question{SceneQuestion::none};

    friend bool operator==(const SceneStepResult&, const SceneStepResult&) = default;
};

struct SceneAudioCommand {
    enum class Kind { music, wave } kind{Kind::music};
    std::int16_t id{};

    friend bool operator==(const SceneAudioCommand&, const SceneAudioCommand&) = default;
};

class SceneSession {
public:
    SceneSession(
        const resource::DataRoot& data_root,
        model::GameSnapshot& snapshot,
        random::LegacyRandom& random,
        std::int16_t scene_id,
        bool use_jump_entrance = false);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] SceneStepResult move(SceneDirection direction);
    [[nodiscard]] SceneStepResult interact();
    [[nodiscard]] SceneStepResult use_item(std::int16_t item_id);
    [[nodiscard]] SceneStepResult open_ui() const noexcept;
    [[nodiscard]] SceneStepResult resume(SceneResponse response, int value = -1);
    [[nodiscard]] SceneStepResult begin_event(
        std::int16_t script_id,
        std::int16_t event_index = -1,
        std::int16_t event_x = -1,
        std::int16_t event_y = -1,
        std::int16_t item_id = -1);
    void idle_tick();
    void periodic_tick();
    [[nodiscard]] bool render(render::IndexedFramebuffer& framebuffer) const;
    [[nodiscard]] bool render_map(render::IndexedFramebuffer& framebuffer) const;

    [[nodiscard]] std::int16_t scene_id() const noexcept { return scene_id_; }
    [[nodiscard]] int scene_x() const noexcept { return scene_x_; }
    [[nodiscard]] int scene_y() const noexcept { return scene_y_; }
    [[nodiscard]] int view_origin_x() const noexcept { return view_origin_x_; }
    [[nodiscard]] int view_origin_y() const noexcept { return view_origin_y_; }
    [[nodiscard]] SceneDirection direction() const noexcept { return direction_; }
    [[nodiscard]] bool weather_enabled() const noexcept { return weather_enabled_; }
    [[nodiscard]] std::int16_t player_frame() const noexcept;
    [[nodiscard]] const SceneStepResult& pending() const noexcept { return pending_; }
    [[nodiscard]] std::span<const std::uint8_t> pending_text() const noexcept {
        return pending_text_;
    }
    [[nodiscard]] std::vector<SceneAudioCommand> take_audio_commands();

private:
    enum class PendingContinuation {
        none,
        conditional,
        battle,
        shop,
    };

    struct WeatherParticle {
        std::int16_t x{};
        std::int16_t y{};
        std::int16_t speed{};
        std::int16_t kind{};
    };

    struct EventContext {
        std::int16_t event_index{-1};
        std::int16_t x{-1};
        std::int16_t y{-1};
        std::int16_t item_id{-1};
    };

    struct QueuedOutput {
        SceneStepResult result;
        std::vector<std::uint8_t> text;
    };

    struct PanState {
        int x{};
        int y{};
        int target_x{};
        int target_y{};
        int step_x{1};
        int step_y{1};
    };

    struct PictureAnimationState {
        std::int16_t event_index{-1};
        int frame{};
        int end_frame{};
    };

    struct ScriptedWalkState {
        int x{};
        int y{};
        int target_x{};
        int target_y{};
        int step_x{1};
        int step_y{1};
    };

    [[nodiscard]] SceneStepResult current_result(SceneStepKind kind) const noexcept;
    [[nodiscard]] SceneStepResult run_event();
    [[nodiscard]] SceneStepResult run_auto_event(SceneStepKind fallback);
    [[nodiscard]] bool load_scene_sprites();
    [[nodiscard]] bool draw_sprite(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t legacy_id,
        int anchor_x,
        int anchor_y) const;
    [[nodiscard]] bool draw_overlay(render::IndexedFramebuffer& framebuffer) const;
    void update_weather();
    [[nodiscard]] bool draw_weather_particle(
        render::IndexedFramebuffer& framebuffer,
        const WeatherParticle& particle) const;
    [[nodiscard]] bool target_is_walkable(int x, int y) const noexcept;
    [[nodiscard]] std::optional<std::int16_t> event_at(int x, int y) const noexcept;
    [[nodiscard]] std::optional<std::int16_t> event_field(
        std::int16_t scene_id,
        std::int16_t event_index,
        model::SceneEventField field) const noexcept;
    void set_event_field(
        std::int16_t scene_id,
        std::int16_t event_index,
        model::SceneEventField field,
        std::int16_t value) noexcept;
    void modify_event(std::span<const std::int16_t, 13> arguments);
    void set_scene_value(
        std::int16_t scene_id,
        std::int16_t layer,
        std::int16_t x,
        std::int16_t y,
        std::int16_t value) noexcept;
    [[nodiscard]] std::int16_t scene_value(
        std::int16_t scene_id,
        std::int16_t layer,
        std::int16_t x,
        std::int16_t y) const noexcept;
    [[nodiscard]] bool party_contains(std::int16_t role_id) const noexcept;
    [[nodiscard]] int inventory_count(std::int16_t item_id) const noexcept;
    void add_inventory(std::int16_t item_id, std::int16_t count);
    void update_book_event_if_ready();
    void clear_role_personal_items(std::int16_t role_id);
    void add_role_item(std::int16_t role_id, std::int16_t item_id, std::int16_t count);
    void queue_dialogue(std::int16_t talk_id, std::int16_t head_id, std::int16_t style);
    void queue_notice(std::vector<std::uint8_t> text);
    [[nodiscard]] SceneStepResult emit_queued();
    [[nodiscard]] std::optional<SceneStepResult> advance_pan_frame();
    [[nodiscard]] std::optional<SceneStepResult> advance_picture_animation_frame();
    [[nodiscard]] std::optional<SceneStepResult> advance_scripted_walk_frame();
    void apply_scripted_walk_step(bool horizontal, int step);
    void commit_header() noexcept;
    void update_view_origin() noexcept;
    void clear_event() noexcept;

    const resource::DataRoot& data_root_;
    model::GameSnapshot& snapshot_;
    random::LegacyRandom& random_;
    SceneAssets assets_;
    resource::SentinelArchive sprites_;
    resource::PackedArchive weather_sprites_;
    compat::LegacyPalette palette_{};
    std::array<std::uint8_t, 4096> rgb4_lookup_{};
    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::int16_t scene_id_{-1};
    int scene_x_{};
    int scene_y_{};
    int view_origin_x_{};
    int view_origin_y_{};
    SceneDirection direction_{SceneDirection::up};
    std::int16_t walk_frame_offset_{};
    std::int16_t animation_counter_{};
    bool weather_enabled_{};
    bool weather_active_{};
    std::array<WeatherParticle, 3> weather_{};
    EventContext event_context_{};
    std::vector<std::int16_t> script_;
    std::ptrdiff_t program_counter_{};
    bool event_active_{};
    SceneStepResult pending_{};
    std::vector<std::uint8_t> pending_text_;
    PendingContinuation continuation_{PendingContinuation::none};
    std::int16_t true_offset_{};
    std::int16_t false_offset_{};
    std::int16_t battle_get_exp_{};
    std::optional<std::int16_t> player_frame_override_;
    std::optional<PanState> pan_state_;
    std::optional<PictureAnimationState> picture_animation_state_;
    std::optional<ScriptedWalkState> scripted_walk_state_;
    std::deque<QueuedOutput> queued_outputs_;
    std::vector<SceneAudioCommand> audio_commands_;
    std::string error_;
};

}  // namespace openlegend::scene
