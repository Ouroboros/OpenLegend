#include <algorithm>
#include <array>
#include <limits>

#include "openlegend/model/playthrough_transition.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "test_support.hpp"

int main() {
    using namespace openlegend;
    auto loaded = persistence::load_baseline(test::game_data_root());
    OL_CHECK(loaded);
    if (!loaded) {
        return 1;
    }
    auto& baseline = *loaded.snapshot;
    baseline.ranger.roles[50U].set_word(model::role_word::make_item_experience, 23);
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    const auto initial = model::decode_legacy_snapshot(baseline, configuration, &baseline.ranger);
    OL_CHECK(initial.has_value());
    if (!initial) {
        return 1;
    }
    auto source = *initial;
    source.origin = model::SnapshotOrigin::new_game_plus;
    auto& actor = source.ranger.roles[0U];
    actor.ever_joined = true;
    actor.name = u8"繼承";
    actor.level = std::numeric_limits<std::int64_t>::max();
    actor.maximum_hp = 5'000'000'000'000;
    actor.maximum_mp = 4'000'000'000'000;
    actor.hp = 13;
    actor.mp = 17;
    actor.attack = 3'000'000'000'000;
    actor.hurt = 31;
    actor.poison = 29;
    actor.physical_power = 7;
    actor.sex = 2;
    actor.morality = (actor.morality + 1) % 101;
    actor.fame += 1;
    actor.attack_twice = 1 - actor.attack_twice;
    actor.make_item_experience = 123;
    actor.item_experience = 456;
    actor.magic_ids[0U] = model::MagicId{1};
    actor.magic_levels[0U] = 987'654'321;
    actor.taking_items.fill(model::ItemId{-1});
    actor.taking_counts.fill(0);
    actor.taking_items[0U] = model::ItemId{174};
    actor.taking_counts[0U] = 71;
    actor.frames[0U] = static_cast<std::int16_t>(actor.frames[0U] + 1);
    std::array<model::ItemId, 5> selected_items;
    selected_items.fill(model::ItemId{-1});
    for (const auto& item : baseline.ranger.items) {
        const auto type = item.word(model::item_word::item_type);
        if (type == 3) selected_items[0U] = item.id();
        if (type == 4) selected_items[1U] = item.id();
        if (type == 1 && item.word(model::item_word::equipment_type) == 0) selected_items[2U] = item.id();
        if (type == 2) selected_items[3U] = item.id();
        if (type == 2 && item.word(model::item_word::magic_id) == -1 &&
            item.word(model::item_word::need_experience) > 0) selected_items[4U] = item.id();
    }
    OL_CHECK(std::all_of(selected_items.begin(), selected_items.end(), [](const auto item) { return item.value >= 0; }));
    if (std::any_of(selected_items.begin(), selected_items.end(), [](const auto item) { return item.value < 0; })) {
        return 1;
    }
    actor.equipment[0U] = selected_items[2U];
    actor.practice_item = selected_items[3U];
    actor.no_magic_count[static_cast<std::size_t>(selected_items[4U].value)] = 987'654'321;
    source.ranger.roles[49U].ever_joined = true;
    source.ranger.roles[49U].attack = 7'000'000'000'000;
    source.ranger.roles[50U].attack = 8'000'000'000'000;
    source.ranger.items[static_cast<std::size_t>(selected_items[2U].value)].set_word(model::item_word::user, 0);
    source.scene_maps[0U] ^= 1U;
    source.scene_events[0U] ^= 1U;
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        source.ranger.header.set_inventory(slot, model::ItemId{-1}, 0);
    }
    source.ranger.header.set_inventory(0U, selected_items[0U], 5'000'000'000'000);
    source.ranger.header.set_inventory(1U, selected_items[2U], 1);
    source.ranger.header.set_inventory(2U, model::ItemId{174}, std::numeric_limits<std::int64_t>::max());
    source.ranger.header.set_inventory(3U, selected_items[0U], 17);
    source.ranger.header.set_inventory(4U, selected_items[1U], 11);
    OL_CHECK(source.valid_for_persistence());
    const auto original = source;
    const auto result = model::prepare_next_playthrough(source, baseline, configuration);
    OL_CHECK(result);
    OL_CHECK(source == original);
    if (result) {
        const auto& next = *result.snapshot;
        const auto& inherited = next.ranger.roles[0U];
        const auto& base_actor = initial->ranger.roles[0U];
        OL_CHECK(next.playthrough == 2 && next.origin == model::SnapshotOrigin::new_game_plus);
        OL_CHECK(next.configuration == configuration && next.valid_for_persistence());
        OL_CHECK(inherited.name == actor.name && inherited.level == actor.level && inherited.attack == actor.attack);
        OL_CHECK(inherited.hp == actor.maximum_hp && inherited.maximum_hp == actor.maximum_hp);
        OL_CHECK(inherited.mp == actor.maximum_mp && inherited.maximum_mp == actor.maximum_mp);
        OL_CHECK(inherited.hurt == 0 && inherited.poison == 0 && inherited.physical_power == 100);
        OL_CHECK(inherited.ever_joined && inherited.no_magic_count == actor.no_magic_count);
        OL_CHECK(inherited.magic_ids == actor.magic_ids && inherited.magic_levels == actor.magic_levels);
        OL_CHECK(inherited.sex == base_actor.sex && inherited.morality == base_actor.morality);
        OL_CHECK(inherited.fame == base_actor.fame && inherited.attack_twice == base_actor.attack_twice);
        OL_CHECK(inherited.frames == base_actor.frames && inherited.head_id == base_actor.head_id);
        OL_CHECK(inherited.taking_items == base_actor.taking_items && inherited.taking_counts == base_actor.taking_counts);
        OL_CHECK(inherited.equipment[0U].value == -1 && inherited.equipment[1U].value == -1);
        OL_CHECK(inherited.practice_item.value == -1 && inherited.item_experience == 0 && inherited.make_item_experience == 0);
        OL_CHECK(next.ranger.roles[49U].ever_joined && next.ranger.roles[49U].attack == 7'000'000'000'000);
        auto untouched = initial->ranger.roles[50U];
        untouched.make_item_experience = 0;
        OL_CHECK(next.ranger.roles[50U] == untouched);
        OL_CHECK(next.ranger.items == initial->ranger.items && next.ranger.scenes == initial->ranger.scenes);
        OL_CHECK(next.ranger.shops == initial->ranger.shops && next.ranger.magics == initial->ranger.magics);
        OL_CHECK(next.scene_maps == initial->scene_maps && next.scene_events == initial->scene_events);
        OL_CHECK(next.scene_map_ends == initial->scene_map_ends && next.scene_event_ends == initial->scene_event_ends);
        for (std::size_t slot = 0U; slot < model::kTeamMemberCount; ++slot) {
            OL_CHECK(next.ranger.header.team_member(slot) == initial->ranger.header.team_member(slot));
        }
        constexpr std::array<std::size_t, 4> retained{0U, 2U, 3U, 4U};
        for (std::size_t slot = 0U; slot < retained.size(); ++slot) {
            OL_CHECK(next.ranger.header.inventory_item(slot) == source.ranger.header.inventory_item(retained[slot]));
            OL_CHECK(next.ranger.header.inventory_count(slot) == source.ranger.header.inventory_count(retained[slot]));
        }
        OL_CHECK(next.ranger.header.inventory_item(4U).value == -1);
        auto last_source = source;
        last_source.playthrough = 998;
        const auto last = model::prepare_next_playthrough(last_source, baseline, configuration);
        OL_CHECK(last && last.snapshot->playthrough == 999);
        last_source.playthrough = 999;
        OL_CHECK(!model::prepare_next_playthrough(last_source, baseline, configuration));
    }
    auto invalid = source;
    invalid.origin = model::SnapshotOrigin::legacy;
    OL_CHECK(!model::prepare_next_playthrough(invalid, baseline, configuration));
    invalid = source;
    invalid.ranger.roles[0U].head_id += 1;
    OL_CHECK(!model::prepare_next_playthrough(invalid, baseline, configuration));
    invalid = source;
    invalid.ranger.roles[0U].hp = -1;
    OL_CHECK(!model::prepare_next_playthrough(invalid, baseline, configuration));
    configuration.enabled = false;
    OL_CHECK(!model::prepare_next_playthrough(source, baseline, configuration));
    configuration.enabled = true;
    configuration.maximum_playthroughs = 1;
    OL_CHECK(!model::prepare_next_playthrough(source, baseline, configuration));
    return test::failures == 0 ? 0 : 1;
}
