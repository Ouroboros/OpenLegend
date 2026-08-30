#include "openlegend/app/legacy_game_runtime.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>

#include "openlegend/diagnostics/log.hpp"
#include "openlegend/model/new_game.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/render/legacy_effects.hpp"

namespace openlegend::app {
namespace {

constexpr std::array<std::uint8_t, 26> kCannotLeaveProtagonist{
    0xA9U, 0xEAU, 0xBAU, 0x70U, 0xA1U, 0x49U, 0xA8U, 0x53U, 0xA6U,
    0xB3U, 0xA7U, 0x41U, 0xB9U, 0x43U, 0xC0U, 0xB8U, 0xB6U, 0x69U,
    0xA6U, 0xE6U, 0xA4U, 0xA3U, 0xA4U, 0x55U, 0xA5U, 0x68U};

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

}  // namespace

std::string_view ending_terminal_message() noexcept {
    return " Thanks for playing this game ! \n Oriental Software Studio 1996  \n";
}

LegacyGameRuntime::LegacyGameRuntime(
    std::filesystem::path data_root, const std::uint32_t random_seed)
    : data_root_path_(std::move(data_root)),
      data_root_(data_root_path_),
      random_(random_seed),
      title_renderer_(data_root_),
      basic_renderer_(data_root_) {
    if (!title_renderer_.valid()) {
        startup_error_ = title_renderer_.error();
    } else if (!basic_renderer_.valid()) {
        startup_error_ = basic_renderer_.error();
    }
    if (startup_error_.empty()) {
        diagnostics::log_info("LegacyGameRuntime initialized view=title");
    } else {
        diagnostics::log_error("LegacyGameRuntime initialization failed: " + startup_error_);
    }
}

void LegacyGameRuntime::advance() {
    if (pending_io_ != PendingIo::none) {
        perform_pending_io();
    }
    if (advance_scene_effect()) {
        world_step_processed_ = false;
        return;
    }
    if (view_ == LegacyGameView::world && world_session_ != nullptr) {
        if (!world_step_processed_) {
            world_session_->idle_tick();
        }
        world_session_->periodic_tick();
        world_session_->idle_animation_tick();
    } else if (view_ == LegacyGameView::scene && scene_session_ != nullptr &&
               scene_session_->pending().kind == scene::SceneStepKind::stay) {
        const auto direction = scene_direction_input_;
        scene_direction_input_.reset();
        bool interact_requested = false;
        bool ui_requested = false;
        if (!direction.has_value() && scene_interact_requested_) {
            interact_requested = true;
            scene_interact_requested_ = false;
        } else if (!direction.has_value() && scene_ui_requested_) {
            ui_requested = true;
            scene_ui_requested_ = false;
        }
        handle_scene_result(
            scene_session_->tick(direction, interact_requested, ui_requested));
    } else {
        scene_direction_input_.reset();
    }
    world_step_processed_ = false;
}

void LegacyGameRuntime::finish_presented_tick() {
    if (view_ != LegacyGameView::world || world_session_ == nullptr) {
        return;
    }
    periodic_counter_ = static_cast<std::int16_t>((periodic_counter_ + 1) % 5);
    if (periodic_counter_ == 1) {
        world_session_->cycle_palette();
    }
}

bool LegacyGameRuntime::handle_world_input(
    const bool left,
    const bool up,
    const bool down,
    const bool right,
    const bool menu_requested) {
    if (!valid() ||
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
        } else {
            scene_direction_input_.reset();
        }
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
        static_cast<void>(start_scene(result.scene_id, LegacyGameView::world));
    }
    return false;
}

void LegacyGameRuntime::handle_key(
    const std::uint8_t translated_key,
    const bool control_down,
    const bool shift_down) {
    if (!valid() || translated_key == 0U || view_ == LegacyGameView::exited) {
        return;
    }
    diagnostics::log_debug(
        "runtime key translated=" + std::to_string(translated_key) +
        " control=" + (control_down ? std::string{"true"} : std::string{"false"}) +
        " shift=" + (shift_down ? std::string{"true"} : std::string{"false"}) +
        " view=" + std::string{view_name(view_)});
    if (view_ == LegacyGameView::error) {
        visible_error_.clear();
        set_view(error_return_view_, "dismiss error");
        return;
    }

    switch (view_) {
    case LegacyGameView::title:
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
            attribute_controller_ = std::make_unique<ui::NewGameAttributeController>(
                ranger->roles[0], random_);
            set_view(LegacyGameView::attributes, "name entry accepted");
        }
        break;
    case LegacyGameView::attributes:
        if (attribute_controller_ != nullptr &&
            attribute_controller_->handle_key(translated_key) ==
                ui::AttributeRollStatus::accepted) {
            attribute_controller_.reset();
            update_menu_counts();
            static_cast<void>(start_world(LegacyGameView::title));
        }
        break;
    case LegacyGameView::world:
        if (translated_key == 0x1BU) {
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
            } else if (translated_key == 0x0DU || translated_key == 0x20U) {
                scene_interact_requested_ = true;
            }
        }
        break;
    }
    case LegacyGameView::game_menu:
        handle_game_menu_result(game_menu_.handle_key(translated_key));
        break;
    case LegacyGameView::error:
    case LegacyGameView::exited:
        break;
    }
}

