#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iterator>
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

namespace item_word = openlegend::model::item_word;
namespace magic_word = openlegend::model::magic_word;
namespace role_word = openlegend::model::role_word;

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

std::uint64_t fnv1a_render_plan(const openlegend::battle::BattleRenderPlan& plan) {
    std::vector<std::int16_t> words;
    words.reserve(plan.commands.size() * 9U);
    for (const auto& command : plan.commands) {
        words.insert(
            words.end(),
            {
                static_cast<std::int16_t>(command.kind),
                command.map_x,
                command.map_y,
                static_cast<std::int16_t>(command.screen_x),
                static_cast<std::int16_t>(command.screen_y),
                static_cast<std::int16_t>(command.sprite_id),
                command.overlay_variant,
                command.style,
                command.value,
            });
    }
    return fnv1a_words(words);
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

void run_detox_action_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& actor = ranger.roles[1U];
    auto& target = ranger.roles[3U];
    actor.set_word(openlegend::model::role_word::detoxification, 80);
    actor.set_word(openlegend::model::role_word::physical_power, 1);
    target.set_word(openlegend::model::role_word::poison, 90);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    setup.combatants()[1U].words[combatant_word::side] =
        setup.combatants()[0U].words[combatant_word::side];
    OL_CHECK(setup.detox_targeting_range(0U) == 6);
    openlegend::random::LegacyRandom random{1U};
    const auto result = setup.apply_detox_target(0U, BattlePathCoord{26, 26}, random);
    OL_CHECK(result.has_value());
    OL_CHECK(result->hit_count == 1);
    OL_CHECK(result->effect_kind == 3);
    OL_CHECK(random.state() == 2'524'885'223U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xab559939923b4f74ULL);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 26);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 64);
    OL_CHECK(setup.finish_detox_action(0U));
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 1);
    OL_CHECK(actor.word(openlegend::model::role_word::physical_power) == 0);

    actor.set_word(openlegend::model::role_word::detoxification, 20);
    target.set_word(openlegend::model::role_word::poison, 41);
    random.seed(1U);
    OL_CHECK(setup.apply_detox_value(0U, 1U, random) == 0);
    OL_CHECK(random.state() == 2'524'885'223U);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 41);
    actor.set_word(openlegend::model::role_word::detoxification, 500);
    target.set_word(openlegend::model::role_word::poison, 99);
    random.seed(1U);
    OL_CHECK(setup.apply_detox_value(0U, 1U, random) == 99);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 0);
    actor.set_word(openlegend::model::role_word::detoxification, 0);
    target.set_word(openlegend::model::role_word::poison, 100);
    random.seed(1U);
    OL_CHECK(setup.apply_detox_value(0U, 1U, random) == 0);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 100);
    target.set_word(openlegend::model::role_word::poison, 101);
    random.seed(1U);
    OL_CHECK(setup.apply_detox_value(0U, 1U, random) == 0);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 99);

    data.occupancy()[26U * 64U + 26U] = -1;
    random.seed(1U);
    const auto empty = setup.apply_detox_target(0U, BattlePathCoord{25, 24}, random);
    OL_CHECK(empty.has_value());
    OL_CHECK(empty->hit_count == 0);
    OL_CHECK(!empty->effect_kind.has_value());
    OL_CHECK(random.state() == 1U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 2);
    OL_CHECK(std::ranges::count(setup.attack_effects(), 1) == 1);

    data.occupancy()[26U * 64U + 26U] = 1;
    setup.combatants()[1U].words[combatant_word::side] = 1;
    random.seed(1U);
    const auto enemy = setup.apply_detox_target(0U, BattlePathCoord{26, 26}, random);
    OL_CHECK(enemy.has_value());
    OL_CHECK(enemy->hit_count == 0);
    OL_CHECK(!enemy->effect_kind.has_value());
    OL_CHECK(random.state() == 1U);
    OL_CHECK(std::ranges::none_of(setup.attack_effects(), [](const std::int16_t value) {
        return value != 0;
    }));
}

