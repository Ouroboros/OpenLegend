#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_pathing.hpp"
#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "test_support.hpp"

#ifndef OPENLEGEND_GAME_DATA_ROOT
#error OPENLEGEND_GAME_DATA_ROOT must name the read-only original data directory
#endif

namespace {

std::uint64_t fnv1a_words(const std::span<const std::int16_t> words) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const auto word : words) {
        const auto raw = static_cast<std::uint16_t>(word);
        hash ^= static_cast<std::uint8_t>(raw);
        hash *= 0x100000001b3ULL;
        hash ^= static_cast<std::uint8_t>(raw >> 8U);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

void run_real_asset_fixtures(const openlegend::resource::DataRoot& data_root) {
    struct Fixture {
        std::int16_t battle_id;
        std::int16_t battlefield_id;
        std::int16_t music_id;
        std::uint64_t definition_hash;
        std::uint64_t battlefield_hash;
    };
    constexpr std::array fixtures{
        Fixture{0, 0, 5, 0xca8bf0ffb5fb1174ULL, 0x004d07e3421dbf99ULL},
        Fixture{4, 2, 7, 0x703a3afde8945d4dULL, 0xae3409d798fd5167ULL},
        Fixture{93, 24, 7, 0x04873ba87ef6e4bdULL, 0xbe54d444b579fde5ULL},
        Fixture{139, 21, 7, 0x33392999c31679ceULL, 0x5b6319016ff6273fULL},
    };

    for (const auto& fixture : fixtures) {
        openlegend::battle::BattleData data{data_root, fixture.battle_id};
        OL_CHECK(data.valid());
        OL_CHECK(data.battle_id() == fixture.battle_id);
        OL_CHECK(data.battlefield_id() == fixture.battlefield_id);
        OL_CHECK(data.music_id() == fixture.music_id);
        OL_CHECK(fnv1a_words(data.definition()) == fixture.definition_hash);
        OL_CHECK(fnv1a_words(data.battlefield()) == fixture.battlefield_hash);
        OL_CHECK(std::ranges::all_of(data.occupancy(), [](const std::int16_t value) {
            return value == -1;
        }));
    }
}

void run_pathing_tests(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    struct Fixture {
        std::int16_t battle_id;
        BattlePathCoord source;
        BattlePathCoord target;
        BattlePathCoord occupied;
        BattlePathCoord first_step;
        std::int16_t target_distance;
        std::uint64_t movement_hash;
        std::uint64_t occupied_hash;
        std::uint64_t targeting_hash;
        std::uint64_t marked_hash;
    };
    constexpr std::array fixtures{
        Fixture{
            0,
            {32, 20},
            {21, 23},
            {33, 20},
            {31, 20},
            14,
            0x773478cb5fde310dULL,
            0x8b3c54e9cbef5effULL,
            0x773478cb5fde310dULL,
            0x3555eec69bfdbfc0ULL,
        },
        Fixture{
            93,
            {34, 29},
            {12, 29},
            {35, 29},
            {33, 29},
            22,
            0xc4e9944b25f2c2bbULL,
            0x407e0a6bb5fd7397ULL,
            0xc4e9944b25f2c2bbULL,
            0x6760d37356a2b33aULL,
        },
    };

    for (const auto& fixture : fixtures) {
        BattleData data{data_root, fixture.battle_id};
        OL_CHECK(data.valid());
        std::ranges::fill(data.occupancy(), static_cast<std::int16_t>(-1));
        BattlePathing pathing{data};

        pathing.build(fixture.source, BattlePathMode::movement);
        OL_CHECK(fnv1a_words(pathing.values()) == fixture.movement_hash);

        const auto occupied_index = static_cast<std::size_t>(fixture.occupied.y) * 64U +
            static_cast<std::size_t>(fixture.occupied.x);
        data.occupancy()[occupied_index] = 7;
        pathing.build(fixture.source, BattlePathMode::movement);
        OL_CHECK(fnv1a_words(pathing.values()) == fixture.occupied_hash);
        OL_CHECK(pathing.value(fixture.occupied) == kBattlePathBlocked);

        pathing.build(fixture.source, BattlePathMode::targeting);
        OL_CHECK(fnv1a_words(pathing.values()) == fixture.targeting_hash);
        OL_CHECK(pathing.value(fixture.target) == fixture.target_distance);
        OL_CHECK(pathing.mark_shortest_path(fixture.source, fixture.target));
        OL_CHECK(fnv1a_words(pathing.values()) == fixture.marked_hash);
        OL_CHECK(pathing.next_marked_step(fixture.source) == fixture.first_step);
        pathing.consume(fixture.source);
        OL_CHECK(pathing.value(fixture.source) == kBattlePathConsumed);
    }
}

openlegend::model::RangerState make_ranger(
    const std::array<std::int16_t, openlegend::model::kTeamMemberCount>& party) {
    openlegend::model::RangerState ranger;
    for (std::size_t role = 0U; role < ranger.roles.size(); ++role) {
        ranger.roles[role].set_word(
            openlegend::model::role_word::head_id,
            static_cast<std::int16_t>(role % 17U));
        for (std::size_t equipment = 0U;
             equipment < openlegend::model::role_word::equipment_count;
             ++equipment) {
            ranger.roles[role].set_word(
                openlegend::model::role_word::equipment_begin + equipment, -1);
        }
    }
    for (std::size_t index = 0U; index < party.size(); ++index) {
        ranger.header.set_team_member(index, openlegend::model::CharacterId{party[index]});
    }
    return ranger;
}

void run_movement_step_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    ranger.roles[0U].set_word(openlegend::model::role_word::speed, 50);
    ranger.roles[0U].set_word(openlegend::model::role_word::physical_power, 1);
    BattleData data{data_root, 0};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.apply(PartySelectionAction::previous) == PartySelectionResult::changed);
    OL_CHECK(setup.apply(PartySelectionAction::activate) == PartySelectionResult::complete);

    auto& actor = setup.combatants()[0U].words;
    actor[combatant_word::initial_mode] = 3;
    actor[combatant_word::sprite] = 5112;
    actor[combatant_word::round_value] = 5;
    BattlePathing pathing{data};
    const BattlePathCoord source{32, 20};
    const BattlePathCoord target{21, 23};
    pathing.build(source, BattlePathMode::targeting);
    OL_CHECK(pathing.mark_shortest_path(source, target));

    OL_CHECK((setup.move_one_marked_step(pathing, 0U) == BattlePathCoord{31, 20}));
    OL_CHECK(pathing.value(source) == kBattlePathConsumed);
    OL_CHECK(data.occupancy()[20U * 64U + 32U] == -1);
    OL_CHECK(data.occupancy()[20U * 64U + 31U] == 0);
    OL_CHECK(actor[combatant_word::x] == 31);
    OL_CHECK(actor[combatant_word::y] == 20);
    OL_CHECK(actor[combatant_word::initial_mode] == 2);
    OL_CHECK(actor[combatant_word::sprite] == 5110);
    OL_CHECK(actor[combatant_word::round_value] == 4);
    OL_CHECK(ranger.roles[0U].word(openlegend::model::role_word::physical_power) == 0);
    OL_CHECK(setup.movement_should_stop(
        0U, BattlePathCoord{31, 20}, 1U, BattleMovementStopRule::destination, 0));
    OL_CHECK(!setup.movement_should_stop(
        0U, target, 1U, BattleMovementStopRule::destination, 0));
    OL_CHECK(setup.movement_should_stop(
        0U, target, 1U, BattleMovementStopRule::in_range, 13));
    OL_CHECK(!setup.movement_should_stop(
        0U, target, 1U, BattleMovementStopRule::aligned_in_range, 13));

    OL_CHECK((setup.move_one_marked_step(pathing, 0U) == BattlePathCoord{30, 20}));
    OL_CHECK(pathing.value(BattlePathCoord{31, 20}) == kBattlePathConsumed);
    OL_CHECK(data.occupancy()[20U * 64U + 31U] == -1);
    OL_CHECK(data.occupancy()[20U * 64U + 30U] == 0);
    OL_CHECK(actor[combatant_word::round_value] == 3);
    OL_CHECK(ranger.roles[0U].word(openlegend::model::role_word::physical_power) == 0);
    actor[combatant_word::round_value] = 0;
    OL_CHECK(setup.movement_should_stop(
        0U, target, 1U, BattleMovementStopRule::aligned_in_range, 0));
}

