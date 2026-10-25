#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"

namespace openlegend::resource {

// 按编号打开的资源文件。原程序按 Z.DAT 中的文件名表、以编号为下标取文件名。
enum class AssetFileKind : std::size_t {
    scene_sprite_index,
    scene_sprite_group,
    battlefield_index,
    battlefield_group,
    fight_index,
    fight_group,
};

inline constexpr std::size_t kAssetFileKindCount = 6U;

// 各类资源的文件名列表，第n项为编号n的文件名；列表为空时使用原版命名
// （SDXnnn、SMPnnn、WDXnnn、WMPnnn、FIGHTnnn.IDX、FIGHTnnn.GRP）。
struct AssetFileNames {
    std::array<std::vector<std::string>, kAssetFileKindCount> lists;

    // 编号超出所配置列表或原版命名范围（0～999）时为空。
    NODISCARD std::optional<std::string> name(AssetFileKind kind, std::int64_t id) const;
};

}  // namespace openlegend::resource
