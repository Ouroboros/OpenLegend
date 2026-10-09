#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <toml++/toml.hpp>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/toml_snapshot.hpp"
#include "test_support.hpp"

namespace {

using namespace openlegend;
using namespace openlegend::persistence;

constexpr std::int64_t kWide = 5'000'000'000'000;
constexpr std::int64_t kMaximum = std::numeric_limits<std::int64_t>::max();
std::size_t rejection_checks{};

struct ScalarCase {
    std::string_view key;
    std::size_t word;
    std::int64_t value;
};

constexpr auto kScalars = std::to_array<ScalarCase>({
    {"increased_life", model::role_word::increased_life, 10},
    {"unused", model::role_word::unused, std::numeric_limits<std::int64_t>::min()},
    {"sex", model::role_word::sexual, -32768},
    {"level", model::role_word::level, kWide + 1},
    {"experience", model::role_word::experience, kWide + 2},
    {"hp", model::role_word::hp, kWide + 3},
    {"maximum_hp", model::role_word::maximum_hp, kWide + 4},
    {"hurt", model::role_word::hurt, 99},
    {"poison", model::role_word::poison, 99},
    {"physical_power", model::role_word::physical_power, 100},
    {"make_item_experience", model::role_word::make_item_experience, kWide + 7},
    {"mp_type", model::role_word::mp_type, 2},
    {"mp", model::role_word::mp, kWide + 5},
    {"maximum_mp", model::role_word::maximum_mp, kWide + 6},
    {"attack", model::role_word::attack, kWide + 8},
    {"speed", model::role_word::speed, 100},
    {"defence", model::role_word::defence, kWide + 9},
    {"medicine", model::role_word::medicine, kWide + 10},
    {"use_poison", model::role_word::use_poison, kWide + 11},
    {"detoxification", model::role_word::detoxification, kWide + 12},
    {"anti_poison", model::role_word::anti_poison, kWide + 13},
    {"fist", model::role_word::fist, kWide + 14},
    {"sword", model::role_word::sword, kWide + 15},
    {"knife", model::role_word::knife, kWide + 16},
    {"unusual", model::role_word::unusual, kWide + 17},
    {"hidden_weapon", model::role_word::hidden_weapon, kWide + 18},
    {"knowledge", model::role_word::knowledge, kWide + 19},
    {"morality", model::role_word::morality, 73},
    {"attack_with_poison", model::role_word::attack_with_poison, kWide + 20},
    {"attack_twice", model::role_word::attack_twice, 1},
    {"fame", model::role_word::fame, kWide + 21},
    {"iq", model::role_word::iq, 98},
    {"item_experience", model::role_word::item_experience, kWide + 22},
});

NODISCARD toml::table& section(toml::table& document, const std::string_view key) {
    return *document.get(key)->as_table();
}

NODISCARD toml::array& roles(toml::table& document) {
    return *document.get("roles")->as_array();
}

NODISCARD toml::table& role(toml::table& document, const std::size_t index = 0U) {
    return *roles(document).get(index)->as_table();
}

NODISCARD std::string format(const toml::table& document) {
    std::ostringstream output;
    output << toml::toml_formatter{document};
    return output.str();
}

template <typename Value>
void replace_array_value(toml::array& array, const std::size_t index, Value&& value) {
    array.replace(array.cbegin() + static_cast<std::ptrdiff_t>(index), std::forward<Value>(value));
}

template <typename Mutation>
void rejects(
    const toml::table& source, const TomlSnapshotContext& context, Mutation mutation,
    const std::source_location location = std::source_location::current()) {
    ++rejection_checks;
    auto changed = source;
    mutation(changed);
    const auto input = format(changed);
    const auto result = decode_toml_snapshot(input, TomlSaveKind::ordinary, SaveSlot::one, context);
    if (result) {
        std::cerr << "unexpectedly accepted malformed TOML at line " << location.line() << '\n';
    }
    OL_CHECK(!result && !result.save.has_value() && result.status != TomlSaveStatus::ready);
    OL_CHECK(!result.detail.empty());
}

void set_wide_state(model::RuntimeGameSnapshot& snapshot, const model::RangerState& baseline) {
    auto& actor = snapshot.ranger.roles[0U];
    for (const auto& field : kScalars) {
        actor.set_word(field.word, field.value);
    }
    actor.name = u8"五字角色名";
    actor.nickname = u8"繼承";
    actor.magic_ids[0U] = model::MagicId{1};
    actor.magic_levels[0U] = kMaximum;
    actor.taking_items[0U] = model::ItemId{0};
    actor.taking_counts[0U] = kWide + 23;
    for (std::size_t index = 0U; index < actor.frames.size(); ++index) {
        actor.frames[index] = static_cast<std::int16_t>(index + 1U);
    }
    bool found_manual = false;
    for (std::size_t item_id = 0U; item_id < snapshot.ranger.items.size(); ++item_id) {
        const auto& item = snapshot.ranger.items[item_id];
        if (item.word(model::item_word::item_type) == 2 &&
            item.word(model::item_word::magic_id) == -1 &&
            item.word(model::item_word::need_experience) > 0) {
            actor.practice_item = model::ItemId{static_cast<std::int16_t>(item_id)};
            actor.no_magic_count[item_id] = kMaximum;
            found_manual = true;
            break;
        }
    }
    OL_CHECK(found_manual);
    for (const auto index : {1U, 3U}) {
        snapshot.ranger.roles[index] = actor;
        snapshot.ranger.roles[index].id = model::CharacterId{static_cast<std::int16_t>(index)};
        snapshot.ranger.roles[index].head_id = baseline.roles[index].word(model::role_word::head_id);
    }
    snapshot.ranger.roles[1U].ever_joined = true;
    snapshot.ranger.roles[3U].name = u8"A";
    snapshot.ranger.roles[3U].nickname.clear();
    snapshot.ranger.roles[2U].attack = kWide + 24;
    for (std::size_t slot = 0U; slot < model::kTeamMemberCount; ++slot) {
        snapshot.ranger.header.set_team_member(slot, model::CharacterId{-1});
    }
    snapshot.ranger.header.set_team_member(0U, model::CharacterId{0});
    snapshot.ranger.header.set_team_member(1U, model::CharacterId{3});
    snapshot.ranger.header.set_word(model::header_word::encode, 3);
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        snapshot.ranger.header.set_inventory(slot, model::ItemId{-1}, 0);
    }
    snapshot.ranger.header.set_inventory(0U, model::ItemId{174}, kWide + 25);
    snapshot.ranger.header.set_inventory(1U, model::ItemId{174}, kMaximum);
    snapshot.ranger.items[1U].set_word(model::item_word::user, 319);
    snapshot.ranger.scenes[2U].set_word(model::scene_metadata_word::entrance_condition, 2);
    snapshot.ranger.shops[4U].set_word(model::shop_word::total_begin + 4U, 17);
    OL_CHECK(snapshot.set_scene_value(0U, model::SceneLayer::building, 17U, 123));
    OL_CHECK(snapshot.set_event_value(99U, 199U, model::SceneEventField::event_1, -1));
    OL_CHECK(snapshot.valid_for_persistence());
}

