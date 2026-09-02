#include "openlegend/app/legacy_game_runtime.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>
#include <utility>

#include "openlegend/diagnostics/log.hpp"
#include "openlegend/model/new_game.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/render/legacy_effects.hpp"

namespace openlegend::app {
namespace {

constexpr std::array<std::int16_t, 25> kLeavePartyRoles{
    1, 2, 9, 16, 17, 25, 28, 29, 35, 36, 37, 38, 44,
    45, 47, 48, 49, 51, 53, 54, 58, 59, 61, 63, 76};

[[nodiscard]] persistence::SaveSlot save_slot(const std::uint8_t slot) noexcept {
    return static_cast<persistence::SaveSlot>(slot);
}

[[nodiscard]] std::string_view view_name(const LegacyGameView view) noexcept {
    switch (view) {
    case LegacyGameView::title: return "title";
    case LegacyGameView::name_entry: return "name_entry";
    case LegacyGameView::attributes: return "attributes";
    case LegacyGameView::world: return "world";
    case LegacyGameView::scene: return "scene";
    case LegacyGameView::battle: return "battle";
    case LegacyGameView::game_menu: return "game_menu";
    case LegacyGameView::error: return "error";
    case LegacyGameView::exited: return "exited";
    }
    return "unknown";
}

[[nodiscard]] std::string_view world_direction_name(
    const world::WorldDirection direction) noexcept {
    switch (direction) {
    case world::WorldDirection::up: return "up";
    case world::WorldDirection::right: return "right";
    case world::WorldDirection::left: return "left";
    case world::WorldDirection::down: return "down";
    }
    return "unknown";
}

[[nodiscard]] world::WorldDirection opposite_world_direction(
    const world::WorldDirection direction) noexcept {
    switch (direction) {
    case world::WorldDirection::up: return world::WorldDirection::down;
    case world::WorldDirection::right: return world::WorldDirection::left;
    case world::WorldDirection::left: return world::WorldDirection::right;
    case world::WorldDirection::down: return world::WorldDirection::up;
    }
    return world::WorldDirection::down;
}

[[nodiscard]] std::string_view world_step_name(const world::WorldStepKind kind) noexcept {
    switch (kind) {
    case world::WorldStepKind::stay: return "stay";
    case world::WorldStepKind::moved: return "moved";
    case world::WorldStepKind::enter_scene: return "enter_scene";
    case world::WorldStepKind::open_ui: return "open_ui";
    }
    return "unknown";
}

[[nodiscard]] std::string_view scene_step_name(const scene::SceneStepKind kind) noexcept {
    switch (kind) {
    case scene::SceneStepKind::stay: return "stay";
    case scene::SceneStepKind::moved: return "moved";
    case scene::SceneStepKind::present: return "present";
    case scene::SceneStepKind::fade_from_black: return "fade_from_black";
    case scene::SceneStepKind::fade_to_black: return "fade_to_black";
    case scene::SceneStepKind::scene_title: return "scene_title";
    case scene::SceneStepKind::dialogue: return "dialogue";
    case scene::SceneStepKind::notice: return "notice";
    case scene::SceneStepKind::question: return "question";
    case scene::SceneStepKind::wait_key: return "wait_key";
    case scene::SceneStepKind::load_menu: return "load_menu";
    case scene::SceneStepKind::death_menu: return "death_menu";
    case scene::SceneStepKind::load_slot: return "load_slot";
    case scene::SceneStepKind::battle: return "battle";
    case scene::SceneStepKind::shop: return "shop";
    case scene::SceneStepKind::open_ui: return "open_ui";
    case scene::SceneStepKind::return_world: return "return_world";
    case scene::SceneStepKind::quit: return "quit";
    }
    return "unknown";
}

[[nodiscard]] std::vector<std::uint8_t> legacy_ascii(const std::string_view text) {
    std::vector<std::uint8_t> result;
    result.reserve(text.size());
    for (const auto character : text) {
        const auto value = static_cast<unsigned char>(character);
        result.push_back(value < 0x80U ? value : static_cast<std::uint8_t>('?'));
    }
    return result;
}

[[nodiscard]] constexpr LegacyKeyStateReset menu_key_state_reset(
    const std::uint8_t translated_key) noexcept {
    if (translated_key == 0x0DU || translated_key == 0x20U || translated_key == 0x96U) {
        return LegacyKeyStateReset::confirmation_group;
    }
    if (translated_key == 0x98U || translated_key == 0x9EU || translated_key == 0x1BU) {
        return LegacyKeyStateReset::translated;
    }
    return LegacyKeyStateReset::none;
}

[[nodiscard]] constexpr LegacyKeyStateReset main_game_menu_key_state_reset(
    const std::uint8_t translated_key) noexcept {
    if (translated_key == 0x98U || translated_key == 0x9EU) {
        return LegacyKeyStateReset::down_translated;
    }
    return menu_key_state_reset(translated_key);
}

}  // namespace

LegacyStartupResources::LegacyStartupResources(const resource::DataRoot& data_root) {
    auto cloud_group = data_root.read("CLOUD.GRP");
    if (!cloud_group) {
        error_ = cloud_group.error;
        return;
    }
    const auto cloud_index = data_root.read("CLOUD.IDX");
    if (!cloud_index) {
        error_ = cloud_index.error;
        return;
    }
    weather_sprites_ = resource::PackedArchive::parse(
        cloud_index.bytes, std::move(cloud_group.bytes));
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

    ranger_ = persistence::load_baseline_ranger(data_root.path());
    if (!ranger_) {
        error_ = ranger_.detail.empty()
            ? std::string{persistence::persistence_status_message(ranger_.status)}
            : ranger_.detail;
    }
}

std::string_view ending_terminal_message() noexcept {
    return " Thanks for playing this game ! \n Oriental Software Studio 1996  \n";
}

LegacyGameRuntime::LegacyGameRuntime(
    std::filesystem::path data_root, const std::uint32_t random_seed)
    : data_root_path_(std::move(data_root)),
      data_root_(data_root_path_),
      basic_renderer_(data_root_),
      startup_resources_(data_root_),
      random_(random_seed) {
    if (!basic_renderer_.valid()) {
        startup_error_ = basic_renderer_.error();
    } else if (!startup_resources_.valid()) {
        startup_error_ = startup_resources_.error();
    }
    if (startup_error_.empty()) {
        world_map_ = std::make_unique<world::WorldMapData>(data_root_);
        if (!world_map_->valid()) {
            startup_error_ = world_map_->error();
        }
    }
    if (startup_error_.empty()) {
        title_renderer_ = std::make_unique<ui::TitleMenuRenderer>(
            data_root_, startup_resources_.palette());
        if (!title_renderer_->valid()) {
            startup_error_ = title_renderer_->error();
        }
    }
    if (startup_error_.empty() && !title_renderer_->render_background(framebuffer_)) {
        startup_error_ = "Unable to render title background";
    }
    if (startup_error_.empty()) {
        title_startup_phase_ = TitleStartupPhase::fade_to_black;
        begin_scene_effect(SceneEffectKind::fade_to_black, 1U);
        diagnostics::log_info("LegacyGameRuntime initialized view=title");
    } else {
        diagnostics::log_error("LegacyGameRuntime initialization failed: " + startup_error_);
    }
}

void LegacyGameRuntime::advance(const std::uint32_t bios_tick) {
    if (pending_io_ != PendingIo::none) {
        if (!pending_io_wait_presented_) {
            world_step_processed_ = false;
            return;
        }
        perform_pending_io();
        world_step_processed_ = false;
        return;
    }
    if ((world_scene_transition_pending_ && !world_scene_transition_presented_) ||
        (world_scene_return_pending_ && !world_scene_return_presented_)) {
        world_step_processed_ = false;
        return;
    }
    if (advance_scene_effect()) {
        world_step_processed_ = false;
        return;
    }
    if (world_menu_event_phase_ == WorldMenuEventPhase::running) {
        world_step_processed_ = false;
        return;
    }
    if (view_ == LegacyGameView::world && world_session_ != nullptr) {
        if (!world_step_processed_) {
            world_session_->idle_tick();
            physical_power_counter_ = world_session_->physical_power_counter();
        }
        world_session_->periodic_tick();
        world_session_->idle_animation_tick();
    } else if (view_ == LegacyGameView::battle && battle_session_ != nullptr) {
        battle_session_->advance(bios_tick);
        finish_battle_if_ready();
    } else if (view_ == LegacyGameView::scene && scene_session_ != nullptr &&
               scene_session_->pending().kind == scene::SceneStepKind::stay) {
        const auto direction = scene_direction_input_;
        scene_direction_input_.reset();
        bool interact_requested = false;
        bool ui_requested = false;
        bool skip_player_idle = false;
        if (!direction.has_value() && scene_interact_requested_) {
            interact_requested = true;
            scene_interact_requested_ = false;
        } else if (!direction.has_value() && scene_ui_requested_) {
            ui_requested = true;
            scene_ui_requested_ = false;
        } else if (!direction.has_value() && scene_idle_skip_requested_) {
            skip_player_idle = true;
            scene_idle_skip_requested_ = false;
        }
        handle_scene_result(scene_session_->tick(
            direction, interact_requested, ui_requested, skip_player_idle));
    } else {
        scene_direction_input_.reset();
    }
    world_step_processed_ = false;
}

void LegacyGameRuntime::finish_presented_tick(const std::uint32_t bios_tick) {
    if (view_ == LegacyGameView::name_entry && name_editor_.has_value()) {
        name_editor_->finish_presented_frame();
    }
    if (pending_name_accept_) {
        if (scene_effect_kind_ != SceneEffectKind::none) {
            scene_effect_presented_ = true;
        }
        return;
    }
    if (pending_io_ != PendingIo::none) {
        pending_io_wait_presented_ = true;
        return;
    }
    if (load_transition_phase_ != LoadTransitionPhase::none ||
        title_startup_phase_ != TitleStartupPhase::none ||
        pending_title_result_.has_value()) {
        if (scene_effect_kind_ != SceneEffectKind::none) {
            scene_effect_presented_ = true;
        }
        return;
    }
    if (view_ == LegacyGameView::battle && battle_session_ != nullptr) {
        const auto start_music =
            battle_session_->phase() == battle::BattleSessionPhase::initial_present &&
            battle_session_->frame_rendered();
        if (start_music) {
            scene_audio_commands_.push_back(scene::SceneAudioCommand{
                scene::SceneAudioCommand::Kind::music,
                battle_session_->data().music_id()});
        }
        battle_session_->finish_presented_tick(bios_tick);
        return;
    }
    if (view_ != LegacyGameView::world || world_session_ == nullptr) {
        return;
    }
    if (leave_protagonist_notice_pending_) {
        if (scene_effect_kind_ != SceneEffectKind::none) {
            scene_effect_presented_ = true;
        }
        return;
    }
    if (world_menu_event_phase_ != WorldMenuEventPhase::none) {
        if (scene_effect_kind_ != SceneEffectKind::none) {
            scene_effect_presented_ = true;
        }
        return;
    }
    if (world_scene_transition_pending_ || world_scene_return_pending_) {
        if (world_scene_transition_pending_ && !world_scene_transition_presented_) {
            world_scene_transition_presented_ = true;
            begin_scene_effect(SceneEffectKind::fade_to_black, 1U);
        } else if (world_scene_return_pending_ && !world_scene_return_presented_) {
            world_scene_return_presented_ = true;
            begin_scene_effect(SceneEffectKind::fade_from_black, 1U);
        }
        return;
    }
    periodic_counter_ = static_cast<std::int16_t>((periodic_counter_ + 1) % 5);
    if (periodic_counter_ == 1) {
        world_session_->cycle_palette();
    }
    if (pending_world_exit_) {
        pending_world_exit_ = false;
        set_view(LegacyGameView::exited, "world system exit after final present");
    }
}

bool LegacyGameRuntime::handle_world_input(
    const bool left,
    const bool up,
    const bool down,
    const bool right,
    const bool menu_requested) {
    if (!valid() || pending_world_exit_ || world_scene_transition_pending_ ||
        world_scene_return_pending_ || load_transition_phase_ != LoadTransitionPhase::none ||
        world_menu_event_phase_ != WorldMenuEventPhase::none ||
        scene_leave_event_phase_ != SceneLeaveEventPhase::none ||
        leave_protagonist_notice_pending_ ||
        ((view_ != LegacyGameView::world || world_session_ == nullptr) &&
         (view_ != LegacyGameView::scene || scene_session_ == nullptr))) {
        return false;
    }
    std::optional<world::WorldDirection> direction;
    if (left) {
        direction = world::WorldDirection::left;
    } else if (up) {
        direction = world::WorldDirection::up;
    } else if (down) {
        direction = world::WorldDirection::down;
    } else if (right) {
        direction = world::WorldDirection::right;
    }
    if (!direction.has_value()) {
        if (menu_requested && view_ == LegacyGameView::world) {
            world_step_processed_ = true;
            world_session_->prepare_game_menu_frame();
            update_menu_counts();
            game_menu_.set_context(ui::GameMenuContext::world);
            game_menu_.show_main();
            menu_return_view_ = LegacyGameView::world;
            set_view(LegacyGameView::game_menu, "open world menu");
            return true;
        }
        return false;
    }
    world_step_processed_ = true;
    if (view_ == LegacyGameView::scene) {
        if (scene_session_->pending().kind == scene::SceneStepKind::stay) {
            scene_direction_input_ = static_cast<scene::SceneDirection>(
                static_cast<std::int16_t>(*direction));
            return true;
        }
        scene_direction_input_.reset();
        return false;
    }
    const auto result = world_session_->move(*direction);
    diagnostics::log_info(
        "world input direction=" + std::string{world_direction_name(*direction)} +
        " result=" + std::string{world_step_name(result.kind)} +
        " x=" + std::to_string(result.world_x) +
        " y=" + std::to_string(result.world_y) +
        " frame=" + std::to_string(world_session_->player_frame()));
    if (result.kind == world::WorldStepKind::enter_scene) {
        scene_request_ = result.scene_id;
        world_move_continuation_ = result.continuation;
        world_scene_transition_pending_ = true;
        world_scene_transition_presented_ = false;
        clear_scene_effect();
    }
    return true;
}

LegacyKeyStateReset LegacyGameRuntime::handle_key(
    const std::uint8_t translated_key,
    const bool control_down,
    const bool shift_down,
    const std::optional<std::uint32_t> bios_tick) {
    if (!valid() || translated_key == 0U || view_ == LegacyGameView::exited ||
        pending_world_exit_ || world_scene_transition_pending_ || world_scene_return_pending_ ||
        load_transition_phase_ != LoadTransitionPhase::none ||
        title_startup_phase_ != TitleStartupPhase::none ||
        pending_title_result_.has_value() || pending_name_accept_ ||
        pending_new_game_wait_present_ || pending_new_game_scene_start_) {
        return LegacyKeyStateReset::none;
    }
    diagnostics::log_debug(
        "runtime key translated=" + std::to_string(translated_key) +
        " control=" + (control_down ? std::string{"true"} : std::string{"false"}) +
        " shift=" + (shift_down ? std::string{"true"} : std::string{"false"}) +
        " view=" + std::string{view_name(view_)});
    if (view_ == LegacyGameView::error) {
        visible_error_.clear();
        set_view(error_return_view_, "dismiss error");
        return LegacyKeyStateReset::none;
    }

    auto key_state_reset = LegacyKeyStateReset::none;
    switch (view_) {
    case LegacyGameView::title:
        key_state_reset = menu_key_state_reset(translated_key);
        handle_title_result(title_menu_.handle_key(translated_key));
        break;
    case LegacyGameView::name_entry:
        if (name_editor_.has_value() &&
            name_editor_->handle_key(translated_key, control_down, shift_down) ==
                ui::NameEditStatus::completed) {
            auto* ranger = game_state_.ranger();
            if (ranger == nullptr || !model::set_protagonist_name(*ranger, name_editor_->name())) {
                show_error("Unable to store protagonist name", LegacyGameView::title);
                break;
            }
            pending_name_accept_ = true;
            begin_scene_effect(SceneEffectKind::present, 30U);
        }
        break;
    case LegacyGameView::attributes:
        if (attribute_controller_ != nullptr &&
            attribute_controller_->handle_key(translated_key) ==
                ui::AttributeRollStatus::accepted) {
            pending_new_game_wait_present_ = true;
            begin_scene_effect(SceneEffectKind::present, 1U);
        }
        break;
    case LegacyGameView::world:
        if (world_menu_event_phase_ == WorldMenuEventPhase::running &&
            world_menu_event_session_ != nullptr) {
            const auto pending_kind = world_menu_event_session_->pending().kind;
            if (pending_kind == scene::SceneStepKind::dialogue ||
                pending_kind == scene::SceneStepKind::notice ||
                pending_kind == scene::SceneStepKind::wait_key) {
                handle_world_menu_event_result(
                    world_menu_event_session_->resume(scene::SceneResponse::acknowledge));
            } else if (pending_kind == scene::SceneStepKind::question) {
                handle_world_menu_event_result(world_menu_event_session_->resume(
                    translated_key == static_cast<std::uint8_t>('Y')
                        ? scene::SceneResponse::yes
                        : scene::SceneResponse::no));
            }
        } else if (translated_key == 0x1BU) {
            world_session_->prepare_game_menu_frame();
            update_menu_counts();
            game_menu_.set_context(ui::GameMenuContext::world);
            game_menu_.show_main();
            menu_return_view_ = LegacyGameView::world;
            set_view(LegacyGameView::game_menu, "open world menu");
        }
        break;
    case LegacyGameView::scene: {
        if (scene_session_ == nullptr) {
            break;
        }
        const auto pending_kind = scene_session_->pending().kind;
        if (pending_kind == scene::SceneStepKind::question) {
            handle_scene_result(scene_session_->resume(
                translated_key == static_cast<std::uint8_t>('Y')
                    ? scene::SceneResponse::yes
                    : scene::SceneResponse::no));
        } else if (pending_kind == scene::SceneStepKind::shop) {
            if (translated_key >= static_cast<std::uint8_t>('1') &&
                translated_key <= static_cast<std::uint8_t>('5')) {
                handle_scene_result(scene_session_->resume(
                    scene::SceneResponse::yes,
                    static_cast<int>(translated_key - static_cast<std::uint8_t>('1'))));
            } else if (translated_key == 0x1BU) {
                handle_scene_result(scene_session_->resume(scene::SceneResponse::cancel));
            }
        } else if (pending_kind == scene::SceneStepKind::load_menu ||
                   pending_kind == scene::SceneStepKind::death_menu) {
            handle_scene_result(scene_session_->resume(
                scene::SceneResponse::acknowledge, static_cast<int>(translated_key)));
        } else if (pending_kind == scene::SceneStepKind::dialogue ||
                   pending_kind == scene::SceneStepKind::notice ||
                   pending_kind == scene::SceneStepKind::scene_title ||
                   pending_kind == scene::SceneStepKind::wait_key) {
            handle_scene_result(scene_session_->resume(scene::SceneResponse::acknowledge));
        } else if (pending_kind == scene::SceneStepKind::stay) {
            if (translated_key == 0x1BU) {
                scene_ui_requested_ = true;
            } else if (translated_key == 0x0DU || translated_key == 0x20U ||
                       translated_key == 0x96U) {
                scene_interact_requested_ = true;
                key_state_reset = LegacyKeyStateReset::confirmation_group;
            } else if (translated_key == static_cast<std::uint8_t>('L')) {
                scene_idle_skip_requested_ = true;
                key_state_reset = LegacyKeyStateReset::edge;
            }
        }
        break;
    }
    case LegacyGameView::battle:
        if (battle_session_ != nullptr) {
            if (battle_session_->phase() ==
                battle::BattleSessionPhase::player_status_selection) {
                key_state_reset = menu_key_state_reset(translated_key);
            }
            static_cast<void>(battle_session_->handle_key(translated_key, bios_tick));
            finish_battle_if_ready();
        }
        break;
    case LegacyGameView::game_menu: {
        const auto screen = game_menu_.screen();
        if (screen == ui::GameMenuScreen::main) {
            key_state_reset = main_game_menu_key_state_reset(translated_key);
        } else if (screen == ui::GameMenuScreen::party_select ||
                   screen == ui::GameMenuScreen::system ||
                   screen == ui::GameMenuScreen::load_slots ||
                   screen == ui::GameMenuScreen::save_slots) {
            key_state_reset = menu_key_state_reset(translated_key);
        }
        if (screen == ui::GameMenuScreen::item_confirmation) {
            handle_menu_item_confirmation(translated_key);
        } else {
            handle_game_menu_result(game_menu_.handle_key(translated_key));
        }
        break;
    }
    case LegacyGameView::error:
    case LegacyGameView::exited:
        break;
    }
    return key_state_reset;
}

bool LegacyGameRuntime::render() {
    if (!valid()) {
        return false;
    }
    if (title_startup_phase_ == TitleStartupPhase::fade_to_black) {
        if (scene_effect_kind_ != SceneEffectKind::fade_to_black) {
            return false;
        }
        if (scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_to_black(framebuffer_.palette());
        }
        if (scene_effect_frame_ >= scene_effect_palettes_.size()) {
            return false;
        }
        framebuffer_.set_palette(scene_effect_palettes_[scene_effect_frame_]);
        scene_effect_presented_ = true;
        return true;
    }
    if (title_startup_phase_ == TitleStartupPhase::black_menu_present) {
        if (!title_renderer_->render(title_menu_, framebuffer_)) {
            return false;
        }
        const auto black = render::legacy_fade_to_black(framebuffer_.palette());
        if (black.empty()) {
            return false;
        }
        framebuffer_.set_palette(black.back());
        scene_effect_presented_ = true;
        return true;
    }
    if (title_startup_phase_ == TitleStartupPhase::fade_from_black) {
        if (!title_renderer_->render(title_menu_, framebuffer_)) {
            return false;
        }
        if (scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_from_black(framebuffer_.palette());
        }
        if (scene_effect_frame_ >= scene_effect_palettes_.size()) {
            return false;
        }
        framebuffer_.set_palette(scene_effect_palettes_[scene_effect_frame_]);
        scene_effect_presented_ = true;
        return true;
    }
    if (load_transition_phase_ == LoadTransitionPhase::fade_to_black) {
        if (scene_effect_kind_ != SceneEffectKind::fade_to_black) {
            return false;
        }
        if (scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_to_black(framebuffer_.palette());
        }
        if (scene_effect_frame_ >= scene_effect_palettes_.size()) {
            return false;
        }
        framebuffer_.set_palette(scene_effect_palettes_[scene_effect_frame_]);
        scene_effect_presented_ = true;
        return true;
    }
    if (pending_new_game_wait_present_) {
        if (view_ != LegacyGameView::attributes ||
            scene_effect_kind_ != SceneEffectKind::present ||
            !title_renderer_->render_new_game_wait(framebuffer_)) {
            return false;
        }
        scene_effect_presented_ = true;
        return true;
    }
    if (pending_new_game_scene_start_) {
        if (view_ != LegacyGameView::attributes ||
            scene_effect_kind_ != SceneEffectKind::fade_to_black ||
            !title_renderer_->render_new_game_wait(framebuffer_)) {
            return false;
        }
        if (scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_to_black(framebuffer_.palette());
        }
        if (scene_effect_frame_ >= scene_effect_palettes_.size()) {
            return false;
        }
        framebuffer_.set_palette(scene_effect_palettes_[scene_effect_frame_]);
        scene_effect_presented_ = true;
        return true;
    }
    switch (view_) {
    case LegacyGameView::title:
        return title_renderer_->render(title_menu_, framebuffer_);
    case LegacyGameView::name_entry:
        return name_editor_.has_value() &&
               basic_renderer_.render_name_entry(
                   *title_renderer_, *name_editor_, framebuffer_);
    case LegacyGameView::attributes: {
        const auto* ranger = game_state_.ranger();
        return ranger != nullptr && name_editor_.has_value() &&
               basic_renderer_.render_attributes(
                   *title_renderer_,
                   ranger->roles[0],
                   name_editor_->name(),
                   framebuffer_);
    }
    case LegacyGameView::world: {
        const auto freeze_leave_frame =
            world_menu_event_phase_ == WorldMenuEventPhase::leave_post_fade_to_black;
        if (world_session_ == nullptr ||
            (!freeze_leave_frame && !world_session_->render(framebuffer_))) {
            return false;
        }
        if (world_menu_event_phase_ == WorldMenuEventPhase::running &&
            (world_menu_event_session_ == nullptr ||
             !world_menu_event_session_->render_overlay(framebuffer_))) {
            return false;
        }
        if (world_menu_event_phase_ == WorldMenuEventPhase::leave_post_redraw_present) {
            const auto black = render::legacy_fade_to_black(framebuffer_.palette());
            if (black.empty()) {
                return false;
            }
            framebuffer_.set_palette(black.back());
        }
        if (world_scene_return_pending_ && !world_scene_return_presented_) {
            const auto black = render::legacy_fade_to_black(framebuffer_.palette());
            if (black.empty()) {
                return false;
            }
            framebuffer_.set_palette(black.back());
        }
        const auto world_effect_presented =
            (world_scene_transition_pending_ && world_scene_transition_presented_) ||
            (world_scene_return_pending_ && world_scene_return_presented_) ||
            world_menu_event_phase_ == WorldMenuEventPhase::fade_to_black ||
            world_menu_event_phase_ == WorldMenuEventPhase::fade_from_black ||
            world_menu_event_phase_ == WorldMenuEventPhase::leave_post_fade_to_black ||
            world_menu_event_phase_ == WorldMenuEventPhase::leave_post_fade_from_black ||
            load_transition_phase_ == LoadTransitionPhase::fade_from_black;
        if (world_effect_presented &&
            scene_effect_kind_ == SceneEffectKind::fade_to_black &&
            scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_to_black(framebuffer_.palette());
        } else if (world_effect_presented &&
                   scene_effect_kind_ == SceneEffectKind::fade_from_black &&
                   scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_from_black(framebuffer_.palette());
        }
        if (world_effect_presented && !scene_effect_palettes_.empty() &&
            scene_effect_frame_ < scene_effect_palettes_.size()) {
            framebuffer_.set_palette(scene_effect_palettes_[scene_effect_frame_]);
        }
        if (load_transition_phase_ == LoadTransitionPhase::first_black_present ||
            load_transition_phase_ == LoadTransitionPhase::second_black_present) {
            const auto black = render::legacy_fade_to_black(framebuffer_.palette());
            if (black.empty()) {
                return false;
            }
            framebuffer_.set_palette(black.back());
        }
        if (world_effect_presented ||
            load_transition_phase_ == LoadTransitionPhase::first_black_present ||
            load_transition_phase_ == LoadTransitionPhase::second_black_present) {
            scene_effect_presented_ = true;
        }
        return true;
    }
    case LegacyGameView::scene:
        if (scene_session_ == nullptr ||
            (scene_leave_event_phase_ != SceneLeaveEventPhase::fade_to_black &&
             !scene_session_->render(framebuffer_))) {
            return false;
        }
        if (scene_leave_event_phase_ == SceneLeaveEventPhase::redraw_present) {
            const auto black = render::legacy_fade_to_black(framebuffer_.palette());
            if (black.empty()) {
                return false;
            }
            framebuffer_.set_palette(black.back());
        }
        if (scene_effect_kind_ == SceneEffectKind::fade_from_black &&
            scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_from_black(framebuffer_.palette());
        } else if (scene_effect_kind_ == SceneEffectKind::fade_to_black &&
                   scene_effect_palettes_.empty()) {
            scene_effect_palettes_ = render::legacy_fade_to_black(framebuffer_.palette());
        }
        if (!scene_effect_palettes_.empty() &&
            scene_effect_frame_ < scene_effect_palettes_.size()) {
            framebuffer_.set_palette(scene_effect_palettes_[scene_effect_frame_]);
        }
        scene_effect_presented_ = scene_effect_kind_ != SceneEffectKind::none;
        return true;
    case LegacyGameView::battle:
        return battle_session_ != nullptr && battle_session_->render(framebuffer_);
    case LegacyGameView::game_menu: {
        const auto* ranger = game_state_.ranger();
        const auto base_rendered = menu_return_view_ == LegacyGameView::scene
                                       ? scene_session_ != nullptr && scene_session_->render(framebuffer_)
                                       : world_session_ != nullptr && world_session_->render(framebuffer_);
        if (ranger == nullptr || !base_rendered) {
            return false;
        }
        const auto exact_party_selection =
            game_menu_.screen() == ui::GameMenuScreen::party_select;
        const auto exact_party_notice =
            game_menu_.screen() == ui::GameMenuScreen::party_notice;
        const auto exact_status_page =
            game_menu_.screen() == ui::GameMenuScreen::status_panel;
        const auto exact_item_effect =
            game_menu_.screen() == ui::GameMenuScreen::item_effect;
        if (exact_party_selection || exact_party_notice || exact_status_page ||
            exact_item_effect) {
            if (game_menu_status_renderer_ == nullptr) {
                game_menu_status_renderer_ =
                    std::make_unique<battle::BattleRenderer>(data_root_, 0);
            }
            if (!game_menu_status_renderer_->valid()) {
                return false;
            }
            const auto command = game_menu_.pending_party_command();
            if (exact_party_selection &&
                !basic_renderer_.render_game_menu_main(game_menu_, framebuffer_)) {
                return false;
            }
            if (exact_party_selection &&
                game_menu_.party_stage() == ui::GameMenuPartyStage::source) {
                const auto kind = command == ui::GameMenuCommand::medicine
                    ? battle::PartyAbilityKind::medicine
                    : battle::PartyAbilityKind::detoxification;
                if (!game_menu_status_renderer_->render_party_ability_selection(
                        *ranger,
                        game_menu_.party_options(),
                        game_menu_.party_selection(),
                        kind,
                        framebuffer_)) {
                    return false;
                }
            } else if (exact_party_selection) {
                const auto kind = command == ui::GameMenuCommand::medicine
                    ? battle::PartySelectionKind::medicine_target
                    : command == ui::GameMenuCommand::detoxification
                    ? battle::PartySelectionKind::detoxification_target
                    : command == ui::GameMenuCommand::leave_party
                    ? battle::PartySelectionKind::leave_party
                    : command == ui::GameMenuCommand::items &&
                              game_menu_.item_target_kind() ==
                                  ui::GameMenuItemTargetKind::equipment
                    ? battle::PartySelectionKind::equipment_target
                    : command == ui::GameMenuCommand::items &&
                              game_menu_.item_target_kind() ==
                                  ui::GameMenuItemTargetKind::practice
                    ? battle::PartySelectionKind::practice_target
                    : command == ui::GameMenuCommand::items
                    ? battle::PartySelectionKind::item_target
                    : battle::PartySelectionKind::status;
                if (!game_menu_status_renderer_->render_character_selection(
                        *ranger,
                        game_menu_.party_selection(),
                        kind,
                        framebuffer_,
                        command == ui::GameMenuCommand::items
                            ? pending_menu_item_id_
                            : std::nullopt)) {
                    return false;
                }
            } else if (exact_party_notice) {
                const auto amount = game_menu_.party_action_amount();
                const auto kind = command == ui::GameMenuCommand::medicine
                    ? battle::PartyAbilityKind::medicine
                    : battle::PartyAbilityKind::detoxification;
                if ((!amount.has_value() &&
                     !basic_renderer_.render_game_menu_main(game_menu_, framebuffer_)) ||
                    !game_menu_status_renderer_->render_party_action_notice(
                        kind, amount, framebuffer_)) {
                    return false;
                }
            } else if (exact_item_effect) {
                if (!pending_menu_item_slot_.has_value() ||
                    !pending_menu_item_effect_.has_value()) {
                    return false;
                }
                if (!pending_menu_item_id_.has_value() ||
                    !game_menu_status_renderer_->render_item_effect(
                        *ranger,
                        *pending_menu_item_id_,
                        *pending_menu_item_effect_,
                        framebuffer_)) {
                    return false;
                }
            } else {
                const auto role_id = ranger->header.team_member(
                    game_menu_.selected_party_slot()).value;
                if (!game_menu_status_renderer_->render_character_status(
                        *ranger,
                        role_id,
                        game_menu_.status_page(),
                        framebuffer_)) {
                    return false;
                }
            }
        } else if (!basic_renderer_.render_game_menu(
                       game_menu_, *ranger, framebuffer_)) {
            return false;
        }
        return pending_io_ == PendingIo::none || basic_renderer_.render_io_wait(framebuffer_);
    }
    case LegacyGameView::error: {
        bool base_rendered = false;
        if (error_return_view_ == LegacyGameView::title) {
            base_rendered = title_renderer_->render(title_menu_, framebuffer_);
        } else if ((error_return_view_ == LegacyGameView::scene ||
                    (error_return_view_ == LegacyGameView::game_menu &&
                     menu_return_view_ == LegacyGameView::scene)) &&
                   scene_session_ != nullptr) {
            base_rendered = scene_session_->render(framebuffer_);
        } else if (world_session_ != nullptr) {
            base_rendered = world_session_->render(framebuffer_);
        }
        return base_rendered && basic_renderer_.render_error(visible_error_, framebuffer_);
    }
    case LegacyGameView::exited:
        return true;
    }
    return false;
}

void LegacyGameRuntime::begin_new_game() {
    pending_name_accept_ = false;
    pending_new_game_wait_present_ = false;
    pending_new_game_scene_start_ = false;
    attribute_controller_.reset();
    name_editor_.reset();
    world_session_.reset();
    scene_session_.reset();
    world_menu_event_session_.reset();
    battle_session_.reset();
    scene_request_.reset();
    battle_request_.reset();
    pending_menu_item_slot_.reset();
    pending_menu_item_id_.reset();
    pending_menu_item_role_.reset();
    pending_menu_item_effect_.reset();
    world_menu_event_phase_ = WorldMenuEventPhase::none;
    scene_leave_event_phase_ = SceneLeaveEventPhase::none;
    world_menu_event_script_id_.reset();
    scene_leave_event_script_id_.reset();
    leave_protagonist_notice_pending_ = false;
    scene_audio_commands_.clear();
    clear_scene_effect();
    auto loaded = persistence::load_baseline_scenes(
        data_root_path_, startup_resources_.ranger());
    if (!loaded) {
        show_error(
            std::string{persistence::persistence_status_message(loaded.status)},
            LegacyGameView::title);
        return;
    }
    if (!game_state_.import_snapshot(std::move(*loaded.snapshot))) {
        show_error("Baseline snapshot import failed", LegacyGameView::title);
        return;
    }

    name_editor_.emplace(data_root_);
    if (!name_editor_->valid()) {
        show_error(name_editor_->error(), LegacyGameView::title);
        return;
    }
    set_view(LegacyGameView::name_entry, "new game baseline loaded");
}

void LegacyGameRuntime::perform_pending_io() {
    const auto operation = pending_io_;
    pending_io_ = PendingIo::none;
    pending_io_wait_presented_ = false;
    game_menu_.complete_slot_operation();
    if (operation == PendingIo::load) {
        attribute_controller_.reset();
        auto loaded = persistence::load_numbered_slot(
            data_root_path_,
            save_slot(pending_slot_),
            startup_resources_.ranger_index_bytes());
        if (!loaded) {
            if (error_return_view_ == LegacyGameView::scene && scene_session_ != nullptr &&
                scene_session_->pending().kind == scene::SceneStepKind::load_slot) {
                handle_scene_result(scene_session_->resume(scene::SceneResponse::cancel));
            }
            show_error(
                std::string{persistence::persistence_status_message(loaded.status)},
                error_return_view_);
            return;
        }
        load_return_view_ = error_return_view_;
        pending_loaded_snapshot_ = std::move(*loaded.snapshot);
        load_transition_phase_ = LoadTransitionPhase::fade_to_black;
        begin_scene_effect(SceneEffectKind::fade_to_black, 1U);
        return;
    }

    const auto scene_snapshot = game_state_.export_snapshot();
    if (!scene_snapshot.has_value()) {
        show_error("No game state is available to save", LegacyGameView::game_menu);
        return;
    }
    auto written = persistence::write_numbered_slot_scene_archives(
        data_root_path_, save_slot(pending_slot_), *scene_snapshot);
    if (!written) {
        show_error(
            std::string{persistence::persistence_status_message(written.status)},
            LegacyGameView::game_menu);
        return;
    }
    if (world_session_ != nullptr) {
        world_session_->sync_persistent_state(menu_return_view_ == LegacyGameView::world);
    }
    const auto ranger_snapshot = game_state_.export_snapshot();
    if (!ranger_snapshot.has_value()) {
        show_error("No game state is available to save", LegacyGameView::game_menu);
        return;
    }
    written = persistence::write_numbered_slot_ranger(
        data_root_path_, save_slot(pending_slot_), *ranger_snapshot);
    if (!written) {
        show_error(
            std::string{persistence::persistence_status_message(written.status)},
            LegacyGameView::game_menu);
    }
}

bool LegacyGameRuntime::activate_pending_load() {
    if (!pending_loaded_snapshot_.has_value()) {
        load_transition_phase_ = LoadTransitionPhase::none;
        show_error("No loaded snapshot is available", load_return_view_);
        return false;
    }

    auto loaded_snapshot = std::move(*pending_loaded_snapshot_);
    pending_loaded_snapshot_.reset();
    pending_new_game_wait_present_ = false;
    pending_new_game_scene_start_ = false;
    const auto replacing_scene = scene_session_ != nullptr;
    if (scene_session_ != nullptr) {
        physical_power_counter_ = scene_session_->physical_power_counter();
    } else if (world_session_ != nullptr) {
        physical_power_counter_ = world_session_->physical_power_counter();
    }
    world_session_.reset();
    scene_session_.reset();
    world_menu_event_session_.reset();
    battle_session_.reset();
    scene_request_.reset();
    battle_request_.reset();
    pending_menu_item_slot_.reset();
    pending_menu_item_id_.reset();
    pending_menu_item_role_.reset();
    pending_menu_item_effect_.reset();
    world_menu_event_phase_ = WorldMenuEventPhase::none;
    scene_leave_event_phase_ = SceneLeaveEventPhase::none;
    world_menu_event_script_id_.reset();
    scene_leave_event_script_id_.reset();
    leave_protagonist_notice_pending_ = false;
    scene_audio_commands_.clear();
    if (!game_state_.import_snapshot(std::move(loaded_snapshot))) {
        load_transition_phase_ = LoadTransitionPhase::none;
        show_error("Save snapshot import failed", load_return_view_);
        return false;
    }
    update_menu_counts();
    if (!start_world(load_return_view_)) {
        load_transition_phase_ = LoadTransitionPhase::none;
        return false;
    }
    if (replacing_scene) {
        clear_scene_exit_key_states_requested_ = true;
    }
    load_transition_phase_ = LoadTransitionPhase::first_black_present;
    begin_scene_effect(SceneEffectKind::present, 1U);
    return true;
}

bool LegacyGameRuntime::start_world(const LegacyGameView error_return_view) {
    scene_request_.reset();
    battle_request_.reset();
    world_scene_transition_pending_ = false;
    world_scene_transition_presented_ = false;
    world_scene_return_pending_ = false;
    world_scene_return_presented_ = false;
    scene_session_.reset();
    battle_session_.reset();
    clear_scene_effect();
    world_step_processed_ = false;
    scene_direction_input_.reset();
    scene_interact_requested_ = false;
    scene_ui_requested_ = false;
    scene_idle_skip_requested_ = false;
    auto* ranger = game_state_.ranger();
    if (ranger == nullptr) {
        show_error("No game state is available for the world map", error_return_view);
        return false;
    }
    if (world_map_ == nullptr || !world_map_->valid()) {
        show_error("World map startup resources are unavailable", error_return_view);
        return false;
    }
    world_session_ = std::make_unique<world::WorldSession>(
        data_root_,
        *world_map_,
        *ranger,
        random_,
        startup_resources_.weather_sprites(),
        startup_resources_.palette());
    if (!world_session_->valid()) {
        show_error(world_session_->error(), error_return_view);
        world_session_.reset();
        return false;
    }
    world_session_->set_physical_power_counter(physical_power_counter_);
    game_menu_.set_context(ui::GameMenuContext::world);
    set_view(LegacyGameView::world, "world session started");
    diagnostics::log_info(
        "world session ready x=" + std::to_string(world_session_->world_x()) +
        " y=" + std::to_string(world_session_->world_y()) +
        " direction=" + std::string{world_direction_name(world_session_->direction())} +
        " frame=" + std::to_string(world_session_->player_frame()));
    return true;
}

bool LegacyGameRuntime::start_scene(
    const std::int16_t scene_id,
    const LegacyGameView error_return_view,
    const std::optional<scene::SceneEntryOverride> entry_override) {
    auto* snapshot = game_state_.snapshot();
    if (snapshot == nullptr) {
        show_error("No game state is available for the scene", error_return_view);
        return false;
    }
    if (world_session_ != nullptr) {
        physical_power_counter_ = world_session_->physical_power_counter();
        scene_entry_world_direction_ = world_session_->direction();
        snapshot->ranger.header.set_word(
            model::header_word::face_towards,
            static_cast<std::int16_t>(*scene_entry_world_direction_));
    }
    battle_session_.reset();
    battle_request_.reset();
    clear_scene_effect();
    scene_direction_input_.reset();
    scene_interact_requested_ = false;
    scene_ui_requested_ = false;
    scene_idle_skip_requested_ = false;
    scene_session_ = std::make_unique<scene::SceneSession>(
        data_root_,
        *snapshot,
        random_,
        scene_id,
        false,
        std::nullopt,
        periodic_counter_,
        entry_override);
    if (!scene_session_->valid()) {
        show_error(scene_session_->error(), error_return_view);
        scene_session_.reset();
        return false;
    }
    scene_session_->set_physical_power_counter(physical_power_counter_);
    game_menu_.set_context(ui::GameMenuContext::scene);
    set_view(LegacyGameView::scene, "scene session started");
    diagnostics::log_info(
        "scene session ready id=" + std::to_string(scene_session_->scene_id()) +
        " x=" + std::to_string(scene_session_->scene_x()) +
        " y=" + std::to_string(scene_session_->scene_y()) +
        " frame=" + std::to_string(scene_session_->player_frame()));
    handle_scene_result(scene_session_->pending());
    return true;
}

bool LegacyGameRuntime::start_battle(
    const std::int16_t battle_id, const bool grant_experience) {
    auto* ranger = game_state_.ranger();
    if (ranger == nullptr || scene_session_ == nullptr) {
        show_error("No game state is available for battle", LegacyGameView::scene);
        return false;
    }
    clear_scene_effect();
    battle_session_ = std::make_unique<battle::BattleSession>(
        data_root_,
        *ranger,
        random_,
        battle_id,
        grant_experience,
        retained_battle_render_state_);
    if (!battle_session_->valid()) {
        show_error(battle_session_->error(), LegacyGameView::scene);
        battle_session_.reset();
        return false;
    }
    set_view(LegacyGameView::battle, "battle session started");
    diagnostics::log_info(
        "battle session ready id=" + std::to_string(battle_id) +
        " phase=" +
        (battle_session_->phase() == battle::BattleSessionPhase::party_selection
             ? std::string{"party_selection"}
             : std::string{"initial_present"}));
    return true;
}

void LegacyGameRuntime::finish_battle_if_ready() {
    if (battle_session_ == nullptr || !battle_session_->finished()) {
        return;
    }
    const auto battle_id = battle_session_->battle_id();
    const auto result = battle_session_->result();
    std::int16_t restore_music = 0;
    if (const auto* ranger = game_state_.ranger();
        ranger != nullptr && scene_session_ != nullptr && scene_session_->scene_id() >= 0 &&
        static_cast<std::size_t>(scene_session_->scene_id()) < ranger->scenes.size()) {
        restore_music = ranger->scenes[static_cast<std::size_t>(
            scene_session_->scene_id())].word(model::scene_metadata_word::entrance_music);
        if (restore_music < 0) {
            restore_music = 0;
        }
    }
    diagnostics::log_info(
        "battle runtime completion id=" + std::to_string(battle_id) +
        " result=" + std::to_string(static_cast<int>(result)) +
        " restore_music=" + std::to_string(restore_music));
    retained_battle_render_state_ = battle_session_->render_state();
    battle_session_.reset();
    battle_request_.reset();
    scene_audio_commands_.push_back(scene::SceneAudioCommand{
        scene::SceneAudioCommand::Kind::music, restore_music});
    if (scene_session_ == nullptr) {
        show_error("No scene session is available after battle", LegacyGameView::world);
        return;
    }
    set_view(LegacyGameView::scene, "battle session completed");
    handle_scene_result(scene_session_->resume(
        result == battle::BattleStepResult::victory
            ? scene::SceneResponse::battle_victory
            : scene::SceneResponse::battle_defeat));
}

void LegacyGameRuntime::handle_scene_result(const scene::SceneStepResult& result) {
    diagnostics::log_info(
        "scene result kind=" + std::string{scene_step_name(result.kind)} +
        " scene=" + std::to_string(result.scene_id) +
        " x=" + std::to_string(result.scene_x) +
        " y=" + std::to_string(result.scene_y) +
        " battle=" + std::to_string(result.battle_id) +
        " wait_ticks=" + std::to_string(result.wait_ticks));
    if (scene_session_ != nullptr) {
        periodic_counter_ = scene_session_->periodic_counter();
        physical_power_counter_ = scene_session_->physical_power_counter();
        auto commands = scene_session_->take_audio_commands();
        scene_audio_commands_.insert(
            scene_audio_commands_.end(),
            std::make_move_iterator(commands.begin()),
            std::make_move_iterator(commands.end()));
    }
    switch (result.kind) {
    case scene::SceneStepKind::return_world: {
        if (result.save_slot >= 0 && result.save_slot <= 2) {
            pending_scene_load_slot_ = static_cast<std::uint8_t>(result.save_slot);
        }
        const auto restored_direction = scene_entry_world_direction_.has_value()
            ? std::optional<world::WorldDirection>{
                  opposite_world_direction(*scene_entry_world_direction_)}
            : std::nullopt;
        scene_entry_world_direction_.reset();
        scene_session_.reset();

        const auto retained_world = world_session_ != nullptr && world_map_ != nullptr;
        if (retained_world) {
            world_session_->set_physical_power_counter(physical_power_counter_);
            scene_request_.reset();
            battle_request_.reset();
            clear_scene_effect();
            world_step_processed_ = false;
            scene_direction_input_.reset();
            scene_interact_requested_ = false;
            scene_ui_requested_ = false;
            game_menu_.set_context(ui::GameMenuContext::world);
            set_view(LegacyGameView::world, "scene returned to retained world");
            if (restored_direction.has_value()) {
                world_session_->restore_direction_after_scene(*restored_direction);
            }
            world_scene_return_pending_ = true;
            world_scene_return_presented_ = false;
            clear_scene_effect();
        } else {
            world_move_continuation_.reset();
            if (start_world(LegacyGameView::scene)) {
                world_scene_return_pending_ = true;
                world_scene_return_presented_ = false;
            }
        }
        break;
    }
    case scene::SceneStepKind::quit:
        clear_scene_exit_key_states_requested_ = true;
        clear_scene_effect();
        ending_complete_ = result.ending_complete;
        fade_music_on_exit_ = result.ending_complete;
        set_view(LegacyGameView::exited, "scene requested quit");
        break;
    case scene::SceneStepKind::load_slot:
        if (result.save_slot >= 0 && result.save_slot <= 2) {
            pending_slot_ = static_cast<std::uint8_t>(result.save_slot);
            pending_io_ = PendingIo::load;
            pending_io_wait_presented_ = true;
            error_return_view_ = LegacyGameView::scene;
            perform_pending_io();
        }
        break;
    case scene::SceneStepKind::battle:
        battle_request_ = result.battle_id;
        static_cast<void>(start_battle(result.battle_id, result.battle_get_exp != 0));
        break;
    case scene::SceneStepKind::present:
        begin_scene_effect(SceneEffectKind::present, result.wait_ticks);
        break;
    case scene::SceneStepKind::fade_from_black:
        begin_scene_effect(SceneEffectKind::fade_from_black, result.wait_ticks);
        break;
    case scene::SceneStepKind::fade_to_black:
        if (scene_session_ != nullptr && scene_session_->exit_transition_pending()) {
            clear_scene_exit_key_states_requested_ = true;
        }
        begin_scene_effect(SceneEffectKind::fade_to_black, result.wait_ticks);
        break;
    case scene::SceneStepKind::open_ui:
        update_menu_counts();
        game_menu_.set_context(ui::GameMenuContext::scene);
        game_menu_.show_main();
        menu_return_view_ = LegacyGameView::scene;
        set_view(LegacyGameView::game_menu, "scene requested UI");
        break;
    case scene::SceneStepKind::stay:
    case scene::SceneStepKind::moved:
        if (scene_leave_event_phase_ == SceneLeaveEventPhase::running) {
            scene_leave_event_phase_ = SceneLeaveEventPhase::fade_to_black;
            begin_scene_effect(SceneEffectKind::fade_to_black, 1U);
        }
        break;
    case scene::SceneStepKind::scene_title:
    case scene::SceneStepKind::dialogue:
    case scene::SceneStepKind::question:
    case scene::SceneStepKind::wait_key:
    case scene::SceneStepKind::load_menu:
    case scene::SceneStepKind::death_menu:
    case scene::SceneStepKind::notice:
    case scene::SceneStepKind::shop:
        break;
    }
}

bool LegacyGameRuntime::advance_scene_effect() {
    if (scene_effect_kind_ == SceneEffectKind::none) {
        return false;
    }
    if (!scene_effect_presented_) {
        return true;
    }
    if (scene_effect_kind_ == SceneEffectKind::present && scene_effect_wait_ticks_ > 1U) {
        --scene_effect_wait_ticks_;
        return true;
    }
    if (scene_effect_kind_ != SceneEffectKind::present &&
        scene_effect_frame_ + 1U < scene_effect_palettes_.size()) {
        ++scene_effect_frame_;
        scene_effect_presented_ = false;
        return true;
    }
    if (scene_effect_kind_ != SceneEffectKind::present && scene_effect_wait_ticks_ > 1U) {
        --scene_effect_wait_ticks_;
        return true;
    }
    clear_scene_effect();
    if (pending_name_accept_) {
        pending_name_accept_ = false;
        auto* ranger = game_state_.ranger();
        if (ranger == nullptr) {
            show_error("No protagonist is available after name entry", LegacyGameView::title);
        } else {
            attribute_controller_ = std::make_unique<ui::NewGameAttributeController>(
                ranger->roles[0], random_);
            set_view(LegacyGameView::attributes, "name entry delay completed");
        }
    } else if (pending_new_game_wait_present_) {
        pending_new_game_wait_present_ = false;
        pending_new_game_scene_start_ = true;
        begin_scene_effect(SceneEffectKind::fade_to_black, 1U);
    } else if (pending_new_game_scene_start_) {
        pending_new_game_scene_start_ = false;
        attribute_controller_.reset();
        update_menu_counts();
        static_cast<void>(start_scene(
            70,
            LegacyGameView::title,
            scene::SceneEntryOverride{
                19, 20, scene::SceneDirection::right, 6890, 691}));
    } else if (title_startup_phase_ == TitleStartupPhase::fade_to_black) {
        title_startup_phase_ = TitleStartupPhase::black_menu_present;
        begin_scene_effect(SceneEffectKind::present, 1U);
    } else if (title_startup_phase_ == TitleStartupPhase::black_menu_present) {
        title_startup_phase_ = TitleStartupPhase::fade_from_black;
        begin_scene_effect(SceneEffectKind::fade_from_black, 1U);
    } else if (title_startup_phase_ == TitleStartupPhase::fade_from_black) {
        title_startup_phase_ = TitleStartupPhase::none;
    } else if (pending_title_result_.has_value()) {
        const auto result = *pending_title_result_;
        pending_title_result_.reset();
        if (result.command == ui::TitleCommand::start_new_game) {
            begin_new_game();
        } else if (result.command == ui::TitleCommand::load_slot) {
            pending_slot_ = result.slot;
            pending_io_ = PendingIo::load;
            pending_io_wait_presented_ = false;
            error_return_view_ = LegacyGameView::title;
            title_menu_.show_please_wait();
        }
    } else if (load_transition_phase_ == LoadTransitionPhase::fade_to_black) {
        static_cast<void>(activate_pending_load());
    } else if (load_transition_phase_ == LoadTransitionPhase::first_black_present) {
        load_transition_phase_ = LoadTransitionPhase::second_black_present;
        begin_scene_effect(SceneEffectKind::present, 1U);
    } else if (load_transition_phase_ == LoadTransitionPhase::second_black_present) {
        load_transition_phase_ = LoadTransitionPhase::fade_from_black;
        begin_scene_effect(SceneEffectKind::fade_from_black, 1U);
    } else if (load_transition_phase_ == LoadTransitionPhase::fade_from_black) {
        load_transition_phase_ = LoadTransitionPhase::none;
        if (load_return_view_ == LegacyGameView::game_menu) {
            menu_return_view_ = LegacyGameView::world;
            game_menu_.set_context(ui::GameMenuContext::world);
            set_view(LegacyGameView::game_menu, "loaded world behind system menu");
        } else {
            return false;
        }
    } else if (leave_protagonist_notice_pending_) {
        leave_protagonist_notice_pending_ = false;
        game_menu_.show_notice(ui::GameMenuNotice::leave_protagonist);
        set_view(LegacyGameView::game_menu, "show protagonist leave notice");
    } else if (world_menu_event_phase_ == WorldMenuEventPhase::leave_pre_script_present) {
        if (world_menu_event_session_ == nullptr ||
            !world_menu_event_script_id_.has_value()) {
            world_menu_event_session_.reset();
            world_menu_event_script_id_.reset();
            world_menu_event_phase_ = WorldMenuEventPhase::none;
        } else {
            const auto script_id = *world_menu_event_script_id_;
            world_menu_event_script_id_.reset();
            world_menu_event_phase_ = WorldMenuEventPhase::running;
            handle_world_menu_event_result(
                world_menu_event_session_->begin_event(script_id));
        }
    } else if (scene_leave_event_phase_ == SceneLeaveEventPhase::pre_script_present) {
        if (scene_session_ == nullptr || !scene_leave_event_script_id_.has_value()) {
            scene_leave_event_script_id_.reset();
            scene_leave_event_phase_ = SceneLeaveEventPhase::none;
        } else {
            const auto script_id = *scene_leave_event_script_id_;
            scene_leave_event_script_id_.reset();
            scene_leave_event_phase_ = SceneLeaveEventPhase::running;
            handle_scene_result(scene_session_->begin_event(script_id));
        }
    } else if (world_menu_event_phase_ == WorldMenuEventPhase::leave_post_fade_to_black) {
        world_menu_event_session_.reset();
        world_menu_event_phase_ = WorldMenuEventPhase::leave_post_redraw_present;
        begin_scene_effect(SceneEffectKind::present, 1U);
    } else if (world_menu_event_phase_ == WorldMenuEventPhase::leave_post_redraw_present) {
        world_menu_event_phase_ = WorldMenuEventPhase::leave_post_fade_from_black;
        begin_scene_effect(SceneEffectKind::fade_from_black, 1U);
    } else if (world_menu_event_phase_ == WorldMenuEventPhase::leave_post_fade_from_black) {
        world_menu_event_phase_ = WorldMenuEventPhase::none;
        update_menu_counts();
        game_menu_.set_context(ui::GameMenuContext::world);
        game_menu_.show_main();
        set_view(LegacyGameView::game_menu, "leave-party event returned to world menu");
    } else if (scene_leave_event_phase_ == SceneLeaveEventPhase::fade_to_black) {
        scene_leave_event_phase_ = SceneLeaveEventPhase::redraw_present;
        begin_scene_effect(SceneEffectKind::present, 1U);
    } else if (scene_leave_event_phase_ == SceneLeaveEventPhase::redraw_present) {
        scene_leave_event_phase_ = SceneLeaveEventPhase::fade_from_black;
        begin_scene_effect(SceneEffectKind::fade_from_black, 1U);
    } else if (scene_leave_event_phase_ == SceneLeaveEventPhase::fade_from_black) {
        scene_leave_event_phase_ = SceneLeaveEventPhase::none;
        update_menu_counts();
        game_menu_.set_context(ui::GameMenuContext::scene);
        game_menu_.show_main();
        set_view(LegacyGameView::game_menu, "leave-party event returned to scene menu");
    } else if (world_menu_event_phase_ == WorldMenuEventPhase::present ||
               world_menu_event_phase_ == WorldMenuEventPhase::fade_to_black ||
               world_menu_event_phase_ == WorldMenuEventPhase::fade_from_black) {
        world_menu_event_phase_ = WorldMenuEventPhase::running;
        if (world_menu_event_session_ != nullptr) {
            handle_world_menu_event_result(
                world_menu_event_session_->resume(scene::SceneResponse::acknowledge));
        }
    } else if (world_scene_transition_pending_) {
        world_scene_transition_pending_ = false;
        world_scene_transition_presented_ = false;
        if (!scene_request_.has_value() ||
            !start_scene(*scene_request_, LegacyGameView::world)) {
            world_move_continuation_.reset();
        }
    } else if (world_scene_return_pending_) {
        world_scene_return_pending_ = false;
        world_scene_return_presented_ = false;
        const auto continuation = world_move_continuation_;
        world_move_continuation_.reset();
        if (continuation.has_value() && world_session_ != nullptr) {
            const auto resumed = world_session_->resume_move_after_scene(*continuation);
            diagnostics::log_info(
                "world move resumed after scene result=" +
                std::string{world_step_name(resumed.kind)} +
                " x=" + std::to_string(resumed.world_x) +
                " y=" + std::to_string(resumed.world_y) +
                " frame=" + std::to_string(world_session_->player_frame()));
            world_session_->periodic_tick();
            world_session_->idle_animation_tick();
        }
        if (pending_scene_load_slot_.has_value()) {
            pending_slot_ = *pending_scene_load_slot_;
            pending_scene_load_slot_.reset();
            pending_io_ = PendingIo::load;
            pending_io_wait_presented_ = true;
            error_return_view_ = LegacyGameView::world;
            framebuffer_.clear(0U);
            perform_pending_io();
        }
    } else if (view_ == LegacyGameView::scene && scene_session_ != nullptr) {
        handle_scene_result(scene_session_->resume(scene::SceneResponse::acknowledge));
    }
    return true;
}

void LegacyGameRuntime::begin_scene_effect(
    const SceneEffectKind kind, const std::uint16_t wait_ticks) {
    scene_effect_kind_ = kind;
    scene_effect_palettes_.clear();
    scene_effect_frame_ = 0U;
    scene_effect_wait_ticks_ = std::max<std::uint16_t>(wait_ticks, 1U);
    scene_effect_presented_ = false;
}

void LegacyGameRuntime::clear_scene_effect() noexcept {
    scene_effect_kind_ = SceneEffectKind::none;
    scene_effect_palettes_.clear();
    scene_effect_frame_ = 0U;
    scene_effect_wait_ticks_ = 1U;
    scene_effect_presented_ = false;
}

std::vector<scene::SceneAudioCommand> LegacyGameRuntime::take_scene_audio_commands() {
    auto commands = std::move(scene_audio_commands_);
    scene_audio_commands_.clear();
    return commands;
}

std::vector<battle::BattleAudioCommand> LegacyGameRuntime::take_battle_audio_commands() {
    return battle_session_ != nullptr
        ? battle_session_->take_audio_commands()
        : std::vector<battle::BattleAudioCommand>{};
}

void LegacyGameRuntime::update_menu_counts() {
    const auto* ranger = game_state_.ranger();
    if (ranger == nullptr) {
        return;
    }
    std::uint8_t party_count = 1U;
    while (party_count < model::kTeamMemberCount &&
           ranger->header.team_member(party_count).value > 0) {
        ++party_count;
    }
    game_menu_.set_party_count(party_count);
    std::array<std::int16_t, 6U> medicine_abilities{};
    std::array<std::int16_t, 6U> detoxification_abilities{};
    for (std::uint8_t slot = 0U; slot < party_count; ++slot) {
        const auto role_id = ranger->header.team_member(slot).value;
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger->roles.size()) {
            continue;
        }
        const auto& role = ranger->roles[static_cast<std::size_t>(role_id)];
        medicine_abilities[slot] = role.word(model::role_word::medicine);
        detoxification_abilities[slot] = role.word(model::role_word::detoxification);
    }
    game_menu_.set_party_abilities(medicine_abilities, detoxification_abilities);