void run_medicine_action_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& actor = ranger.roles[1U];
    auto& target = ranger.roles[3U];
    actor.set_word(openlegend::model::role_word::medicine, 80);
    actor.set_word(openlegend::model::role_word::physical_power, 51);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::maximum_hp, 200);
    target.set_word(openlegend::model::role_word::hurt, 40);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    setup.combatants()[1U].words[combatant_word::side] =
        setup.combatants()[0U].words[combatant_word::side];
    OL_CHECK(setup.medicine_targeting_range(0U) == 6);
    openlegend::random::LegacyRandom random{1U};
    const auto result = setup.apply_medicine_target(0U, BattlePathCoord{26, 26}, random);
    OL_CHECK(result.has_value());
    OL_CHECK(result->hit_count == 1);
    OL_CHECK(result->effect_kind == 4);
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xab559939923b4f74ULL);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 63);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 163);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 0);
    OL_CHECK(actor.word(openlegend::model::role_word::physical_power) == 49);
    OL_CHECK(setup.finish_medicine_action(0U));
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 1);
    OL_CHECK(actor.word(openlegend::model::role_word::physical_power) == 47);

    actor.set_word(openlegend::model::role_word::medicine, 20);
    actor.set_word(openlegend::model::role_word::physical_power, 60);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::maximum_hp, 200);
    target.set_word(openlegend::model::role_word::hurt, 41);
    random.seed(1U);
    OL_CHECK(setup.apply_medicine_value(0U, 1U, random) == 0);
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 100);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 41);
    OL_CHECK(actor.word(openlegend::model::role_word::physical_power) == 58);

    actor.set_word(openlegend::model::role_word::medicine, 80);
    actor.set_word(openlegend::model::role_word::physical_power, 49);
    random.seed(1U);
    OL_CHECK(setup.apply_medicine_value(0U, 1U, random) == 0);
    OL_CHECK(random.state() == 1U);
    OL_CHECK(actor.word(openlegend::model::role_word::physical_power) == 49);

    constexpr std::array<std::pair<std::int16_t, std::int32_t>, 4> kHurtBands{{
        {25, 67},
        {26, 63},
        {51, 56},
        {76, 43},
    }};
    for (const auto [hurt, expected] : kHurtBands) {
        actor.set_word(openlegend::model::role_word::physical_power, 60);
        target.set_word(openlegend::model::role_word::hp, 0);
        target.set_word(openlegend::model::role_word::maximum_hp, 1'000);
        target.set_word(openlegend::model::role_word::hurt, hurt);
        random.seed(1U);
        OL_CHECK(setup.apply_medicine_value(0U, 1U, random) == expected);
    }

    actor.set_word(openlegend::model::role_word::physical_power, 60);
    target.set_word(openlegend::model::role_word::hp, 190);
    target.set_word(openlegend::model::role_word::maximum_hp, 200);
    target.set_word(openlegend::model::role_word::hurt, 40);
    random.seed(1U);
    OL_CHECK(setup.apply_medicine_value(0U, 1U, random) == 10);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 200);

    data.occupancy()[26U * 64U + 26U] = -1;
    random.seed(1U);
    const auto empty = setup.apply_medicine_target(0U, BattlePathCoord{25, 24}, random);
    OL_CHECK(empty.has_value());
    OL_CHECK(empty->hit_count == 0);
    OL_CHECK(!empty->effect_kind.has_value());
    OL_CHECK(random.state() == 1U);
    OL_CHECK(std::ranges::count(setup.attack_effects(), 1) == 1);

    data.occupancy()[26U * 64U + 26U] = 1;
    setup.combatants()[1U].words[combatant_word::side] = 1;
    random.seed(1U);
    const auto enemy = setup.apply_medicine_target(0U, BattlePathCoord{26, 26}, random);
    OL_CHECK(enemy.has_value());
    OL_CHECK(enemy->hit_count == 0);
    OL_CHECK(!enemy->effect_kind.has_value());
    OL_CHECK(random.state() == 1U);
    OL_CHECK(std::ranges::none_of(setup.attack_effects(), [](const std::int16_t value) {
        return value != 0;
    }));
}

