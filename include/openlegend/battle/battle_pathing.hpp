#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "openlegend/battle/battle_data.hpp"

namespace openlegend::battle {

inline constexpr std::int16_t kBattlePathUnvisited = 254;
inline constexpr std::int16_t kBattlePathMarked = 250;
inline constexpr std::int16_t kBattlePathConsumed = 255;
inline constexpr std::int16_t kBattlePathBlocked = 555;

struct BattlePathCoord {
    std::int16_t x{};
    std::int16_t y{};

    [[nodiscard]] constexpr bool operator==(const BattlePathCoord&) const = default;
};

enum class BattlePathMode {
    movement,
    targeting,
};

class BattlePathing {
public:
    explicit BattlePathing(const BattleData& data) : data_(data) {}

    void build(BattlePathCoord source, BattlePathMode mode);
    [[nodiscard]] std::int16_t value(BattlePathCoord coordinate) const noexcept;
    [[nodiscard]] std::span<const std::int16_t, kBattleOccupancyCells> values() const noexcept {
        return values_;
    }
    [[nodiscard]] bool mark_shortest_path(
        BattlePathCoord source, BattlePathCoord target) noexcept;
    [[nodiscard]] std::optional<BattlePathCoord> next_marked_step(
        BattlePathCoord source) const noexcept;
    void consume(BattlePathCoord coordinate) noexcept;

private:
    [[nodiscard]] static std::optional<std::size_t> legacy_index(
        BattlePathCoord coordinate) noexcept;
    void initialize(BattlePathMode mode);

    const BattleData& data_;
    std::array<std::int16_t, kBattleOccupancyCells> values_{};
};

}  // namespace openlegend::battle