    std::uint16_t inventory_count = 0U;
    while (inventory_count < model::kInventoryCount &&
           ranger->header.inventory_item(inventory_count).value >= 0) {
        ++inventory_count;
    }
    game_menu_.set_inventory_count(inventory_count);
}

void LegacyGameRuntime::set_view(
    const LegacyGameView view, const std::string_view reason) {
    if (view_ == view) {
        return;
    }
    diagnostics::log_info(
        "view " + std::string{view_name(view_)} + " -> " + std::string{view_name(view)} +
        " reason=" + std::string{reason});
    view_ = view;
}

void LegacyGameRuntime::show_error(
    std::string message, const LegacyGameView return_view) {
    diagnostics::log_error(
        "runtime error return_view=" + std::string{view_name(return_view)} +
        " message=" + message);
    show_legacy_error(legacy_ascii(message), return_view);
}

void LegacyGameRuntime::show_legacy_error(
    const std::span<const std::uint8_t> message,
    const LegacyGameView return_view) {
    if (return_view == LegacyGameView::title) {
        title_menu_.show_main();
    }
    visible_error_.assign(message.begin(), message.end());
    error_return_view_ = return_view;
    set_view(LegacyGameView::error, "show legacy error");
}

void LegacyGameRuntime::handle_title_result(const ui::TitleResult result) {
    switch (result.command) {
    case ui::TitleCommand::none: break;
    case ui::TitleCommand::start_new_game:
    case ui::TitleCommand::load_slot:
        pending_title_result_ = result;
        begin_scene_effect(SceneEffectKind::present, 1U);
        break;
    case ui::TitleCommand::exit_game:
        fade_music_on_exit_ = false;
        set_view(LegacyGameView::exited, "title exit");
        break;
    }
}

