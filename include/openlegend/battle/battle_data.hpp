#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "openlegend/resource/binary_file.hpp"

namespace openlegend::battle {

inline constexpr std::size_t kBattleDefinitionWords = 93U;
inline constexpr std::size_t kBattleDefinitionBytes = kBattleDefinitionWords * 2U;
inline constexpr std::size_t kBattlefieldWords = 8'192U;
inline constexpr std::size_t kBattlefieldBytes = kBattlefieldWords * 2U;
inline constexpr std::size_t kBattleExtent = 64U;
inline constexpr std::size_t kBattleOccupancyCells = kBattleExtent * kBattleExtent;

class BattleData {
public:
    BattleData(const resource::DataRoot& data_root, std::int16_t battle_id);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::int16_t battle_id() const noexcept { return battle_id_; }
    [[nodiscard]] std::int16_t battlefield_id() const noexcept { return definition_[6U]; }
    [[nodiscard]] std::int16_t music_id() const noexcept { return definition_[8U]; }
    [[nodiscard]] std::span<const std::int16_t, kBattleDefinitionWords> definition() const noexcept {
        return definition_;
    }
    [[nodiscard]] std::span<const std::int16_t, kBattlefieldWords> battlefield() const noexcept {
        return battlefield_;
    }
    [[nodiscard]] std::span<const std::int16_t, kBattleOccupancyCells> occupancy() const noexcept {
        return occupancy_;
    }
    [[nodiscard]] std::span<std::int16_t, kBattleOccupancyCells> occupancy() noexcept {
        return occupancy_;
    }

private:
    std::array<std::int16_t, kBattleDefinitionWords> definition_{};
    std::array<std::int16_t, kBattlefieldWords> battlefield_{};
    std::array<std::int16_t, kBattleOccupancyCells> occupancy_{};
    std::int16_t battle_id_{};
    std::string error_;
};

}  // namespace openlegend::battle