void run_throwing_weapon_action_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
    }
    auto& actor = ranger.roles[1U];
    auto& target = ranger.roles[3U];
    actor.set_word(openlegend::model::role_word::hidden_weapon, 20);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::maximum_hp, 200);
    target.set_word(openlegend::model::role_word::hurt, 40);
    target.set_word(openlegend::model::role_word::poison, 10);
    target.set_word(openlegend::model::role_word::anti_poison, 5);

    auto& poisoned = ranger.items[102U];
    poisoned.set_word(openlegend::model::item_word::item_type, 4);
    poisoned.set_word(openlegend::model::item_word::hidden_weapon_effect_id, 30);
    poisoned.set_word(openlegend::model::item_word::add_hp, -40);
    poisoned.set_word(openlegend::model::item_word::add_poison, 40);
    ranger.items[97U].set_word(openlegend::model::item_word::item_type, 4);
    ranger.items[10U].set_word(openlegend::model::item_word::item_type, 3);
    ranger.items[11U].set_word(openlegend::model::item_word::item_type, 2);
    ranger.header.set_inventory(0U, openlegend::model::ItemId{102}, 1);
    ranger.header.set_inventory(1U, openlegend::model::ItemId{97}, 2);
    ranger.header.set_inventory(2U, openlegend::model::ItemId{10}, 0);
    ranger.header.set_inventory(3U, openlegend::model::ItemId{11}, 3);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    const auto selection = setup.begin_item_selection();
    OL_CHECK(selection.count == 3);
    OL_CHECK(selection.inventory_slots[0U] == 0);
    OL_CHECK(selection.inventory_slots[1U] == 1);
    OL_CHECK(selection.inventory_slots[2U] == 2);
    OL_CHECK(selection.inventory_slots[3U] == -1);
    OL_CHECK(setup.throwing_weapon_targeting_range(0U) == 2);

    openlegend::random::LegacyRandom random{1U};
    const auto result =
        setup.apply_throwing_weapon_target(0U, BattlePathCoord{26, 26}, 0U, random);
    OL_CHECK(result.has_value());
    OL_CHECK(result->hit_count == 1);
    OL_CHECK(result->effect_id == 30);
    OL_CHECK(result->damage == 21);
    OL_CHECK(result->inventory_consumed);
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xab559939923b4f74ULL);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 79);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 45);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 12);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 21);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 0);
    OL_CHECK(ranger.header.inventory_item(0U).value == 97);
    OL_CHECK(ranger.header.inventory_count(0U) == 2);
    OL_CHECK(ranger.header.inventory_item(1U).value == 10);
    OL_CHECK(ranger.header.inventory_count(1U) == 0);

    auto& plain = ranger.items[96U];
    plain.set_word(openlegend::model::item_word::item_type, 4);
    plain.set_word(openlegend::model::item_word::hidden_weapon_effect_id, 23);
    plain.set_word(openlegend::model::item_word::add_hp, -30);
    plain.set_word(openlegend::model::item_word::add_poison, 0);
    ranger.header.set_inventory(0U, openlegend::model::ItemId{96}, 2);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::hurt, 0);
    target.set_word(openlegend::model::role_word::poison, 10);
    target.set_word(openlegend::model::role_word::anti_poison, 0);
    setup.combatants()[0U].words[combatant_word::action_done] = 0;
    random.seed(2U);
    const auto plain_result =
        setup.apply_throwing_weapon_target(0U, BattlePathCoord{26, 26}, 0U, random);
    OL_CHECK(plain_result.has_value());
    OL_CHECK(plain_result->damage == 16);
    OL_CHECK(random.state() == 2'818'548'041U);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 84);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 4);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 8);
    OL_CHECK(ranger.header.inventory_count(0U) == 1);

    setup.combatants()[0U].words[combatant_word::action_done] = 0;
    setup.combatants()[1U].words[combatant_word::side] =
        setup.combatants()[0U].words[combatant_word::side];
    random.seed(1U);
    const auto friendly =
        setup.apply_throwing_weapon_target(0U, BattlePathCoord{26, 26}, 0U, random);
    OL_CHECK(friendly.has_value());
    OL_CHECK(friendly->hit_count == 0);
    OL_CHECK(!friendly->effect_id.has_value());
    OL_CHECK(random.state() == 1U);
    OL_CHECK(ranger.header.inventory_count(0U) == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);
    OL_CHECK(std::ranges::none_of(setup.attack_effects(), [](const std::int16_t value) {
        return value != 0;
    }));

    setup.combatants()[1U].words[combatant_word::side] = 1;
    data.occupancy()[26U * 64U + 26U] = -1;
    random.seed(1U);
    const auto empty =
        setup.apply_throwing_weapon_target(0U, BattlePathCoord{26, 26}, 0U, random);
    OL_CHECK(empty.has_value());
    OL_CHECK(empty->hit_count == 0);
    OL_CHECK(!empty->effect_id.has_value());
    OL_CHECK(random.state() == 1U);
    OL_CHECK(ranger.header.inventory_count(0U) == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);
    OL_CHECK(std::ranges::count(setup.attack_effects(), 1) == 1);
}

