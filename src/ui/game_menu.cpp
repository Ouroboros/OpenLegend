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
constexpr std::uint8_t kPageDown = 0x99U;
constexpr std::uint8_t kLeft = 0x9AU;
constexpr std::uint8_t kRight = 0x9CU;
constexpr std::uint8_t kUp = 0x9EU;
constexpr std::uint8_t kPageUp = 0x9FU;

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
    set_all_party_options();
}

void GameMenuController::set_party_abilities(
    const std::array<std::int16_t, 6U>& medicine,
    const std::array<std::int16_t, 6U>& detoxification) noexcept {
    medicine_abilities_ = medicine;
    detoxification_abilities_ = detoxification;
}

void GameMenuController::set_inventory_count(const std::uint16_t count) noexcept {
    inventory_count_ = std::min<std::uint16_t>(count, 200U);
}

void GameMenuController::complete_party_action(const std::int32_t amount) noexcept {
    if (screen_ == GameMenuScreen::party_notice &&
        (pending_party_command_ == GameMenuCommand::medicine ||
         pending_party_command_ == GameMenuCommand::detoxification)) {
        party_action_amount_ = amount;
    }
}

void GameMenuController::complete_slot_operation() noexcept {
    if (screen_ == GameMenuScreen::load_slots || screen_ == GameMenuScreen::save_slots) {
        screen_ = GameMenuScreen::system;
    }
}

void GameMenuController::begin_item_target_selection(
    const GameMenuItemTargetKind kind) noexcept {
    pending_party_command_ = GameMenuCommand::items;
    item_target_kind_ = kind;
    set_all_party_options();
    party_stage_ = GameMenuPartyStage::direct;
    party_selection_ = 0U;
    screen_ = GameMenuScreen::party_select;
}

void GameMenuController::show_item_confirmation(
    const GameMenuItemConfirmation confirmation) noexcept {
    item_confirmation_ = confirmation;
    screen_ = GameMenuScreen::item_confirmation;
}

void GameMenuController::show_notice(const GameMenuNotice notice) noexcept {
    notice_ = notice;
    screen_ = GameMenuScreen::notice;
}