void check_round_trips(
    const model::RuntimeGameSnapshot& source, const TomlSnapshotContext& context) {
    const auto decoded_baseline = model::decode_legacy_ranger(context.baseline);
    OL_CHECK(decoded_baseline.has_value());
    for (const auto playthrough : {1LL, 2LL, 999LL}) {
        for (const auto kind : {TomlSaveKind::ordinary, TomlSaveKind::completion}) {
            auto current = source;
            current.playthrough = playthrough;
            current.origin = playthrough == 1 ? model::SnapshotOrigin::legacy : model::SnapshotOrigin::new_game_plus;
            const auto limits = model::calculate_playthrough_limits(current.configuration, playthrough);
            current.ranger.roles[0U].hurt = limits->hurt_maximum;
            const auto metadata = TomlSaveMetadata{
                kind, static_cast<SaveSlot>(playthrough - 1), "2026-09-14T08:00:00Z"};
            const auto original = current;
            const auto encoded = encode_toml_snapshot(current, metadata, context);
            OL_CHECK(encoded);
            OL_CHECK(current == original);
            if (!encoded) {
                std::cerr << encoded.detail << '\n';
                continue;
            }
            auto parsed = toml::parse(*encoded.document);
            OL_CHECK(parsed.size() == 13U && roles(parsed).size() == 3U);
            OL_CHECK(role(parsed, 0U).get("id")->as_integer()->get() == 0);
            OL_CHECK(role(parsed, 1U).get("id")->as_integer()->get() == 1);
            OL_CHECK(role(parsed, 2U).get("id")->as_integer()->get() == 3);
            OL_CHECK(role(parsed).size() == 45U && !role(parsed).contains("head_id"));
            for (const auto& field : kScalars) {
                const auto* value = role(parsed).get(field.key);
                OL_CHECK(value != nullptr && value->is_integer());
                if (value != nullptr && value->is_integer()) {
                    OL_CHECK(value->as_integer()->get() == current.ranger.roles[0U].word(field.word));
                }
            }
            OL_CHECK(section(parsed, "header").get("ship_direction")->as_integer()->get() == 3);
            OL_CHECK(!section(parsed, "header").contains("encode"));
            OL_CHECK(encoded.document->find("[[roles]]") != std::string::npos);
            OL_CHECK(encoded.document->find("s_compressed_base64 = '''") != std::string::npos ||
                encoded.document->find("s_compressed_base64 = \"\"\"") != std::string::npos);
            const auto decoded = decode_toml_snapshot(*encoded.document, kind, metadata.slot, context);
            OL_CHECK(decoded);
            if (decoded && decoded_baseline.has_value()) {
                auto expected = current;
                expected.origin = model::SnapshotOrigin::new_game_plus;
                expected.ranger.roles[2U] = decoded_baseline->roles[2U];
                OL_CHECK(decoded.save->metadata == metadata);
                OL_CHECK(decoded.save->snapshot == expected);
                OL_CHECK(decoded.save->snapshot.ranger.header.inventory_item(2U).value == -1);
                OL_CHECK(decoded.save->snapshot.ranger.header.inventory_count(2U) == 0);
                OL_CHECK(decoded.save->snapshot.ranger.roles[1U].ever_joined);
                OL_CHECK(!decoded.save->snapshot.ranger.roles[3U].ever_joined);
            }
            const auto wrong_kind = kind == TomlSaveKind::ordinary ? TomlSaveKind::completion : TomlSaveKind::ordinary;
            OL_CHECK(!decode_toml_snapshot(*encoded.document, wrong_kind, metadata.slot, context));
            OL_CHECK(!decode_toml_snapshot(*encoded.document, kind, static_cast<SaveSlot>(500U), context));
        }
    }
}

