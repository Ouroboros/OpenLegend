#include "openlegend/app/legacy_game_runtime.hpp"

#include <algorithm>
#include <array>
#include <string_view>

#include "openlegend/model/new_game.hpp"
#include "openlegend/persistence/save_slot.hpp"

namespace openlegend::app {
namespace {

constexpr std::array<std::uint8_t, 26> kCannotLeaveProtagonist{
    0xA9U, 0xEAU, 0xBAU, 0x70U, 0xA1U, 0x49U, 0xA8U, 0x53U, 0xA6U,
    0xB3U, 0xA7U, 0x41U, 0xB9U, 0x43U, 0xC0U, 0xB8U, 0xB6U, 0x69U,
    0xA6U, 0xE6U, 0xA4U, 0xA3U, 0xA4U, 0x55U, 0xA5U, 0x68U};

[[nodiscard]] persistence::SaveSlot save_slot(const std::uint8_t slot) noexcept {
    return static_cast<persistence::SaveSlot>(slot);
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
}

void LegacyGameRuntime::advance() {
    if (pending_io_ != PendingIo::none) {
        perform_pending_io();
    }
    if (view_ == LegacyGameView::world && world_session_ != nullptr) {
        if (!world_step_processed_) {
            world_session_->idle_tick();
        }
        world_session_->periodic_tick();
    }
    world_step_processed_ = false;
}

void LegacyGameRuntime::handle_world_input(
    const bool left, const bool up, const bool down, const bool right) {
    if (!valid() || view_ != LegacyGameView::world || world_session_ == nullptr) {
        return;
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
        return;
    }
    world_step_processed_ = true;
    const auto result = world_session_->move(*direction);
    if (result.kind == world::WorldStepKind::enter_scene) {
        scene_request_ = result.scene_id;
    }
}

void LegacyGameRuntime::handle_key(
    const std::uint8_t translated_key,
    const bool control_down,
    const bool shift_down) {
    if (!valid() || translated_key == 0U || view_ == LegacyGameView::exited) {
        return;
    }
    if (view_ == LegacyGameView::error) {
        visible_error_.clear();
        view_ = error_return_view_;
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
            view_ = LegacyGameView::attributes;
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
            game_menu_.show_main();
            view_ = LegacyGameView::game_menu;
        }
        break;
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
    case LegacyGameView::game_menu: {
        const auto* ranger = game_state_.ranger();
        return ranger != nullptr && world_session_ != nullptr &&
               world_session_->render(framebuffer_) &&
               basic_renderer_.render_game_menu(game_menu_, *ranger, framebuffer_);
    }
    case LegacyGameView::error: {
        bool base_rendered = false;
        if (error_return_view_ == LegacyGameView::title) {
            base_rendered = title_renderer_.render(title_menu_, framebuffer_);
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
    scene_request_.reset();
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
    view_ = LegacyGameView::name_entry;
}

void LegacyGameRuntime::perform_pending_io() {
    const auto operation = pending_io_;
    pending_io_ = PendingIo::none;
    if (operation == PendingIo::load) {
        attribute_controller_.reset();
        auto loaded = persistence::load_numbered_slot(
            data_root_path_, save_slot(pending_slot_));
        if (!loaded) {
            show_error(
                std::string{persistence::persistence_status_message(loaded.status)},
                error_return_view_);
            return;
        }
        world_session_.reset();
        world_map_.reset();
        scene_request_.reset();
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
    world_step_processed_ = false;
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
    view_ = LegacyGameView::world;
    return true;
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

void LegacyGameRuntime::show_error(
    std::string message, const LegacyGameView return_view) {
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
    view_ = LegacyGameView::error;
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
    case ui::TitleCommand::exit_game: view_ = LegacyGameView::exited; break;
    }
}

void LegacyGameRuntime::handle_game_menu_result(const ui::GameMenuResult result) {
    switch (result.command) {
    case ui::GameMenuCommand::none:
    case ui::GameMenuCommand::medicine:
    case ui::GameMenuCommand::detoxification:
    case ui::GameMenuCommand::items:
    case ui::GameMenuCommand::status:
        break;
    case ui::GameMenuCommand::leave_party: {
        const auto* ranger = game_state_.ranger();
        if (ranger != nullptr && result.index < model::kTeamMemberCount &&
            ranger->header.team_member(result.index).value == 0) {
            show_legacy_error(kCannotLeaveProtagonist, LegacyGameView::game_menu);
        }
        break;
    }
    case ui::GameMenuCommand::resume: view_ = LegacyGameView::world; break;
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
    case ui::GameMenuCommand::exit_game: view_ = LegacyGameView::exited; break;
    }
}

}  // namespace openlegend::app
