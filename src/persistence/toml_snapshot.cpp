#include "openlegend/persistence/toml_snapshot.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <toml++/toml.hpp>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/scene_archive_codec.hpp"

namespace openlegend::persistence {
namespace {

struct RoleIntegerField {
    std::string_view name;
    std::int64_t model::RoleState::* member;
};

constexpr auto kRoleIntegers = std::to_array<RoleIntegerField>({
    {"increased_life", &model::RoleState::increased_life},
    {"unused", &model::RoleState::unused},
    {"sex", &model::RoleState::sex},
    {"level", &model::RoleState::level},
    {"experience", &model::RoleState::experience},
    {"hp", &model::RoleState::hp},
    {"maximum_hp", &model::RoleState::maximum_hp},
    {"hurt", &model::RoleState::hurt},
    {"poison", &model::RoleState::poison},
    {"physical_power", &model::RoleState::physical_power},
    {"make_item_experience", &model::RoleState::make_item_experience},
    {"mp_type", &model::RoleState::mp_type},
    {"mp", &model::RoleState::mp},
    {"maximum_mp", &model::RoleState::maximum_mp},
    {"attack", &model::RoleState::attack},
    {"speed", &model::RoleState::speed},
    {"defence", &model::RoleState::defence},
    {"medicine", &model::RoleState::medicine},
    {"use_poison", &model::RoleState::use_poison},
    {"detoxification", &model::RoleState::detoxification},
    {"anti_poison", &model::RoleState::anti_poison},
    {"fist", &model::RoleState::fist},
    {"sword", &model::RoleState::sword},
    {"knife", &model::RoleState::knife},
    {"unusual", &model::RoleState::unusual},
    {"hidden_weapon", &model::RoleState::hidden_weapon},
    {"knowledge", &model::RoleState::knowledge},
    {"morality", &model::RoleState::morality},
    {"attack_with_poison", &model::RoleState::attack_with_poison},
    {"attack_twice", &model::RoleState::attack_twice},
    {"fame", &model::RoleState::fame},
    {"iq", &model::RoleState::iq},
    {"item_experience", &model::RoleState::item_experience},
});

constexpr std::array<std::string_view, model::header_word::team_begin> kHeaderFields{
    "in_ship", "in_sub_map", "main_map_x", "main_map_y", "sub_map_x", "sub_map_y",
    "face_towards", "ship_x", "ship_y", "ship_x_1", "ship_y_1", "ship_direction"};

class InvalidSave final : public std::runtime_error {
public:
    InvalidSave(const TomlSaveStatus value, const std::string& detail)
        : std::runtime_error{detail}, status{value} {}