void check_collection_bounds(
    const model::RuntimeGameSnapshot& source, const TomlSnapshotContext& context) {
    const auto metadata = TomlSaveMetadata{TomlSaveKind::ordinary, SaveSlot::one, "2024-02-29T00:00:00Z"};
    auto full = source;
    for (std::size_t index = 0U; index < full.ranger.roles.size(); ++index) {
        full.ranger.roles[index] = source.ranger.roles[0U];
        full.ranger.roles[index].id = model::CharacterId{static_cast<std::int16_t>(index)};
        full.ranger.roles[index].head_id = context.baseline.roles[index].word(model::role_word::head_id);
        full.ranger.roles[index].ever_joined = true;
    }
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        full.ranger.header.set_inventory(slot, model::ItemId{static_cast<std::int16_t>(slot)}, kWide + static_cast<std::int64_t>(slot));
    }
    const auto encoded_full = encode_toml_snapshot(full, metadata, context);
    OL_CHECK(encoded_full);
    if (encoded_full) {
        const auto decoded = decode_toml_snapshot(*encoded_full.document, metadata.kind, metadata.slot, context);
        OL_CHECK(decoded);
        full.origin = model::SnapshotOrigin::new_game_plus;
        if (decoded) {
            OL_CHECK(decoded.save->snapshot == full);
        }
    }
    auto empty = source;
    for (auto& actor : empty.ranger.roles) {
        actor.ever_joined = false;
    }
    for (std::size_t slot = 0U; slot < model::kTeamMemberCount; ++slot) {
        empty.ranger.header.set_team_member(slot, model::CharacterId{-1});
    }
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        empty.ranger.header.set_inventory(slot, model::ItemId{-1}, 0);
    }
    const auto encoded_empty = encode_toml_snapshot(empty, metadata, context);
    OL_CHECK(encoded_empty);
    if (encoded_empty) {
        auto document = toml::parse(*encoded_empty.document);
        OL_CHECK(roles(document).empty());
        OL_CHECK(section(document, "header").get("inventory")->as_array()->empty());
        const auto decoded = decode_toml_snapshot(*encoded_empty.document, metadata.kind, metadata.slot, context);
        const auto baseline = model::decode_legacy_ranger(context.baseline);
        OL_CHECK(decoded && baseline.has_value());
        if (decoded && baseline.has_value()) {
            OL_CHECK(decoded.save->snapshot.ranger.roles == baseline->roles);
        }
    }
}

