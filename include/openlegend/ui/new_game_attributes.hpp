#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "openlegend/attributes.hpp"
#include "openlegend/model/new_game.hpp"
#include "openlegend/model/runtime_snapshot.hpp"

namespace openlegend::ui {

enum class AttributeRollStatus {
    choosing,
    accepted,
};

class NewGameAttributeController {
public:
    NewGameAttributeController(
        model::RoleState& protagonist, random::LegacyRandom& random);

    NODISCARD AttributeRollStatus handle_key(std::uint8_t translated_key);

    NODISCARD bool cheat_active() const noexcept { return cheat_active_; }

private:
    void reroll();

    model::RoleState& protagonist_;
    random::LegacyRandom& random_;
    std::array<std::uint8_t, 8> key_history_{};
    bool cheat_active_{};
};

}  // namespace openlegend::ui