void run_attack_profile_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& role = ranger.roles[1U];
    role.set_word(openlegend::model::role_word::magic_id_begin + 2U, 5);
    role.set_word(openlegend::model::role_word::magic_level_begin + 2U, 299);
    role.set_word(openlegend::model::role_word::attack_twice, 1);
    role.set_word(openlegend::model::role_word::mp, 3);
    role.set_word(openlegend::model::role_word::physical_power, 2);
    auto& magic = ranger.magics[5U];
    magic.set_word(openlegend::model::magic_word::select_distance_begin + 2U, 7);
    magic.set_word(openlegend::model::magic_word::attack_distance_begin + 2U, 3);
    magic.set_word(openlegend::model::magic_word::attack_area_type, 2);
    magic.set_word(openlegend::model::magic_word::hurt_type, 1);
    magic.set_word(openlegend::model::magic_word::need_mp, 4);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    OL_CHECK(setup.learned_magic_count(0U) == 1U);
    openlegend::random::LegacyRandom random{1U};
    OL_CHECK(setup.automatic_magic_slot(0U, random) == 0);
    OL_CHECK(random.state() == 1U);
    const auto legacy_single = setup.attack_profile(0U, 0);
    OL_CHECK(legacy_single.has_value());
    OL_CHECK(legacy_single->magic_id == 0);

    const auto profile = setup.attack_profile(0U, 2);
    OL_CHECK(profile.has_value());
    OL_CHECK(profile->magic_id == 5);
    OL_CHECK(profile->level_index == 2);
    OL_CHECK(profile->select_distance == 7);
    OL_CHECK(profile->attack_distance == 3);
    OL_CHECK(profile->area_type == 2);
    OL_CHECK(profile->hurt_type == 1);
    OL_CHECK(profile->attack_count == 2);
    OL_CHECK(profile->need_mp == 4);

    OL_CHECK(setup.commit_attack_iteration(0U, 2, 3, random));
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(role.word(openlegend::model::role_word::magic_level_begin + 2U) == 300);
    OL_CHECK(role.word(openlegend::model::role_word::mp) == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 2);

    role.set_word(openlegend::model::role_word::magic_level_begin + 2U, 999);
    role.set_word(openlegend::model::role_word::mp, 10);
    OL_CHECK(!setup.commit_attack_iteration(0U, 2, 2, random));
    OL_CHECK(role.word(openlegend::model::role_word::magic_level_begin + 2U) == 999);
    OL_CHECK(role.word(openlegend::model::role_word::mp) == 6);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 4);
    OL_CHECK(setup.finish_attack(0U));
    OL_CHECK(role.word(openlegend::model::role_word::physical_power) == 0);

    role.set_word(openlegend::model::role_word::magic_id_begin, 6);
    role.set_word(openlegend::model::role_word::magic_id_begin + 4U, 7);
    ranger.magics[6U].set_word(openlegend::model::magic_word::need_mp, 6);
    ranger.magics[7U].set_word(openlegend::model::magic_word::need_mp, 7);
    auto selection = setup.begin_magic_selection(0U);
    OL_CHECK(selection.has_value());
    OL_CHECK(selection->learned_count == 3);
    OL_CHECK(selection->available_count == 2);
    OL_CHECK(selection->available_slots[0U] == 0);
    OL_CHECK(selection->available_slots[1U] == 2);
    OL_CHECK(selection->available_slots[2U] == -1);
    std::vector<std::int16_t> selection_words(
        selection->available_slots.begin(), selection->available_slots.end());
    selection_words.insert(
        selection_words.end(),
        {selection->learned_count, selection->available_count, selection->cursor});
    OL_CHECK(fnv1a_words(selection_words) == 0xc254d2cd83d7da76ULL);
    OL_CHECK(BattleSetup::apply_magic_selection(
                 *selection, BattleMagicSelectionAction::next) ==
             BattleMagicSelectionResult::changed);
    OL_CHECK(selection->cursor == 1);
    OL_CHECK(BattleSetup::apply_magic_selection(
                 *selection, BattleMagicSelectionAction::next) ==
             BattleMagicSelectionResult::changed);
    OL_CHECK(selection->cursor == 0);
    OL_CHECK(BattleSetup::apply_magic_selection(
                 *selection, BattleMagicSelectionAction::previous) ==
             BattleMagicSelectionResult::changed);
    OL_CHECK(selection->cursor == 1);
    OL_CHECK(BattleSetup::apply_magic_selection(
                 *selection, BattleMagicSelectionAction::activate) ==
             BattleMagicSelectionResult::selected);
    OL_CHECK(selection->selected_slot == 2);
    OL_CHECK(BattleSetup::apply_magic_selection(
                 *selection, BattleMagicSelectionAction::next) ==
             BattleMagicSelectionResult::invalid);

    auto cancelled = setup.begin_magic_selection(0U);
    OL_CHECK(cancelled.has_value());
    OL_CHECK(BattleSetup::apply_magic_selection(
                 *cancelled, BattleMagicSelectionAction::cancel) ==
             BattleMagicSelectionResult::cancelled);
    OL_CHECK(cancelled->cancelled);
}