void check_required_fields(const toml::table& source, const TomlSnapshotContext& context) {
    for (const auto& [key, value] : source) {
        static_cast<void>(value);
        const std::string name{key.str()};
        rejects(source, context, [&](auto& document) { document.erase(name); });
        rejects(source, context, [&](auto& document) { document.insert_or_assign(name, false); });
    }
    for (const auto name : {"assets", "header", "item_state", "scene_state", "shop_state", "scene_archives"}) {
        const auto& table = *source.get(name)->as_table();
        for (const auto& [key, value] : table) {
            static_cast<void>(value);
            const std::string field{key.str()};
            rejects(source, context, [&](auto& document) { section(document, name).erase(field); });
            rejects(source, context, [&](auto& document) { section(document, name).insert_or_assign(field, false); });
        }
        rejects(source, context, [&](auto& document) { section(document, name).insert("unknown", 0); });
    }
    const auto& first_role = *source.get("roles")->as_array()->get(0U)->as_table();
    for (const auto& [key, value] : first_role) {
        const std::string field{key.str()};
        rejects(source, context, [&](auto& document) { role(document).erase(field); });
        rejects(source, context, [&](auto& document) {
            if (value.is_string()) {
                role(document).insert_or_assign(field, false);
            } else {
                role(document).insert_or_assign(field, "wrong type");
            }
        });
    }
    for (const auto field : {"item_id", "count"}) {
        rejects(source, context, [&](auto& document) {
            section(document, "header").get("inventory")->as_array()->get(0U)->as_table()->erase(field);
        });
    }
    rejects(source, context, [](auto& document) {
        section(document, "header").get("inventory")->as_array()->get(0U)->as_table()->insert("slot", 0);
    });
    rejects(source, context, [](auto& document) { role(document).insert("head_id", 0); });
    rejects(source, context, [](auto& document) { document.insert("items", toml::array{}); });
    rejects(source, context, [](auto& document) {
        section(document, "header").erase("ship_direction");
        section(document, "header").insert("encode", 3);
    });
}

