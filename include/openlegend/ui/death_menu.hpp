#pragma once

#include <cstdint>

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
    [[nodiscard]] DeathMenuResult handle_key(std::uint8_t translated_key) noexcept;

    void reset() noexcept;

    [[nodiscard]] constexpr DeathMenuScreen screen() const noexcept {
        return screen_;
    }

    [[nodiscard]] constexpr std::uint8_t main_selection() const noexcept {
        return main_selection_;
    }

    [[nodiscard]] constexpr std::uint16_t slot_selection() const noexcept {
        return slot_selection_;
    }

    [[nodiscard]] constexpr bool save_list_active() const noexcept {
        return screen_ == DeathMenuScreen::load_slots ||
            screen_ == DeathMenuScreen::delete_confirmation;
    }

private:
    DeathMenuScreen screen_{DeathMenuScreen::main};
    std::uint8_t main_selection_{};
    std::uint16_t slot_selection_{};
};

}  // namespace openlegend::ui
