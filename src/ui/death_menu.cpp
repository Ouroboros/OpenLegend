#include "openlegend/ui/death_menu.hpp"

#include <algorithm>

#include "openlegend/input/legacy_key.hpp"
#include "openlegend/ui/save_list.hpp"

namespace openlegend::ui {
namespace {

[[nodiscard]] constexpr bool confirms(const std::uint8_t key) noexcept {
    return key == input::legacy_key::enter || key == input::legacy_key::space ||
        key == input::legacy_key::keypad_insert;
}

void move_slot_down(std::uint16_t& selection) noexcept {
    selection = selection + 1U == kSaveSlotCount
        ? 0U
        : static_cast<std::uint16_t>(selection + 1U);
}

void move_slot_up(std::uint16_t& selection) noexcept {
    selection = selection == 0U
        ? static_cast<std::uint16_t>(kSaveSlotCount - 1U)
        : static_cast<std::uint16_t>(selection - 1U);
}

void move_slot_page_down(std::uint16_t& selection) noexcept {
    selection = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(
            static_cast<std::uint32_t>(selection) + kSaveListPageSize,
            kSaveSlotCount - 1U));
}

void move_slot_page_up(std::uint16_t& selection) noexcept {
    selection = selection < kSaveListPageSize
        ? 0U
        : static_cast<std::uint16_t>(selection - kSaveListPageSize);
}

}  // namespace

DeathMenuResult DeathMenuController::handle_key(
    const std::uint8_t translated_key) noexcept {
    if (translated_key == 0U) {
        return {};
    }
    if (screen_ == DeathMenuScreen::delete_confirmation) {
        if (translated_key == input::legacy_key::yes) {
            screen_ = DeathMenuScreen::load_slots;
            return {DeathMenuCommand::delete_slot, slot_selection_};
        }
        if (translated_key == input::legacy_key::no ||
            translated_key == input::legacy_key::escape) {
            screen_ = DeathMenuScreen::load_slots;
        }
        return {};
    }
    if (screen_ == DeathMenuScreen::quit_confirmation) {
        if (translated_key == input::legacy_key::yes) {
            return {DeathMenuCommand::exit_game, 0U};
        }
        screen_ = DeathMenuScreen::main;
        main_selection_ = 1U;
        return {};
    }
    if (screen_ == DeathMenuScreen::main) {
        if (translated_key == input::legacy_key::down ||
            translated_key == input::legacy_key::up) {
            main_selection_ = main_selection_ == 0U ? 1U : 0U;
            return {};
        }
        if (!confirms(translated_key)) {
            return {};
        }
        if (main_selection_ == 0U) {
            screen_ = DeathMenuScreen::load_slots;
            slot_selection_ = 0U;
        } else {
            screen_ = DeathMenuScreen::quit_confirmation;
        }
        return {};
    }

    if (translated_key == input::legacy_key::down) {
        move_slot_down(slot_selection_);
        return {};
    }
    if (translated_key == input::legacy_key::up) {
        move_slot_up(slot_selection_);
        return {};
    }
    if (translated_key == input::legacy_key::home) {
        slot_selection_ = 0U;
        return {};
    }
    if (translated_key == input::legacy_key::end) {
        slot_selection_ = static_cast<std::uint16_t>(kSaveSlotCount - 1U);
        return {};
    }
    if (translated_key == input::legacy_key::page_down) {
        move_slot_page_down(slot_selection_);
        return {};
    }
    if (translated_key == input::legacy_key::page_up) {
        move_slot_page_up(slot_selection_);
        return {};
    }
    if (translated_key == input::legacy_key::delete_save) {
        screen_ = DeathMenuScreen::delete_confirmation;
        return {};
    }
    if (translated_key == input::legacy_key::escape) {
        screen_ = DeathMenuScreen::main;
        main_selection_ = 0U;
        return {};
    }
    if (confirms(translated_key)) {
        return {DeathMenuCommand::load_slot, slot_selection_};
    }
    return {};
}

void DeathMenuController::reset() noexcept {
    screen_ = DeathMenuScreen::main;
    main_selection_ = 0U;
    slot_selection_ = 0U;
}

}  // namespace openlegend::ui
