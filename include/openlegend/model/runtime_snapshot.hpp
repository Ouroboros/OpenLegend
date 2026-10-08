#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/model/new_game_plus_configuration.hpp"
#include "openlegend/model/role_state.hpp"

namespace openlegend::model {

struct RuntimeRangerState {
    RangerHeader header;
    std::vector<RoleState> roles = std::vector<RoleState>(kRoleCount);
    std::vector<ItemRecord> items = std::vector<ItemRecord>(kItemCount);
    std::vector<SceneMetadataRecord> scenes =
        std::vector<SceneMetadataRecord>(kSceneMetadataCount);
    std::vector<MagicRecord> magics = std::vector<MagicRecord>(kMagicCount);
    std::vector<ShopRecord> shops = std::vector<ShopRecord>(kShopCount);

    NODISCARD bool valid() const;

    NODISCARD bool matches_legacy_definitions(const RangerState& baseline) const;

    NODISCARD bool operator==(const RuntimeRangerState&) const = default;
};

enum class SnapshotOrigin {
    legacy,
    new_game_plus,
};

struct RuntimeGameSnapshot : SceneArchives {
    RuntimeRangerState ranger;
    NewGamePlusConfiguration configuration;
    std::int64_t playthrough{1};
    SnapshotOrigin origin{SnapshotOrigin::legacy};

    NODISCARD bool valid() const;

    NODISCARD bool valid_for_persistence() const;

    NODISCARD bool operator==(const RuntimeGameSnapshot&) const = default;
};

NODISCARD std::optional<RuntimeRangerState> decode_legacy_ranger(RangerState ranger);

NODISCARD std::optional<RuntimeGameSnapshot> decode_legacy_snapshot(
    GameSnapshot snapshot, const NewGamePlusConfiguration& configuration = {},
    const RangerState* baseline = nullptr);

NODISCARD std::optional<GameSnapshot> encode_legacy_snapshot(
    const RuntimeGameSnapshot& snapshot);

class RuntimeGameState {
public:
    NODISCARD bool import_snapshot(
        GameSnapshot snapshot, const NewGamePlusConfiguration& configuration = {},
        const RangerState* baseline = nullptr);

    NODISCARD bool import_snapshot(RuntimeGameSnapshot snapshot);

    NODISCARD bool loaded() const noexcept { return snapshot_.has_value(); }

    NODISCARD const RuntimeRangerState* ranger() const noexcept;

    NODISCARD RuntimeRangerState* ranger() noexcept;

    NODISCARD const RuntimeGameSnapshot* snapshot() const noexcept;

    NODISCARD RuntimeGameSnapshot* snapshot() noexcept;

    NODISCARD std::optional<RuntimeGameSnapshot> export_snapshot() const;

private:
    std::optional<RuntimeGameSnapshot> snapshot_;
};

}