void run_attack_animation_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& actor = ranger.roles[1U];
    actor.set_word(openlegend::model::role_word::head_id, 9);
    actor.set_word(openlegend::model::role_word::magic_id_begin + 2U, 5);
    actor.set_word(openlegend::model::role_word::magic_level_begin + 2U, 200);
    actor.set_word(openlegend::model::role_word::frame_begin, 2);
    actor.set_word(openlegend::model::role_word::frame_begin + 1U, 3);
    actor.set_word(openlegend::model::role_word::frame_begin + 2U, 4);
    actor.set_word(openlegend::model::role_word::frame_begin + 5U + 2U, 3);
    actor.set_word(openlegend::model::role_word::frame_begin + 10U + 2U, 5);
    auto& magic = ranger.magics[5U];
    magic.set_word(openlegend::model::magic_word::sound_id, 7);
    magic.set_word(openlegend::model::magic_word::magic_type, 2);
    magic.set_word(openlegend::model::magic_word::effect_id, 2);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    setup.combatants()[0U].words[combatant_word::initial_mode] = 1;
    setup.combatants()[0U].words[combatant_word::sprite] = 5110;
    const auto plan = setup.magic_animation_plan(0U, 2, 100);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->fight_head_id == 9);
    OL_CHECK(plan->magic_sample_id == 7);
    OL_CHECK(plan->effect_sample_id == 2);
    OL_CHECK(plan->clear_effect_after_frames);
    OL_CHECK(plan->frames.size() == 19U);
    OL_CHECK(plan->frames[0U].actor_sprite == 248);
    OL_CHECK(plan->frames[0U].effect_frame == 46);
    OL_CHECK(plan->frames[0U].actor_sprite_updated);
    OL_CHECK(!plan->frames[0U].effect_visible);
    OL_CHECK(plan->frames[2U].actor_sprite == 252);
    OL_CHECK(plan->frames[2U].effect_frame == 48);
    OL_CHECK(plan->frames[2U].dispatch_effect_sample);
    OL_CHECK(plan->frames[4U].actor_sprite == 254);
    OL_CHECK(!plan->frames[4U].actor_sprite_updated);
    OL_CHECK(plan->frames[4U].dispatch_magic_sample);
    OL_CHECK(plan->frames.back().effect_frame == 80);
    OL_CHECK(std::ranges::count_if(plan->frames, [](const auto& frame) {
                 return frame.actor_sprite_updated;
             }) == 4);
    OL_CHECK(std::ranges::count_if(plan->frames, [](const auto& frame) {
                 return frame.effect_visible;
             }) == 17);
    OL_CHECK(std::ranges::count_if(plan->frames, [](const auto& frame) {
                 return frame.dispatch_magic_sample;
             }) == 1);
    OL_CHECK(std::ranges::count_if(plan->frames, [](const auto& frame) {
                 return frame.dispatch_effect_sample;
             }) == 1);
    std::vector<std::int16_t> magic_words;
    for (const auto& frame : plan->frames) {
        magic_words.insert(
            magic_words.end(),
            {frame.actor_sprite,
             frame.effect_frame,
             frame.wait_ticks,
             static_cast<std::int16_t>(frame.actor_sprite_updated),
             static_cast<std::int16_t>(frame.effect_visible),
             static_cast<std::int16_t>(frame.dispatch_magic_sample),
             static_cast<std::int16_t>(frame.dispatch_effect_sample)});
    }
    OL_CHECK(fnv1a_words(magic_words) == 0x5aaffbb1d5697a73ULL);

    const auto effect = BattleSetup::effect_animation_plan(2);
    OL_CHECK(effect.has_value());
    OL_CHECK(effect->magic_sample_id == 13);
    OL_CHECK(effect->effect_sample_id == 2);
    OL_CHECK(effect->prelude_wait_ticks == 100);
    OL_CHECK(effect->dispatch_magic_before_prelude);
    OL_CHECK(effect->dispatch_effect_after_prelude);
    OL_CHECK(effect->clear_effect_after_frames);
    OL_CHECK(effect->frames.size() == 17U);
    OL_CHECK(effect->frames.front().effect_frame == 48);
    OL_CHECK(effect->frames.back().effect_frame == 80);
    std::vector<std::int16_t> effect_words;
    for (const auto& frame : effect->frames) {
        effect_words.insert(
            effect_words.end(),
            {frame.effect_frame,
             frame.wait_ticks,
             static_cast<std::int16_t>(frame.effect_visible)});
    }
    OL_CHECK(fnv1a_words(effect_words) == 0x2b5c87d8e0c754d5ULL);
    OL_CHECK(!BattleSetup::effect_animation_plan(53).has_value());

    const auto damage = BattleSetup::damage_animation_frames(false);
    std::vector<std::int16_t> damage_words;
    for (const auto& frame : damage) {
        damage_words.insert(
            damage_words.end(),
            {frame.phase, frame.wait_ticks, static_cast<std::int16_t>(frame.flash)});
    }
    OL_CHECK(fnv1a_words(damage_words) == 0x364953a2c8f42144ULL);
    OL_CHECK(damage[3U].flash);
    OL_CHECK(!damage[4U].flash);
    const auto suppressed = BattleSetup::damage_animation_frames(true);
    damage_words.clear();
    for (const auto& frame : suppressed) {
        damage_words.insert(
            damage_words.end(),
            {frame.phase, frame.wait_ticks, static_cast<std::int16_t>(frame.flash)});
    }
    OL_CHECK(fnv1a_words(damage_words) == 0xec7a73890ce825c4ULL);
    OL_CHECK(std::ranges::none_of(suppressed, [](const auto& frame) { return frame.flash; }));
}