void run_rest_action_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& actor = ranger.roles[1U];
    actor.set_word(openlegend::model::role_word::speed, 60);
    actor.set_word(openlegend::model::role_word::physical_power, 50);
    actor.set_word(openlegend::model::role_word::hp, 95);
    actor.set_word(openlegend::model::role_word::maximum_hp, 100);
    actor.set_word(openlegend::model::role_word::mp, 48);
    actor.set_word(openlegend::model::role_word::maximum_mp, 50);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    setup.combatants()[0U].words[combatant_word::round_value] = 6;
    openlegend::random::LegacyRandom random{1U};
    const auto rested = setup.rest_actor(0U, random);
    OL_CHECK(rested.has_value());
    OL_CHECK(rested->physical_power == 55);
    OL_CHECK(rested->hp == 99);
    OL_CHECK(rested->mp == 50);
    OL_CHECK(random.state() == 662'824'084U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);

    actor.set_word(openlegend::model::role_word::physical_power, 25);
    actor.set_word(openlegend::model::role_word::hp, 95);
    actor.set_word(openlegend::model::role_word::mp, 48);
    setup.combatants()[0U].words[combatant_word::round_value] = 5;
    setup.combatants()[0U].words[combatant_word::action_done] = 0;
    random.seed(1U);
    const auto tired = setup.rest_actor(0U, random);
    OL_CHECK(tired.has_value());
    OL_CHECK(tired->physical_power == 29);
    OL_CHECK(tired->hp == 95);
    OL_CHECK(tired->mp == 48);
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);
}