void LegacyGameRuntime::handle_game_menu_result(const ui::GameMenuResult result) {
    switch (result.command) {
    case ui::GameMenuCommand::none:
        if (game_menu_.screen() == ui::GameMenuScreen::items ||
            game_menu_.screen() == ui::GameMenuScreen::main) {
            pending_menu_item_slot_.reset();
            pending_menu_item_id_.reset();
            pending_menu_item_role_.reset();
            pending_menu_item_effect_.reset();
        }
        break;
    case ui::GameMenuCommand::status:
        break;
    case ui::GameMenuCommand::medicine:
    case ui::GameMenuCommand::detoxification: {
        auto* ranger = game_state_.ranger();
        if (ranger == nullptr || result.slot >= model::kTeamMemberCount ||
            result.index >= model::kTeamMemberCount) {
            break;
        }
        const auto actor_role_id = ranger->header.team_member(result.slot).value;
        const auto target_role_id = ranger->header.team_member(result.index).value;
        const auto amount = result.command == ui::GameMenuCommand::medicine
            ? battle::apply_role_medicine_value(
                  *ranger, actor_role_id, target_role_id, random_)
            : std::optional<std::int32_t>{battle::apply_role_detox_value(
                  *ranger, actor_role_id, target_role_id, random_)};
        if (amount.has_value()) {
            game_menu_.complete_party_action(*amount);
        }
        break;
    }
    case ui::GameMenuCommand::items:
        handle_menu_item_result(result);
        break;
    case ui::GameMenuCommand::leave_party: {
        const auto* ranger = game_state_.ranger();
        if (ranger == nullptr || result.index >= model::kTeamMemberCount) {
            break;
        }
        const auto role_id = ranger->header.team_member(result.index).value;
        if (role_id == 0) {
            leave_protagonist_notice_pending_ = true;
            set_view(menu_return_view_, "present background before protagonist leave notice");
            begin_scene_effect(SceneEffectKind::present, 1U);
            break;
        }
        const auto entry = std::find(kLeavePartyRoles.begin(), kLeavePartyRoles.end(), role_id);
        if (entry == kLeavePartyRoles.end()) {
            break;
        }
        const auto script_id = static_cast<std::int16_t>(
            950 + 2 * std::distance(kLeavePartyRoles.begin(), entry));
        if (menu_return_view_ == LegacyGameView::world) {
            set_view(LegacyGameView::world, "run world leave-party event");
            if (!begin_world_leave_event(role_id)) {
                show_error("Failed to start leave-party event.", LegacyGameView::game_menu);
            }
        } else if (menu_return_view_ == LegacyGameView::scene && scene_session_ != nullptr) {
            scene_leave_event_script_id_ = script_id;
            scene_leave_event_phase_ = SceneLeaveEventPhase::pre_script_present;
            set_view(LegacyGameView::scene, "present scene before leave-party event");
            begin_scene_effect(SceneEffectKind::present, 1U);
        }
        break;
    }
    case ui::GameMenuCommand::resume:
        set_view(menu_return_view_, "resume from menu");
        if (menu_return_view_ == LegacyGameView::scene && scene_session_ != nullptr &&
            scene_session_->pending().kind == scene::SceneStepKind::open_ui) {
            handle_scene_result(scene_session_->resume(scene::SceneResponse::acknowledge));
        }
        break;
    case ui::GameMenuCommand::load_slot:
        pending_slot_ = result.slot;
        pending_io_ = PendingIo::load;
        pending_io_wait_presented_ = false;
        error_return_view_ = LegacyGameView::game_menu;
        break;
    case ui::GameMenuCommand::save_slot:
        pending_slot_ = result.slot;
        pending_io_ = PendingIo::save;
        pending_io_wait_presented_ = false;
        error_return_view_ = LegacyGameView::game_menu;
        break;
    case ui::GameMenuCommand::exit_game:
        fade_music_on_exit_ = true;
        if (menu_return_view_ == LegacyGameView::world && world_session_ != nullptr) {
            pending_world_exit_ = true;
            world_step_processed_ = true;
            set_view(LegacyGameView::world, "world system exit final frame");
        } else {
            set_view(LegacyGameView::exited, "game menu exit");
        }
        break;
    }
}