    const TomlSaveStatus status;
};

void require(
    const bool condition, const std::string_view detail,
    const TomlSaveStatus status = TomlSaveStatus::invalid_document) {
    if (!condition) {
        throw InvalidSave{status, std::string{detail}};
    }
}

NODISCARD const toml::table& table_value(
    const toml::node* node, const std::size_t count, const std::string_view field) {
    const auto* value = node == nullptr ? nullptr : node->as_table();
    require(value != nullptr, std::string{field} + " must be a table");
    require(value->size() == count, std::string{field} + " has missing or unknown fields");
    return *value;
}

NODISCARD const toml::array& array_value(const toml::node* node, const std::string_view field) {
    const auto* value = node == nullptr ? nullptr : node->as_array();
    require(value != nullptr, std::string{field} + " must be an array");
    return *value;
}

NODISCARD std::int64_t integer_value(const toml::node* node, const std::string_view field) {
    const auto* value = node == nullptr ? nullptr : node->as_integer();
    require(value != nullptr, std::string{field} + " must be an integer");
    return value->get();
}

NODISCARD const std::string& string_value(const toml::node* node, const std::string_view field) {
    const auto* value = node == nullptr ? nullptr : node->as_string();
    require(value != nullptr, std::string{field} + " must be a string");
    return value->get();
}

NODISCARD std::int16_t native_integer(const std::int64_t value, const std::string_view field) {
    require(value >= std::numeric_limits<std::int16_t>::min() &&
        value <= std::numeric_limits<std::int16_t>::max(),
        std::string{field} + " exceeds its native integer domain");
    return static_cast<std::int16_t>(value);
}

template <typename Container>
void read_integer_array(
    const toml::node* node, Container& destination, const std::string_view field) {
    const auto& values = array_value(node, field);
    require(values.size() == destination.size(), std::string{field} + " has an invalid length");
    for (std::size_t index = 0U; index < values.size(); ++index) {
        const auto value = integer_value(values.get(index), field);
        using Value = typename Container::value_type;
        if constexpr (std::is_same_v<Value, std::int64_t>) {
            destination[index] = value;
        } else if constexpr (std::is_same_v<Value, std::int16_t>) {
            destination[index] = native_integer(value, field);
        } else {
            destination[index].value = native_integer(value, field);
        }
    }
}

template <typename Container>
NODISCARD toml::array integer_array(const Container& source) {
    toml::array result;
    for (const auto& value : source) {
        if constexpr (std::is_integral_v<typename Container::value_type>) {
            result.push_back(value);
        } else {
            result.push_back(value.value);
        }
    }
    return result;
}

NODISCARD std::string_view format_name(const TomlSaveKind kind) noexcept {
    switch (kind) {
    case TomlSaveKind::ordinary:
        return "openlegend-ngplus";
    case TomlSaveKind::completion:
        return "openlegend-ngplus-completion";
    }
    return {};
}

void validate_purpose(const TomlSaveKind kind, const SaveSlot slot) {
    require(!format_name(kind).empty() && static_cast<unsigned int>(slot) < kNumberedSaveSlotCount,
        "invalid TOML save kind or slot", TomlSaveStatus::invalid_metadata);
}

NODISCARD bool valid_timestamp(const std::string_view value) noexcept {
    if (value.size() != 20U || value[4U] != '-' || value[7U] != '-' || value[10U] != 'T' ||
        value[13U] != ':' || value[16U] != ':' || value[19U] != 'Z') {
        return false;
    }
    constexpr std::array<std::size_t, 6> starts{0U, 5U, 8U, 11U, 14U, 17U};
    std::array<int, 6> components{};
    for (std::size_t component = 0U; component < starts.size(); ++component) {
        const auto count = component == 0U ? 4U : 2U;
        for (std::size_t digit = 0U; digit < count; ++digit) {
            const auto character = value[starts[component] + digit];
            if (character < '0' || character > '9') {
                return false;
            }
            components[component] = components[component] * 10 + character - '0';
        }
    }
    const auto date = std::chrono::year_month_day{
        std::chrono::year{components[0U]}, std::chrono::month{static_cast<unsigned>(components[1U])},
        std::chrono::day{static_cast<unsigned>(components[2U])}};
    const auto month_end = std::chrono::year_month_day_last{
        date.year(), std::chrono::month_day_last{date.month()}};
    return date.ok() && components[3U] < 24 && components[4U] < 60 && components[5U] <= 60 &&
        (components[5U] != 60 ||
            (components[3U] == 23 && components[4U] == 59 && date.day() == month_end.day()));
}

NODISCARD std::string normalized_fingerprint(const std::string_view value) {
    require(value.size() == 64U, "invalid asset SHA256 length", TomlSaveStatus::incompatible_assets);
    std::string result{value};
    for (auto& character : result) {
        if (character >= 'A' && character <= 'F') {
            character = static_cast<char>(character - 'A' + 'a');
        }
        require((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f'),
            "invalid asset SHA256 character", TomlSaveStatus::incompatible_assets);
    }
    return result;
}

void validate_context(const TomlSnapshotContext& context) {
    require(context.configuration.enabled && context.baseline.valid() &&
        model::calculate_playthrough_limits(context.configuration, 1).has_value() &&
        model::calculate_playthrough_limits(
            context.configuration, context.configuration.maximum_playthroughs).has_value() &&
        (context.configuration.maximum_playthroughs < 2 ||
            model::calculate_playthrough_limits(context.configuration, 2).has_value()),
        "invalid TOML snapshot configuration or baseline", TomlSaveStatus::invalid_snapshot);
    for (std::size_t index = 0U; index < context.baseline.roles.size(); ++index) {
        require(context.baseline.roles[index].id().value == static_cast<std::int16_t>(index),
            "baseline role IDs do not match their indices", TomlSaveStatus::invalid_snapshot);
    }
}

NODISCARD std::array<bool, model::kRoleCount> saved_roles(const model::RuntimeRangerState& ranger) {
    std::array<bool, model::kRoleCount> result{};
    for (std::size_t index = 0U; index < result.size(); ++index) {
        result[index] = ranger.roles[index].ever_joined;
    }
    for (std::size_t slot = 0U; slot < model::kTeamMemberCount; ++slot) {
        const auto role_id = ranger.header.team_member(slot).value;
        require(role_id == -1 || (role_id >= 0 && static_cast<std::size_t>(role_id) < result.size()),
            "invalid team member", TomlSaveStatus::invalid_snapshot);
        if (role_id >= 0) {
            result[static_cast<std::size_t>(role_id)] = true;
        }
    }
    return result;
}

void read_header(const toml::table& document, model::RuntimeRangerHeader& header) {
    const auto& table = table_value(document.get("header"), kHeaderFields.size() + 2U, "header");
    for (std::size_t index = 0U; index < kHeaderFields.size(); ++index) {
        header.set_word(index, native_integer(
            integer_value(table.get(kHeaderFields[index]), kHeaderFields[index]), kHeaderFields[index]));
    }
    std::array<model::CharacterId, model::kTeamMemberCount> team{};
    read_integer_array(table.get("team_members"), team, "header.team_members");
    for (std::size_t index = 0U; index < team.size(); ++index) {
        header.set_team_member(index, team[index]);
    }
    const auto& inventory = array_value(table.get("inventory"), "header.inventory");
    require(inventory.size() <= model::kInventoryCount, "header.inventory is too long");
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        if (index >= inventory.size()) {
            header.set_inventory(index, model::ItemId{-1}, 0);
            continue;
        }
        const auto& entry = table_value(inventory.get(index), 2U, "header.inventory entry");
        const auto item_id = native_integer(integer_value(entry.get("item_id"), "item_id"), "item_id");
        const auto count = integer_value(entry.get("count"), "count");
        require(item_id >= 0 && count > 0, "inventory entries must be occupied and positive");
        header.set_inventory(index, model::ItemId{item_id}, count);
    }
}

void read_role(const toml::table& table, model::RoleState& role) {
    for (const auto& field : kRoleIntegers) {
        role.*field.member = integer_value(table.get(field.name), field.name);
    }
    const auto& name = string_value(table.get("name"), "roles.name");
    const auto& nickname = string_value(table.get("nickname"), "roles.nickname");
    require(name.find('\0') == std::string::npos && nickname.find('\0') == std::string::npos,
        "role names cannot contain NUL", TomlSaveStatus::invalid_snapshot);
    role.name.assign(name.begin(), name.end());
    role.nickname.assign(nickname.begin(), nickname.end());
    role.practice_item.value = native_integer(
        integer_value(table.get("practice_item"), "practice_item"), "practice_item");
    read_integer_array(table.get("equipment"), role.equipment, "equipment");
    read_integer_array(table.get("frames"), role.frames, "frames");
    read_integer_array(table.get("magic_ids"), role.magic_ids, "magic_ids");
    read_integer_array(table.get("magic_levels"), role.magic_levels, "magic_levels");
    read_integer_array(table.get("taking_items"), role.taking_items, "taking_items");
    read_integer_array(table.get("taking_counts"), role.taking_counts, "taking_counts");
    read_integer_array(table.get("no_magic_count"), role.no_magic_count, "no_magic_count");
    const auto* node = table.get("ever_joined");
    const auto* boolean = node == nullptr ? nullptr : node->as_boolean();
    require(boolean != nullptr, "ever_joined must be a boolean");
    role.ever_joined = boolean->get();
}

void read_roles(const toml::table& document, model::RuntimeRangerState& ranger) {
    const auto& roles = array_value(document.get("roles"), "roles");
    require(roles.size() <= ranger.roles.size(), "too many saved roles");
    std::array<bool, model::kRoleCount> present{};
    std::int64_t previous_id = -1;
    for (const auto& node : roles) {
        const auto& table = table_value(&node, kRoleIntegers.size() + 12U, "roles entry");
        const auto role_id = integer_value(table.get("id"), "roles.id");
        require(role_id > previous_id && role_id < static_cast<std::int64_t>(ranger.roles.size()),
            "saved role IDs must be in range and strictly increasing");
        previous_id = role_id;
        const auto index = static_cast<std::size_t>(role_id);
        present[index] = true;
        read_role(table, ranger.roles[index]);
    }
    require(present == saved_roles(ranger), "saved roles do not match team and ever_joined roles");
}

void read_mutable_definitions(const toml::table& document, model::RuntimeRangerState& ranger) {
    const auto& items = table_value(document.get("item_state"), 1U, "item_state");
    std::array<std::int16_t, model::kItemCount> users{};
    read_integer_array(items.get("users"), users, "item_state.users");
    for (std::size_t index = 0U; index < users.size(); ++index) {
        ranger.items[index].set_word(model::item_word::user, users[index]);
    }
    const auto& scenes = table_value(document.get("scene_state"), 1U, "scene_state");
    std::array<std::int16_t, model::kSceneMetadataCount> conditions{};
    read_integer_array(scenes.get("entrance_conditions"), conditions, "scene_state.entrance_conditions");
    for (std::size_t index = 0U; index < conditions.size(); ++index) {
        ranger.scenes[index].set_word(model::scene_metadata_word::entrance_condition, conditions[index]);
    }
    const auto& shops = table_value(document.get("shop_state"), 1U, "shop_state");
    const auto& rows = array_value(shops.get("totals"), "shop_state.totals");
    require(rows.size() == ranger.shops.size(), "shop_state.totals has an invalid row count");
    for (std::size_t shop = 0U; shop < rows.size(); ++shop) {
        std::array<std::int16_t, model::shop_word::item_count> totals{};
        read_integer_array(rows.get(shop), totals, "shop_state.totals row");
        for (std::size_t slot = 0U; slot < totals.size(); ++slot) {
            ranger.shops[shop].set_word(model::shop_word::total_begin + slot, totals[slot]);
        }
    }
}

void read_scene_archives(const toml::table& document, model::SceneArchives& archives) {
    const auto& table = table_value(document.get("scene_archives"), 2U, "scene_archives");
    auto maps = decode_scene_archive(
        string_value(table.get("s_compressed_base64"), "s_compressed_base64"), SceneArchiveKind::maps);
    auto events = decode_scene_archive(
        string_value(table.get("d_compressed_base64"), "d_compressed_base64"), SceneArchiveKind::events);
    require(maps.has_value() && events.has_value(), "invalid compressed scene archives");
    archives.scene_maps = std::move(*maps);
    archives.scene_events = std::move(*events);
    for (std::size_t index = 0U; index < model::kSceneCount; ++index) {
        archives.scene_map_ends[index] = static_cast<std::uint32_t>((index + 1U) * model::kSceneMapBytesPerScene);
        archives.scene_event_ends[index] = static_cast<std::uint32_t>((index + 1U) * model::kSceneEventBytesPerScene);
    }
}

NODISCARD TomlSave read_document(
    const toml::table& document, const TomlSaveKind kind, const SaveSlot slot,
    const TomlSnapshotContext& context) {
    require(document.size() == 13U, "save root has missing or unknown fields");
    require(string_value(document.get("format"), "format") == format_name(kind) &&
        integer_value(document.get("format_version"), "format_version") == 2 &&
        integer_value(document.get("ruleset_version"), "ruleset_version") == 3 &&
        integer_value(document.get("slot"), "slot") == static_cast<std::int64_t>(slot) + 1,
        "save format, version, purpose or slot mismatch", TomlSaveStatus::invalid_metadata);
    TomlSave result;
    result.metadata = {kind, slot, string_value(document.get("timestamp_utc"), "timestamp_utc")};
    require(valid_timestamp(result.metadata.timestamp_utc),
        "invalid RFC3339 UTC save timestamp", TomlSaveStatus::invalid_metadata);
    auto& snapshot = result.snapshot;
    snapshot.playthrough = integer_value(document.get("playthrough"), "playthrough");
    require(model::calculate_playthrough_limits(context.configuration, snapshot.playthrough).has_value(),
        "invalid saved playthrough", TomlSaveStatus::invalid_metadata);
    const auto& assets = table_value(document.get("assets"), kNewGamePlusSaveAssets.size(), "assets");
    for (std::size_t index = 0U; index < kNewGamePlusSaveAssets.size(); ++index) {
        const auto key = kNewGamePlusSaveAssets[index].key;
        require(normalized_fingerprint(string_value(assets.get(key), key)) ==
            normalized_fingerprint(context.fingerprints.sha256[index]),
            std::string{"asset version mismatch: "} + std::string{key}, TomlSaveStatus::incompatible_assets);
    }
    auto ranger = model::decode_legacy_ranger(context.baseline);
    require(ranger.has_value(), "cannot decode the baseline", TomlSaveStatus::invalid_snapshot);
    snapshot.ranger = std::move(*ranger);
    snapshot.configuration = context.configuration;
    snapshot.origin = model::SnapshotOrigin::new_game_plus;
    read_header(document, snapshot.ranger.header);
    read_roles(document, snapshot.ranger);
    read_mutable_definitions(document, snapshot.ranger);
    read_scene_archives(document, snapshot);
    require(snapshot.valid_for_persistence(), "invalid saved field values or references",
        TomlSaveStatus::invalid_snapshot);
    return result;
}

NODISCARD toml::table write_header(const model::RuntimeRangerHeader& header) {
    toml::table result;
    for (std::size_t index = 0U; index < kHeaderFields.size(); ++index) {
        result.insert(kHeaderFields[index], header.word(index));
    }
    toml::array team;
    for (std::size_t index = 0U; index < model::kTeamMemberCount; ++index) {
        team.push_back(header.team_member(index).value);
    }
    result.insert("team_members", std::move(team));
    toml::array inventory;
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        const auto item_id = header.inventory_item(index).value;
        if (item_id == -1) {
            break;
        }
        toml::table entry{{"item_id", item_id}, {"count", header.inventory_count(index)}};
        entry.is_inline(true);
        inventory.push_back(std::move(entry));
    }
    result.insert("inventory", std::move(inventory));
    return result;
}

NODISCARD toml::table write_role(const model::RoleState& role) {
    toml::table result;
    result.insert("id", role.id.value);
    for (const auto& field : kRoleIntegers) {
        result.insert(field.name, role.*field.member);
    }
    result.insert("name", std::string{reinterpret_cast<const char*>(role.name.data()), role.name.size()});
    result.insert("nickname", std::string{reinterpret_cast<const char*>(role.nickname.data()), role.nickname.size()});
    result.insert("practice_item", role.practice_item.value);
    result.insert("equipment", integer_array(role.equipment));
    result.insert("frames", integer_array(role.frames));
    result.insert("magic_ids", integer_array(role.magic_ids));
    result.insert("magic_levels", integer_array(role.magic_levels));
    result.insert("taking_items", integer_array(role.taking_items));
    result.insert("taking_counts", integer_array(role.taking_counts));
    result.insert("ever_joined", role.ever_joined);
    result.insert("no_magic_count", integer_array(role.no_magic_count));
    return result;
}

void write_mutable_definitions(toml::table& document, const model::RuntimeRangerState& ranger) {
    toml::array users;
    for (const auto& item : ranger.items) {
        users.push_back(item.word(model::item_word::user));
    }
    document.insert("item_state", toml::table{{"users", std::move(users)}});
    toml::array conditions;
    for (const auto& scene : ranger.scenes) {
        conditions.push_back(scene.word(model::scene_metadata_word::entrance_condition));
    }
    document.insert("scene_state", toml::table{{"entrance_conditions", std::move(conditions)}});
    toml::array rows;
    for (const auto& shop : ranger.shops) {
        toml::array totals;
        for (std::size_t slot = 0U; slot < model::shop_word::item_count; ++slot) {
            totals.push_back(shop.word(model::shop_word::total_begin + slot));
        }
        rows.push_back(std::move(totals));
    }
    document.insert("shop_state", toml::table{{"totals", std::move(rows)}});
}

NODISCARD std::string write_document(
    const model::RuntimeGameSnapshot& snapshot, const TomlSaveMetadata& metadata,
    const TomlSnapshotContext& context) {
    toml::table document{
        {"format", format_name(metadata.kind)}, {"format_version", 2}, {"ruleset_version", 3},
        {"slot", static_cast<std::int64_t>(metadata.slot) + 1},
        {"timestamp_utc", metadata.timestamp_utc}, {"playthrough", snapshot.playthrough}};
    toml::table assets;
    for (std::size_t index = 0U; index < kNewGamePlusSaveAssets.size(); ++index) {
        assets.insert(kNewGamePlusSaveAssets[index].key, normalized_fingerprint(context.fingerprints.sha256[index]));
    }
    document.insert("assets", std::move(assets));
    document.insert("header", write_header(snapshot.ranger.header));
    toml::array roles;
    const auto selected = saved_roles(snapshot.ranger);
    for (std::size_t index = 0U; index < selected.size(); ++index) {
        if (selected[index]) {
            roles.push_back(write_role(snapshot.ranger.roles[index]));
        }
    }
    document.insert("roles", std::move(roles));
    write_mutable_definitions(document, snapshot.ranger);
    auto maps = encode_scene_archive(snapshot.scene_maps, SceneArchiveKind::maps);
    auto events = encode_scene_archive(snapshot.scene_events, SceneArchiveKind::events);
    require(maps.has_value() && events.has_value(), "cannot compress scene archives", TomlSaveStatus::invalid_snapshot);
    document.insert("scene_archives", toml::table{
        {"s_compressed_base64", std::move(*maps)}, {"d_compressed_base64", std::move(*events)}});
    std::ostringstream output;
    output << toml::toml_formatter{document};
    require(output.good(), "cannot format the TOML snapshot", TomlSaveStatus::invalid_snapshot);
    auto result = output.str();
    result += '\n';
    require(result.size() <= kMaximumTomlSaveBytes, "TOML snapshot exceeds 64 MiB");
    return result;
}

}

