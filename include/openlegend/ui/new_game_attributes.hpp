#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "openlegend/model/new_game.hpp"

namespace openlegend::ui {

enum class AttributeRollStatus {
    choosing,
    accepted,
};

class NewGameAttributeController {
public:
    NewGameAttributeController(
        model::RoleRecord& protagonist, random::LegacyRandom& random) noexcept;

    [[nodiscard]] AttributeRollStatus handle_key(std::uint8_t translated_key) noexcept;
    [[nodiscard]] bool cheat_active() const noexcept { return cheat_active_; }

private:
    void reroll() noexcept;

    model::RoleRecord& protagonist_;
    random::LegacyRandom& random_;
    std::array<std::uint8_t, 8> key_history_{};
    bool cheat_active_{};
};

}  // namespace openlegend::ui
