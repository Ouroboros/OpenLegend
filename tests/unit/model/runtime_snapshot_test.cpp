#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>

#include "openlegend/model/runtime_snapshot.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "test_support.hpp"

namespace {

void check_complete_legacy_conversion() {
    using namespace openlegend;

    auto loaded = persistence::load_baseline(test::game_data_root());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    auto runtime = model::decode_legacy_snapshot(*loaded.snapshot);
    OL_CHECK(runtime.has_value());
    if (!runtime.has_value()) {
        return;
    }
    OL_CHECK(runtime->valid());
    OL_CHECK(runtime->playthrough == 1);
    OL_CHECK(runtime->scene_maps == loaded.snapshot->scene_maps);
    OL_CHECK(runtime->scene_events == loaded.snapshot->scene_events);
    OL_CHECK(model::encode_legacy_header(runtime->ranger.header) == loaded.snapshot->ranger.header);
    OL_CHECK(runtime->ranger.items == loaded.snapshot->ranger.items);
    OL_CHECK(runtime->ranger.scenes == loaded.snapshot->ranger.scenes);
    OL_CHECK(runtime->ranger.magics == loaded.snapshot->ranger.magics);
    OL_CHECK(runtime->ranger.shops == loaded.snapshot->ranger.shops);
    const auto encoded = model::encode_legacy_snapshot(*runtime);
    OL_CHECK(encoded.has_value());
    if (encoded.has_value()) {
        OL_CHECK(model::decode_legacy_snapshot(*encoded) == runtime);
    }

    model::RuntimeGameState state;
    OL_CHECK(state.import_snapshot(std::move(*runtime)));
    OL_CHECK(state.loaded());
    auto* roles = state.ranger();
    OL_CHECK(roles != nullptr);
    if (roles == nullptr) {
        return;
    }
    roles->roles[0].experience = 1'000'000'000'000;
    roles->roles[0].maximum_hp = 5'000'000'000;
    roles->roles[0].hp = 4'000'000'000;
    const auto saved = state.export_snapshot();
    OL_CHECK(saved.has_value());
    if (!saved.has_value()) {
        return;
    }
    OL_CHECK(saved->ranger.roles[0].experience == 1'000'000'000'000);
    OL_CHECK(saved->ranger.roles[0].hp == 4'000'000'000);
    OL_CHECK(!model::encode_legacy_snapshot(*saved).has_value());
    OL_CHECK(state.snapshot()->ranger.roles[0].hp == 4'000'000'000);
    auto invalid = *saved;
    invalid.scene_maps.pop_back();
    OL_CHECK(!state.import_snapshot(std::move(invalid)));
    OL_CHECK(state.export_snapshot() == saved);
    invalid = *saved;
    invalid.ranger.roles[0].no_magic_count.pop_back();
    OL_CHECK(!state.import_snapshot(std::move(invalid)));
    OL_CHECK(state.export_snapshot() == saved);
    invalid = *saved;
    invalid.ranger.roles[0].name = u8"😀";
    OL_CHECK(!state.import_snapshot(std::move(invalid)));
    OL_CHECK(state.export_snapshot() == saved);
    invalid = *saved;
    invalid.ranger.roles[0].nickname = std::u8string{u8'A', u8'\0', u8'B'};
    OL_CHECK(!state.import_snapshot(std::move(invalid)));
    OL_CHECK(state.export_snapshot() == saved);
    invalid = *saved;
    invalid.origin = static_cast<model::SnapshotOrigin>(2);
    OL_CHECK(!state.import_snapshot(std::move(invalid)));
    OL_CHECK(state.export_snapshot() == saved);
    invalid = *saved;
    invalid.playthrough = 2;
    OL_CHECK(!state.import_snapshot(std::move(invalid)));
    OL_CHECK(state.export_snapshot() == saved);
    auto malformed = *loaded.snapshot;
    malformed.ranger.roles[0].bytes[model::role_word::name_byte] = 0xFFU;
    OL_CHECK(!state.import_snapshot(std::move(malformed)));
    OL_CHECK(state.export_snapshot() == saved);
}

void check_inventory_width_and_legacy_boundary() {
    using namespace openlegend;

    const auto loaded = persistence::load_baseline(test::game_data_root());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    auto snapshot = model::decode_legacy_snapshot(*loaded.snapshot);
    OL_CHECK(snapshot.has_value());
    if (!snapshot.has_value()) {
        return;
    }
    const auto item_id = snapshot->ranger.header.inventory_item(0U);
    constexpr std::array<std::int64_t, 4> counts{
        32'767, 32'768, 5'000'000'000, std::numeric_limits<std::int64_t>::max()};
    for (const auto count : counts) {
        snapshot->ranger.header.set_inventory(0U, item_id, count);
        OL_CHECK(snapshot->valid_for_persistence());
        model::RuntimeGameState state;
        OL_CHECK(state.import_snapshot(*snapshot));
        OL_CHECK(state.export_snapshot() == snapshot);
        OL_CHECK(state.ranger()->header.inventory_count(0U) == count);
        const auto encoded = model::encode_legacy_snapshot(*snapshot);
        OL_CHECK(encoded.has_value() == (count <= 32'767));
        if (encoded.has_value()) {
            OL_CHECK(encoded->ranger.header.inventory_count(0U) == count);
        }
    }
    snapshot->ranger.header.set_inventory(0U, item_id, -1);
    OL_CHECK(!snapshot->valid_for_persistence());
    snapshot->ranger.header.set_inventory(0U, model::ItemId{-1}, 1);
    OL_CHECK(!snapshot->valid_for_persistence());
    auto legacy_header = loaded.snapshot->ranger.header;
    legacy_header.set_inventory(0U, item_id, -32'768);
    const auto widened = model::decode_legacy_header(legacy_header);
    OL_CHECK(widened.inventory_count(0U) == -32'768);
    OL_CHECK(model::encode_legacy_header(widened) == legacy_header);
}

void check_inventory_operation_transactions() {
    using namespace openlegend::model;

    RuntimeRangerHeader header;
    const ItemId item{174};
    header.set_inventory(0U, item, 5'000'000'000);
    header.set_inventory(1U, item, std::numeric_limits<std::int64_t>::max());
    const auto before_overflow = header;
    OL_CHECK(!header.add_inventory(item, 1));
    OL_CHECK(header == before_overflow);
    OL_CHECK(header.change_first_inventory(item, -1));
    OL_CHECK(header.inventory_count(0U) == 4'999'999'999);
    OL_CHECK(header.inventory_count(1U) == std::numeric_limits<std::int64_t>::max());
    header.set_inventory(0U, item, std::numeric_limits<std::int64_t>::min());
    const auto before_underflow = header;
    OL_CHECK(!header.change_first_inventory(item, -1));
    OL_CHECK(header == before_underflow);
    OL_CHECK(header.change_first_inventory(item, 1));
    OL_CHECK(header.inventory_count(0U) == std::numeric_limits<std::int64_t>::max());
    OL_CHECK(header.inventory_item(1U).value == -1);
    OL_CHECK(header.inventory_count(1U) == 0);
}

void check_numeric_domains_and_references() {
    using namespace openlegend;

    const auto loaded = persistence::load_baseline(test::game_data_root());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    const auto baseline = model::decode_legacy_snapshot(*loaded.snapshot);
    OL_CHECK(baseline.has_value());
    if (!baseline.has_value()) {
        return;
    }
    model::RuntimeGameState state;
    OL_CHECK(state.import_snapshot(*baseline));
    const auto reject = [&state, &baseline](model::RuntimeGameSnapshot snapshot) {
        OL_CHECK(!state.import_snapshot(std::move(snapshot)));
        OL_CHECK(state.export_snapshot() == baseline);
    };
    const auto& protagonist = baseline->ranger.roles[0];
    const auto invalid_values = std::array{
        std::pair{&model::RoleState::experience, std::int64_t{-1}},
        std::pair{&model::RoleState::hp, protagonist.maximum_hp + 1},
        std::pair{&model::RoleState::mp, protagonist.maximum_mp + 1},
        std::pair{&model::RoleState::increased_life, std::int64_t{0}},
        std::pair{&model::RoleState::level, std::int64_t{0}},
        std::pair{&model::RoleState::hurt, std::int64_t{100}},
        std::pair{&model::RoleState::poison, std::int64_t{101}},
        std::pair{&model::RoleState::physical_power, std::int64_t{101}},
        std::pair{&model::RoleState::mp_type, std::int64_t{3}},
        std::pair{&model::RoleState::attack_twice, std::int64_t{2}},
        std::pair{&model::RoleState::iq, std::int64_t{101}},
        std::pair{&model::RoleState::sex, std::int64_t{-32'769}}};
    for (const auto& [field, value] : invalid_values) {
        auto invalid = *baseline;
        invalid.ranger.roles[0].*field = value;
        reject(std::move(invalid));
    }
    auto invalid = *baseline;
    invalid.ranger.roles[0].equipment[0] = model::ItemId{200};
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.roles[0].practice_item = model::ItemId{0};
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.roles[0].magic_ids[0] = model::MagicId{93};
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.roles[0].magic_ids[0] = model::MagicId{0};
    invalid.ranger.roles[0].magic_levels[0] = 1;
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.roles[0].taking_items[0] = model::ItemId{-1};
    invalid.ranger.roles[0].taking_counts[0] = 1;
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.roles[0].no_magic_count[0] = 1;
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.header.set_team_member(1U, model::CharacterId{0});
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.header.set_inventory(5U, model::ItemId{0}, 1);
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.items[0].set_word(model::item_word::user, 320);
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.header.set_word(model::header_word::in_sub_map, 2);
    reject(std::move(invalid));
    invalid = *baseline;
    invalid.ranger.roles[83].ever_joined = true;
    OL_CHECK(baseline->ranger.roles[83].hp > baseline->ranger.roles[83].maximum_hp ||
        baseline->ranger.roles[83].mp > baseline->ranger.roles[83].maximum_mp);
    reject(std::move(invalid));

    auto wide = *baseline;
    wide.configuration.enabled = true;
    wide.origin = model::SnapshotOrigin::new_game_plus;
    wide.playthrough = 2;
    for (const auto playthrough : std::array<std::int64_t, 3>{1, 2, 999}) {
        wide.playthrough = playthrough;
        for (const auto speed : std::array<std::int64_t, 6>{
                 -1, 0, 100, 101, 5'000'000'000, std::numeric_limits<std::int64_t>::max()}) {
            wide.ranger.roles[0].speed = speed;
            OL_CHECK(wide.valid());
            const auto speed_valid = speed >= 0 && speed <= 100;
            OL_CHECK(wide.valid_for_persistence() == speed_valid);
            if (!speed_valid) {
                reject(wide);
            }
        }
    }
    for (const auto speed : std::array<std::int64_t, 2>{101, 32767}) {
        auto legacy = *baseline;
        legacy.ranger.roles[0].speed = speed;
        OL_CHECK(legacy.valid_for_persistence());
        const auto encoded = model::encode_legacy_snapshot(legacy);
        OL_CHECK(encoded.has_value());
        if (encoded.has_value()) {
            OL_CHECK(model::decode_legacy_snapshot(*encoded) == legacy);
            model::NewGamePlusConfiguration configuration;
            configuration.enabled = true;
            OL_CHECK(!model::decode_legacy_snapshot(*encoded, configuration).has_value());
        }
    }
    wide.playthrough = 2;
    wide.ranger.roles[0].speed = 100;
    wide.ranger.roles[0].attack = 5'000'000'000;
    wide.ranger.roles[0].knowledge = 6'000'000'000;
    wide.ranger.roles[0].hurt = 199;
    wide.ranger.roles[0].poison = 99;
    const auto manual = std::ranges::find_if(wide.ranger.items, [](const auto& item) {
        return item.word(model::item_word::item_type) == 2 &&
            item.word(model::item_word::magic_id) == -1 &&
            item.word(model::item_word::need_experience) > 0;
    });
    OL_CHECK(manual != wide.ranger.items.end());
    if (manual != wide.ranger.items.end()) {
        wide.ranger.roles[0].no_magic_count[static_cast<std::size_t>(manual->id().value)] =
            7'000'000'000;
    }
    OL_CHECK(wide.valid_for_persistence());
    wide.ranger.roles[0].poison = 100;
    OL_CHECK(wide.valid());
    OL_CHECK(!wide.valid_for_persistence());
    wide.ranger.roles[0].poison = 99;
    wide.ranger.roles[0].hurt = 200;
    OL_CHECK(!wide.valid_for_persistence());
    wide.ranger.roles[0].hurt = 199;
    wide.configuration.hurt_cap_step = -99;
    wide.configuration.maximum_playthroughs = 2;
    wide.ranger.roles[0].hurt = 0;
    OL_CHECK(wide.valid_for_persistence());
    wide.ranger.roles[0].hurt = 1;
    OL_CHECK(!wide.valid_for_persistence());
}

void check_static_definitions() {
    using namespace openlegend;

    const auto loaded = persistence::load_baseline(test::game_data_root());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    const auto runtime = model::decode_legacy_snapshot(*loaded.snapshot);
    OL_CHECK(runtime.has_value());
    if (!runtime.has_value()) {
        return;
    }
    const auto& baseline = loaded.snapshot->ranger;
    OL_CHECK(runtime->ranger.matches_legacy_definitions(baseline));
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    model::RuntimeGameState state;
    OL_CHECK(state.import_snapshot(*loaded.snapshot, configuration, &baseline));
    const auto saved = state.export_snapshot();
    auto invalid = *loaded.snapshot;
    invalid.ranger.magics[0].bytes.back() ^= 1U;
    OL_CHECK(!state.import_snapshot(std::move(invalid), configuration, &baseline));
    OL_CHECK(state.export_snapshot() == saved);
    auto changed = runtime->ranger;
    changed.items[0].set_word(model::item_word::user, 1);
    changed.scenes[0].set_word(model::scene_metadata_word::entrance_condition, 1);
    changed.shops[0].set_word(model::shop_word::total_begin, 1);
    changed.roles[0].hp = 1;
    changed.roles[0].name = u8"新名字";
    OL_CHECK(changed.matches_legacy_definitions(baseline));
    changed.items[0].bytes.back() ^= 1U;
    OL_CHECK(!changed.matches_legacy_definitions(baseline));
    changed = runtime->ranger;
    changed.scenes[0].bytes.back() ^= 1U;
    OL_CHECK(!changed.matches_legacy_definitions(baseline));
    changed = runtime->ranger;
    changed.magics[0].bytes.back() ^= 1U;
    OL_CHECK(!changed.matches_legacy_definitions(baseline));
    changed = runtime->ranger;
    changed.shops[0].bytes[model::shop_word::price_begin * 2U] ^= 1U;
    OL_CHECK(!changed.matches_legacy_definitions(baseline));
    changed = runtime->ranger;
    changed.roles[0].head_id += 1;
    OL_CHECK(!changed.matches_legacy_definitions(baseline));
    changed = runtime->ranger;
    changed.roles[0].id = model::CharacterId{1};
    OL_CHECK(!changed.matches_legacy_definitions(baseline));
}

void check_save_format_boundaries() {
    using namespace openlegend;

    auto loaded = persistence::load_baseline(test::game_data_root());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    auto runtime = model::decode_legacy_snapshot(*loaded.snapshot);
    OL_CHECK(runtime.has_value());
    if (!runtime.has_value()) {
        return;
    }
    runtime->origin = model::SnapshotOrigin::new_game_plus;
    OL_CHECK(!model::encode_legacy_snapshot(*runtime).has_value());
    runtime->origin = model::SnapshotOrigin::legacy;
    runtime->playthrough = 2;
    OL_CHECK(!model::encode_legacy_snapshot(*runtime).has_value());
    runtime->playthrough = 1;
    runtime->configuration.enabled = true;
    OL_CHECK(!model::encode_legacy_snapshot(*runtime).has_value());
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    configuration.hurt_cap_step = 250;
    const auto configured = model::decode_legacy_snapshot(*loaded.snapshot, configuration);
    OL_CHECK(configured.has_value());
    if (configured.has_value()) {
        OL_CHECK(configured->configuration == configuration);
        OL_CHECK(configured->playthrough == 1);
        OL_CHECK(!model::encode_legacy_snapshot(*configured).has_value());
    }
    configuration.maximum_playthroughs = 0;
    OL_CHECK(!model::decode_legacy_snapshot(*loaded.snapshot, configuration).has_value());
    runtime->configuration.enabled = false;
    runtime->ranger.roles[0].ever_joined = true;
    OL_CHECK(!model::encode_legacy_snapshot(*runtime).has_value());
    runtime->ranger.roles[0].ever_joined = false;
    runtime->ranger.roles[0].no_magic_count[0] = 1;
    OL_CHECK(!model::encode_legacy_snapshot(*runtime).has_value());
}

}

int main() {
    check_complete_legacy_conversion();
    check_inventory_width_and_legacy_boundary();
    check_inventory_operation_transactions();
    check_numeric_domains_and_references();
    check_static_definitions();
    check_save_format_boundaries();
    return openlegend::test::failures == 0 ? 0 : 1;
}
