#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include <toml++/toml.hpp>

#include "openlegend/attributes.hpp"
#include "openlegend/resource/asset_file_names.hpp"

namespace openlegend::app {

// MOD配置文件的格式版本；文件中的format须与此相同。
inline constexpr std::int64_t kModConfigurationFormat = 1;

// MOD配置（由openlegend.toml的[paths] mod指定，与data_dir配套）。
// 未指定时使用程序内置的原版设定。
struct ModConfiguration {
    std::string name;
    // [paths]：覆盖openlegend.toml的data_dir、save_dir；相对路径以可执行文件目录为基准。
    std::optional<std::string> data_directory;
    std::optional<std::string> save_directory;
    // [new_game_plus]：NG+设置，字段与取值由NG+配置统一校验；未写时不开启NG+。
    std::optional<toml::table> new_game_plus;
    // [saves] last_slot：程序记忆的上次存档槽；取值无效时当作没有记忆。
    std::optional<std::uint16_t> last_save_slot;
    // [[merge]]：合战规则，由parse_battle_merge_rules按数据目录核对资源后生效。
    std::optional<toml::array> merges;
    // [files]：按编号打开的资源文件名表；未列出的类别使用原版命名。
    resource::AssetFileNames files;
};

struct ModConfigurationLoadResult {
    ModConfiguration configuration;
    // 文件不存在或格式错误时给出说明。
    std::string error;
};

NODISCARD ModConfigurationLoadResult load_mod_configuration(const std::filesystem::path& path);

}  // namespace openlegend::app