GameMenuResult GameMenuController::handle_key(const std::uint8_t translated_key) noexcept {
    if (screen_ == GameMenuScreen::party_notice) {
        if (translated_key != 0U) {
            screen_ = GameMenuScreen::main;
            party_action_amount_.reset();
        }
        return {};
    }
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
    if (screen_ == GameMenuScreen::item_effect || screen_ == GameMenuScreen::notice) {
        if (translated_key != 0U) {
            screen_ = GameMenuScreen::main;
        }
        return {};
    }
    if (screen_ == GameMenuScreen::item_confirmation) {
        return {};
    }

    if (translated_key == kDown) {
        switch (screen_) {
        case GameMenuScreen::main: move_down(selection_, visible_main_items()); break;
        case GameMenuScreen::party_select: move_down(party_selection_, party_option_count_); break;
        case GameMenuScreen::items:
            if (item_row_ < 2U) {
                ++item_row_;
            } else if (item_page_ < 37U) {
                ++item_page_;
            }
            break;
        case GameMenuScreen::system: move_down(system_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::load_slots:
        case GameMenuScreen::save_slots: move_down(slot_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::party_notice:
        case GameMenuScreen::status_panel:
        case GameMenuScreen::item_confirmation:
        case GameMenuScreen::item_effect:
        case GameMenuScreen::notice:
        case GameMenuScreen::quit_confirmation: break;
        }
        return {};
    }
    if (translated_key == kUp) {
        switch (screen_) {
        case GameMenuScreen::main: move_up(selection_, visible_main_items()); break;
        case GameMenuScreen::party_select: move_up(party_selection_, party_option_count_); break;
        case GameMenuScreen::items:
            if (item_row_ > 0U) {
                --item_row_;
            } else if (item_page_ > 0U) {
                --item_page_;
            }
            break;
        case GameMenuScreen::system: move_up(system_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::load_slots:
        case GameMenuScreen::save_slots: move_up(slot_selection_, std::uint8_t{3U}); break;
        case GameMenuScreen::party_notice:
        case GameMenuScreen::status_panel:
        case GameMenuScreen::item_confirmation:
        case GameMenuScreen::item_effect:
        case GameMenuScreen::notice:
        case GameMenuScreen::quit_confirmation: break;
        }
        return {};
    }
    if (translated_key == kEscape) {
        switch (screen_) {
        case GameMenuScreen::main: return {GameMenuCommand::resume, 0U, 0U};
        case GameMenuScreen::party_select:
            if (party_stage_ == GameMenuPartyStage::target) {
                set_ability_party_options(pending_party_command_);
                party_stage_ = GameMenuPartyStage::source;
                party_selection_ = source_party_selection_;
            } else {
                screen_ = GameMenuScreen::main;
            }
            return {};
        case GameMenuScreen::items:
        case GameMenuScreen::system:
            screen_ = GameMenuScreen::main;
            return {};
        case GameMenuScreen::load_slots:
        case GameMenuScreen::save_slots:
            screen_ = GameMenuScreen::system;
            return {};
        case GameMenuScreen::party_notice:
        case GameMenuScreen::status_panel:
        case GameMenuScreen::item_confirmation:
        case GameMenuScreen::item_effect:
        case GameMenuScreen::notice:
        case GameMenuScreen::quit_confirmation: break;
        }
    }
    if (screen_ == GameMenuScreen::items) {
        if (translated_key == kLeft) {
            item_column_ = item_column_ == 0U ? 4U : static_cast<std::uint8_t>(item_column_ - 1U);
            return {};
        }
        if (translated_key == kRight) {
            item_column_ = item_column_ == 4U ? 0U : static_cast<std::uint8_t>(item_column_ + 1U);
            return {};
        }
        if (translated_key == kPageDown) {
            if (item_page_ < 35U) {
                item_page_ = static_cast<std::uint8_t>(item_page_ + 3U);
            }
            return {};
        }
        if (translated_key == kPageUp) {
            if (item_page_ > 2U) {
                item_page_ = static_cast<std::uint8_t>(item_page_ - 3U);
            }
            return {};
        }
    }
    if (!confirms(translated_key)) {
        return {};
    }

    switch (screen_) {
    case GameMenuScreen::main: return confirm_main();
    case GameMenuScreen::party_select:
        if ((pending_party_command_ == GameMenuCommand::medicine ||
             pending_party_command_ == GameMenuCommand::detoxification) &&
            party_stage_ == GameMenuPartyStage::source) {
            source_party_selection_ = party_selection_;
            source_party_slot_ = selected_party_slot();
            set_all_party_options();
            party_stage_ = GameMenuPartyStage::target;
            party_selection_ = 0U;
            return {};
        }
        if (pending_party_command_ == GameMenuCommand::medicine ||
            pending_party_command_ == GameMenuCommand::detoxification) {
            const auto target_slot = selected_party_slot();
            screen_ = GameMenuScreen::party_notice;
            party_action_amount_.reset();
            return {pending_party_command_, source_party_slot_, target_slot};
        }
        if (pending_party_command_ == GameMenuCommand::status) {
            screen_ = GameMenuScreen::status_panel;
            status_page_ = 0U;
            return {};
        }
        if (pending_party_command_ == GameMenuCommand::items) {
            const auto target_slot = selected_party_slot();
            screen_ = GameMenuScreen::main;
            return {GameMenuCommand::items, target_slot, item_selection()};
        }
        screen_ = GameMenuScreen::main;
        return {pending_party_command_, 0U, selected_party_slot()};
    case GameMenuScreen::items:
        if (item_selection() < inventory_count_) {
            screen_ = GameMenuScreen::main;
            return {GameMenuCommand::items, 0U, item_selection()};
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
    case GameMenuScreen::party_notice:
    case GameMenuScreen::status_panel:
    case GameMenuScreen::item_confirmation:
    case GameMenuScreen::item_effect:
    case GameMenuScreen::notice:
    case GameMenuScreen::quit_confirmation: return {};
    }
    return {};
}

GameMenuResult GameMenuController::confirm_main() noexcept {
    party_action_amount_.reset();
    switch (selection_) {
    case 0U:
        pending_party_command_ = GameMenuCommand::medicine;
        set_ability_party_options(pending_party_command_);
        party_stage_ = GameMenuPartyStage::source;
        if (party_option_count_ == 0U) {
            screen_ = GameMenuScreen::party_notice;
            return {};
        }
        break;
    case 1U:
        pending_party_command_ = GameMenuCommand::detoxification;
        set_ability_party_options(pending_party_command_);
        party_stage_ = GameMenuPartyStage::source;
        if (party_option_count_ == 0U) {
            screen_ = GameMenuScreen::party_notice;
            return {};
        }
        break;
    case 2U:
        screen_ = GameMenuScreen::items;
        item_page_ = 0U;
        item_row_ = 0U;
        item_column_ = 0U;
        return {};
    case 3U:
        pending_party_command_ = GameMenuCommand::status;
        set_all_party_options();
        party_stage_ = GameMenuPartyStage::direct;
        break;
    case 4U:
        pending_party_command_ = GameMenuCommand::leave_party;
        set_all_party_options();
        party_stage_ = GameMenuPartyStage::direct;
        break;
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

void GameMenuController::set_all_party_options() noexcept {
    party_option_count_ = party_count_;
    for (std::uint8_t slot = 0U; slot < party_count_; ++slot) {
        party_options_[slot] = slot;
    }
    if (party_selection_ >= party_option_count_) {
        party_selection_ = 0U;
    }
}

void GameMenuController::set_ability_party_options(const GameMenuCommand command) noexcept {
    const auto& abilities = command == GameMenuCommand::medicine
        ? medicine_abilities_
        : detoxification_abilities_;
    party_option_count_ = 0U;
    for (std::uint8_t slot = 0U; slot < party_count_; ++slot) {
        if (abilities[slot] >= 10) {
            party_options_[party_option_count_++] = slot;
        }
    }
    party_selection_ = 0U;
}

}  // namespace openlegend::ui