void check_arrays_and_roles(const toml::table& source, const TomlSnapshotContext& context) {
    for (const auto name : {"equipment", "frames", "magic_ids", "magic_levels", "taking_items", "taking_counts", "no_magic_count"}) {
        rejects(source, context, [&](auto& document) { role(document).get(name)->as_array()->pop_back(); });
        rejects(source, context, [&](auto& document) { role(document).get(name)->as_array()->push_back(0); });
        rejects(source, context, [&](auto& document) { replace_array_value(*role(document).get(name)->as_array(), 0U, "wrong type"); });
    }
    for (const auto& [table, field] : std::array{
            std::pair{"header", "team_members"}, std::pair{"item_state", "users"},
            std::pair{"scene_state", "entrance_conditions"}, std::pair{"shop_state", "totals"}}) {
        rejects(source, context, [&](auto& document) { section(document, table).get(field)->as_array()->pop_back(); });
        rejects(source, context, [&](auto& document) { section(document, table).get(field)->as_array()->push_back(0); });
    }
    rejects(source, context, [](auto& document) {
        section(document, "shop_state").get("totals")->as_array()->get(0U)->as_array()->pop_back();
    });
    rejects(source, context, [](auto& document) {
        auto& inventory = *section(document, "header").get("inventory")->as_array();
        const auto entry = *inventory.get(0U)->as_table();
        while (inventory.size() <= model::kInventoryCount) {
            inventory.push_back(entry);
        }
    });
    rejects(source, context, [](auto& document) { roles(document).erase(roles(document).begin()); });
    rejects(source, context, [](auto& document) { roles(document).push_back(role(document)); });
    rejects(source, context, [](auto& document) { role(document, 1U).insert_or_assign("id", 0); });
    rejects(source, context, [](auto& document) { role(document).insert_or_assign("id", -1); });
    rejects(source, context, [](auto& document) { role(document, 2U).insert_or_assign("id", 320); });
    rejects(source, context, [](auto& document) { role(document, 1U).insert_or_assign("ever_joined", false); });
    rejects(source, context, [](auto& document) {
        auto first = role(document, 0U);
        auto second = role(document, 1U);
        replace_array_value(roles(document), 0U, std::move(second));
        replace_array_value(roles(document), 1U, std::move(first));
    });
}

void check_domains(const toml::table& source, const TomlSnapshotContext& context) {
    for (const auto& [key, value] : std::to_array<std::pair<std::string_view, std::int64_t>>({
            {"increased_life", 0},
            {"increased_life", 11}, {"sex", 32768}, {"level", 0}, {"experience", -1},
            {"hp", kMaximum}, {"maximum_hp", -1}, {"hurt", 100}, {"poison", 100},
            {"physical_power", 101}, {"make_item_experience", -1}, {"mp_type", 3},
            {"mp", kMaximum}, {"maximum_mp", -1}, {"attack", -1}, {"speed", 101},
            {"defence", -1}, {"medicine", -1}, {"use_poison", -1}, {"detoxification", -1},
            {"anti_poison", -1}, {"fist", -1}, {"sword", -1}, {"knife", -1}, {"unusual", -1},
            {"hidden_weapon", -1}, {"knowledge", -1}, {"morality", -1},
            {"attack_with_poison", -1}, {"attack_twice", 2}, {"fame", -1}, {"iq", 101},
            {"practice_item", 200}, {"item_experience", -1}})) {
        rejects(source, context, [&](auto& document) { role(document).insert_or_assign(key, value); });
    }
    for (const auto& [field, value] : std::to_array<std::pair<std::string_view, std::int64_t>>({
            {"equipment", 200}, {"frames", 32768}, {"magic_ids", 93}, {"magic_levels", -1},
            {"taking_items", 200}, {"taking_counts", -1}, {"no_magic_count", -1}})) {
        rejects(source, context, [&](auto& document) {
            replace_array_value(*role(document).get(field)->as_array(), 0U, value);
        });
    }
    const auto nonmanual = std::ranges::find_if(context.baseline.items, [](const auto& item) {
        return item.word(model::item_word::item_type) != 2 ||
            item.word(model::item_word::magic_id) != -1 || item.word(model::item_word::need_experience) <= 0;
    });
    OL_CHECK(nonmanual != context.baseline.items.end());
    const auto nonmanual_index = static_cast<std::size_t>(nonmanual - context.baseline.items.begin());
    rejects(source, context, [&](auto& document) {
        replace_array_value(*role(document).get("no_magic_count")->as_array(), nonmanual_index, 1);
    });
    rejects(source, context, [](auto& document) { replace_array_value(*role(document).get("magic_ids")->as_array(), 0U, 0); });
    rejects(source, context, [](auto& document) { replace_array_value(*role(document).get("taking_items")->as_array(), 0U, -1); });
    rejects(source, context, [](auto& document) {
        replace_array_value(*section(document, "item_state").get("users")->as_array(), 0U, 320);
    });
    for (const auto& name : {std::string{"继承"}, std::string{"六字角色姓名"}, std::string{"A\0B", 3U}}) {
        rejects(source, context, [&](auto& document) { role(document).insert_or_assign("name", name); });
        rejects(source, context, [&](auto& document) { role(document).insert_or_assign("nickname", name); });
    }
    rejects(source, context, [](auto& document) {
        replace_array_value(*section(document, "header").get("team_members")->as_array(), 1U, 0);
    });
    for (const auto value : {-1LL, 0LL, 201LL}) {
        rejects(source, context, [&](auto& document) {
            auto& entry = *section(document, "header").get("inventory")->as_array()->get(0U)->as_table();
            entry.insert_or_assign(value == 201 ? "item_id" : "count", value);
        });
    }
    rejects(source, context, [](auto& document) {
        auto& inventory = *section(document, "header").get("inventory")->as_array();
        auto& entry = *inventory.get(0U)->as_table();
        entry.insert_or_assign("item_id", -1);
        entry.insert_or_assign("count", 0);
    });
}

