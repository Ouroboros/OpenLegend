#include "mod_configuration.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <string_view>
#include <system_error>

#include <toml++/toml.hpp>

#include "openlegend/persistence/save_slot.hpp"

namespace openlegend::app {
namespace {

constexpr std::array<std::string_view, 7U> kRootKeys{
    "format", "name", "paths", "new_game_plus", "saves", "merge", "files"};

constexpr std::array<std::string_view, 2U> kPathKeys{"data_dir", "save_dir"};

constexpr std::array<std::string_view, resource::kAssetFileKindCount> kFileKeys{
    "scene_sprite_index", "scene_sprite_group", "battlefield_index",
    "battlefield_group", "fight_index", "fight_group"};

// 文件名须为数据目录下的单个文件名，不含路径分隔符。
NODISCARD bool file_name_valid(const std::string_view name) noexcept {
    return !name.empty() && name.find_first_of("/\\:") == std::string_view::npos &&
        name != "." && name != "..";
}

NODISCARD std::string read_files(const toml::table& files, resource::AssetFileNames& names) {
    for (const auto& [key, node] : files) {
        const auto found = std::find(kFileKeys.begin(), kFileKeys.end(), key.str());
        if (found == kFileKeys.end()) {
            return "files." + std::string{key.str()} + " is not a known file table";
        }
        const auto* values = node.as_array();
        if (values == nullptr || values->empty()) {
            return "files." + std::string{key.str()} + " must be a non-empty array";
        }
        auto& list = names.lists[static_cast<std::size_t>(found - kFileKeys.begin())];
        for (const auto& value : *values) {
            const auto name = value.value<std::string>();
            if (!name.has_value() || !file_name_valid(*name)) {
                return "files." + std::string{key.str()} + " entries must be plain file names";
            }
            list.push_back(*name);
        }
    }
    return {};
}

NODISCARD std::string read_paths(const toml::table& paths, ModConfiguration& configuration) {
    for (const auto& [key, node] : paths) {
        if (std::find(kPathKeys.begin(), kPathKeys.end(), key.str()) == kPathKeys.end()) {
            return "paths." + std::string{key.str()} + " is not a known field";
        }
        const auto value = node.value<std::string>();
        if (!value.has_value() || value->empty()) {
            return "paths." + std::string{key.str()} + " must be a non-empty string";
        }
        (key.str() == "data_dir" ? configuration.data_directory : configuration.save_directory) =
            *value;
    }
    return {};
}

}  // namespace

ModConfigurationLoadResult load_mod_configuration(const std::filesystem::path& path) {
    ModConfigurationLoadResult result;
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        result.error = "file not found";
        return result;
    }
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        result.error = "unable to read the file";
        return result;
    }
    toml::table document;
    try {
        document = toml::parse(input, path.string());
    } catch (const toml::parse_error& parse_error) {
        result.error = std::string{parse_error.description()};
        return result;
    }
    for (const auto& [key, node] : document) {
        if (std::find(kRootKeys.begin(), kRootKeys.end(), key.str()) == kRootKeys.end()) {
            result.error = std::string{key.str()} + " is not a known field";
            return result;
        }
    }
    const auto format = document.get_as<std::int64_t>("format");
    if (format == nullptr || format->get() != kModConfigurationFormat) {
        result.error = "format must be " + std::to_string(kModConfigurationFormat);
        return result;
    }
    const auto* name = document.get_as<std::string>("name");
    if (name == nullptr || name->get().empty()) {
        result.error = "name must be a non-empty string";
        return result;
    }
    result.configuration.name = name->get();
    if (const auto* node = document.get("paths")) {
        const auto* paths = node->as_table();
        if (paths == nullptr) {
            result.error = "paths must be a table";
            return result;
        }
        if (result.error = read_paths(*paths, result.configuration); !result.error.empty()) {
            return result;
        }
    }
    if (const auto* node = document.get("new_game_plus")) {
        const auto* table = node->as_table();
        if (table == nullptr) {
            result.error = "new_game_plus must be a table";
            return result;
        }
        result.configuration.new_game_plus = *table;
    }
    if (const auto* node = document.get("saves")) {
        const auto* saves = node->as_table();
        if (saves == nullptr) {
            result.error = "saves must be a table";
            return result;
        }
        for (const auto& [key, value] : *saves) {
            static_cast<void>(value);
            if (key.str() != "last_slot") {
                result.error = "saves." + std::string{key.str()} + " is not a known field";
                return result;
            }
        }
        const auto slot = (*saves)["last_slot"].value<std::int64_t>();
        if (slot.has_value() && *slot >= 0 &&
            *slot < static_cast<std::int64_t>(persistence::kNumberedSaveSlotCount)) {
            result.configuration.last_save_slot = static_cast<std::uint16_t>(*slot);
        }
    }
    if (const auto* node = document.get("merge")) {
        const auto* merges = node->as_array();
        if (merges == nullptr) {
            result.error = "merge must be an array of tables ([[merge]])";
            return result;
        }
        result.configuration.merges = *merges;
    }
    if (const auto* node = document.get("files")) {
        const auto* files = node->as_table();
        if (files == nullptr) {
            result.error = "files must be a table";
            return result;
        }
        result.error = read_files(*files, result.configuration.files);
    }
    return result;
}

}  // namespace openlegend::app