void LegacyGameRuntime::handle_menu_item_result(const ui::GameMenuResult result) {
    auto* ranger = game_state_.ranger();
    if (ranger == nullptr) {
        return;
    }
    const auto clear_pending = [this]() noexcept {
        pending_menu_item_slot_.reset();
        pending_menu_item_id_.reset();
        pending_menu_item_role_.reset();
        pending_menu_item_effect_.reset();
    };

    if (!pending_menu_item_slot_.has_value()) {
        if (result.index >= model::kInventoryCount) {
            return;
        }
        const auto item_id = ranger->header.inventory_item(result.index).value;
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger->items.size()) {
            return;
        }
        const auto& item = ranger->items[static_cast<std::size_t>(item_id)];
        if (item.word(model::item_word::show_introduction) != 1) {
            game_menu_.show_items();
            return;
        }
        const auto item_type = item.word(model::item_word::item_type);
        if (item_type == 0 && menu_return_view_ == LegacyGameView::scene &&
            scene_session_ != nullptr) {
            clear_pending();
            set_view(LegacyGameView::scene, "scene event item selected");
            handle_scene_result(scene_session_->use_menu_item(item_id));
            return;
        }
        pending_menu_item_slot_ = result.index;
        pending_menu_item_id_ = item_id;
        if (item_type == 1) {
            game_menu_.begin_item_target_selection(ui::GameMenuItemTargetKind::equipment);
        } else if (item_type == 2) {
            if (item.word(model::item_word::user) >= 0) {
                game_menu_.show_item_confirmation(
                    ui::GameMenuItemConfirmation::practice_reassign);
            } else {
                game_menu_.begin_item_target_selection(ui::GameMenuItemTargetKind::practice);
            }
        } else if (item_type == 3) {
            game_menu_.begin_item_target_selection(ui::GameMenuItemTargetKind::consumable);
        } else {
            clear_pending();
            game_menu_.show_main();
        }
        return;
    }

    if (!pending_menu_item_id_.has_value() || result.slot >= model::kTeamMemberCount) {
        clear_pending();
        game_menu_.show_main();
        return;
    }
    const auto role_id = ranger->header.team_member(result.slot).value;
    const auto item_id = *pending_menu_item_id_;
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger->roles.size() ||
        item_id < 0 || static_cast<std::size_t>(item_id) >= ranger->items.size()) {
        clear_pending();
        game_menu_.show_main();
        return;
    }
    pending_menu_item_role_ = role_id;
    auto& role = ranger->roles[static_cast<std::size_t>(role_id)];
    const auto& item = ranger->items[static_cast<std::size_t>(item_id)];
    const auto item_type = item.word(model::item_word::item_type);
    if (item_type == 1) {
        if (!battle::role_meets_item_requirements(*ranger, role_id, item_id)) {
            game_menu_.show_notice(ui::GameMenuNotice::equipment_unsuitable);
            return;
        }
        (void)battle::equip_role_item(*ranger, role_id, item_id);
        clear_pending();
        game_menu_.show_main();
        return;
    }
    if (item_type == 2) {
        const auto magic_id = item.word(model::item_word::magic_id);
        bool known_magic = false;
        for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
            if (role.word(model::role_word::magic_id_begin + slot) == magic_id) {
                known_magic = true;
            }
        }
        if (magic_id != -1 &&
            role.word(model::role_word::magic_id_begin + model::role_word::magic_count - 1U) > 0 &&
            !known_magic) {
            game_menu_.show_notice(ui::GameMenuNotice::practice_magic_full);
            return;
        }
        if (!battle::role_meets_item_requirements(*ranger, role_id, item_id)) {
            game_menu_.show_notice(ui::GameMenuNotice::practice_unsuitable);
            return;
        }
        const auto record_item_id = item.word(model::item_word::id);
        if ((record_item_id == 78 || record_item_id == 93) &&
            role.word(model::role_word::sexual) == 0) {
            game_menu_.show_item_confirmation(
                ui::GameMenuItemConfirmation::practice_castration);
            return;
        }
        (void)battle::assign_role_practice_item(*ranger, role_id, item_id);
        clear_pending();
        game_menu_.show_main();
        return;
    }
    if (item_type == 3) {
        auto effect = battle::apply_role_item_effect(
            *ranger, role_id, role_id, item_id, random_);
        if (effect.has_value() && effect->has_effect) {
            if (battle::consume_inventory_item_slot(*ranger, *pending_menu_item_slot_)) {
                effect->item_consumed = true;
            }
            pending_menu_item_effect_ = *effect;
            update_menu_counts();
            game_menu_.show_item_effect();
        } else {
            clear_pending();
            game_menu_.show_main();
        }
        return;
    }
    clear_pending();
    game_menu_.show_main();
}

