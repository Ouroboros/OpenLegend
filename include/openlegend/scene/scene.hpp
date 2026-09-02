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

struct SceneDate {
    int year{};
    int month{};
    int day{};

    friend bool operator==(const SceneDate&, const SceneDate&) = default;
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
    wait_key,
    load_menu,
    death_menu,
    load_slot,
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
    std::int16_t battle_get_exp{};
    std::int16_t shop_id{-1};
    std::int16_t menu_index{-1};
    std::int16_t save_slot{-1};
    std::uint16_t wait_ticks{1U};
    SceneQuestion question{SceneQuestion::none};
    bool death_confirm{};
    bool ending_complete{};

    friend bool operator==(const SceneStepResult&, const SceneStepResult&) = default;
};

struct SceneAudioCommand {
    enum class Kind { music, wave } kind{Kind::music};
    std::int16_t id{};
    bool force{};

    friend bool operator==(const SceneAudioCommand&, const SceneAudioCommand&) = default;
};

struct SceneEntryOverride {
    std::int16_t x{};
    std::int16_t y{};
    SceneDirection direction{SceneDirection::up};
    std::int16_t player_frame{-1};
    std::int16_t script_id{-1};

    friend bool operator==(const SceneEntryOverride&, const SceneEntryOverride&) = default;
};

enum class SceneSessionContext {
    scene,
    world_event_overlay,
    retained_scene_event,
};

class SceneSession {
public:
    SceneSession(
        const resource::DataRoot& data_root,
        model::GameSnapshot& snapshot,
        random::LegacyRandom& random,
        std::int16_t scene_id,
        bool use_jump_entrance = false,
        std::optional<SceneDate> death_date_override = std::nullopt,
        std::int16_t periodic_counter = 0,
        std::optional<SceneEntryOverride> entry_override = std::nullopt,
        SceneSessionContext context = SceneSessionContext::scene,
        std::span<const std::uint16_t> fixed_shadow_mask = {},
        std::span<const std::uint16_t> shifted_shadow_mask = {});

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] SceneStepResult tick(
        std::optional<SceneDirection> direction,
        bool interact_requested,
        bool ui_requested,
        bool skip_player_idle = false);
    [[nodiscard]] SceneStepResult move(SceneDirection direction);
    [[nodiscard]] SceneStepResult interact();
    [[nodiscard]] SceneStepResult use_item(std::int16_t item_id);
    [[nodiscard]] SceneStepResult use_menu_item(std::int16_t item_id);
    [[nodiscard]] SceneStepResult use_retained_menu_item(std::int16_t item_id);
    [[nodiscard]] SceneStepResult open_ui() noexcept;
    [[nodiscard]] SceneStepResult resume(SceneResponse response, int value = -1);
    [[nodiscard]] SceneStepResult begin_event(
        std::int16_t script_id,
        std::int16_t event_index = -1,
        std::int16_t event_x = -1,
        std::int16_t event_y = -1,
        std::int16_t item_id = -1);
    void idle_tick();
    [[nodiscard]] bool render(render::IndexedFramebuffer& framebuffer) const;
    [[nodiscard]] bool render_map(render::IndexedFramebuffer& framebuffer) const;
    [[nodiscard]] bool render_overlay(render::IndexedFramebuffer& framebuffer) const;

    [[nodiscard]] std::int16_t scene_id() const noexcept { return scene_id_; }
    [[nodiscard]] int scene_x() const noexcept { return scene_x_; }
    [[nodiscard]] int scene_y() const noexcept { return scene_y_; }
    [[nodiscard]] int view_origin_x() const noexcept { return view_origin_x_; }
    [[nodiscard]] int view_origin_y() const noexcept { return view_origin_y_; }
    [[nodiscard]] SceneDirection direction() const noexcept { return direction_; }
    [[nodiscard]] bool weather_enabled() const noexcept { return shadow_state_ > 0; }
    [[nodiscard]] std::int16_t periodic_counter() const noexcept { return periodic_counter_; }
    [[nodiscard]] std::int16_t physical_power_counter() const noexcept {
        return physical_power_counter_;
    }
    void set_physical_power_counter(const std::int16_t counter) noexcept {
        physical_power_counter_ = counter;
    }
    [[nodiscard]] std::int16_t player_frame() const noexcept;
    [[nodiscard]] const SceneStepResult& pending() const noexcept { return pending_; }
    [[nodiscard]] bool exit_transition_pending() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> pending_text() const noexcept {
        return pending_text_;
    }
    [[nodiscard]] std::vector<SceneAudioCommand> take_audio_commands();