void run_poison_action_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& actor = ranger.roles[1U];
    auto& target = ranger.roles[3U];
    actor.set_word(openlegend::model::role_word::use_poison, 80);
    actor.set_word(openlegend::model::role_word::physical_power, 1);
    target.set_word(openlegend::model::role_word::anti_poison, 20);
    target.set_word(openlegend::model::role_word::poison, 90);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    OL_CHECK(setup.poison_targeting_range(0U) == 6);
    const auto result = setup.apply_poison_target(0U, BattlePathCoord{26, 26});
    OL_CHECK(result.has_value());
    OL_CHECK(result->hit_count == 1);
    OL_CHECK(result->effect_kind == 2);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xab559939923b4f74ULL);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 9);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 99);
    OL_CHECK(setup.finish_poison_action(0U));
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 1);
    OL_CHECK(actor.word(openlegend::model::role_word::physical_power) == 0);

    actor.set_word(openlegend::model::role_word::use_poison, 10);
    target.set_word(openlegend::model::role_word::anti_poison, 20);
    target.set_word(openlegend::model::role_word::poison, 0);
    OL_CHECK(setup.apply_poison_value(0U, 1U) == 0);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 0);
    actor.set_word(openlegend::model::role_word::use_poison, 500);
    target.set_word(openlegend::model::role_word::anti_poison, 0);
    OL_CHECK(setup.apply_poison_value(0U, 1U) == 99);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 99);

    data.occupancy()[26U * 64U + 26U] = -1;
    const auto empty = setup.apply_poison_target(0U, BattlePathCoord{25, 24});
    OL_CHECK(empty.has_value());
    OL_CHECK(empty->hit_count == 0);
    OL_CHECK(!empty->effect_kind.has_value());
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 2);
    OL_CHECK(std::ranges::count(setup.attack_effects(), 1) == 1);

    const auto friendly = setup.apply_poison_target(0U, BattlePathCoord{26, 24});
    OL_CHECK(friendly.has_value());
    OL_CHECK(friendly->hit_count == 0);
    OL_CHECK(!friendly->effect_kind.has_value());
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 2);
    OL_CHECK(std::ranges::none_of(setup.attack_effects(), [](const std::int16_t value) {
        return value != 0;
    }));
}

