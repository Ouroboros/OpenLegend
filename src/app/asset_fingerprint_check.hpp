#pragma once

#include <filesystem>
#include <string>

#include <toml++/toml.hpp>

#include "openlegend/attributes.hpp"

namespace openlegend::app {

// 核对配置中列出的资源文件（相对数据目录）SHA-256：返回空串表示全部相符，
// 否则返回第一个不符项的说明（含实际值）。
NODISCARD std::string check_asset_fingerprints(
    const toml::table& assets, const std::filesystem::path& data_root);

}  // namespace openlegend::app
