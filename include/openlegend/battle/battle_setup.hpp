#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/model/game_snapshot.hpp"

namespace openlegend::battle {

inline constexpr std::size_t kBattleCombatantCount = 26U;
inline constexpr std::size_t kBattleCombatantWords = 14U;
inline constexpr std::size_t kBattlePartySlots = 6U;
inline constexpr std::size_t kBattleEnemySlots = 20U;

namespace combatant_word {
inline constexpr std::size_t role_id = 0U;
inline constexpr std::size_t side = 1U;
inline constexpr std::size_t x = 2U;
inline constexpr std::size_t y = 3U;
inline constexpr std::size_t initial_mode = 4U;
inline constexpr std::size_t sprite = 8U;
}  // namespace combatant_word

struct BattleCombatant {
    std::array<std::int16_t, kBattleCombatantWords> words{};
};

enum class PartySelectionAction {
    next,
    previous,
    activate,
};

enum class PartySelectionResult {
    waiting,
    changed,
    complete,
    invalid,
};

class BattleSetup {
public:
    BattleSetup(BattleData& data, const model::RangerState& ranger);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] bool waiting_for_party_selection() const noexcept { return waiting_; }
    [[nodiscard]] std::size_t party_prefix_length() const noexcept { return party_prefix_length_; }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    [[nodiscard]] std::int16_t combatant_count() const noexcept { return combatant_count_; }
    [[nodiscard]] std::span<const std::int16_t, kBattlePartySlots> selection_states() const noexcept {
        return selection_states_;
    }
    [[nodiscard]] std::span<const BattleCombatant, kBattleCombatantCount> combatants() const noexcept {
        return combatants_;
    }

    [[nodiscard]] PartySelectionResult apply(PartySelectionAction action);

private:
    void initialize_combatants();
    void initialize_party();
    [[nodiscard]] bool append_combatant(
        std::int16_t role_id,
        std::int16_t side,
        std::int16_t x,
        std::int16_t y,
        std::int16_t initial_mode);
    [[nodiscard]] bool append_enemies();
    [[nodiscard]] std::int16_t sprite_word(
        std::int16_t role_id, std::int16_t initial_mode) const noexcept;

    BattleData& data_;
    const model::RangerState& ranger_;
    std::array<BattleCombatant, kBattleCombatantCount> combatants_{};
    std::array<std::int16_t, kBattlePartySlots> selection_states_{};
    std::int16_t combatant_count_{};
    std::size_t party_prefix_length_{kBattlePartySlots};
    std::size_t cursor_{};
    bool waiting_{};
    std::string error_;
};

}  // namespace openlegend::battle