void LegacyGameRuntime::handle_menu_item_confirmation(
    const std::uint8_t translated_key) {
    const auto confirmation = game_menu_.item_confirmation();
    auto* ranger = game_state_.ranger();
    if (translated_key != static_cast<std::uint8_t>('Y') || ranger == nullptr) {
        pending_menu_item_slot_.reset();
        pending_menu_item_id_.reset();
        pending_menu_item_role_.reset();
        pending_menu_item_effect_.reset();
        game_menu_.show_main();
        return;
    }
    if (confirmation == ui::GameMenuItemConfirmation::practice_reassign) {
        game_menu_.begin_item_target_selection(ui::GameMenuItemTargetKind::practice);
        return;
    }
    if (pending_menu_item_id_.has_value() && pending_menu_item_role_.has_value() &&
        *pending_menu_item_id_ >= 0 && *pending_menu_item_role_ >= 0 &&
        static_cast<std::size_t>(*pending_menu_item_id_) < ranger->items.size() &&
        static_cast<std::size_t>(*pending_menu_item_role_) < ranger->roles.size()) {
        auto& role = ranger->roles[static_cast<std::size_t>(*pending_menu_item_role_)];
        role.set_word(model::role_word::sexual, 2);
        (void)battle::assign_role_practice_item(
            *ranger, *pending_menu_item_role_, *pending_menu_item_id_);
    }
    pending_menu_item_slot_.reset();
    pending_menu_item_id_.reset();
    pending_menu_item_role_.reset();
    pending_menu_item_effect_.reset();
    game_menu_.show_main();
}

