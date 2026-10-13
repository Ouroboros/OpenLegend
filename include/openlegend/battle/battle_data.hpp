#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "openlegend/attributes.hpp"
#include "openlegend/model/practice.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::battle {

inline constexpr std::size_t kBattleDefinitionWords = 93U;
inline constexpr std::size_t kBattleDefinitionBytes = kBattleDefinitionWords * 2U;
inline constexpr std::size_t kBattlefieldWords = 8'192U;
inline constexpr std::size_t kBattlefieldBytes = kBattlefieldWords * 2U;
inline constexpr std::size_t kBattleExtent = 64U;
inline constexpr std::size_t kBattleOccupancyCells = kBattleExtent * kBattleExtent;
inline constexpr std::size_t kOriginalLevelCount = 30U;

struct ProgressionData {
    std::array<std::uint16_t, kOriginalLevelCount> thresholds{};
    model::PracticeRules practice_rules{};
    std::int64_t original_maximum_hp{};
    // 原版物品效果中最大内力的上限（与最大生命同为999），药品回内力的倍率基准。
    std::int64_t original_maximum_mp{};
    std::string error;
};

NODISCARD ProgressionData load_progression_data(const resource::DataRoot& data_root);

class BattleData {
public:
    // merged_battle_id有效时，把该战斗的敌方并入本战空余的敌方槽位（就近另寻站位），
    // 并合计整场经验；用于NG+最终战正邪合战。
    BattleData(const resource::DataRoot& data_root, std::int16_t battle_id,
        std::int16_t merged_battle_id = -1);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD std::int16_t battle_id() const noexcept { return battle_id_; }

    NODISCARD std::int16_t battlefield_id() const noexcept { return definition_[6U]; }

    NODISCARD std::int16_t music_id() const noexcept { return definition_[8U]; }

    NODISCARD std::span<const std::uint16_t, kOriginalLevelCount> experience_thresholds() const noexcept {
        return experience_thresholds_;
    }

    NODISCARD const model::PracticeRules& practice_rules() const noexcept { return practice_rules_; }

    NODISCARD std::int64_t original_maximum_hp() const noexcept { return original_maximum_hp_; }

    NODISCARD std::int64_t original_maximum_mp() const noexcept { return original_maximum_mp_; }

    NODISCARD std::span<const std::int16_t, kBattleDefinitionWords> definition() const noexcept {
        return definition_;
    }

    NODISCARD std::span<const std::int16_t, kBattlefieldWords> battlefield() const noexcept {
        return battlefield_;
    }

    NODISCARD std::span<const std::int16_t, kBattleOccupancyCells> occupancy() const noexcept {
        return occupancy_;
    }

    NODISCARD std::span<std::int16_t, kBattleOccupancyCells> occupancy() noexcept {
        return occupancy_;
    }

private:
    NODISCARD bool merge_enemies(std::span<const std::uint8_t> war, std::int16_t merged_battle_id);

    std::array<std::uint16_t, kOriginalLevelCount> experience_thresholds_{};
    model::PracticeRules practice_rules_{};
    std::int64_t original_maximum_hp_{};
    std::int64_t original_maximum_mp_{};
    std::array<std::int16_t, kBattleDefinitionWords> definition_{};
    std::array<std::int16_t, kBattlefieldWords> battlefield_{};
    std::array<std::int16_t, kBattleOccupancyCells> occupancy_{};
    std::int16_t battle_id_{};
    std::string error_;
};

}  // namespace openlegend::battle
