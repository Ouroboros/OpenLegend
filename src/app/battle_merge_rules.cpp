#include "battle_merge_rules.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

#include <toml++/toml.hpp>

#include "asset_fingerprint_check.hpp"

namespace openlegend::app {
namespace {

NODISCARD std::optional<std::int16_t> read_int16(const toml::node* node) {
    if (node == nullptr) {
        return std::nullopt;
    }
    const auto value = node->value<std::int64_t>();
    if (!value.has_value() || *value < std::numeric_limits<std::int16_t>::min() ||
        *value > std::numeric_limits<std::int16_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::int16_t>(*value);
}

}  // namespace

BattleMergeRulesLoadResult load_battle_merge_rules(
    const std::filesystem::path& path, const std::filesystem::path& data_root) {
    BattleMergeRulesLoadResult result;
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        return result;
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        result.error = "unable to read " + path.string();
        return result;
    }
    toml::table document;
    try {
        document = toml::parse(input, path.string());
    } catch (const toml::parse_error& parse_error) {
        result.error = std::string{parse_error.description()};
        return result;
    }
    const auto* merges = document.get_as<toml::array>("merge");
    if (merges == nullptr) {
        return result;
    }
    for (std::size_t index = 0U; index < merges->size(); ++index) {
        const auto fail = [&result, index](const std::string_view detail) {
            result.rules.clear();
            result.error = "[[merge]] #" + std::to_string(index + 1U) + ": " + std::string{detail};
        };
        const auto* table = merges->get(index)->as_table();
        if (table == nullptr) {
            fail("must be a table");
            return result;
        }
        // 每条规则自带[merge.assets]，列出它所依赖的资源文件及SHA-256。
        const auto* assets = table->get_as<toml::table>("assets");
        if (assets == nullptr || assets->empty()) {
            fail("assets must list the asset files and SHA-256 this rule depends on");
            return result;
        }
        for (const auto& [key, node] : *assets) {
            if (!node.value<std::string>().has_value()) {
                fail("assets." + std::string{key.str()} + " must be a SHA-256 string");
                return result;
            }
        }
        scene::BattleMergeRule rule;
        const auto battle = read_int16(table->get("battle"));
        const auto merge_battle = read_int16(table->get("merge_battle"));
        if (!battle.has_value() || *battle < 0 || !merge_battle.has_value() || *merge_battle < 0) {
            fail("battle and merge_battle must be non-negative integers");
            return result;
        }
        rule.battle = *battle;
        rule.merge_battle = *merge_battle;
        if (const auto* node = table->get("new_game_plus_only")) {
            const auto value = node->value<bool>();
            if (!value.has_value()) {
                fail("new_game_plus_only must be a boolean");
                return result;
            }
            rule.new_game_plus_only = *value;
        }
        if (const auto* node = table->get("role")) {
            const auto value = read_int16(node);
            if (!value.has_value() || *value < 0) {
                fail("role must be a non-negative integer");
                return result;
            }
            rule.role = *value;
        }
        for (const auto [key, field] : {std::pair{"morality_min", &rule.morality_min},
                 std::pair{"morality_max", &rule.morality_max}}) {
            if (const auto* node = table->get(key)) {
                const auto value = node->value<std::int64_t>();
                if (!value.has_value()) {
                    fail(std::string{key} + " must be an integer");
                    return result;
                }
                *field = *value;
            }
        }
        if (const auto* node = table->get("dialogues")) {
            const auto* lines = node->as_array();
            if (lines == nullptr) {
                fail("dialogues must be an array of [talk_id, head_id, style]");
                return result;
            }
            for (const auto& line : *lines) {
                const auto* entry = line.as_array();
                const auto talk = entry != nullptr && entry->size() == 3U
                    ? read_int16(entry->get(0)) : std::nullopt;
                const auto head = talk.has_value() ? read_int16(entry->get(1)) : std::nullopt;
                const auto style = head.has_value() ? read_int16(entry->get(2)) : std::nullopt;
                if (!style.has_value()) {
                    fail("dialogues must be an array of [talk_id, head_id, style]");
                    return result;
                }
                rule.dialogues.push_back({*talk, *head, *style});
            }
        }
        // 资源与规则编写时不符（脚本或数据被改过）时只停用这一条。
        if (auto mismatch = check_asset_fingerprints(*assets, data_root); !mismatch.empty()) {
            result.skipped.push_back(
                "[[merge]] #" + std::to_string(index + 1U) + " assets." + std::move(mismatch));
            continue;
        }
        result.rules.push_back(std::move(rule));
    }
    return result;
}

}  // namespace openlegend::app
