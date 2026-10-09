#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/toml_slot.hpp"
#include "openlegend/persistence/ordinary_slot.hpp"
#include "test_support.hpp"

namespace {

using namespace openlegend;
using namespace openlegend::persistence;

void prepare_snapshot(model::RuntimeGameSnapshot& snapshot, const model::RangerState& baseline) {
    model::RoleState actor;
    actor.id = model::CharacterId{0};
    actor.head_id = baseline.roles[0U].word(model::role_word::head_id);
    actor.increased_life = 6;
    actor.level = 1;
    actor.name = u8"繼承";
    actor.hp = 5'000'000'000'000;
    actor.maximum_hp = actor.hp + 1;
    actor.mp = 9'000'000'000'000;
    actor.maximum_mp = actor.mp + 1;
    actor.physical_power = 100;
    actor.attack = std::numeric_limits<std::int64_t>::max();
    actor.equipment.fill(model::ItemId{-1});
    actor.taking_items.fill(model::ItemId{-1});
    actor.magic_ids.fill(model::MagicId{0});
    actor.magic_ids[0U] = model::MagicId{1};
    actor.magic_levels[0U] = 5'000'000'000'000;
    actor.practice_item = model::ItemId{-1};
    actor.ever_joined = true;
    bool found_manual{};
    for (std::size_t item = 0U; item < baseline.items.size(); ++item) {
        const auto& definition = baseline.items[item];
        if (definition.word(model::item_word::item_type) == 2 &&
            definition.word(model::item_word::magic_id) == -1 &&
            definition.word(model::item_word::need_experience) > 0) {
            actor.no_magic_count[item] = 5'000'000'000'000;
            found_manual = true;
            break;
        }
    }
    OL_CHECK(found_manual);
    snapshot.ranger.roles[0U] = actor;
    actor.id = model::CharacterId{1};
    actor.head_id = baseline.roles[1U].word(model::role_word::head_id);
    snapshot.ranger.roles[1U] = actor;
    for (std::size_t slot = 0U; slot < model::kTeamMemberCount; ++slot) {
        snapshot.ranger.header.set_team_member(slot, model::CharacterId{-1});
    }
    snapshot.ranger.header.set_team_member(0U, model::CharacterId{0});
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        snapshot.ranger.header.set_inventory(slot, model::ItemId{-1}, 0);
    }
    snapshot.ranger.header.set_inventory(0U, model::ItemId{174}, 5'000'000'000'000);
    snapshot.ranger.header.set_inventory(1U, model::ItemId{174}, 3);
    snapshot.ranger.items[1U].set_word(model::item_word::user, 319);
    std::ranges::fill(snapshot.scene_maps, 0U);
    std::ranges::fill(snapshot.scene_events, 0U);
    OL_CHECK(snapshot.set_scene_value(99U, model::SceneLayer::building, 4095U, 234));
    OL_CHECK(snapshot.set_event_value(99U, 199U, model::SceneEventField::event_1, 345));
    snapshot.origin = model::SnapshotOrigin::new_game_plus;
    OL_CHECK(snapshot.valid_for_persistence());
}

void check_paths(const std::filesystem::path& root) {
    for (unsigned int index = 0U; index < kNumberedSaveSlotCount; ++index) {
        auto name = std::to_string(index + 1U);
        name.insert(0U, 3U - name.size(), '0');
        name += ".toml";
        for (const auto kind : {TomlSaveKind::ordinary, TomlSaveKind::completion}) {
            const auto path = toml_slot_path(root, kind, static_cast<SaveSlot>(index));
            OL_CHECK(path == root / (kind == TomlSaveKind::ordinary ? "ngplus" : "completed") / name);
        }
    }
    OL_CHECK(!toml_slot_path(root, TomlSaveKind::ordinary, static_cast<SaveSlot>(999U)));
    OL_CHECK(!toml_slot_path(root, static_cast<TomlSaveKind>(9), SaveSlot::one));
}

