#pragma once

#include <cstdint>

namespace openlegend::model {

template <typename Tag>
struct LegacyId {
    std::int16_t value{-1};

    [[nodiscard]] constexpr bool present() const noexcept { return value >= 0; }
    [[nodiscard]] constexpr bool operator==(const LegacyId&) const = default;
};

struct CharacterIdTag;
struct ItemIdTag;
struct SceneIdTag;
struct MagicIdTag;
struct BattleIdTag;
struct TalkIdTag;

using CharacterId = LegacyId<CharacterIdTag>;
using ItemId = LegacyId<ItemIdTag>;
using SceneId = LegacyId<SceneIdTag>;
using MagicId = LegacyId<MagicIdTag>;
using BattleId = LegacyId<BattleIdTag>;
using TalkId = LegacyId<TalkIdTag>;

struct WorldCoord {
    std::int16_t value{};
    [[nodiscard]] constexpr bool operator==(const WorldCoord&) const = default;
};

struct SceneCoord {
    std::int16_t value{};
    [[nodiscard]] constexpr bool operator==(const SceneCoord&) const = default;
};

struct PaletteIndex {
    std::uint8_t value{};
    [[nodiscard]] constexpr bool operator==(const PaletteIndex&) const = default;
};

}  // namespace openlegend::model
