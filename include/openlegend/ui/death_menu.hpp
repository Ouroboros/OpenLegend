#pragma once

#include <cstdint>

#include "openlegend/attributes.hpp"

namespace openlegend::ui {

enum class DeathMenuScreen {
    main,
    load_slots,
    delete_confirmation,
    quit_confirmation,
};

enum class DeathMenuCommand {
    none,
    load_slot,
    delete_slot,
    exit_game,
};

struct DeathMenuResult {
    DeathMenuCommand command{DeathMenuCommand::none};
    std::uint16_t slot{};
};

class DeathMenuController {
public:
    NODISCARD DeathMenuResult handle_key(std::uint8_t translated_key) noexcept;

    void reset() noexcept;

    NODISCARD constexpr DeathMenuScreen screen() const noexcept {
        return screen_;
    }

    NODISCARD constexpr std::uint8_t main_selection() const noexcept {
        return main_selection_;
    }

    NODISCARD constexpr std::uint16_t slot_selection() const noexcept {
        return slot_selection_;
    }

    NODISCARD constexpr bool save_list_active() const noexcept {
        return screen_ == DeathMenuScreen::load_slots ||
            screen_ == DeathMenuScreen::delete_confirmation;
    }

private:
    DeathMenuScreen screen_{DeathMenuScreen::main};
    std::uint8_t main_selection_{};
    std::uint16_t slot_selection_{};
};

}  // namespace openlegend::ui
