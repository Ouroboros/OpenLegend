#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <toml++/toml.hpp>

#include "openlegend/attributes.hpp"
#include "openlegend/scene/scene.hpp"

namespace openlegend::app {

struct BattleMergeRulesLoadResult {
    std::vector<scene::BattleMergeRule> rules;
    // 解析或取值错误时给出说明，规则全部不生效。
    std::string error;
    // 资源指纹不符而停用的规则及原因，其余规则照常生效。
    std::vector<std::string> skipped;
};

// 解析MOD配置中的合战规则，每条规则为一个[[merge]]表，其[merge.assets]子表列出该规则所依赖
// 的资源文件及其SHA-256；读取data_root中对应文件核对，任一不符则只停用该条规则。
NODISCARD BattleMergeRulesLoadResult parse_battle_merge_rules(
    const toml::array& merges, const std::filesystem::path& data_root);

}  // namespace openlegend::app