bool LegacyGameRuntime::begin_world_leave_event(const std::int16_t role_id) {
    auto* snapshot = game_state_.snapshot();
    const auto entry = std::find(kLeavePartyRoles.begin(), kLeavePartyRoles.end(), role_id);
    if (snapshot == nullptr || world_session_ == nullptr || entry == kLeavePartyRoles.end()) {
        return false;
    }
    const auto script_id = static_cast<std::int16_t>(
        950 + 2 * std::distance(kLeavePartyRoles.begin(), entry));
    clear_scene_effect();
    world_menu_event_session_ = std::make_unique<scene::SceneSession>(
        data_root_,
        *snapshot,
        random_,
        0,
        false,
        std::nullopt,
        periodic_counter_,
        std::nullopt,
        scene::SceneSessionContext::world_event_overlay);
    if (!world_menu_event_session_->valid()) {
        world_menu_event_session_.reset();
        world_menu_event_phase_ = WorldMenuEventPhase::none;
        return false;
    }
    world_menu_event_script_id_ = script_id;
    world_menu_event_phase_ = WorldMenuEventPhase::leave_pre_script_present;
    begin_scene_effect(SceneEffectKind::present, 1U);
    return true;
}

void LegacyGameRuntime::handle_world_menu_event_result(
    const scene::SceneStepResult& result) {
    if (world_menu_event_session_ == nullptr) {
        world_menu_event_phase_ = WorldMenuEventPhase::none;
        return;
    }
    auto commands = world_menu_event_session_->take_audio_commands();
    scene_audio_commands_.insert(
        scene_audio_commands_.end(),
        std::make_move_iterator(commands.begin()),
        std::make_move_iterator(commands.end()));
    switch (result.kind) {
    case scene::SceneStepKind::dialogue:
    case scene::SceneStepKind::notice:
    case scene::SceneStepKind::question:
    case scene::SceneStepKind::wait_key:
        world_menu_event_phase_ = WorldMenuEventPhase::running;
        break;
    case scene::SceneStepKind::present:
        world_menu_event_phase_ = WorldMenuEventPhase::present;
        begin_scene_effect(SceneEffectKind::present, result.wait_ticks);
        break;
    case scene::SceneStepKind::fade_to_black:
        world_menu_event_phase_ = WorldMenuEventPhase::fade_to_black;
        begin_scene_effect(SceneEffectKind::fade_to_black, result.wait_ticks);
        break;
    case scene::SceneStepKind::fade_from_black:
        world_menu_event_phase_ = WorldMenuEventPhase::fade_from_black;
        begin_scene_effect(SceneEffectKind::fade_from_black, result.wait_ticks);
        break;
    case scene::SceneStepKind::stay:
    case scene::SceneStepKind::moved:
    case scene::SceneStepKind::return_world:
        periodic_counter_ = world_menu_event_session_->periodic_counter();
        world_menu_event_phase_ = WorldMenuEventPhase::leave_post_fade_to_black;
        update_menu_counts();
        begin_scene_effect(SceneEffectKind::fade_to_black, 1U);
        break;
    default:
        world_menu_event_session_.reset();
        world_menu_event_phase_ = WorldMenuEventPhase::none;
        break;
    }
}

}  // namespace openlegend::app
