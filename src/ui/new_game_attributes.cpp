#include "openlegend/ui/new_game_attributes.hpp"

#include <algorithm>

namespace openlegend::ui {
namespace {

constexpr std::array<std::uint8_t, 8> kBabeRuth{'B', 'A', 'B', 'E', 'R', 'U', 'T', 'H'};

}  // namespace

NewGameAttributeController::NewGameAttributeController(
    model::RoleRecord& protagonist, random::LegacyRandom& random) noexcept
    : protagonist_(protagonist), random_(random) {
    reroll();
}

AttributeRollStatus NewGameAttributeController::handle_key(
    const std::uint8_t translated_key) noexcept {
    if (translated_key == 'Y') {
        return AttributeRollStatus::accepted;
    }

    std::move(key_history_.begin() + 1, key_history_.end(), key_history_.begin());
    key_history_.back() = translated_key;
    if (key_history_ == kBabeRuth) {
        model::apply_baberuth_attributes(protagonist_);
        cheat_active_ = true;
    } else {
        reroll();
    }
    return AttributeRollStatus::choosing;
}

void NewGameAttributeController::reroll() noexcept {
    model::roll_protagonist_attributes(protagonist_, random_);
    cheat_active_ = false;
}

}  // namespace openlegend::ui