void run_damage_formula_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& actor = ranger.roles[1U];
    auto& target = ranger.roles[3U];
    actor.set_word(openlegend::model::role_word::magic_id_begin + 2U, 5);
    actor.set_word(openlegend::model::role_word::magic_level_begin + 2U, 200);
    actor.set_word(openlegend::model::role_word::mp, 10);
    actor.set_word(openlegend::model::role_word::maximum_mp, 20);
    actor.set_word(openlegend::model::role_word::attack, 30);
    actor.set_word(openlegend::model::role_word::physical_power, 10);
    actor.set_word(openlegend::model::role_word::attack_with_poison, 30);
    target.set_word(openlegend::model::role_word::level, 4);
    target.set_word(openlegend::model::role_word::hp, 30);
    target.set_word(openlegend::model::role_word::defence, 5);
    target.set_word(openlegend::model::role_word::hurt, 0);
    target.set_word(openlegend::model::role_word::poison, 0);
    target.set_word(openlegend::model::role_word::anti_poison, 0);
    auto& magic = ranger.magics[5U];
    magic.set_word(openlegend::model::magic_word::need_mp, 4);
    magic.set_word(openlegend::model::magic_word::attack_begin + 2U, 30);
    magic.set_word(openlegend::model::magic_word::add_mp_begin + 2U, 20);
    magic.set_word(openlegend::model::magic_word::hurt_mp_begin + 2U, 15);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    openlegend::random::LegacyRandom hp_random{1U};
    const auto exact = setup.apply_hp_damage(0U, 1U, 2, 1, 0, hp_random);
    OL_CHECK(exact.has_value());
    OL_CHECK(exact->damage == 30);
    OL_CHECK(exact->cost_scale == 3);
    OL_CHECK(hp_random.state() == 2'524'885'223U);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 0);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 3);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 4);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 6);

    target.set_word(openlegend::model::role_word::hp, 29);
    target.set_word(openlegend::model::role_word::hurt, 0);
    target.set_word(openlegend::model::role_word::poison, 0);
    setup.combatants()[0U].words[combatant_word::attack_counter] = 0;
    hp_random.seed(1U);
    const auto underkill = setup.apply_hp_damage(0U, 1U, 2, 1, 0, hp_random);
    OL_CHECK(underkill.has_value());
    OL_CHECK(underkill->damage == 30);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 46);

    actor.set_word(openlegend::model::role_word::knowledge, 81);
    actor.set_word(openlegend::model::role_word::hp, 1);
    target.set_word(openlegend::model::role_word::knowledge, 82);
    actor.set_word(openlegend::model::role_word::physical_power, 0);
    actor.set_word(openlegend::model::role_word::equipment_begin, 10);
    target.set_word(openlegend::model::role_word::equipment_begin, 11);
    ranger.items[10U].set_word(openlegend::model::item_word::add_attack, 6);
    ranger.items[11U].set_word(openlegend::model::item_word::add_defence, 2);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::hurt, 0);
    target.set_word(openlegend::model::role_word::poison, 0);
    target.set_word(openlegend::model::role_word::anti_poison, 100);
    setup.combatants()[0U].words[combatant_word::attack_counter] = 0;
    hp_random.seed(1U);
    const auto fallback = setup.apply_hp_damage(0U, 1U, 2, 11, 3, hp_random);
    OL_CHECK(fallback.has_value());
    OL_CHECK(fallback->damage == 14);
    OL_CHECK(hp_random.state() == 3'295'386'429U);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 86);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 2);

    actor.set_word(openlegend::model::role_word::mp, 10);
    actor.set_word(openlegend::model::role_word::maximum_mp, 20);
    target.set_word(openlegend::model::role_word::mp, 50);
    openlegend::random::LegacyRandom mp_random{1U};
    const auto drained = setup.apply_mp_damage(0U, 1U, 2, mp_random);
    OL_CHECK(drained == 15);
    OL_CHECK(mp_random.state() == 4'182'499'122U);
    OL_CHECK(actor.word(openlegend::model::role_word::mp) == 23);
    OL_CHECK(actor.word(openlegend::model::role_word::maximum_mp) == 23);
    OL_CHECK(target.word(openlegend::model::role_word::mp) == 35);
}

void run_attack_area_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& actor = ranger.roles[1U];
    auto& target = ranger.roles[3U];
    actor.set_word(openlegend::model::role_word::magic_id_begin + 2U, 5);
    actor.set_word(openlegend::model::role_word::magic_level_begin + 2U, 200);
    actor.set_word(openlegend::model::role_word::mp, 10);
    actor.set_word(openlegend::model::role_word::maximum_mp, 20);
    actor.set_word(openlegend::model::role_word::attack, 30);
    actor.set_word(openlegend::model::role_word::physical_power, 10);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::mp, 50);
    target.set_word(openlegend::model::role_word::defence, 5);
    target.set_word(openlegend::model::role_word::hurt, 0);
    target.set_word(openlegend::model::role_word::anti_poison, 100);
    auto& magic = ranger.magics[5U];
    magic.set_word(openlegend::model::magic_word::need_mp, 4);
    magic.set_word(openlegend::model::magic_word::attack_begin + 2U, 30);
    magic.set_word(openlegend::model::magic_word::select_distance_begin + 2U, 2);
    magic.set_word(openlegend::model::magic_word::attack_distance_begin + 2U, 1);
    magic.set_word(openlegend::model::magic_word::hurt_type, 0);
    magic.set_word(openlegend::model::magic_word::attack_area_type, 0);
    magic.set_word(openlegend::model::magic_word::add_mp_begin + 2U, 20);
    magic.set_word(openlegend::model::magic_word::hurt_mp_begin + 2U, 15);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    data.occupancy()[25U * 64U + 25U] = 0;
    setup.clear_attack_effects();
    openlegend::random::LegacyRandom random{1U};
    const auto square = setup.apply_attack_area(0U, 2, BattlePathCoord{26, 26}, 0, random);
    OL_CHECK(square.has_value());
    OL_CHECK(square->hit_count == 1);
    OL_CHECK(square->effect_kind == 1);
    OL_CHECK(std::ranges::count(setup.attack_effects(), static_cast<std::int16_t>(1)) == 8);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xe5f47b0a810ce2bdULL);
    OL_CHECK(setup.attack_effects()[25U * 64U + 25U] == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 29);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 71);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 2);
    OL_CHECK(setup.last_hp_cost_scale() == 3);
    OL_CHECK(random.state() == 2'524'885'223U);

    magic.set_word(openlegend::model::magic_word::attack_area_type, 2);
    magic.set_word(openlegend::model::magic_word::hurt_type, 1);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::hurt, 0);
    setup.combatants()[0U].words[combatant_word::initial_mode] = 2;
    setup.combatants()[0U].words[combatant_word::attack_counter] = 0;
    data.occupancy()[25U * 64U + 26U] = 0;
    setup.clear_attack_effects();
    random.seed(1U);
    const auto cross = setup.apply_attack_area(0U, 2, BattlePathCoord{0, 0}, 0, random);
    OL_CHECK(cross.has_value());
    OL_CHECK(cross->hit_count == 1);
    OL_CHECK(cross->effect_kind == 1);
    OL_CHECK(std::ranges::count(setup.attack_effects(), static_cast<std::int16_t>(1)) == 7);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0x3144c415023d9464ULL);
    OL_CHECK(setup.attack_effects()[25U * 64U + 26U] == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 2);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 29);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 71);
    OL_CHECK(target.word(openlegend::model::role_word::mp) == 50);
    OL_CHECK(random.state() == 2'524'885'223U);

    magic.set_word(openlegend::model::magic_word::attack_area_type, 3);
    magic.set_word(openlegend::model::magic_word::attack_distance_begin + 2U, 0);
    actor.set_word(openlegend::model::role_word::mp, 10);
    actor.set_word(openlegend::model::role_word::maximum_mp, 20);
    target.set_word(openlegend::model::role_word::mp, 50);
    setup.clear_attack_effects();
    random.seed(1U);
    const auto mp_square = setup.apply_attack_area(0U, 2, BattlePathCoord{26, 26}, 0, random);
    OL_CHECK(mp_square.has_value());
    OL_CHECK(mp_square->hit_count == 1);
    OL_CHECK(mp_square->effect_kind == 3);
    OL_CHECK(std::ranges::count(setup.attack_effects(), static_cast<std::int16_t>(1)) == 1);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xab559939923b4f74ULL);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 15);
    OL_CHECK(actor.word(openlegend::model::role_word::mp) == 23);
    OL_CHECK(actor.word(openlegend::model::role_word::maximum_mp) == 23);
    OL_CHECK(target.word(openlegend::model::role_word::mp) == 35);
    OL_CHECK(setup.last_hp_cost_scale() == 3);
    OL_CHECK(random.state() == 4'182'499'122U);

    magic.set_word(openlegend::model::magic_word::attack_area_type, 1);
    magic.set_word(openlegend::model::magic_word::select_distance_begin + 2U, 2);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::hurt, 0);
    setup.combatants()[0U].words[combatant_word::initial_mode] = 1;
    setup.combatants()[0U].words[combatant_word::attack_counter] = 0;
    data.occupancy()[25U * 64U + 26U] = -1;
    setup.clear_attack_effects();
    random.seed(1U);
    const auto line = setup.apply_line_attack_area(0U, 2, 3, 0, random);
    OL_CHECK(line.has_value());
    OL_CHECK(line->hit_count == 1);
    OL_CHECK(line->effect_kind == 1);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xae7c1e4e161ac125ULL);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 1);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 29);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 71);
    OL_CHECK(target.word(openlegend::model::role_word::mp) == 35);
    OL_CHECK(random.state() == 2'524'885'223U);

    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::hurt, 0);
    setup.combatants()[0U].words[combatant_word::attack_counter] = 0;
    data.occupancy()[25U * 64U + 26U] = 0;
    setup.clear_attack_effects();
    random.seed(1U);
    const auto friend_skipped = setup.apply_line_attack_area(0U, 2, 3, 0, random);
    OL_CHECK(friend_skipped.has_value());
    OL_CHECK(friend_skipped->hit_count == 1);
    OL_CHECK(friend_skipped->effect_kind == 1);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xab559939923b4f74ULL);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 71);
    OL_CHECK(random.state() == 2'524'885'223U);

    setup.clear_attack_effects();
    random.seed(1U);
    const auto invalid_direction = setup.apply_line_attack_area(0U, 2, 4, 0, random);
    OL_CHECK(invalid_direction.has_value());
    OL_CHECK(invalid_direction->hit_count == 0);
    OL_CHECK(!invalid_direction->effect_kind.has_value());
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xb9d103fd6854a325ULL);
    OL_CHECK(random.state() == 1U);
}

