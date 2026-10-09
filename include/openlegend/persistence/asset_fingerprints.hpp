#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "openlegend/attributes.hpp"

namespace openlegend::persistence {

struct SaveAssetDefinition {
    std::string_view key;
    std::string_view filename;
};

inline constexpr std::array<SaveAssetDefinition, 9> kNewGamePlusSaveAssets{{
    {"ranger_idx_sha256", "RANGER.IDX"},
    {"ranger_grp_sha256", "RANGER.GRP"},
    {"allsin_idx_sha256", "ALLSIN.IDX"},
    {"allsin_grp_sha256", "ALLSIN.GRP"},
    {"alldef_idx_sha256", "ALLDEF.IDX"},
    {"alldef_grp_sha256", "ALLDEF.GRP"},
    {"kdef_idx_sha256", "KDEF.IDX"},
    {"kdef_grp_sha256", "KDEF.GRP"},
    {"war_sta_sha256", "WAR.STA"},
}};

struct AssetFingerprints {
    std::array<std::string, kNewGamePlusSaveAssets.size()> sha256;

    NODISCARD bool operator==(const AssetFingerprints&) const = default;
};

struct AssetFingerprintResult {
    std::optional<AssetFingerprints> fingerprints;
    std::filesystem::path path;
    std::string error;

    NODISCARD explicit operator bool() const noexcept {
        return fingerprints.has_value() && error.empty();
    }
};

NODISCARD AssetFingerprintResult fingerprint_new_game_plus_assets(
    const std::filesystem::path& root);

}