TomlSnapshotReadResult decode_toml_snapshot(
    const std::string_view document, const TomlSaveKind kind, const SaveSlot slot,
    const TomlSnapshotContext& context) {
    try {
        validate_purpose(kind, slot);
        validate_context(context);
        require(document.size() <= kMaximumTomlSaveBytes, "TOML snapshot exceeds 64 MiB");
        auto table = toml::parse(document);
        auto save = read_document(table, kind, slot, context);
        return {TomlSaveStatus::ready, std::move(save), {}};
    } catch (const toml::parse_error& error) {
        return {TomlSaveStatus::invalid_document, std::nullopt, std::string{error.description()}};
    } catch (const InvalidSave& error) {
        return {error.status, std::nullopt, error.what()};
    }
}

TomlSnapshotWriteResult encode_toml_snapshot(
    const model::RuntimeGameSnapshot& snapshot, const TomlSaveMetadata& metadata,
    const TomlSnapshotContext& context) {
    try {
        validate_purpose(metadata.kind, metadata.slot);
        validate_context(context);
        require(valid_timestamp(metadata.timestamp_utc),
            "invalid RFC3339 UTC save timestamp", TomlSaveStatus::invalid_metadata);
        require(snapshot.configuration == context.configuration && snapshot.valid_for_persistence() &&
            snapshot.ranger.matches_legacy_definitions(context.baseline),
            "invalid snapshot or static definitions", TomlSaveStatus::invalid_snapshot);
        auto document = write_document(snapshot, metadata, context);
        auto decoded = decode_toml_snapshot(document, metadata.kind, metadata.slot, context);
        require(static_cast<bool>(decoded), decoded.detail, decoded.status);
        auto baseline = model::decode_legacy_ranger(context.baseline);
        require(baseline.has_value(), "cannot decode the baseline", TomlSaveStatus::invalid_snapshot);
        auto expected_ranger = snapshot.ranger;
        const auto selected = saved_roles(snapshot.ranger);
        for (std::size_t index = 0U; index < selected.size(); ++index) {
            if (!selected[index]) {
                expected_ranger.roles[index] = std::move(baseline->roles[index]);
            }
        }
        require(decoded.save->metadata == metadata && decoded.save->snapshot.ranger == expected_ranger &&
            static_cast<const model::SceneArchives&>(decoded.save->snapshot) ==
                static_cast<const model::SceneArchives&>(snapshot) &&
            decoded.save->snapshot.playthrough == snapshot.playthrough &&
            decoded.save->snapshot.configuration == snapshot.configuration,
            "TOML snapshot failed its complete read-back comparison", TomlSaveStatus::invalid_snapshot);
        return {TomlSaveStatus::ready, std::move(document), {}};
    } catch (const InvalidSave& error) {
        return {error.status, std::nullopt, error.what()};
    }
}

}