void run_party_selection_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    BattleData data{data_root, 0};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    OL_CHECK(setup.waiting_for_party_selection());
    OL_CHECK(setup.party_prefix_length() == 3U);
    OL_CHECK(setup.combatant_count() == 1);
    OL_CHECK(setup.selection_states()[0U] == 2);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::role_id] == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::sprite] == 5110);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::sprite] == 5098);

    OL_CHECK(setup.apply(PartySelectionAction::previous) == PartySelectionResult::changed);
    OL_CHECK(setup.cursor() == 3U);
    OL_CHECK(setup.apply(PartySelectionAction::next) == PartySelectionResult::changed);
    OL_CHECK(setup.cursor() == 0U);
    OL_CHECK(setup.apply(PartySelectionAction::next) == PartySelectionResult::changed);
    OL_CHECK(setup.apply(PartySelectionAction::activate) == PartySelectionResult::changed);
    OL_CHECK(setup.selection_states()[1U] == 1);
    OL_CHECK(setup.apply(PartySelectionAction::next) == PartySelectionResult::changed);
    OL_CHECK(setup.apply(PartySelectionAction::activate) == PartySelectionResult::changed);
    OL_CHECK(setup.selection_states()[2U] == 1);
    OL_CHECK(setup.apply(PartySelectionAction::next) == PartySelectionResult::changed);
    OL_CHECK(setup.apply(PartySelectionAction::activate) == PartySelectionResult::complete);

    OL_CHECK(!setup.waiting_for_party_selection());
    OL_CHECK(setup.combatant_count() == 4);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::role_id] == 2);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::x] == 36);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::y] == 17);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::sprite] == 5126);
    OL_CHECK(setup.combatants()[2U].words[combatant_word::role_id] == 3);
    OL_CHECK(setup.combatants()[2U].words[combatant_word::x] == 35);
    OL_CHECK(setup.combatants()[2U].words[combatant_word::y] == 20);
    OL_CHECK(setup.combatants()[3U].words[combatant_word::role_id] == 1);
    OL_CHECK(setup.combatants()[3U].words[combatant_word::sprite] == 5116);
    OL_CHECK(data.occupancy()[20U * 64U + 32U] == 0);
    OL_CHECK(data.occupancy()[17U * 64U + 36U] == 1);
    OL_CHECK(data.occupancy()[20U * 64U + 35U] == 2);
    OL_CHECK(data.occupancy()[23U * 64U + 21U] == 3);
}

