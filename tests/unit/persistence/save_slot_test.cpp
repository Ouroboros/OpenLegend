#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "test_support.hpp"

namespace {

using openlegend::model::GameSnapshot;
using openlegend::persistence::SaveFileSet;
using openlegend::persistence::SaveFileSetKind;
using openlegend::persistence::SaveSlot;

static_assert(openlegend::model::RangerHeader::word_count == 418U);
static_assert(openlegend::model::RoleRecord::word_count == 91U);
static_assert(openlegend::model::ItemRecord::word_count == 95U);
static_assert(openlegend::model::SceneMetadataRecord::word_count == 26U);
static_assert(openlegend::model::MagicRecord::word_count == 68U);
static_assert(openlegend::model::ShopRecord::word_count == 15U);
static_assert(sizeof(openlegend::model::GameSnapshot) < 4'096U);

[[nodiscard]] std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    const auto file = openlegend::resource::read_binary_file(path);
    OL_CHECK(static_cast<bool>(file));
    return file.bytes;
}

void write_bytes(const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    OL_CHECK(static_cast<bool>(output));
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    OL_CHECK(static_cast<bool>(output));
}

[[nodiscard]] std::array<std::filesystem::path, 6> paths(const SaveFileSet& files) {
    return {
        files.ranger_index,
        files.ranger_group,
        files.scene_map_index,
        files.scene_map_group,
        files.scene_event_index,
        files.scene_event_group};
}

[[nodiscard]] SaveFileSet corresponding_file_set(
    const SaveFileSet& source, const std::filesystem::path& root) {
    switch (source.kind) {
    case SaveFileSetKind::baseline:
        return openlegend::persistence::baseline_file_set(root);
    case SaveFileSetKind::working_copy:
        return openlegend::persistence::working_copy_file_set(root);
    case SaveFileSetKind::numbered_slot:
        return *openlegend::persistence::numbered_file_set(root, *source.slot);
    }
    return openlegend::persistence::baseline_file_set(root);
}

void compare_file_sets(const SaveFileSet& left, const SaveFileSet& right) {
    const auto left_paths = paths(left);
    const auto right_paths = paths(right);
    for (std::size_t index = 0U; index < left_paths.size(); ++index) {
        OL_CHECK(read_bytes(left_paths[index]) == read_bytes(right_paths[index]));
    }
}

void round_trip_case(
    const std::filesystem::path& output_root,
    const std::string& label,
    const SaveFileSet& source_files) {
    const auto loaded = openlegend::persistence::load_snapshot(source_files);
    OL_CHECK(static_cast<bool>(loaded));
    OL_CHECK(loaded.snapshot->valid());

    const auto case_root = output_root / label;
    std::filesystem::remove_all(case_root);
    OL_CHECK(std::filesystem::create_directories(case_root));
    const auto output_files = corresponding_file_set(source_files, case_root);
    const auto write_result = openlegend::persistence::write_snapshot(output_files, *loaded.snapshot);
    OL_CHECK(static_cast<bool>(write_result));
    compare_file_sets(source_files, output_files);

    const auto reloaded = openlegend::persistence::load_snapshot(output_files);
    OL_CHECK(static_cast<bool>(reloaded));
    OL_CHECK(*loaded.snapshot == *reloaded.snapshot);
}

void check_baseline_golden(const GameSnapshot& snapshot) {
    using namespace openlegend::model;

    OL_CHECK(snapshot.ranger.header.word(header_word::in_ship) == 0);
    OL_CHECK(snapshot.ranger.header.word(header_word::in_sub_map) == 0);
    OL_CHECK(snapshot.ranger.header.word(header_word::main_map_x) == 357);
    OL_CHECK(snapshot.ranger.header.word(header_word::main_map_y) == 235);
    OL_CHECK(snapshot.ranger.header.word(header_word::face_towards) == 1);
    OL_CHECK(snapshot.ranger.header.word(header_word::ship_x) == 109);
    OL_CHECK(snapshot.ranger.header.word(header_word::ship_y) == 100);
    OL_CHECK(snapshot.ranger.header.team_member(0U).value == 0);
    OL_CHECK(snapshot.ranger.header.team_member(5U).value == -1);
    OL_CHECK(snapshot.ranger.header.inventory_item(0U).value == 0);
    OL_CHECK(snapshot.ranger.header.inventory_count(0U) == 3);
    OL_CHECK(snapshot.ranger.header.inventory_item(3U).value == 21);
    OL_CHECK(snapshot.ranger.header.inventory_count(3U) == 3);

    const auto& role = snapshot.ranger.roles.front();
    OL_CHECK(role.id().value == 0);
    OL_CHECK(role.word(role_word::level) == 1);
    OL_CHECK(role.word(role_word::hp) == 32);
    OL_CHECK(role.word(role_word::maximum_hp) == 32);
    OL_CHECK(role.word(role_word::attack) == 25);
    OL_CHECK(role.word(role_word::morality) == 50);
    OL_CHECK(role.word(role_word::magic_id_begin) == 1);
    OL_CHECK(role.word(role_word::taking_item_count_begin) == 3);
    OL_CHECK(snapshot.ranger.roles.back().id().value == 319);

    const auto& item = snapshot.ranger.items.front();
    OL_CHECK(item.id().value == 0);
    OL_CHECK(item.word(item_word::magic_id) == -1);
    OL_CHECK(item.word(item_word::item_type) == 3);
    OL_CHECK(item.word(item_word::make_item_begin) == -1);
    OL_CHECK(snapshot.ranger.items.back().id().value == 199);

    const auto& scene = snapshot.ranger.scenes.front();
    OL_CHECK(scene.id().value == 0);
    OL_CHECK(scene.word(scene_metadata_word::exit_music) == 0);
    OL_CHECK(scene.word(scene_metadata_word::entrance_music) == -1);
    OL_CHECK(scene.word(scene_metadata_word::main_entrance_x_1) == 85);
    OL_CHECK(scene.word(scene_metadata_word::entrance_x) == 26);
    OL_CHECK(snapshot.ranger.scenes.back().id().value == 83);

    const auto& magic = snapshot.ranger.magics.front();
    OL_CHECK(magic.id().value == 0);
    OL_CHECK(magic.word(magic_word::sound_id) == 1);
    OL_CHECK(magic.word(magic_word::magic_type) == 1);
    OL_CHECK(magic.word(magic_word::effect_id) == 12);
    OL_CHECK(magic.word(magic_word::select_distance_begin) == 1);
    OL_CHECK(snapshot.ranger.magics.back().id().value == 92);

    const auto& shop = snapshot.ranger.shops.front();
    OL_CHECK(shop.item(0U).value == 7);
    OL_CHECK(shop.word(shop_word::total_begin) == 1000);
    OL_CHECK(shop.word(shop_word::price_begin) == 80);
    OL_CHECK(snapshot.ranger.shops.back().item(0U).value == 19);

    OL_CHECK(snapshot.scene_value(0U, SceneLayer::earth, 0U) == 942);
    OL_CHECK(snapshot.scene_value(0U, SceneLayer::building, 0U) == 0);
    OL_CHECK(snapshot.scene_value(0U, SceneLayer::event_index, 0U) == -1);
    OL_CHECK(snapshot.scene_value(99U, SceneLayer::decoration_height, 4095U) == 0);
    OL_CHECK(snapshot.event_value(0U, 0U, SceneEventField::cannot_walk) == 1);
    OL_CHECK(snapshot.event_value(0U, 0U, SceneEventField::index) == 1);
    OL_CHECK(snapshot.event_value(0U, 0U, SceneEventField::current_picture) == 5166);
    OL_CHECK(snapshot.event_value(99U, 199U, SceneEventField::y) == 0);
}

[[nodiscard]] bool only_offsets_changed(
    const std::vector<std::uint8_t>& before,
    const std::vector<std::uint8_t>& after,
    const std::span<const std::size_t> allowed_offsets) {
    if (before.size() != after.size()) {
        return false;
    }
    bool changed = false;
    for (std::size_t index = 0U; index < before.size(); ++index) {
        if (before[index] == after[index]) {
            continue;
        }
        changed = true;
        if (std::find(allowed_offsets.begin(), allowed_offsets.end(), index) ==
            allowed_offsets.end()) {
            return false;
        }
    }
    return changed;
}

void check_mutation_isolation(
    const std::filesystem::path& output_root,
    const SaveFileSet& baseline_files,
    const GameSnapshot& baseline) {
    using namespace openlegend::model;

    auto mutated = baseline;
    mutated.ranger.header.set_word(header_word::main_map_x, -1234);
    mutated.ranger.roles[0].set_word(role_word::maximum_hp, 321);
    mutated.ranger.items[0].set_word(item_word::add_attack, -11);
    OL_CHECK(mutated.set_scene_value(99U, SceneLayer::building, 4095U, -222));
    OL_CHECK(mutated.set_event_value(99U, 199U, SceneEventField::y, 333));

    const auto root = output_root / "mutated";
    std::filesystem::remove_all(root);
    OL_CHECK(std::filesystem::create_directories(root));
    const auto output_files = openlegend::persistence::baseline_file_set(root);
    OL_CHECK(openlegend::persistence::write_snapshot(output_files, mutated));
    const auto loaded = openlegend::persistence::load_snapshot(output_files);
    OL_CHECK(static_cast<bool>(loaded));
    OL_CHECK(loaded.snapshot->ranger.header.word(header_word::main_map_x) == -1234);
    OL_CHECK(loaded.snapshot->ranger.roles[0].word(role_word::maximum_hp) == 321);
    OL_CHECK(loaded.snapshot->ranger.items[0].word(item_word::add_attack) == -11);
    OL_CHECK(loaded.snapshot->scene_value(99U, SceneLayer::building, 4095U) == -222);
    OL_CHECK(loaded.snapshot->event_value(99U, 199U, SceneEventField::y) == 333);

    constexpr std::array<std::size_t, 6> ranger_offsets{
        4U,
        5U,
        kRangerCumulativeEnds[0] + role_word::maximum_hp * 2U,
        kRangerCumulativeEnds[0] + role_word::maximum_hp * 2U + 1U,
        kRangerCumulativeEnds[1] + item_word::add_attack * 2U,
        kRangerCumulativeEnds[1] + item_word::add_attack * 2U + 1U};
    OL_CHECK(only_offsets_changed(
        read_bytes(baseline_files.ranger_group),
        read_bytes(output_files.ranger_group),
        ranger_offsets));

    const std::size_t scene_word =
        99U * kSceneLayerCount * kSceneTileCount +
        static_cast<std::size_t>(SceneLayer::building) * kSceneTileCount + 4095U;
    const std::array<std::size_t, 2> scene_offsets{scene_word * 2U, scene_word * 2U + 1U};
    OL_CHECK(only_offsets_changed(
        read_bytes(baseline_files.scene_map_group),
        read_bytes(output_files.scene_map_group),
        scene_offsets));

    const std::size_t event_word =
        99U * kSceneEventCount * kSceneEventWordCount + 199U * kSceneEventWordCount +
        static_cast<std::size_t>(SceneEventField::y);
    const std::array<std::size_t, 2> event_offsets{event_word * 2U, event_word * 2U + 1U};
    OL_CHECK(only_offsets_changed(
        read_bytes(baseline_files.scene_event_group),
        read_bytes(output_files.scene_event_group),
        event_offsets));

    OL_CHECK(read_bytes(baseline_files.ranger_index) == read_bytes(output_files.ranger_index));
    OL_CHECK(read_bytes(baseline_files.scene_map_index) == read_bytes(output_files.scene_map_index));
    OL_CHECK(read_bytes(baseline_files.scene_event_index) == read_bytes(output_files.scene_event_index));
}

void check_runtime_slot_contract(
    const std::filesystem::path& game_root, const std::filesystem::path& output_root) {
    constexpr std::array<SaveSlot, 3> slots{SaveSlot::one, SaveSlot::two, SaveSlot::three};
    for (const SaveSlot slot : slots) {
        const auto runtime = openlegend::persistence::load_numbered_slot(game_root, slot);
        const auto paired = openlegend::persistence::load_snapshot(
            *openlegend::persistence::numbered_file_set(game_root, slot));
        OL_CHECK(static_cast<bool>(runtime));
        OL_CHECK(static_cast<bool>(paired));
        if (runtime && paired) {
            OL_CHECK(*runtime.snapshot == *paired.snapshot);
        }
    }

    const auto source = openlegend::persistence::load_numbered_slot(game_root, SaveSlot::one);
    OL_CHECK(static_cast<bool>(source));
    if (!source) {
        return;
    }

    const auto root = output_root / "runtime-slot";
    std::filesystem::remove_all(root);
    OL_CHECK(std::filesystem::create_directories(root));
    std::filesystem::copy_file(
        game_root / "RANGER.IDX",
        root / "RANGER.IDX",
        std::filesystem::copy_options::overwrite_existing);

    const auto slot_files = *openlegend::persistence::numbered_file_set(root, SaveSlot::one);
    const std::array<std::uint8_t, 5> sentinel{0x49U, 0x44U, 0x58U, 0x21U, 0x0AU};
    write_bytes(slot_files.ranger_index, sentinel);
    write_bytes(slot_files.scene_map_index, sentinel);
    write_bytes(slot_files.scene_event_index, sentinel);
    write_bytes(slot_files.ranger_group, sentinel);

    OL_CHECK(openlegend::persistence::write_numbered_slot_scene_archives(
        root, SaveSlot::one, *source.snapshot));
    OL_CHECK(read_bytes(slot_files.ranger_group) ==
             std::vector<std::uint8_t>(sentinel.begin(), sentinel.end()));
    OL_CHECK(openlegend::persistence::write_numbered_slot_ranger(
        root, SaveSlot::one, *source.snapshot));
    OL_CHECK(read_bytes(slot_files.ranger_index) ==
             std::vector<std::uint8_t>(sentinel.begin(), sentinel.end()));
    OL_CHECK(read_bytes(slot_files.scene_map_index) ==
             std::vector<std::uint8_t>(sentinel.begin(), sentinel.end()));
    OL_CHECK(read_bytes(slot_files.scene_event_index) ==
             std::vector<std::uint8_t>(sentinel.begin(), sentinel.end()));

    const auto original_files =
        *openlegend::persistence::numbered_file_set(game_root, SaveSlot::one);
    OL_CHECK(read_bytes(slot_files.ranger_group) == read_bytes(original_files.ranger_group));
    OL_CHECK(read_bytes(slot_files.scene_map_group) == read_bytes(original_files.scene_map_group));
    OL_CHECK(read_bytes(slot_files.scene_event_group) == read_bytes(original_files.scene_event_group));

    auto reloaded = openlegend::persistence::load_numbered_slot(root, SaveSlot::one);
    OL_CHECK(static_cast<bool>(reloaded));
    OL_CHECK(*source.snapshot == *reloaded.snapshot);

    std::filesystem::remove(slot_files.ranger_index);
    std::filesystem::remove(slot_files.scene_map_index);
    std::filesystem::remove(slot_files.scene_event_index);
    reloaded = openlegend::persistence::load_numbered_slot(root, SaveSlot::one);
    OL_CHECK(static_cast<bool>(reloaded));
    OL_CHECK(*source.snapshot == *reloaded.snapshot);

    auto shared_index = read_bytes(root / "RANGER.IDX");
    shared_index.pop_back();
    write_bytes(root / "RANGER.IDX", shared_index);
    OL_CHECK(openlegend::persistence::load_numbered_slot(root, SaveSlot::one).status ==
             openlegend::persistence::PersistenceStatus::invalid_ranger_index_size);
}

void restore_snapshot(const SaveFileSet& files, const GameSnapshot& snapshot) {
    const auto result = openlegend::persistence::write_snapshot(files, snapshot);
    OL_CHECK(static_cast<bool>(result));
}

void check_malformed_rejection(
    const std::filesystem::path& output_root, const GameSnapshot& baseline) {
    using openlegend::persistence::PersistenceStatus;

    const auto root = output_root / "malformed";
    std::filesystem::remove_all(root);
    OL_CHECK(std::filesystem::create_directories(root));
    const auto files = openlegend::persistence::baseline_file_set(root);
    restore_snapshot(files, baseline);

    auto bytes = read_bytes(files.ranger_index);
    bytes.pop_back();
    write_bytes(files.ranger_index, bytes);
    OL_CHECK(openlegend::persistence::load_snapshot(files).status ==
             PersistenceStatus::invalid_ranger_index_size);
    restore_snapshot(files, baseline);

    bytes = read_bytes(files.ranger_index);
    std::copy_n(bytes.begin(), 4U, bytes.begin() + 4);
    write_bytes(files.ranger_index, bytes);
    OL_CHECK(openlegend::persistence::load_snapshot(files).status ==
             PersistenceStatus::invalid_ranger_index_order);
    restore_snapshot(files, baseline);

    bytes = read_bytes(files.ranger_index);
    bytes[4] = static_cast<std::uint8_t>(bytes[4] + 2U);
    write_bytes(files.ranger_index, bytes);
    OL_CHECK(openlegend::persistence::load_snapshot(files).status ==
             PersistenceStatus::invalid_ranger_layout);
    restore_snapshot(files, baseline);

    bytes = read_bytes(files.ranger_group);
    bytes.pop_back();
    write_bytes(files.ranger_group, bytes);
    OL_CHECK(openlegend::persistence::load_snapshot(files).status ==
             PersistenceStatus::invalid_ranger_group_size);
    restore_snapshot(files, baseline);

    bytes = read_bytes(files.scene_map_index);
    bytes.pop_back();
    write_bytes(files.scene_map_index, bytes);
    OL_CHECK(openlegend::persistence::load_snapshot(files).status ==
             PersistenceStatus::invalid_scene_index_size);
    restore_snapshot(files, baseline);

    bytes = read_bytes(files.scene_map_index);
    bytes[0] = static_cast<std::uint8_t>(bytes[0] + 2U);
    write_bytes(files.scene_map_index, bytes);
    OL_CHECK(openlegend::persistence::load_snapshot(files).status ==
             PersistenceStatus::invalid_scene_index_layout);
    restore_snapshot(files, baseline);

    bytes = read_bytes(files.scene_event_group);
    bytes.pop_back();
    write_bytes(files.scene_event_group, bytes);
    OL_CHECK(openlegend::persistence::load_snapshot(files).status ==
             PersistenceStatus::invalid_scene_group_size);
    restore_snapshot(files, baseline);

    auto invalid = baseline;
    invalid.scene_maps.pop_back();
    OL_CHECK(openlegend::persistence::write_snapshot(files, invalid).status ==
             PersistenceStatus::invalid_snapshot);

    auto invalid_ranger = baseline;
    invalid_ranger.ranger.roles.pop_back();
    OL_CHECK(!invalid_ranger.valid());
    OL_CHECK(openlegend::persistence::write_snapshot(files, invalid_ranger).status ==
             PersistenceStatus::invalid_snapshot);

    const auto invalid_slot = static_cast<SaveSlot>(99U);
    OL_CHECK(!openlegend::persistence::numbered_file_set(root, invalid_slot).has_value());
    OL_CHECK(openlegend::persistence::load_numbered_slot(root, invalid_slot).status ==
             PersistenceStatus::invalid_slot);
    OL_CHECK(openlegend::persistence::write_numbered_slot(root, invalid_slot, baseline).status ==
             PersistenceStatus::invalid_slot);

    const auto missing = openlegend::persistence::load_baseline(root / "missing");
    OL_CHECK(missing.status == PersistenceStatus::read_failed);

    const auto no_directory_files = openlegend::persistence::baseline_file_set(root / "no-directory");
    OL_CHECK(openlegend::persistence::write_snapshot(no_directory_files, baseline).status ==
             PersistenceStatus::write_failed);
}

void check_game_state_boundary(const GameSnapshot& baseline) {
    openlegend::model::GameState state;
    OL_CHECK(!state.loaded());
    OL_CHECK(state.ranger() == nullptr);
    OL_CHECK(!state.export_snapshot().has_value());

    OL_CHECK(state.import_snapshot(baseline));
    OL_CHECK(state.loaded());
    OL_CHECK(state.ranger() != nullptr);
    state.ranger()->header.set_team_member(1U, openlegend::model::CharacterId{7});
    state.ranger()->header.set_inventory(0U, openlegend::model::ItemId{174}, 999);
    const auto exported = state.export_snapshot();
    OL_CHECK(exported.has_value());
    OL_CHECK(exported->ranger.header.team_member(1U).value == 7);
    OL_CHECK(exported->ranger.header.inventory_item(0U).value == 174);
    OL_CHECK(exported->ranger.header.inventory_count(0U) == 999);

    auto invalid = baseline;
    invalid.scene_events.clear();
    OL_CHECK(!state.import_snapshot(std::move(invalid)));
    OL_CHECK(state.ranger()->header.team_member(1U).value == 7);
}

}  // namespace