void check_round_trips(const std::filesystem::path& root,
    const model::RuntimeGameSnapshot& source, const TomlSnapshotContext& context) {
    for (const auto playthrough : {1LL, 2LL, 999LL}) {
        for (const auto kind : {TomlSaveKind::ordinary, TomlSaveKind::completion}) {
            auto snapshot = source;
            snapshot.playthrough = playthrough;
            const auto slot = static_cast<SaveSlot>(playthrough - 1);
            const auto metadata = TomlSaveMetadata{kind, slot, "2026-09-14T08:00:00Z"};
            const auto before = snapshot;
            const auto written = write_toml_slot(root, snapshot, metadata, context);
            OL_CHECK(written);
            if (!written) {
                std::cerr << written.detail << '\n';
                continue;
            }
            const auto loaded = load_toml_slot(root, kind, slot, context);
            OL_CHECK(loaded);
            if (loaded) {
                OL_CHECK(loaded.save->snapshot == snapshot);
                OL_CHECK(loaded.save->metadata == metadata);
            }
            snapshot.ranger.roles[0U].hp -= 7;
            OL_CHECK(write_toml_slot(root, snapshot, metadata, context));
            const auto replaced = load_toml_slot(root, kind, slot, context);
            OL_CHECK(replaced);
            if (replaced) {
                OL_CHECK(replaced.save->snapshot == snapshot);
            }
            auto temporary = written.path;
            temporary += ".tmp";
            OL_CHECK(!std::filesystem::exists(temporary));
            temporary = written.path;
            temporary += ".rollback.tmp";
            OL_CHECK(!std::filesystem::exists(temporary));
            snapshot.ranger.roles[0U].hp += 7;
            OL_CHECK(snapshot == before);
        }
    }
}

void check_rejections(const std::filesystem::path& root,
    const model::RuntimeGameSnapshot& source, const TomlSnapshotContext& context) {
    const auto metadata = TomlSaveMetadata{TomlSaveKind::ordinary, SaveSlot::one, "2026-09-14T08:00:00Z"};
    const auto path = *toml_slot_path(root, metadata.kind, metadata.slot);
    const auto before = read_save_file(path, kMaximumTomlSaveBytes);
    OL_CHECK(before);
    auto invalid = source;
    invalid.ranger.roles[0U].hp = invalid.ranger.roles[0U].maximum_hp + 1;
    const auto invalid_before = invalid;
    const auto rejected = write_toml_slot(root, invalid, metadata, context);
    OL_CHECK(!rejected && rejected.snapshot_status == TomlSaveStatus::invalid_snapshot);
    OL_CHECK(invalid == invalid_before);
    OL_CHECK(read_save_file(path, kMaximumTomlSaveBytes).bytes == before.bytes);
    OL_CHECK(!write_toml_slot(root / "invalid-new-directory", invalid, metadata, context));
    OL_CHECK(!std::filesystem::exists(root / "invalid-new-directory"));
    const auto missing = load_toml_slot(root, TomlSaveKind::ordinary, static_cast<SaveSlot>(499U), context);
    OL_CHECK(!missing && !missing.save && missing.file_status == SaveFileStatus::not_found);

    const auto wrong_slot = *toml_slot_path(root, TomlSaveKind::ordinary, SaveSlot::three);
    std::filesystem::copy_file(path, wrong_slot);
    const auto wrong = load_toml_slot(root, TomlSaveKind::ordinary, SaveSlot::three, context);
    OL_CHECK(!wrong && !wrong.save && wrong.snapshot_status == TomlSaveStatus::invalid_metadata);
    const auto wrong_kind = root / "wrong-kind" / "completed" / "001.toml";
    std::filesystem::create_directories(wrong_kind.parent_path());
    std::filesystem::copy_file(path, wrong_kind);
    OL_CHECK(!load_toml_slot(root / "wrong-kind", TomlSaveKind::completion, SaveSlot::one, context));

    auto fingerprints = context.fingerprints;
    fingerprints.sha256[0U][0U] = fingerprints.sha256[0U][0U] == '0' ? '1' : '0';
    const auto incompatible = load_toml_slot(root, metadata.kind, metadata.slot,
        {context.baseline, fingerprints, context.configuration});
    OL_CHECK(!incompatible && !incompatible.save && incompatible.snapshot_status == TomlSaveStatus::incompatible_assets);

    auto disabled = context.configuration;
    disabled.enabled = false;
    const auto disabled_context = TomlSnapshotContext{context.baseline, context.fingerprints, disabled};
    OL_CHECK(!load_toml_slot(root, metadata.kind, metadata.slot, disabled_context));
    OL_CHECK(!write_toml_slot(root, source, metadata, disabled_context));
    OL_CHECK(read_save_file(path, kMaximumTomlSaveBytes).bytes == before.bytes);
    OL_CHECK(!write_toml_slot(root / "disabled", source, metadata, disabled_context));
    OL_CHECK(!std::filesystem::exists(root / "disabled"));
    OL_CHECK(!load_toml_slot(root, static_cast<TomlSaveKind>(9), SaveSlot::one, context));
    OL_CHECK(!load_toml_slot(root, metadata.kind, static_cast<SaveSlot>(999U), context));

    const auto temporary = std::filesystem::path{path}.concat(".tmp");
    const std::vector<std::uint8_t> collision{123U};
    OL_CHECK(replace_save_file(temporary, collision, kMaximumTomlSaveBytes));
    const auto blocked = write_toml_slot(root, source, metadata, context);
    OL_CHECK(!blocked && blocked.file_status != SaveFileStatus::ready);
    OL_CHECK(read_save_file(path, kMaximumTomlSaveBytes).bytes == before.bytes);
    OL_CHECK(read_save_file(temporary, kMaximumTomlSaveBytes).bytes == collision);
    std::filesystem::remove(temporary);

    const std::vector<std::uint8_t> malformed{'f', 'o', 'r', 'm', 'a', 't', '=', '['};
    OL_CHECK(replace_save_file(path, malformed, kMaximumTomlSaveBytes));
    const auto bad = load_toml_slot(root, metadata.kind, metadata.slot, context);
    OL_CHECK(!bad && !bad.save && bad.file_status == SaveFileStatus::ready &&
        bad.snapshot_status == TomlSaveStatus::invalid_document);
    OL_CHECK(read_save_file(path, kMaximumTomlSaveBytes).bytes == malformed);
    std::filesystem::resize_file(path, kMaximumTomlSaveBytes + 1U);
    const auto oversized = load_toml_slot(root, metadata.kind, metadata.slot, context);
    OL_CHECK(!oversized && !oversized.save && oversized.file_status == SaveFileStatus::too_large);
    std::filesystem::remove(path);
    std::filesystem::create_directory(path);
    const auto directory = load_toml_slot(root, metadata.kind, metadata.slot, context);
    OL_CHECK(!directory && !directory.save && directory.file_status == SaveFileStatus::not_regular);
}