void run_fixed_and_duplicate_tests(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    BattleData fixed_data{data_root, 4};
    BattleSetup fixed{fixed_data, ranger};
    OL_CHECK(fixed.valid());
    OL_CHECK(!fixed.waiting_for_party_selection());
    OL_CHECK(fixed.combatant_count() == 2);
    OL_CHECK(fixed.combatants()[0U].words[combatant_word::role_id] == 1);
    OL_CHECK(fixed.combatants()[0U].words[combatant_word::side] == 0);
    OL_CHECK(fixed.combatants()[0U].words[combatant_word::initial_mode] == 2);
    OL_CHECK(fixed.combatants()[1U].words[combatant_word::role_id] == 3);
    OL_CHECK(fixed.combatants()[1U].words[combatant_word::side] == 1);
    OL_CHECK(fixed_data.occupancy()[24U * 64U + 26U] == 0);
    OL_CHECK(fixed_data.occupancy()[26U * 64U + 26U] == 1);

    BattleData duplicate_data{data_root, 93};
    auto single_ranger = make_ranger({0, -1, -1, -1, -1, -1});
    BattleSetup duplicate{duplicate_data, single_ranger};
    OL_CHECK(duplicate.valid());
    OL_CHECK(duplicate.waiting_for_party_selection());
    OL_CHECK(duplicate.apply(PartySelectionAction::next) == PartySelectionResult::changed);
    OL_CHECK(duplicate.apply(PartySelectionAction::activate) == PartySelectionResult::complete);
    OL_CHECK(duplicate.combatant_count() == 17);
    OL_CHECK(duplicate.combatants()[9U].words[combatant_word::role_id] == 286);
    OL_CHECK(duplicate.combatants()[11U].words[combatant_word::role_id] == 288);
    OL_CHECK(duplicate_data.occupancy()[34U * 64U + 13U] == 11);
}