void check_metadata_and_archives(const toml::table& source, const TomlSnapshotContext& context) {
    for (const auto field : {"format_version", "ruleset_version", "slot", "playthrough"}) {
        for (const auto value : {0LL, -1LL, 1000LL}) {
            rejects(source, context, [&](auto& document) { document.insert_or_assign(field, value); });
        }
    }
    for (const auto timestamp : {"2023-02-29T00:00:00Z", "2026-13-01T00:00:00Z", "2026-09-14T24:00:00Z",
            "2026-09-14T08:60:00Z", "2026-09-14T08:00:60Z", "2026-09-14T23:59:60Z",
            "2026-09-14T08:00:00+00:00", "2026-09-14t08:00:00z", "2026-09-14T08:00:00.0Z"}) {
        rejects(source, context, [&](auto& document) { document.insert_or_assign("timestamp_utc", timestamp); });
    }
    for (const auto& asset : kNewGamePlusSaveAssets) {
        rejects(source, context, [&](auto& document) {
            auto value = section(document, "assets").get(asset.key)->as_string()->get();
            value[0U] = value[0U] == '0' ? '1' : '0';
            section(document, "assets").insert_or_assign(asset.key, value);
        });
    }
    for (const auto key : {"s_compressed_base64", "d_compressed_base64"}) {
        rejects(source, context, [&](auto& document) { section(document, "scene_archives").insert_or_assign(key, ""); });
        rejects(source, context, [&](auto& document) {
            auto value = section(document, "scene_archives").get(key)->as_string()->get();
            value += ' ';
            section(document, "scene_archives").insert_or_assign(key, value);
        });
        rejects(source, context, [&](auto& document) {
            auto value = section(document, "scene_archives").get(key)->as_string()->get();
            value[8U] = value[8U] == 'A' ? 'B' : 'A';
            section(document, "scene_archives").insert_or_assign(key, value);
        });
    }
    auto text = format(source);
    const auto duplicate = decode_toml_snapshot("slot = 1\n" + text, TomlSaveKind::ordinary, SaveSlot::one, context);
    OL_CHECK(!duplicate && duplicate.status == TomlSaveStatus::invalid_document);
    const auto playthrough_position = text.find("playthrough = 1");
    OL_CHECK(playthrough_position != std::string::npos);
    if (playthrough_position != std::string::npos) {
        auto overflowing = text;
        overflowing.replace(playthrough_position, std::string_view{"playthrough = 1"}.size(),
            "playthrough = 9223372036854775808");
        OL_CHECK(!decode_toml_snapshot(overflowing, TomlSaveKind::ordinary, SaveSlot::one, context));
    }
    OL_CHECK(!decode_toml_snapshot(std::string(kMaximumTomlSaveBytes + 1U, ' '),
        TomlSaveKind::ordinary, SaveSlot::one, context));
    OL_CHECK(!decode_toml_snapshot(text, static_cast<TomlSaveKind>(9), SaveSlot::one, context));
    OL_CHECK(!decode_toml_snapshot(text, TomlSaveKind::ordinary, static_cast<SaveSlot>(999U), context));
}

