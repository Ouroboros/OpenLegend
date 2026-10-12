#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include "openlegend/attributes.hpp"
#include "openlegend/ui/save_list.hpp"

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
    item_confirmation,
    item_effect,
    notice,
    system,
    load_slots,
    save_slots,
    delete_confirmation,
    quit_confirmation,
};

inline constexpr std::uint8_t kItemCategoryCount = 8U;
inline constexpr std::uint16_t kItemListVisibleRows = 11U;

enum class GameMenuItemTargetKind {
    equipment,
    practice,
    consumable,
};

enum class GameMenuItemConfirmation {
    practice_reassign,
    practice_castration,
};

enum class GameMenuNotice {
    leave_protagonist,
    equipment_unsuitable,
    practice_magic_full,
    practice_unsuitable,
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
    delete_slot,
    exit_game,
};

struct GameMenuResult {
    GameMenuCommand command{GameMenuCommand::none};
    std::uint16_t slot{};
    std::uint16_t index{};
};

class GameMenuController {
public:
    explicit GameMenuController(GameMenuContext context = GameMenuContext::world) noexcept
        : context_(context) {}

    NODISCARD GameMenuResult handle_key(std::uint8_t translated_key) noexcept;

    void set_context(GameMenuContext context) noexcept;

    void set_party_count(std::uint8_t count) noexcept;

    void set_party_abilities(
        const std::array<std::int64_t, 6U>& medicine,
        const std::array<std::int64_t, 6U>& detoxification) noexcept;

    void set_inventory_count(std::uint16_t count) noexcept;

    void set_inventory_slots(std::span<const std::int16_t> slots) noexcept;

    void set_inventory_categories(std::span<const std::uint8_t> categories) noexcept;

    NODISCARD constexpr std::uint8_t item_category_selection() const noexcept {
        return item_category_selection_;
    }

    void show_main() noexcept {
        screen_ = GameMenuScreen::main;
        selection_ = 0U;
    }

    void complete_party_action(
        std::int64_t amount,
        std::optional<std::int64_t> hurt_amount = std::nullopt) noexcept;

    NODISCARD constexpr std::optional<std::int64_t>
        party_action_hurt_amount() const noexcept {
        return party_action_hurt_amount_;
    }

    void complete_slot_operation() noexcept;

    void begin_item_target_selection(GameMenuItemTargetKind kind) noexcept;

    void show_item_confirmation(GameMenuItemConfirmation confirmation) noexcept;

    void show_items() noexcept { screen_ = GameMenuScreen::items; }

    void show_item_effect() noexcept { screen_ = GameMenuScreen::item_effect; }

    void show_notice(GameMenuNotice notice) noexcept;

    NODISCARD constexpr GameMenuContext context() const noexcept { return context_; }

    NODISCARD constexpr GameMenuScreen screen() const noexcept { return screen_; }

    NODISCARD constexpr std::uint8_t selection() const noexcept { return selection_; }

    NODISCARD constexpr std::uint8_t party_selection() const noexcept {
        return party_selection_;
    }

    NODISCARD constexpr GameMenuCommand pending_party_command() const noexcept {
        return pending_party_command_;
    }

    NODISCARD constexpr GameMenuPartyStage party_stage() const noexcept {
        return party_stage_;
    }

    NODISCARD constexpr std::span<const std::uint8_t> party_options() const noexcept {
        return std::span<const std::uint8_t>{party_options_}.first(party_option_count_);
    }

    NODISCARD constexpr std::uint8_t selected_party_slot() const noexcept {
        return party_options_[party_selection_];
    }

    NODISCARD constexpr std::optional<std::int64_t> party_action_amount() const noexcept {
        return party_action_amount_;
    }

    NODISCARD constexpr std::uint16_t item_selection() const noexcept {
        return item_cursor_;
    }

    NODISCARD constexpr std::span<const std::int16_t> inventory_slots() const noexcept {
        return std::span<const std::int16_t>{inventory_slots_}.first(inventory_count_);
    }

    NODISCARD constexpr std::optional<std::uint16_t>
        selected_inventory_slot() const noexcept {
        const auto selection = item_selection();
        if (selection >= inventory_count_ || inventory_slots_[selection] < 0) {
            return std::nullopt;
        }
        return static_cast<std::uint16_t>(inventory_slots_[selection]);
    }

    NODISCARD constexpr std::uint16_t item_cursor() const noexcept { return item_cursor_; }

    NODISCARD constexpr std::uint16_t item_scroll() const noexcept { return item_scroll_; }

    NODISCARD constexpr GameMenuItemTargetKind item_target_kind() const noexcept {
        return item_target_kind_;
    }

    NODISCARD constexpr GameMenuItemConfirmation item_confirmation() const noexcept {
        return item_confirmation_;
    }

    NODISCARD constexpr GameMenuNotice notice() const noexcept { return notice_; }

    NODISCARD constexpr std::uint8_t status_page() const noexcept { return status_page_; }

    void set_status_page_count(std::uint8_t count) noexcept {
        status_page_count_ = count == 0U ? std::uint8_t{1U} : count;
        if (status_page_ >= status_page_count_) {
            status_page_ = static_cast<std::uint8_t>(status_page_count_ - 1U);
        }
    }

    NODISCARD constexpr std::uint8_t system_selection() const noexcept {
        return system_selection_;
    }

    NODISCARD constexpr std::uint16_t slot_selection() const noexcept {
        return slot_selection_;
    }

    constexpr void set_slot_selection(const std::uint16_t selection) noexcept {
        slot_selection_ = selection < kSaveSlotCount ? selection : std::uint16_t{0U};
    }

    NODISCARD constexpr GameMenuScreen delete_return_screen() const noexcept {
        return delete_return_screen_;
    }

    NODISCARD constexpr std::uint8_t visible_main_items() const noexcept {
        return context_ == GameMenuContext::world ? 6U : 4U;
    }

private:
    NODISCARD GameMenuResult confirm_main() noexcept;

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
    std::array<std::int64_t, 6U> medicine_abilities_{};
    std::array<std::int64_t, 6U> detoxification_abilities_{};
    std::optional<std::int64_t> party_action_amount_;
    std::optional<std::int64_t> party_action_hurt_amount_;
    GameMenuItemTargetKind item_target_kind_{GameMenuItemTargetKind::equipment};
    GameMenuItemConfirmation item_confirmation_{GameMenuItemConfirmation::practice_reassign};
    GameMenuNotice notice_{GameMenuNotice::leave_protagonist};
    std::uint8_t status_page_{};
    std::uint8_t status_page_count_{2U};
    std::uint8_t system_selection_{};
    std::uint16_t slot_selection_{};
    GameMenuScreen delete_return_screen_{GameMenuScreen::load_slots};
    void apply_item_category() noexcept;

    void move_item_cursor(int delta) noexcept;

    std::array<std::int16_t, 200U> inventory_slots_{};
    std::uint16_t inventory_count_{};
    std::array<std::int16_t, 200U> base_inventory_slots_{};
    std::uint16_t base_inventory_count_{};
    std::array<std::uint8_t, 200U> base_inventory_categories_{};
    std::uint8_t item_category_selection_{};
    std::uint16_t item_cursor_{};
    std::uint16_t item_scroll_{};
};

}  // namespace openlegend::ui