void run_turn_order_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    ranger.roles[1U].set_word(openlegend::model::role_word::speed, 10);
    ranger.roles[1U].set_word(openlegend::model::role_word::equipment_begin, 5);
    ranger.items[5U].set_word(openlegend::model::item_word::add_speed, 30);
    ranger.roles[3U].set_word(openlegend::model::role_word::speed, 40);
    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    OL_CHECK(setup.sort_by_effective_speed());
    OL_CHECK(setup.combatants()[0U].words[combatant_word::role_id] == 1);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::role_id] == 3);

    ranger.roles[3U].set_word(openlegend::model::role_word::speed, 41);
    OL_CHECK(setup.sort_by_effective_speed());
    OL_CHECK(setup.combatants()[0U].words[combatant_word::role_id] == 3);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::role_id] == 1);
    OL_CHECK(data.occupancy()[26U * 64U + 26U] == 0);
    OL_CHECK(data.occupancy()[24U * 64U + 26U] == 1);

    setup.combatants()[0U].words[combatant_word::occupancy_hidden] = 1;
    ranger.roles[1U].set_word(openlegend::model::role_word::speed, 20);
    ranger.roles[1U].set_word(openlegend::model::role_word::hurt, 40);
    ranger.roles[3U].set_word(openlegend::model::role_word::hurt, 200);
    OL_CHECK(setup.prepare_round());
    OL_CHECK(setup.combatants()[0U].words[combatant_word::role_id] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::round_value] == 2);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::sprite] == 5118);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::role_id] == 3);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::round_value] == 0);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::sprite] == 5132);
    OL_CHECK(data.occupancy()[24U * 64U + 26U] == 0);
    OL_CHECK(data.occupancy()[26U * 64U + 26U] == -1);
}

void run_outcome_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        ranger.roles[1U].set_word(openlegend::model::role_word::hp, 1);
        ranger.roles[3U].set_word(openlegend::model::role_word::hp, 1);
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.evaluate_outcome() == BattleOutcome::ongoing);
        ranger.roles[3U].set_word(openlegend::model::role_word::hp, 0);
        OL_CHECK(setup.evaluate_outcome() == BattleOutcome::victory);
        OL_CHECK(setup.combatants()[1U].words[combatant_word::occupancy_hidden] == 1);
        OL_CHECK(data.occupancy()[26U * 64U + 26U] == -1);
    }
    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        ranger.roles[1U].set_word(openlegend::model::role_word::hp, 0);
        ranger.roles[3U].set_word(openlegend::model::role_word::hp, 1);
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.evaluate_outcome() == BattleOutcome::defeat);
    }
    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        ranger.roles[1U].set_word(openlegend::model::role_word::hp, 0);
        ranger.roles[3U].set_word(openlegend::model::role_word::hp, 0);
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.evaluate_outcome() == BattleOutcome::victory);
    }
}

void run_all_definition_tests(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 1, 2, 3, 4, 5});
    for (std::int16_t battle_id = 0; battle_id < 140; ++battle_id) {
        BattleData data{data_root, battle_id};
        OL_CHECK(data.valid());
        OL_CHECK(data.definition().size() == kBattleDefinitionWords);
        OL_CHECK(data.battlefield().size() == kBattlefieldWords);
        OL_CHECK(data.occupancy().size() == kBattleOccupancyCells);
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.valid());
        if (setup.waiting_for_party_selection()) {
            for (std::size_t step = 0U; step < setup.party_prefix_length(); ++step) {
                OL_CHECK(setup.apply(PartySelectionAction::next) == PartySelectionResult::changed);
            }
            OL_CHECK(setup.apply(PartySelectionAction::activate) == PartySelectionResult::complete);
        }
        OL_CHECK(setup.valid());
        OL_CHECK(!setup.waiting_for_party_selection());
        OL_CHECK(setup.combatant_count() > 0);
        OL_CHECK(setup.combatant_count() <= static_cast<std::int16_t>(kBattleCombatantCount));
    }

    const BattleData negative{data_root, -1};
    const BattleData past_end{data_root, 140};
    OL_CHECK(!negative.valid());
    OL_CHECK(!past_end.valid());
}

}  // namespace

int main() {
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
    OL_CHECK(std::filesystem::is_directory(root));
    const openlegend::resource::DataRoot data_root{root};
    run_real_asset_fixtures(data_root);
    run_pathing_tests(data_root);
    run_movement_step_test(data_root);
    run_attack_profile_test(data_root);
    run_attack_animation_test(data_root);
    run_poison_action_test(data_root);
    run_damage_formula_test(data_root);
    run_attack_area_test(data_root);
    run_party_selection_test(data_root);
    run_fixed_and_duplicate_tests(data_root);
    run_turn_order_test(data_root);
    run_outcome_test(data_root);
    run_all_definition_tests(data_root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