bool LegacyGameRuntime::render() {
    if (!valid()) {
        return false;
    }
    switch (view_) {
    case LegacyGameView::title:
        return title_renderer_.render(title_menu_, framebuffer_);
    case LegacyGameView::name_entry:
        return name_editor_.has_value() &&
               basic_renderer_.render_name_entry(
                   title_renderer_, *name_editor_, framebuffer_);
    case LegacyGameView::attributes: {
        const auto* ranger = game_state_.ranger();
        return ranger != nullptr && name_editor_.has_value() &&
               basic_renderer_.render_attributes(
                   title_renderer_,
                   ranger->roles[0],
                   name_editor_->name(),
                   attribute_controller_ != nullptr && attribute_controller_->cheat_active(),
                   framebuffer_);
    }
    case LegacyGameView::world:
        return world_session_ != nullptr && world_session_->render(framebuffer_);
    case LegacyGameView::scene:
        if (scene_session_ == nullptr || !scene_session_->render(framebuffer_)) {
            return false;
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
    case LegacyGameView::game_menu: {
        const auto* ranger = game_state_.ranger();
        const auto base_rendered = menu_return_view_ == LegacyGameView::scene
                                       ? scene_session_ != nullptr && scene_session_->render(framebuffer_)
                                       : world_session_ != nullptr && world_session_->render(framebuffer_);
        return ranger != nullptr && base_rendered &&
               basic_renderer_.render_game_menu(game_menu_, *ranger, framebuffer_);
    }
    case LegacyGameView::error: {
        bool base_rendered = false;
        if (error_return_view_ == LegacyGameView::title) {
            base_rendered = title_renderer_.render(title_menu_, framebuffer_);
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
    attribute_controller_.reset();
    name_editor_.reset();
    world_session_.reset();
    world_map_.reset();
    scene_session_.reset();
    scene_request_.reset();
    battle_request_.reset();
    scene_audio_commands_.clear();
    clear_scene_effect();
    auto loaded = persistence::load_baseline(data_root_path_);
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
    if (operation == PendingIo::load) {
        attribute_controller_.reset();
        auto loaded = persistence::load_numbered_slot(
            data_root_path_, save_slot(pending_slot_));
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
        world_session_.reset();
        world_map_.reset();
        scene_session_.reset();
        scene_request_.reset();
        battle_request_.reset();
        scene_audio_commands_.clear();
        clear_scene_effect();
        if (!game_state_.import_snapshot(std::move(*loaded.snapshot))) {
            show_error("Save snapshot import failed", error_return_view_);
            return;
        }
        update_menu_counts();
        static_cast<void>(start_world(error_return_view_));
        return;
    }

    const auto snapshot = game_state_.export_snapshot();
    if (!snapshot.has_value()) {
        show_error("No game state is available to save", LegacyGameView::game_menu);
        return;
    }
    const auto written = persistence::write_numbered_slot(
        data_root_path_, save_slot(pending_slot_), *snapshot);
    if (!written) {
        show_error(
            std::string{persistence::persistence_status_message(written.status)},
            LegacyGameView::game_menu);
    }
}

bool LegacyGameRuntime::start_world(const LegacyGameView error_return_view) {
    scene_request_.reset();
    battle_request_.reset();
    scene_session_.reset();
    clear_scene_effect();
    world_step_processed_ = false;
    scene_direction_input_.reset();
    scene_interact_requested_ = false;
    scene_ui_requested_ = false;
    auto* ranger = game_state_.ranger();
    if (ranger == nullptr) {
        show_error("No game state is available for the world map", error_return_view);
        return false;
    }
    world_map_ = std::make_unique<world::WorldMapData>(data_root_);
    if (!world_map_->valid()) {
        show_error(world_map_->error(), error_return_view);
        return false;
    }
    world_session_ = std::make_unique<world::WorldSession>(
        data_root_, *world_map_, *ranger, random_);
    if (!world_session_->valid()) {
        show_error(world_session_->error(), error_return_view);
        world_session_.reset();
        world_map_.reset();
        return false;
    }
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
    const std::int16_t scene_id, const LegacyGameView error_return_view) {
    auto* snapshot = game_state_.snapshot();
    if (snapshot == nullptr) {
        show_error("No game state is available for the scene", error_return_view);
        return false;
    }
    if (world_session_ != nullptr) {
        scene_entry_world_direction_ = world_session_->direction();
    }
    world_session_.reset();
    world_map_.reset();
    battle_request_.reset();
    clear_scene_effect();
    scene_direction_input_.reset();
    scene_interact_requested_ = false;
    scene_ui_requested_ = false;
    scene_session_ = std::make_unique<scene::SceneSession>(
        data_root_, *snapshot, random_, scene_id, false, std::nullopt, periodic_counter_);
    if (!scene_session_->valid()) {
        show_error(scene_session_->error(), error_return_view);
        scene_session_.reset();
        return false;
    }
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
        auto commands = scene_session_->take_audio_commands();
        scene_audio_commands_.insert(
            scene_audio_commands_.end(),
            std::make_move_iterator(commands.begin()),
            std::make_move_iterator(commands.end()));
    }
    switch (result.kind) {
    case scene::SceneStepKind::return_world:
        if (scene_entry_world_direction_.has_value()) {
            if (auto* ranger = game_state_.ranger(); ranger != nullptr) {
                ranger->header.set_word(
                    model::header_word::face_towards,
                    static_cast<std::int16_t>(
                        opposite_world_direction(*scene_entry_world_direction_)));
            }
        }
        scene_entry_world_direction_.reset();
        scene_session_.reset();
        static_cast<void>(start_world(LegacyGameView::scene));
        break;
    case scene::SceneStepKind::quit:
        clear_scene_effect();
        ending_complete_ = result.ending_complete;
        set_view(LegacyGameView::exited, "scene requested quit");
        break;
    case scene::SceneStepKind::load_slot:
        if (result.save_slot >= 0 && result.save_slot <= 2) {
            pending_slot_ = static_cast<std::uint8_t>(result.save_slot);
            pending_io_ = PendingIo::load;
            error_return_view_ = LegacyGameView::scene;
        }
        break;
    case scene::SceneStepKind::battle:
        battle_request_ = result.battle_id;
        break;
    case scene::SceneStepKind::present:
        begin_scene_effect(SceneEffectKind::present, result.wait_ticks);
        break;
    case scene::SceneStepKind::fade_from_black:
        begin_scene_effect(SceneEffectKind::fade_from_black, result.wait_ticks);
        break;
    case scene::SceneStepKind::fade_to_black:
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
    if (view_ == LegacyGameView::scene && scene_session_ != nullptr) {
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

void LegacyGameRuntime::update_menu_counts() {
    const auto* ranger = game_state_.ranger();
    if (ranger == nullptr) {
        return;
    }
    std::uint8_t party_count = 0U;
    while (party_count < model::kTeamMemberCount &&
           ranger->header.team_member(party_count).value >= 0) {
        ++party_count;
    }
    game_menu_.set_party_count(party_count);

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
    case ui::TitleCommand::start_new_game: begin_new_game(); break;
    case ui::TitleCommand::load_slot:
        pending_slot_ = result.slot;
        pending_io_ = PendingIo::load;
        error_return_view_ = LegacyGameView::title;
        title_menu_.show_please_wait();
        break;
    case ui::TitleCommand::exit_game: set_view(LegacyGameView::exited, "title exit"); break;
    }
}

void LegacyGameRuntime::handle_game_menu_result(const ui::GameMenuResult result) {
    switch (result.command) {
    case ui::GameMenuCommand::none:
    case ui::GameMenuCommand::medicine:
    case ui::GameMenuCommand::detoxification:
    case ui::GameMenuCommand::status:
        break;
    case ui::GameMenuCommand::items: {
        if (menu_return_view_ != LegacyGameView::scene || scene_session_ == nullptr) {
            break;
        }
        const auto* ranger = game_state_.ranger();
        if (ranger == nullptr || result.index >= model::kInventoryCount) {
            break;
        }
        const auto item_id = ranger->header.inventory_item(result.index).value;
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger->items.size() ||
            ranger->items[static_cast<std::size_t>(item_id)].word(model::item_word::item_type) != 0) {
            break;
        }
        set_view(LegacyGameView::scene, "scene item selected");
        handle_scene_result(scene_session_->use_menu_item(item_id));
        break;
    }
    case ui::GameMenuCommand::leave_party: {
        const auto* ranger = game_state_.ranger();
        if (ranger != nullptr && result.index < model::kTeamMemberCount &&
            ranger->header.team_member(result.index).value == 0) {
            show_legacy_error(kCannotLeaveProtagonist, LegacyGameView::game_menu);
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
        error_return_view_ = LegacyGameView::game_menu;
        break;
    case ui::GameMenuCommand::save_slot:
        pending_slot_ = result.slot;
        pending_io_ = PendingIo::save;
        error_return_view_ = LegacyGameView::game_menu;
        break;
    case ui::GameMenuCommand::exit_game:
        set_view(LegacyGameView::exited, "game menu exit");
        break;
    }
}

}  // namespace openlegend::app