int main() {
    const auto game_root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
    const auto output_root = openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT);
    std::filesystem::create_directories(output_root);

    const auto baseline_files = openlegend::persistence::baseline_file_set(game_root);
    const auto baseline = openlegend::persistence::load_baseline(game_root);
    const auto working = openlegend::persistence::load_working_copy(game_root);
    OL_CHECK(static_cast<bool>(baseline));
    OL_CHECK(static_cast<bool>(working));
    if (baseline) {
        check_baseline_golden(*baseline.snapshot);
        check_game_state_boundary(*baseline.snapshot);
        check_mutation_isolation(output_root, baseline_files, *baseline.snapshot);
        check_runtime_slot_contract(game_root, output_root);
        check_malformed_rejection(output_root, *baseline.snapshot);
    }

    round_trip_case(output_root, "baseline", baseline_files);
    round_trip_case(
        output_root,
        "working",
        openlegend::persistence::working_copy_file_set(game_root));
    round_trip_case(
        output_root,
        "slot-1",
        *openlegend::persistence::numbered_file_set(game_root, SaveSlot::one));
    round_trip_case(
        output_root,
        "slot-2",
        *openlegend::persistence::numbered_file_set(game_root, SaveSlot::two));
    round_trip_case(
        output_root,
        "slot-3",
        *openlegend::persistence::numbered_file_set(game_root, SaveSlot::three));

    if (openlegend::test::failures != 0) {
        std::cerr << openlegend::test::failures << " persistence test(s) failed\n";
        return 1;
    }
    std::cout << "OpenLegend persistence tests passed\n";
    return 0;
}