void check_ordinary_routing(const std::filesystem::path& root,
    const model::RuntimeGameSnapshot& source, const TomlSnapshotContext& context) {
    std::filesystem::create_directories(root);
    const auto index = read_save_file(test::game_data_root() / "RANGER.IDX", 24U);
    OL_CHECK(index);
    if (!index) {
        return;
    }
    const model::NewGamePlusConfiguration disabled;
    const OrdinarySlotContext legacy_context{context.baseline, *index.bytes, disabled, nullptr};
    const OrdinarySlotContext enabled_context{
        context.baseline, *index.bytes, context.configuration, &context.fingerprints};
    auto legacy = source;
    legacy.configuration = disabled;
    legacy.origin = model::SnapshotOrigin::legacy;
    for (std::size_t role = 0U; role < 2U; ++role) {
        auto& actor = legacy.ranger.roles[role];
        actor.hp = 999;
        actor.maximum_hp = 999;
        actor.mp = 999;
        actor.maximum_mp = 999;
        actor.attack = 100;
        actor.magic_levels[0U] = 998;
        actor.ever_joined = false;
        std::ranges::fill(actor.no_magic_count, 0);
    }
    legacy.ranger.header.set_inventory(0U, model::ItemId{174}, 500);
    OL_CHECK(legacy.valid_for_persistence());
    OL_CHECK(write_ordinary_slot(root, SaveSlot::one, legacy, legacy_context, {}));
    OL_CHECK(!std::filesystem::exists(root / "ngplus"));
    const auto legacy_loaded = load_ordinary_slot(root, SaveSlot::one, legacy_context);
    OL_CHECK(legacy_loaded && legacy_loaded.snapshot == legacy && !legacy_loaded.metadata);
    const auto fallback = load_ordinary_slot(root, SaveSlot::one, enabled_context);
    OL_CHECK(fallback && fallback.format == OrdinarySaveFormat::legacy);
    if (fallback) {
        OL_CHECK(write_ordinary_slot(root, SaveSlot::one, *fallback.snapshot,
            enabled_context, "2026-09-14T08:00:00Z"));
    }
    const auto toml_path = *toml_slot_path(root, TomlSaveKind::ordinary, SaveSlot::one);
    OL_CHECK(std::filesystem::exists(toml_path));
    auto wide = source;
    wide.ranger.roles[0U].level = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(write_ordinary_slot(root, SaveSlot::one, wide, enabled_context, "2026-09-14T08:00:00Z"));
    const auto selected = select_ordinary_slot(root, SaveSlot::one, true);
    OL_CHECK(selected && selected.format == OrdinarySaveFormat::toml && selected.path == toml_path);
    const auto loaded = load_ordinary_slot(root, SaveSlot::one, enabled_context);
    OL_CHECK(loaded && loaded.snapshot == wide && loaded.metadata);
    OL_CHECK(load_ordinary_slot(root, SaveSlot::one, legacy_context).snapshot == legacy);
    const TomlSaveMetadata completion{TomlSaveKind::completion, SaveSlot::one, "2026-09-14T08:00:00Z"};
    OL_CHECK(write_toml_slot(root, wide, completion, context));
    const auto completion_path = *toml_slot_path(root, TomlSaveKind::completion, SaveSlot::one);
    auto wrong_fingerprints = context.fingerprints;
    wrong_fingerprints.sha256[0U][0U] = wrong_fingerprints.sha256[0U][0U] == '0' ? '1' : '0';
    const OrdinarySlotContext incompatible{context.baseline, *index.bytes, context.configuration, &wrong_fingerprints};
    OL_CHECK(load_ordinary_slot(root, SaveSlot::one, incompatible).status == PersistenceStatus::incompatible_assets);
    const std::vector<std::uint8_t> malformed{'b', 'a', 'd', '=', '['};
    OL_CHECK(replace_save_file(toml_path, malformed, kMaximumTomlSaveBytes));
    const auto damaged = load_ordinary_slot(root, SaveSlot::one, enabled_context);
    OL_CHECK(!damaged && !damaged.snapshot && damaged.status == PersistenceStatus::invalid_toml);
    OL_CHECK(write_ordinary_slot(root, SaveSlot::one, legacy, legacy_context, {}));
    OL_CHECK(read_save_file(toml_path, kMaximumTomlSaveBytes).bytes == malformed);
    OL_CHECK(delete_ordinary_slot(root, SaveSlot::one, true));
    OL_CHECK(load_ordinary_slot(root, SaveSlot::one, enabled_context).format == OrdinarySaveFormat::legacy);
    OL_CHECK(load_ordinary_slot(root, SaveSlot::one, legacy_context).snapshot == legacy);
    OL_CHECK(std::filesystem::exists(completion_path));
    OL_CHECK(write_ordinary_slot(root, SaveSlot::one, wide, enabled_context, "2026-09-14T08:00:00Z"));
    OL_CHECK(delete_ordinary_slot(root, SaveSlot::one, false));
    OL_CHECK(std::filesystem::exists(toml_path));
    OL_CHECK(!load_ordinary_slot(root, SaveSlot::one, legacy_context));
    OL_CHECK(load_ordinary_slot(root, SaveSlot::one, enabled_context).snapshot == wide);
    OL_CHECK(delete_ordinary_slot(root, SaveSlot::one, true));
    OL_CHECK(!load_ordinary_slot(root, SaveSlot::one, enabled_context));
    OL_CHECK(std::filesystem::exists(completion_path));
    OL_CHECK(!select_ordinary_slot(root, static_cast<SaveSlot>(999U), true));
}

}

int main() {
    try {
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
        prepare_snapshot(*snapshot, baseline.snapshot->ranger);
        const auto original = *snapshot;
        const auto context = TomlSnapshotContext{baseline.snapshot->ranger, *fingerprints.fingerprints, configuration};
        const auto root = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "toml-slots";
        std::filesystem::remove_all(root);
        check_paths(root);
        check_round_trips(root, *snapshot, context);
        check_rejections(root, *snapshot, context);
        check_ordinary_routing(root / "routing", *snapshot, context);
        OL_CHECK(*snapshot == original);
        std::filesystem::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return test::failures == 0 ? 0 : 1;
}
