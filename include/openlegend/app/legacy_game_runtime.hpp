#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "openlegend/battle/battle_session.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/scene/scene.hpp"
#include "openlegend/ui/basic_ui_renderer.hpp"
#include "openlegend/ui/game_menu.hpp"
#include "openlegend/ui/new_game_attributes.hpp"
#include "openlegend/ui/new_game_name_editor.hpp"
#include "openlegend/ui/title_menu.hpp"
#include "openlegend/world/world_map.hpp"

namespace openlegend::app {

[[nodiscard]] std::string_view ending_terminal_message() noexcept;

enum class LegacyGameView {
    title,
    name_entry,
    attributes,
    world,
    scene,
    battle,
    game_menu,
    error,
    exited,
};

class LegacyGameRuntime {
public:
    LegacyGameRuntime(std::filesystem::path data_root, std::uint32_t random_seed);

    void advance(std::uint32_t bios_tick = 0U);
    void handle_key(
        std::uint8_t translated_key,
        bool control_down,
        bool shift_down,
        std::optional<std::uint32_t> bios_tick = std::nullopt);
    bool handle_world_input(
        bool left,
        bool up,
        bool down,
        bool right,
        bool menu_requested = false);
    void finish_presented_tick(std::uint32_t bios_tick = 0U);
    [[nodiscard]] bool render();

    [[nodiscard]] bool valid() const noexcept { return startup_error_.empty(); }
    [[nodiscard]] bool running() const noexcept { return view_ != LegacyGameView::exited; }
    [[nodiscard]] bool ending_complete() const noexcept { return ending_complete_; }
    [[nodiscard]] LegacyGameView view() const noexcept { return view_; }
    [[nodiscard]] const std::string& error() const noexcept { return startup_error_; }
    [[nodiscard]] render::IndexedFramebuffer& framebuffer() noexcept { return framebuffer_; }
    [[nodiscard]] const model::GameState& game_state() const noexcept { return game_state_; }
    [[nodiscard]] std::optional<std::int16_t> scene_request() const noexcept {
        return scene_request_;
    }
    [[nodiscard]] std::optional<std::int16_t> battle_request() const noexcept {
        return battle_request_;
    }
    [[nodiscard]] std::vector<scene::SceneAudioCommand> take_scene_audio_commands();
    [[nodiscard]] std::vector<battle::BattleAudioCommand> take_battle_audio_commands();

private:
    enum class PendingIo {
        none,
        load,
        save,
    };

    enum class SceneEffectKind {
        none,
        present,
        fade_from_black,
        fade_to_black,
    };

    enum class WorldMenuEventPhase {
        none,
        running,
        present,
        fade_to_black,
        fade_from_black,
        leave_pre_script_present,
        leave_post_fade_to_black,
        leave_post_redraw_present,
        leave_post_fade_from_black,
    };

    enum class SceneLeaveEventPhase {
        none,
        running,
        pre_script_present,
        fade_to_black,
        redraw_present,
        fade_from_black,
    };

    void begin_new_game();
    void perform_pending_io();
    [[nodiscard]] bool start_world(LegacyGameView error_return_view);
    [[nodiscard]] bool start_scene(
        std::int16_t scene_id,
        LegacyGameView error_return_view,
        std::optional<scene::SceneEntryOverride> entry_override = std::nullopt);
    [[nodiscard]] bool start_battle(
        std::int16_t battle_id, bool grant_experience);
    void finish_battle_if_ready();
    void handle_scene_result(const scene::SceneStepResult& result);
    [[nodiscard]] bool advance_scene_effect();
    void begin_scene_effect(SceneEffectKind kind, std::uint16_t wait_ticks = 1U);
    void clear_scene_effect() noexcept;
    void update_menu_counts();
    void set_view(LegacyGameView view, std::string_view reason);
    void show_error(std::string message, LegacyGameView return_view);
    void show_legacy_error(
        std::span<const std::uint8_t> message, LegacyGameView return_view);
    void handle_title_result(ui::TitleResult result);
    void handle_game_menu_result(ui::GameMenuResult result);
    void handle_menu_item_result(ui::GameMenuResult result);
    void handle_menu_item_confirmation(std::uint8_t translated_key);
    [[nodiscard]] bool begin_world_leave_event(std::int16_t role_id);
    void handle_world_menu_event_result(const scene::SceneStepResult& result);

    std::filesystem::path data_root_path_;
    resource::DataRoot data_root_;
    random::LegacyRandom random_;
    model::GameState game_state_;
    render::IndexedFramebuffer framebuffer_;
    ui::TitleMenuController title_menu_;
    ui::TitleMenuRenderer title_renderer_;
    ui::BasicUiRenderer basic_renderer_;
    ui::GameMenuController game_menu_;
    std::unique_ptr<battle::BattleRenderer> game_menu_status_renderer_;
    std::optional<ui::NewGameNameEditor> name_editor_;
    std::unique_ptr<ui::NewGameAttributeController> attribute_controller_;
    std::unique_ptr<world::WorldMapData> world_map_;
    std::unique_ptr<world::WorldSession> world_session_;
    std::unique_ptr<scene::SceneSession> scene_session_;
    std::unique_ptr<scene::SceneSession> world_menu_event_session_;
    std::unique_ptr<battle::BattleSession> battle_session_;
    battle::BattleRenderState retained_battle_render_state_{};
    LegacyGameView view_{LegacyGameView::title};
    LegacyGameView menu_return_view_{LegacyGameView::world};
    LegacyGameView error_return_view_{LegacyGameView::title};
    PendingIo pending_io_{PendingIo::none};
    bool pending_io_wait_presented_{};
    std::uint8_t pending_slot_{};
    std::optional<std::int16_t> scene_request_;
    std::optional<std::int16_t> battle_request_;
    std::optional<std::uint16_t> pending_menu_item_slot_;
    std::optional<std::int16_t> pending_menu_item_id_;
    std::optional<std::int16_t> pending_menu_item_role_;
    std::optional<battle::BattleItemEffectResult> pending_menu_item_effect_;
    std::vector<scene::SceneAudioCommand> scene_audio_commands_;
    SceneEffectKind scene_effect_kind_{SceneEffectKind::none};
    std::vector<compat::LegacyPalette> scene_effect_palettes_;
    std::size_t scene_effect_frame_{};
    std::uint16_t scene_effect_wait_ticks_{1U};
    std::int16_t periodic_counter_{};
    bool scene_effect_presented_{};
    bool world_step_processed_{};
    std::optional<scene::SceneDirection> scene_direction_input_;
    std::optional<world::WorldDirection> scene_entry_world_direction_;
    std::optional<world::WorldMoveContinuation> world_move_continuation_;
    bool world_scene_transition_pending_{};
    bool world_scene_transition_presented_{};
    bool world_scene_return_pending_{};
    bool world_scene_return_presented_{};
    WorldMenuEventPhase world_menu_event_phase_{WorldMenuEventPhase::none};
    SceneLeaveEventPhase scene_leave_event_phase_{SceneLeaveEventPhase::none};
    std::optional<std::int16_t> world_menu_event_script_id_;
    std::optional<std::int16_t> scene_leave_event_script_id_;
    bool leave_protagonist_notice_pending_{};
    bool scene_interact_requested_{};
    bool scene_ui_requested_{};
    bool ending_complete_{};
    std::vector<std::uint8_t> visible_error_;
    std::string startup_error_;
};

}  // namespace openlegend::app
