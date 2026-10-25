#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

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