void run_wait_auto_render_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto wait_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    BattleData wait_data{data_root, 3};
    BattleSetup wait_setup{wait_data, wait_ranger};
    OL_CHECK(wait_setup.valid());
    OL_CHECK(wait_setup.apply(PartySelectionAction::previous) == PartySelectionResult::changed);
    OL_CHECK(wait_setup.apply(PartySelectionAction::activate) == PartySelectionResult::complete);
    OL_CHECK(wait_setup.combatant_count() == 5);
    constexpr std::array<std::int16_t, 5> kBefore{0, 101, 102, 103, 104};
    for (std::size_t slot = 0U; slot < kBefore.size(); ++slot) {
        OL_CHECK(wait_setup.combatants()[slot].words[combatant_word::role_id] == kBefore[slot]);
    }
    OL_CHECK(wait_setup.defer_turn_to_end(1U) == 4U);
    constexpr std::array<std::int16_t, 5> kAfter{0, 102, 103, 104, 101};
    for (std::size_t slot = 0U; slot < kAfter.size(); ++slot) {
        OL_CHECK(wait_setup.combatants()[slot].words[combatant_word::role_id] == kAfter[slot]);
    }
    OL_CHECK(wait_setup.combatants()[4U].words[combatant_word::action_done] == 0);
    OL_CHECK(!wait_setup.automatic_enabled());
    wait_setup.enable_automatic_mode();
    OL_CHECK(wait_setup.automatic_enabled());

    auto render_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    render_ranger.roles[1U].set_word(openlegend::model::role_word::use_poison, 80);
    render_ranger.roles[3U].set_word(openlegend::model::role_word::anti_poison, 0);
    render_ranger.roles[3U].set_word(openlegend::model::role_word::poison, 0);
    BattleData render_data{data_root, 4};
    BattleSetup render_setup{render_data, render_ranger};
    OL_CHECK(render_setup.valid());
    const auto marked = render_setup.apply_poison_target(0U, BattlePathCoord{26, 26});
    OL_CHECK(marked.has_value());
    OL_CHECK(marked->hit_count == 1);
    render_setup.combatants()[1U].words[combatant_word::damage_value] = 17;

    std::array<std::int16_t, kBattleOccupancyCells> path_values{};
    path_values[25U * 64U + 25U] = 10;
    path_values[24U * 64U + 24U] = kBattlePathBlocked;
    const BattleRenderState state{
        .view_x = 15,
        .view_y = 17,
        .path_limit = 5,
        .primary_cursor = {27, 26},
        .primary_cursor_alternate = false,
        .secondary_cursor_visible = true,
        .secondary_cursor = {25, 27},
        .highlight_enabled = true,
        .highlight_mode = 2,
        .effect_visible = true,
        .effect_id = 2,
        .effect_frame_offset = 4,
        .damage_kind = 3,
        .damage_text_offset = 2,
    };
    const auto plan = render_setup.battle_render_plan(state, path_values);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->commands.size() == 1'157U);
    OL_CHECK(fnv1a_render_plan(*plan) == 0xb9f8a428699b3712ULL);
    OL_CHECK(std::ranges::count_if(plan->commands, [](const BattleRenderCommand& command) {
        return command.kind == BattleRenderCommandKind::legacy_sprite;
    }) == 1'152);
    OL_CHECK(std::ranges::count_if(plan->commands, [](const BattleRenderCommand& command) {
        return command.kind == BattleRenderCommandKind::cursor_overlay;
    }) == 3);
    OL_CHECK(std::ranges::count_if(plan->commands, [](const BattleRenderCommand& command) {
        return command.kind == BattleRenderCommandKind::highlighted_sprite;
    }) == 1);
    OL_CHECK(std::ranges::count_if(plan->commands, [](const BattleRenderCommand& command) {
        return command.kind == BattleRenderCommandKind::damage_text;
    }) == 1);

    std::vector<BattleRenderCommand> cursor_commands;
    std::ranges::copy_if(
        plan->commands,
        std::back_inserter(cursor_commands),
        [](const BattleRenderCommand& command) {
            return command.kind == BattleRenderCommandKind::cursor_overlay;
        });
    OL_CHECK(cursor_commands.size() == 3U);
    OL_CHECK(cursor_commands[0U].map_x == 25 && cursor_commands[0U].map_y == 25);
    OL_CHECK(cursor_commands[0U].screen_x == 163 && cursor_commands[0U].screen_y == 81);
    OL_CHECK(cursor_commands[0U].overlay_variant == 0 && cursor_commands[0U].style == 3);
    OL_CHECK(cursor_commands[1U].map_x == 25 && cursor_commands[1U].map_y == 27);
    OL_CHECK(cursor_commands[1U].screen_x == 127 && cursor_commands[1U].screen_y == 99);
    OL_CHECK(cursor_commands[1U].overlay_variant == 1 && cursor_commands[1U].style == 3);
    OL_CHECK(cursor_commands[2U].map_x == 27 && cursor_commands[2U].map_y == 26);
    OL_CHECK(cursor_commands[2U].screen_x == 181 && cursor_commands[2U].screen_y == 108);
    OL_CHECK(cursor_commands[2U].overlay_variant == 0 && cursor_commands[2U].style == 2);

    std::vector<BattleRenderCommand> target_commands;
    std::ranges::copy_if(
        plan->commands,
        std::back_inserter(target_commands),
        [](const BattleRenderCommand& command) {
            return command.map_x == 26 && command.map_y == 26;
        });
    OL_CHECK(target_commands.size() == 4U);
    OL_CHECK(target_commands[0U].kind == BattleRenderCommandKind::legacy_sprite);
    OL_CHECK(target_commands[0U].sprite_id == 8);
    OL_CHECK(target_commands[1U].kind == BattleRenderCommandKind::highlighted_sprite);
    OL_CHECK(target_commands[1U].sprite_id == 5'132);
    OL_CHECK(target_commands[1U].style == 47);
    OL_CHECK(target_commands[2U].kind == BattleRenderCommandKind::legacy_sprite);
    OL_CHECK(target_commands[2U].sprite_id == 8);
    OL_CHECK(target_commands[3U].kind == BattleRenderCommandKind::damage_text);
    OL_CHECK(target_commands[3U].screen_x == 163);
    OL_CHECK(target_commands[3U].screen_y == 35);
    OL_CHECK(target_commands[3U].overlay_variant == 1);
    OL_CHECK(target_commands[3U].style == static_cast<std::int16_t>(0x9193U));
    OL_CHECK(target_commands[3U].value == 17);

    auto no_range_state = state;
    no_range_state.path_limit = 0;
    no_range_state.secondary_cursor_visible = false;
    const auto no_range_plan = render_setup.battle_render_plan(no_range_state, {});
    OL_CHECK(no_range_plan.has_value());
    OL_CHECK(no_range_plan->commands.size() == 1'154U);
    OL_CHECK(std::ranges::none_of(no_range_plan->commands, [](const BattleRenderCommand& command) {
        return command.kind == BattleRenderCommandKind::cursor_overlay;
    }));
}

void run_ai_selector_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;

    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    BattleData data{data_root, 3};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    OL_CHECK(setup.apply(PartySelectionAction::previous) == PartySelectionResult::changed);
    OL_CHECK(setup.apply(PartySelectionAction::activate) == PartySelectionResult::complete);
    OL_CHECK(setup.combatant_count() == 5);

    const auto reset = [&]() {
        for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
            ranger.header.set_inventory(
                slot, openlegend::model::ItemId{-1}, static_cast<std::int16_t>(0));
        }
        for (std::size_t item = 0U; item < 8U; ++item) {
            ranger.items[item].set_word(item_word::add_hp, 0);
            ranger.items[item].set_word(item_word::add_poison, 0);
            ranger.items[item].set_word(item_word::add_mp, 0);
            ranger.items[item].set_word(item_word::add_use_poison, 0);
        }
        for (std::size_t role = 0U; role < 5U; ++role) {
            auto& record = ranger.roles[role];
            record.set_word(role_word::hp, 100);
            record.set_word(role_word::maximum_hp, 100);
            record.set_word(role_word::hurt, 0);
            record.set_word(role_word::poison, 0);
            record.set_word(role_word::physical_power, 100);
            record.set_word(role_word::mp, 0);
            record.set_word(role_word::maximum_mp, 0);
            record.set_word(role_word::attack, 10);
            record.set_word(role_word::medicine, 0);
            record.set_word(role_word::use_poison, 0);
            record.set_word(role_word::detoxification, 0);
            record.set_word(role_word::hidden_weapon, 0);
            for (std::size_t slot = 0U; slot < role_word::magic_count; ++slot) {
                record.set_word(role_word::magic_id_begin + slot, 0);
            }
            for (std::size_t slot = 0U; slot < role_word::taking_item_count; ++slot) {
                record.set_word(role_word::taking_item_begin + slot, -1);
            }
            auto& combatant = setup.combatants()[role].words;
            combatant[combatant_word::role_id] = static_cast<std::int16_t>(role);
            combatant[combatant_word::side] = role < 3U ? 0 : 1;
            combatant[combatant_word::x] = static_cast<std::int16_t>(10 + role);
            combatant[combatant_word::y] = static_cast<std::int16_t>(20 + role);
            combatant[combatant_word::occupancy_hidden] = 0;
            combatant[combatant_word::ai_action] = -1;
        }
    };

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 21);
    ranger.roles[0U].set_word(role_word::hurt, 50);
    auto choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::medicine);
    OL_CHECK(choice->target_slot == 0);
    OL_CHECK((choice->target == BattlePathCoord{10, 20}));
    OL_CHECK(setup.combatants()[0U].words[combatant_word::ai_action] == 5);

    reset();
    ranger.items[5U].set_word(item_word::add_hp, 1);
    ranger.header.set_inventory(2U, openlegend::model::ItemId{5}, 0);
    choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::item);
    OL_CHECK(choice->item_source == BattleAiItemSource::inventory);
    OL_CHECK(choice->item_slot == 2);
    OL_CHECK(choice->target_slot == 0);

    reset();
    ranger.roles[0U].set_word(role_word::hurt, 80);
    ranger.roles[1U].set_word(role_word::medicine, 51);
    choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::request_medicine);
    OL_CHECK(choice->target_slot == 1);
    OL_CHECK((choice->target == BattlePathCoord{11, 21}));

    reset();
    ranger.roles[0U].set_word(role_word::detoxification, 22);
    ranger.roles[0U].set_word(role_word::poison, 51);
    ranger.roles[0U].set_word(role_word::physical_power, 51);
    choice = setup.choose_ai_poisoned_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::detox);
    OL_CHECK(choice->target_slot == 0);

    reset();
    ranger.items[6U].set_word(item_word::add_poison, -1);
    ranger.header.set_inventory(1U, openlegend::model::ItemId{6}, 0);
    choice = setup.choose_ai_poisoned_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::none);
    ranger.items[6U].set_word(item_word::add_use_poison, -1);
    choice = setup.choose_ai_poisoned_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::item);
    OL_CHECK(choice->item_source == BattleAiItemSource::inventory);
    OL_CHECK(choice->item_slot == 1);

    reset();
    setup.combatants()[0U].words[combatant_word::side] = 1;
    ranger.roles[0U].set_word(role_word::taking_item_begin + 2U, 6);
    ranger.items[6U].set_word(item_word::add_poison, -1);
    choice = setup.choose_ai_poisoned_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::item);
    OL_CHECK(choice->item_source == BattleAiItemSource::carried);
    OL_CHECK(choice->item_slot == 2);

    reset();
    ranger.items[7U].set_word(item_word::add_mp, 1);
    ranger.header.set_inventory(3U, openlegend::model::ItemId{7}, 0);
    choice = setup.choose_ai_low_mp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::item);
    OL_CHECK(choice->item_slot == 3);

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 80);
    ranger.roles[1U].set_word(role_word::hp, 24);
    ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom medicine_random{1U};
    choice = setup.choose_ai_medicine_target(0U, medicine_random);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::medicine);
    OL_CHECK(choice->target_slot == 1);
    OL_CHECK(medicine_random.state() == 662'824'084U);

    reset();
    ranger.roles[0U].set_word(role_word::detoxification, 80);
    ranger.roles[1U].set_word(role_word::poison, 35);
    openlegend::random::LegacyRandom detox_random{1U};
    choice = setup.choose_ai_detox_target(0U, detox_random);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::detox);
    OL_CHECK(choice->target_slot == 1);
    OL_CHECK(detox_random.state() == 662'824'084U);

    reset();
    ranger.roles[0U].set_word(role_word::hp, 1);
    ranger.roles[0U].set_word(role_word::attack, 1);
    ranger.roles[0U].set_word(role_word::medicine, 20);
    ranger.roles[0U].set_word(role_word::physical_power, 50);
    ranger.roles[1U].set_word(role_word::hp, 500);
    ranger.roles[1U].set_word(role_word::maximum_hp, 600);
    ranger.roles[1U].set_word(role_word::attack, 0);
    ranger.roles[2U].set_word(role_word::hp, 500);
    ranger.roles[2U].set_word(role_word::maximum_hp, 800);
    ranger.roles[2U].set_word(role_word::attack, 0);
    ranger.roles[3U].set_word(role_word::hp, 100);
    ranger.roles[3U].set_word(role_word::attack, 100);
    ranger.roles[4U].set_word(role_word::hp, 100);
    ranger.roles[4U].set_word(role_word::attack, 100);
    openlegend::random::LegacyRandom aid_random{1U};
    choice = setup.choose_ai_offensive_action(0U, aid_random);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::medicine);
    OL_CHECK(choice->target_slot == 2);
    OL_CHECK(aid_random.state() == 1U);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 100);
    openlegend::random::LegacyRandom poison_random{1U};
    choice = setup.choose_ai_offensive_action(0U, poison_random);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::use_poison);
    OL_CHECK(choice->action_code_written);
    OL_CHECK(poison_random.state() == 2'524'885'223U);

    reset();
    ranger.roles[0U].set_word(role_word::hidden_weapon, 100);
    ranger.items[5U].set_word(item_word::add_hp, -100);
    ranger.header.set_inventory(4U, openlegend::model::ItemId{5}, 0);
    openlegend::random::LegacyRandom throwing_random{1U};
    choice = setup.choose_ai_offensive_action(0U, throwing_random);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::throwing_weapon);
    OL_CHECK(choice->item_source == BattleAiItemSource::inventory);
    OL_CHECK(choice->item_slot == 4);
    OL_CHECK(throwing_random.state() == 2'524'885'223U);

    reset();
    ranger.roles[0U].set_word(role_word::physical_power, 100);
    ranger.roles[0U].set_word(role_word::mp, 5);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::need_mp, 5);
    setup.combatants()[0U].words[combatant_word::ai_action] = 77;
    openlegend::random::LegacyRandom attack_random{1U};
    choice = setup.choose_ai_offensive_action(0U, attack_random);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::attack);
    OL_CHECK(!choice->action_code_written);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::ai_action] == 77);
    OL_CHECK(attack_random.state() == 1'103'527'590U);
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
    run_detox_action_test(data_root);
    run_medicine_action_test(data_root);
    run_throwing_weapon_action_test(data_root);
    run_rest_action_test(data_root);
    run_wait_auto_render_test(data_root);
    run_ai_selector_test(data_root);
    run_damage_formula_test(data_root);
    run_attack_area_test(data_root);
    run_party_selection_test(data_root);
    run_fixed_and_duplicate_tests(data_root);
    run_turn_order_test(data_root);
    run_outcome_test(data_root);
    run_all_definition_tests(data_root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