void check_write_rejections(const model::RuntimeGameSnapshot& source, const TomlSnapshotContext& context) {
    const auto metadata = TomlSaveMetadata{TomlSaveKind::ordinary, SaveSlot::one, "2026-09-14T08:00:00Z"};
    for (const auto mutation : {0, 1, 2, 3, 4, 5, 6}) {
        auto current = source;
        switch (mutation) {
        case 0: current.ranger.roles[0U].hp = kMaximum; break;
        case 1: current.ranger.roles[0U].speed = 101; break;
        case 2: current.ranger.header.set_inventory(0U, model::ItemId{-1}, 0); break;
        case 3: current.ranger.roles[0U].head_id = -1; break;
        case 4: current.ranger.magics[1U].set_word(model::magic_word::attack_begin, -1); break;
        case 5: current.scene_events.pop_back(); break;
        case 6: current.ranger.roles[0U].name = std::u8string{u8"A\0B", 3U}; break;
        }
        const auto before = current;
        const auto result = encode_toml_snapshot(current, metadata, context);
        OL_CHECK(!result && !result.document.has_value() && !result.detail.empty());
        OL_CHECK(current == before);
    }
    auto disabled = context.configuration;
    disabled.enabled = false;
    OL_CHECK(!encode_toml_snapshot(source, metadata, {context.baseline, context.fingerprints, disabled}));
    auto bad_time = metadata;
    bad_time.timestamp_utc = "2026-02-30T00:00:00Z";
    OL_CHECK(!encode_toml_snapshot(source, bad_time, context));
}

}

int main() {
    const auto baseline = load_baseline(test::game_data_root());
    const auto fingerprints = fingerprint_new_game_plus_assets(test::game_data_root());
    OL_CHECK(baseline && fingerprints);
    if (!baseline || !fingerprints) {
        return 1;
    }
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    auto snapshot = model::decode_legacy_snapshot(*baseline.snapshot, configuration, &baseline.snapshot->ranger);
    OL_CHECK(snapshot.has_value());
    if (!snapshot.has_value()) {
        return 1;
    }
    const auto context = TomlSnapshotContext{baseline.snapshot->ranger, *fingerprints.fingerprints, configuration};
    const auto baseline_before = context.baseline;
    const auto fingerprints_before = context.fingerprints;
    const auto configuration_before = context.configuration;
    set_wide_state(*snapshot, context.baseline);
    check_round_trips(*snapshot, context);
    std::ranges::fill(snapshot->scene_maps, 0U);
    std::ranges::fill(snapshot->scene_events, 0U);
    const auto encoded = encode_toml_snapshot(*snapshot,
        {TomlSaveKind::ordinary, SaveSlot::one, "2026-09-14T08:00:00Z"}, context);
    OL_CHECK(encoded);
    if (!encoded) {
        std::cerr << encoded.detail << '\n';
        return 1;
    }
    const auto document = toml::parse(*encoded.document);
    check_collection_bounds(*snapshot, context);
    check_required_fields(document, context);
    check_arrays_and_roles(document, context);
    check_domains(document, context);
    check_metadata_and_archives(document, context);
    check_write_rejections(*snapshot, context);
    OL_CHECK(context.baseline == baseline_before && context.fingerprints == fingerprints_before);
    OL_CHECK(context.configuration == configuration_before);
    std::cout << "Malformed TOML rejection checks: " << rejection_checks << '\n';
    return test::failures == 0 ? 0 : 1;
}
