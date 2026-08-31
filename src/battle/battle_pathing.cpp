#include "openlegend/battle/battle_pathing.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace openlegend::battle {
namespace {

constexpr std::array<BattlePathCoord, 4> kLegacyDirections{{
    {0, -1},
    {1, 0},
    {-1, 0},
    {0, 1},
}};

constexpr std::array<std::int16_t, 9> kBlockedTileBegin{
    0x0166, 0x0176, 0x01CA, 0x01FA, 0x0332, 0x0346, 0x03A6, 0x03F8, 0x052C};
constexpr std::array<std::int16_t, 9> kBlockedTileEnd{
    0x016A, 0x017C, 0x01D0, 0x0262, 0x0338, 0x0346, 0x03A8, 0x03FE, 0x0544};

[[nodiscard]] bool blocked_ground(const std::int16_t tile) noexcept {
    for (std::size_t index = 0U; index < kBlockedTileBegin.size(); ++index) {
        if (tile >= kBlockedTileBegin[index] && tile <= kBlockedTileEnd[index]) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] constexpr std::int16_t increment_mod(
    const std::int16_t value, const std::int16_t modulus) noexcept {
    return static_cast<std::int16_t>((static_cast<std::int32_t>(value) + 1) % modulus);
}

}  // namespace

std::optional<std::size_t> BattlePathing::legacy_index(
    const BattlePathCoord coordinate) noexcept {
    if (coordinate.x < 0 || coordinate.y < 0 || coordinate.x > 64 || coordinate.y > 64) {
        return std::nullopt;
    }
    const auto index = static_cast<std::int32_t>(coordinate.y) * 64 + coordinate.x;
    if (index < 0 || index >= static_cast<std::int32_t>(kBattleOccupancyCells)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(index);
}

void BattlePathing::initialize(const BattlePathMode mode) {
    const auto battlefield = data_.battlefield();
    const auto occupancy = data_.occupancy();
    for (std::size_t index = 0U; index < values_.size(); ++index) {
        const auto upper_layer = battlefield[kBattleOccupancyCells + index];
        if (mode == BattlePathMode::targeting) {
            values_[index] = upper_layer != 0 ? kBattlePathBlocked : kBattlePathUnvisited;
            continue;
        }
        values_[index] = upper_layer != 0 || occupancy[index] != -1 ||
                blocked_ground(battlefield[index])
            ? kBattlePathBlocked
            : kBattlePathUnvisited;
    }
}

void BattlePathing::build(const BattlePathCoord source, const BattlePathMode mode) {
    initialize(mode);
    const auto source_index = legacy_index(source);
    if (!source_index) {
        return;
    }
    values_[*source_index] = 0;

    std::array<BattlePathCoord, 255> queue{};
    queue[0] = BattlePathCoord{0, -1};
    queue[1] = source;
    std::int16_t read_index = 0;
    std::int16_t write_index = 2;
    std::int16_t distance = 0;

    const auto enqueue = [&](const BattlePathCoord coordinate) {
        queue[static_cast<std::size_t>(write_index)] = coordinate;
        write_index = increment_mod(write_index, 255);
    };
    const auto dequeue = [&]() {
        const auto coordinate = queue[static_cast<std::size_t>(read_index)];
        read_index = increment_mod(read_index, 255);
        return coordinate;
    };

    while (true) {
        auto current = dequeue();
        if (current.y < 0) {
            distance = increment_mod(distance, 128);
            enqueue(BattlePathCoord{0, -1});
            current = dequeue();
            if (current.y < 0) {
                break;
            }
        }

        for (const auto direction : kLegacyDirections) {
            const BattlePathCoord next{
                static_cast<std::int16_t>(current.x + direction.x),
                static_cast<std::int16_t>(current.y + direction.y),
            };
            const auto next_index = legacy_index(next);
            if (next_index && values_[*next_index] == kBattlePathUnvisited) {
                enqueue(next);
                values_[*next_index] = distance;
            }
        }
    }
}

std::int16_t BattlePathing::value(const BattlePathCoord coordinate) const noexcept {
    const auto index = legacy_index(coordinate);
    return index ? values_[*index] : kBattlePathBlocked;
}

bool BattlePathing::mark_shortest_path(
    const BattlePathCoord source, const BattlePathCoord target) noexcept {
    const auto target_index = legacy_index(target);
    const auto source_index = legacy_index(source);
    if (!target_index || !source_index || values_[*target_index] < 0 ||
        values_[*target_index] >= kBattlePathUnvisited) {
        return false;
    }

    auto current = target;
    auto distance = values_[*target_index];
    values_[*target_index] = kBattlePathMarked;
    for (std::size_t step = 0U; current != source && step < values_.size(); ++step) {
        distance = static_cast<std::int16_t>((distance + 127) % 128);
        bool found = false;
        for (const auto direction : kLegacyDirections) {
            const BattlePathCoord previous{
                static_cast<std::int16_t>(current.x + direction.x),
                static_cast<std::int16_t>(current.y + direction.y),
            };
            if (previous.x < 0 || previous.y < 0 || previous.x >= 64 || previous.y >= 64) {
                continue;
            }
            const auto previous_index = legacy_index(previous);
            if (previous_index && values_[*previous_index] == distance) {
                values_[*previous_index] = kBattlePathMarked;
                current = previous;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return current == source;
}

std::optional<BattlePathCoord> BattlePathing::next_marked_step(
    const BattlePathCoord source) const noexcept {
    for (const auto direction : kLegacyDirections) {
        const BattlePathCoord next{
            static_cast<std::int16_t>(source.x + direction.x),
            static_cast<std::int16_t>(source.y + direction.y),
        };
        if (next.x < 0 || next.y < 0 || next.x >= 64 || next.y >= 64) {
            continue;
        }
        const auto next_index = legacy_index(next);
        if (next_index && values_[*next_index] == kBattlePathMarked) {
            return next;
        }
    }
    return std::nullopt;
}

void BattlePathing::consume(const BattlePathCoord coordinate) noexcept {
    const auto index = legacy_index(coordinate);
    if (index) {
        values_[*index] = kBattlePathConsumed;
    }
}

}  // namespace openlegend::battle
