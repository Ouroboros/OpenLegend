#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/scene/scene.hpp"

namespace openlegend::app {

struct BattleMergeRulesLoadResult {
    std::vector<scene::BattleMergeRule> rules;
    // 文件不存在时为空且无错误；解析或取值错误时给出说明，规则全部不生效。
    std::string error;
    // 资源指纹不符而停用的规则及原因，其余规则照常生效。
    std::vector<std::string> skipped;
};

// 读取合战规则文件，每条规则为一个[[merge]]表，其[merge.assets]子表列出该规则所依赖的
// 资源文件及其SHA-256；读取data_root中对应文件核对，任一不符则只停用该条规则。
NODISCARD BattleMergeRulesLoadResult load_battle_merge_rules(
    const std::filesystem::path& path, const std::filesystem::path& data_root);

}  // namespace openlegend::app
