#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace openlegend::ui {

enum class GameMenuContext {
    world,
    scene,
};

enum class GameMenuScreen {
    main,
    party_select,
    party_notice,
    status_panel,
    items,
    system,
    load_slots,
    save_slots,
    quit_confirmation,
};

enum class GameMenuPartyStage {
    direct,
    source,
    target,
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
    void set_party_abilities(
        const std::array<std::int16_t, 6U>& medicine,
        const std::array<std::int16_t, 6U>& detoxification) noexcept;
    void set_inventory_count(std::uint16_t count) noexcept;
    void show_main() noexcept {
        screen_ = GameMenuScreen::main;
        selection_ = 0U;
    }
    void complete_party_action(std::int32_t amount) noexcept;
    void complete_slot_operation() noexcept;

    [[nodiscard]] constexpr GameMenuContext context() const noexcept { return context_; }
    [[nodiscard]] constexpr GameMenuScreen screen() const noexcept { return screen_; }
    [[nodiscard]] constexpr std::uint8_t selection() const noexcept { return selection_; }
    [[nodiscard]] constexpr std::uint8_t party_selection() const noexcept {
        return party_selection_;
    }
    [[nodiscard]] constexpr GameMenuCommand pending_party_command() const noexcept {
        return pending_party_command_;
    }
    [[nodiscard]] constexpr GameMenuPartyStage party_stage() const noexcept {
        return party_stage_;
    }
    [[nodiscard]] constexpr std::span<const std::uint8_t> party_options() const noexcept {
        return std::span<const std::uint8_t>{party_options_}.first(party_option_count_);
    }
    [[nodiscard]] constexpr std::uint8_t selected_party_slot() const noexcept {
        return party_options_[party_selection_];
    }
    [[nodiscard]] constexpr std::optional<std::int32_t> party_action_amount() const noexcept {
        return party_action_amount_;
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
    void set_all_party_options() noexcept;
    void set_ability_party_options(GameMenuCommand command) noexcept;

    GameMenuContext context_{GameMenuContext::world};
    GameMenuScreen screen_{GameMenuScreen::main};
    GameMenuCommand pending_party_command_{GameMenuCommand::none};
    GameMenuPartyStage party_stage_{GameMenuPartyStage::direct};
    std::uint8_t selection_{};
    std::uint8_t party_selection_{};
    std::uint8_t party_count_{1U};
    std::uint8_t party_option_count_{1U};
    std::uint8_t source_party_selection_{};
    std::uint8_t source_party_slot_{};
    std::array<std::uint8_t, 6U> party_options_{0U, 1U, 2U, 3U, 4U, 5U};
    std::array<std::int16_t, 6U> medicine_abilities_{};
    std::array<std::int16_t, 6U> detoxification_abilities_{};
    std::optional<std::int32_t> party_action_amount_;
    std::uint8_t status_page_{};
    std::uint8_t system_selection_{};
    std::uint8_t slot_selection_{};
    std::uint16_t item_selection_{};
    std::uint16_t inventory_count_{};
};

}  // namespace openlegend::ui
