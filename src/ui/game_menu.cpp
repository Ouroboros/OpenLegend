#include "openlegend/ui/game_menu.hpp"

#include <algorithm>

namespace openlegend::ui {
namespace {

constexpr std::uint8_t kEnter = 0x0DU;
constexpr std::uint8_t kEscape = 0x1BU;
constexpr std::uint8_t kSpace = 0x20U;
constexpr std::uint8_t kY = 0x59U;
constexpr std::uint8_t kKeypadInsert = 0x96U;
constexpr std::uint8_t kDown = 0x98U;
constexpr std::uint8_t kUp = 0x9EU;

[[nodiscard]] constexpr bool confirms(const std::uint8_t key) noexcept {
    return key == kEnter || key == kSpace || key == kKeypadInsert;
}

template <typename Integer>
void move_down(Integer& selection, const Integer count) noexcept {
    if (count == 0) {
        return;
    }
    selection = selection + 1 == count ? 0 : static_cast<Integer>(selection + 1);
}

template <typename Integer>
void move_up(Integer& selection, const Integer count) noexcept {
    if (count == 0) {
        return;
    }
    selection = selection == 0 ? static_cast<Integer>(count - 1)
                               : static_cast<Integer>(selection - 1);
}

}  // namespace

void GameMenuController::set_context(const GameMenuContext context) noexcept {
    context_ = context;
    if (selection_ >= visible_main_items()) {
        selection_ = 0U;
    }
    if (context_ == GameMenuContext::scene &&
        (screen_ == GameMenuScreen::system || screen_ == GameMenuScreen::load_slots ||
         screen_ == GameMenuScreen::save_slots ||
         screen_ == GameMenuScreen::quit_confirmation)) {
        screen_ = GameMenuScreen::main;
    }
}

void GameMenuController::set_party_count(const std::uint8_t count) noexcept {
    party_count_ = std::clamp<std::uint8_t>(count, 1U, 6U);
    if (party_selection_ >= party_count_) {
        party_selection_ = 0U;
    }
}

void GameMenuController::set_inventory_count(const std::uint16_t count) noexcept {
    inventory_count_ = std::min<std::uint16_t>(count, 200U);
    if (item_selection_ >= inventory_count_) {
        item_selection_ = 0U;
    }
}

void GameMenuController::complete_slot_operation() noexcept {
    if (screen_ == GameMenuScreen::load_slots || screen_ == GameMenuScreen::save_slots) {
        screen_ = GameMenuScreen::system;
    }
}

GameMenuResult GameMenuController::handle_key(const std::uint8_t translated_key) noexcept {
    if (screen_ == GameMenuScreen::quit_confirmation) {
        screen_ = GameMenuScreen::system;
        if (translated_key == kY) {
            return {GameMenuCommand::exit_game, 0U, 0U};
        }
        return {};
    }
    if (screen_ == GameMenuScreen::status_panel) {
        if (translated_key != 0U) {
            if (status_page_ == 0U) {
                status_page_ = 1U;
            } else {
                screen_ = GameMenuScreen::main;
                status_page_ = 0U;
            }
        }
        return {};
    }

    if (translated_key == kDown) {
        switch (screen_) {
        case GameMenuScreen::main: move_down(selection_, visible_main_items()); break;
        case GameMenuScreen::party_select: move_down(party_selection_, party_count_); break;
        case GameMenuScreen::items: move_down(item_selection_, inventory_count_); break;
        case GameMenuScreen::system: move_down(system_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::load_slots:
        case GameMenuScreen::save_slots: move_down(slot_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::status_panel:
        case GameMenuScreen::quit_confirmation: break;
        }
        return {};
    }
    if (translated_key == kUp) {
        switch (screen_) {
        case GameMenuScreen::main: move_up(selection_, visible_main_items()); break;
        case GameMenuScreen::party_select: move_up(party_selection_, party_count_); break;
        case GameMenuScreen::items: move_up(item_selection_, inventory_count_); break;
        case GameMenuScreen::system: move_up(system_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::load_slots:
        case GameMenuScreen::save_slots: move_up(slot_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::status_panel:
        case GameMenuScreen::quit_confirmation: break;
        }
        return {};
    }
    if (translated_key == kEscape) {
        switch (screen_) {
        case GameMenuScreen::main: return {GameMenuCommand::resume, 0U, 0U};
        case GameMenuScreen::party_select:
        case GameMenuScreen::items:
        case GameMenuScreen::system:
            screen_ = GameMenuScreen::main;
            return {};
        case GameMenuScreen::load_slots:
        case GameMenuScreen::save_slots:
            screen_ = GameMenuScreen::system;
            return {};
        case GameMenuScreen::status_panel:
        case GameMenuScreen::quit_confirmation: break;
        }
    }
    if (!confirms(translated_key)) {
        return {};
    }

    switch (screen_) {
    case GameMenuScreen::main: return confirm_main();
    case GameMenuScreen::party_select:
        if (pending_party_command_ == GameMenuCommand::status) {
            screen_ = GameMenuScreen::status_panel;
            status_page_ = 0U;
            return {};
        }
        screen_ = GameMenuScreen::main;
        return {pending_party_command_, 0U, party_selection_};
    case GameMenuScreen::items:
        if (inventory_count_ != 0U) {
            screen_ = GameMenuScreen::main;
            return {GameMenuCommand::items, 0U, item_selection_};
        }
        return {};
    case GameMenuScreen::system:
        slot_selection_ = 0U;
        if (system_selection_ == 0U) {
            screen_ = GameMenuScreen::load_slots;
        } else if (system_selection_ == 1U) {
            screen_ = GameMenuScreen::save_slots;
        } else {
            screen_ = GameMenuScreen::quit_confirmation;
        }
        return {};
    case GameMenuScreen::load_slots:
    case GameMenuScreen::save_slots: {
        const auto command = screen_ == GameMenuScreen::load_slots
                                 ? GameMenuCommand::load_slot
                                 : GameMenuCommand::save_slot;
        return {command, slot_selection_, 0U};
    }
    case GameMenuScreen::status_panel:
    case GameMenuScreen::quit_confirmation: return {};
    }
    return {};
}

GameMenuResult GameMenuController::confirm_main() noexcept {
    switch (selection_) {
    case 0U: pending_party_command_ = GameMenuCommand::medicine; break;
    case 1U: pending_party_command_ = GameMenuCommand::detoxification; break;
    case 2U:
        screen_ = GameMenuScreen::items;
        item_selection_ = 0U;
        return {};
    case 3U: pending_party_command_ = GameMenuCommand::status; break;
    case 4U: pending_party_command_ = GameMenuCommand::leave_party; break;
    case 5U:
        screen_ = GameMenuScreen::system;
        system_selection_ = 0U;
        return {};
    default: return {};
    }
    screen_ = GameMenuScreen::party_select;
    party_selection_ = 0U;
    return {};
}

}  // namespace openlegend::ui
