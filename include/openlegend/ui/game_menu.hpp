#pragma once

#include <cstdint>

namespace openlegend::ui {

enum class GameMenuContext {
    world,
    scene,
};

enum class GameMenuScreen {
    main,
    party_select,
    status_panel,
    items,
    system,
    load_slots,
    save_slots,
    quit_confirmation,
};

enum class GameMenuCommand {
    none,
    resume,
    medicine,
    detoxification,
    items,
    status,
    leave_party,
    load_slot,
    save_slot,
    exit_game,
};

struct GameMenuResult {
    GameMenuCommand command{GameMenuCommand::none};
    std::uint8_t slot{};
    std::uint16_t index{};
};

class GameMenuController {
public:
    explicit GameMenuController(GameMenuContext context = GameMenuContext::world) noexcept
        : context_(context) {}

    [[nodiscard]] GameMenuResult handle_key(std::uint8_t translated_key) noexcept;
    void set_context(GameMenuContext context) noexcept;
    void set_party_count(std::uint8_t count) noexcept;
    void set_inventory_count(std::uint16_t count) noexcept;
    void show_main() noexcept { screen_ = GameMenuScreen::main; }

    [[nodiscard]] constexpr GameMenuContext context() const noexcept { return context_; }
    [[nodiscard]] constexpr GameMenuScreen screen() const noexcept { return screen_; }
    [[nodiscard]] constexpr std::uint8_t selection() const noexcept { return selection_; }
    [[nodiscard]] constexpr std::uint8_t party_selection() const noexcept {
        return party_selection_;
    }
    [[nodiscard]] constexpr std::uint16_t item_selection() const noexcept {
        return item_selection_;
    }
    [[nodiscard]] constexpr std::uint8_t status_page() const noexcept { return status_page_; }
    [[nodiscard]] constexpr std::uint8_t system_selection() const noexcept {
        return system_selection_;
    }
    [[nodiscard]] constexpr std::uint8_t slot_selection() const noexcept {
        return slot_selection_;
    }
    [[nodiscard]] constexpr std::uint8_t visible_main_items() const noexcept {
        return context_ == GameMenuContext::world ? 6U : 4U;
    }

private:
    [[nodiscard]] GameMenuResult confirm_main() noexcept;

    GameMenuContext context_{GameMenuContext::world};
    GameMenuScreen screen_{GameMenuScreen::main};
    GameMenuCommand pending_party_command_{GameMenuCommand::none};
    std::uint8_t selection_{};
    std::uint8_t party_selection_{};
    std::uint8_t party_count_{1U};
    std::uint8_t status_page_{};
    std::uint8_t system_selection_{};
    std::uint8_t slot_selection_{};
    std::uint16_t item_selection_{};
    std::uint16_t inventory_count_{};
};

}  // namespace openlegend::ui