private:
    enum class PendingContinuation {
        none,
        conditional,
        conditional_after_present,
        battle,
        shop,
        shop_feedback,
        scene_entry,
        scene_jump,
        scene_exit,
        scene_title,
    };

    enum class TickContinuation {
        none,
        after_action,
        after_scene_present,
        after_auto_event,
    };

    struct EventContext {
        std::int16_t event_index{-1};
        std::int16_t x{-1};
        std::int16_t y{-1};
        std::int16_t item_id{-1};
    };

    struct PendingJump {
        std::int16_t scene_id{-1};
        bool use_jump_entrance{};
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

    struct DualPictureAnimationState {
        std::int16_t first_event{-1};
        int first_picture{};
        int first_end_picture{};
        std::int16_t second_event{-1};
        int second_picture{};
        bool skip_negative_events{};
        bool quit_after{};
    };

    struct ThreeStatueAnimationState {
        int phase{};
        int value{7664};
    };

    struct LoadMenuState {
        enum class Phase { fade_in, menu, load_slot_fade, scene_exit_fade, confirm }
            phase{Phase::fade_in};
        std::int16_t selection{};
        std::int16_t selected_slot{-1};
    };

    struct DeathMenuState {
        enum class Phase { fade_in, menu, load_slot_clear, confirm } phase{Phase::fade_in};
        std::int16_t selection{};
        std::int16_t selected_slot{-1};
        int year{};
        int month{};
        int day{};
    };

    struct EndingState {
        enum class Phase {
            title_draw,
            title_fade_out,
            word_scroll_setup,
            word_scroll,
            kend_setup,
            kend_frames,
            kend_fade_out,
            credits_setup,
            credits_scroll,
            credits_fade_out,
            finish,
        };
        Phase phase{Phase::title_draw};
        int word_first_y{210};
        int word_second_y{313};
        std::size_t kend_frame{};
        std::array<int, 20> credit_y{};
    };

    struct TournamentTrialState {
        enum class Phase {
            choose_opponent,
            awaiting_battle,
            after_victory,
            interround_fade,
            interround_finish,
            finale_fade,
            finale_finish,
            reward_notice,
        };
        std::array<bool, 36> chosen{};
        int group{};
        int victories{};
        Phase phase{Phase::choose_opponent};
    };

    [[nodiscard]] SceneStepResult current_result(SceneStepKind kind) const noexcept;
    [[nodiscard]] SceneStepResult show_scene_title();
    [[nodiscard]] SceneStepResult run_event();
    [[nodiscard]] SceneStepResult run_auto_event(SceneStepKind fallback);
    [[nodiscard]] SceneStepResult finish_tick_after_action(SceneStepKind fallback);
    [[nodiscard]] SceneStepResult finish_tick_after_scene_present(SceneStepKind fallback);
    [[nodiscard]] SceneStepResult finish_tick_after_auto_event(SceneStepKind fallback);
    [[nodiscard]] SceneStepResult resolve_scene_transition(SceneStepKind fallback);
    [[nodiscard]] SceneStepResult complete_scene_jump();
    void queue_scene_music(std::size_t metadata_word);
    [[nodiscard]] bool load_scene_sprites();
    [[nodiscard]] bool draw_sprite(
        render::IndexedFramebuffer& framebuffer,
        std::int16_t legacy_id,
        int anchor_x,
        int anchor_y) const;
    [[nodiscard]] bool draw_overlay(render::IndexedFramebuffer& framebuffer) const;
    void cycle_palette();
    [[nodiscard]] bool target_is_walkable(int x, int y) const noexcept;
    [[nodiscard]] std::optional<std::int16_t> event_at(int x, int y) const noexcept;
    [[nodiscard]] std::optional<std::int16_t> item_event_at(int x, int y) const noexcept;
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
    [[nodiscard]] bool inventory_contains_id(std::int16_t item_id) const noexcept;
    [[nodiscard]] std::optional<std::int16_t> first_inventory_count(
        std::int16_t item_id) const noexcept;
    [[nodiscard]] int inventory_count(std::int16_t item_id) const noexcept;
    void add_inventory(std::int16_t item_id, std::int16_t count);
    void change_first_inventory(std::int16_t item_id, std::int16_t count);
    void update_book_event_if_ready();
    void close_shop_events();
    void remove_team_role(std::int16_t role_id);
    void clear_role_personal_items(std::int16_t role_id);
    void add_role_item(std::int16_t role_id, std::int16_t item_id, std::int16_t count);
    void queue_dialogue(std::int16_t talk_id, std::int16_t head_id, std::int16_t style);
    void queue_notice(std::vector<std::uint8_t> text, std::int16_t style = 0);
    [[nodiscard]] SceneStepResult emit_queued();
    [[nodiscard]] std::optional<SceneStepResult> advance_pan_frame();
    [[nodiscard]] std::optional<SceneStepResult> advance_picture_animation_frame();
    [[nodiscard]] std::optional<SceneStepResult> advance_scripted_walk_frame();
    [[nodiscard]] std::optional<SceneStepResult> advance_dual_picture_animation_frame();
    [[nodiscard]] std::optional<SceneStepResult> advance_three_statue_animation_frame();
    [[nodiscard]] SceneStepResult start_load_menu();
    [[nodiscard]] SceneStepResult advance_load_menu(int translated_key);
    [[nodiscard]] bool render_load_menu();
    [[nodiscard]] SceneStepResult start_death_menu();
    [[nodiscard]] SceneStepResult advance_death_menu(int translated_key);
    [[nodiscard]] bool load_death_image();
    [[nodiscard]] bool render_death_menu();
    void blend_panel_rectangle(
        render::IndexedFramebuffer& framebuffer, int x, int y, int width, int height) const;
    [[nodiscard]] bool draw_panel(
        render::IndexedFramebuffer& framebuffer, int x, int y, int width, int height) const;
    void blend_panel(
        render::IndexedFramebuffer& framebuffer, int x, int y, int width, int height) const;
    [[nodiscard]] bool draw_panel_border(
        render::IndexedFramebuffer& framebuffer, int x, int y, int width, int height) const;
    [[nodiscard]] bool draw_portrait(
        render::IndexedFramebuffer& framebuffer, std::int16_t head_id, int x, int y) const;
    [[nodiscard]] bool draw_death_panel(int x, int y, int width, int height);
    [[nodiscard]] SceneStepResult start_ending();
    [[nodiscard]] SceneStepResult advance_ending();
    [[nodiscard]] bool load_ending_assets();
    [[nodiscard]] bool load_ending_frames();
    [[nodiscard]] bool draw_ending_word(std::int16_t legacy_id, int x, int y);
    [[nodiscard]] bool set_ending_frame(std::size_t frame);
    [[nodiscard]] bool draw_ending_credits();
    [[nodiscard]] std::optional<SceneStepResult> advance_tournament_trial(
        SceneStepKind previous_kind, SceneResponse response);
    void apply_scripted_walk_step(bool horizontal, int step);
    void set_animated_picture(std::int16_t event_index, std::int16_t picture);
    void commit_header() noexcept;
    void update_view_origin() noexcept;
    [[nodiscard]] bool prepare_event(
        std::int16_t script_id,
        std::int16_t event_index,
        std::int16_t event_x,
        std::int16_t event_y,
        std::int16_t item_id);
    void clear_event() noexcept;
    void advance_event_pictures();
    void player_idle_tick();
    void cancel_player_idle_animation() noexcept;

    const resource::DataRoot& data_root_;
    model::GameSnapshot& snapshot_;
    random::LegacyRandom& random_;
    std::optional<SceneDate> death_date_override_;
    SceneAssets assets_;
    resource::PackedArchive portraits_;
    resource::SentinelArchive sprites_;
    resource::PackedArchive ending_words_;
    resource::PackedArchive ending_frames_;
    compat::LegacyPalette palette_{};
    compat::LegacyPalette ending_palette_{};
    std::vector<std::uint16_t> fixed_shadow_mask_;
    std::vector<std::uint16_t> shifted_shadow_mask_;
    render::IndexedFramebuffer ending_framebuffer_;
    render::IndexedFramebuffer load_menu_framebuffer_;
    render::IndexedFramebuffer death_framebuffer_;
    std::vector<std::uint8_t> death_image_;
    std::array<std::uint8_t, 4096> rgb4_lookup_{};
    std::vector<std::uint8_t> ascii_font_;
    std::vector<std::uint8_t> big5_font_;
    std::int16_t scene_id_{-1};
    SceneSessionContext context_{SceneSessionContext::scene};
    int scene_x_{};
    int scene_y_{};
    int view_origin_x_{};
    int view_origin_y_{};
    SceneDirection direction_{SceneDirection::up};
    std::int16_t walk_frame_offset_{};
    std::int16_t player_idle_counter_{};
    std::int16_t idle_animation_counter_{};
    std::int16_t idle_animation_delay_{};
    std::int16_t idle_animation_step_{};
    std::int16_t physical_power_counter_{};
    std::int16_t animation_counter_{};
    std::int16_t periodic_counter_{};
    bool idle_animation_{};
    std::int16_t shadow_state_{};
    EventContext event_context_{};
    std::vector<std::int16_t> script_;
    std::ptrdiff_t program_counter_{};
    bool event_active_{};
    SceneStepResult pending_{};
    std::vector<std::uint8_t> pending_text_;
    PendingContinuation continuation_{PendingContinuation::none};
    TickContinuation tick_continuation_{TickContinuation::none};
    SceneStepKind tick_fallback_{SceneStepKind::stay};
    std::optional<std::int16_t> pending_menu_item_;
    std::optional<std::int16_t> initial_script_;
    bool menu_item_event_active_{};
    std::optional<PendingJump> pending_jump_;
    std::int16_t exit_music_override_{-1};
    std::int16_t true_offset_{};
    std::int16_t false_offset_{};
    std::int16_t battle_get_exp_{};
    std::optional<std::int16_t> player_frame_override_;
    std::optional<PanState> pan_state_;
    std::optional<PictureAnimationState> picture_animation_state_;
    std::optional<ScriptedWalkState> scripted_walk_state_;
    std::optional<DualPictureAnimationState> dual_picture_animation_state_;
    std::optional<ThreeStatueAnimationState> three_statue_animation_state_;
    std::optional<LoadMenuState> load_menu_state_;
    std::optional<std::int16_t> pending_load_slot_;
    std::optional<DeathMenuState> death_menu_state_;
    std::optional<EndingState> ending_state_;
    std::optional<TournamentTrialState> tournament_trial_state_;
    std::deque<QueuedOutput> queued_outputs_;
    std::vector<SceneAudioCommand> audio_commands_;
    std::string error_;
};

}  // namespace openlegend::scene
