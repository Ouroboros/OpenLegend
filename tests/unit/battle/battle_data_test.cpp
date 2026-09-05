#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_pathing.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/battle/battle_session.hpp"
#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/diagnostics/log.hpp"
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

std::uint64_t fnv1a_bytes(const std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const auto byte : bytes) {
        hash ^= byte;
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

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

void finish_player_menu_redraw(openlegend::battle::BattleSession& session) {
    using openlegend::battle::BattleSessionPhase;
    if (session.phase() != BattleSessionPhase::player_action_initial_present &&
        session.phase() != BattleSessionPhase::player_action_return_present) {
        return;
    }
    openlegend::render::IndexedFramebuffer frame;
    OL_CHECK(session.render(frame));
    session.finish_presented_tick();
    if (session.phase() == BattleSessionPhase::player_action) {
        OL_CHECK(session.render(frame));
        session.finish_presented_tick();
    }
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

void initialize_ranger(
    openlegend::model::RangerState& ranger,
    const std::array<std::int16_t, openlegend::model::kTeamMemberCount>& party) {
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
}

openlegend::model::RangerState make_ranger(
    const std::array<std::int16_t, openlegend::model::kTeamMemberCount>& party) {
    openlegend::model::RangerState ranger;
    initialize_ranger(ranger, party);
    return ranger;
}

void finish_battle_entry_fade(openlegend::battle::BattleSession& session) {
    using namespace openlegend::battle;
    OL_CHECK(session.phase() == BattleSessionPhase::initial_fade_to_black);
    OL_CHECK(session.finish_initial_fade_to_black());
    OL_CHECK(session.phase() == BattleSessionPhase::initial_present);
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

    const auto fixed = setup.magic_animation_plan(
        0U, 2, 0, 30, kBattleFightPointerBase);
    OL_CHECK(fixed.has_value());
    OL_CHECK(fixed->fight_head_id == 9);
    OL_CHECK(fixed->magic_sample_id == 7);
    OL_CHECK(fixed->effect_sample_id == 30);
    OL_CHECK(!fixed->frames.empty());
    OL_CHECK(fixed->frames.front().actor_sprite >= 2 * kBattleFightPointerBase);
    OL_CHECK(fixed->frames.front().dispatch_magic_sample);
    OL_CHECK(fixed->frames.front().dispatch_effect_sample);
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
    OL_CHECK(!result->inventory_consumed);
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(fnv1a_words(setup.attack_effects()) == 0xab559939923b4f74ULL);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 79);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 45);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 12);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::damage_value] == 21);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::attack_counter] == 0);
    OL_CHECK(ranger.header.inventory_item(0U).value == 102);
    OL_CHECK(ranger.header.inventory_count(0U) == 1);
    OL_CHECK(setup.finish_throwing_weapon_action(0U, 0U));
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);
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
    OL_CHECK(ranger.header.inventory_count(0U) == 2);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);
    OL_CHECK(setup.finish_throwing_weapon_action(0U, 0U));
    OL_CHECK(ranger.header.inventory_count(0U) == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);

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

    data.occupancy()[26U * 64U + 26U] = 1;
    setup.combatants()[0U].words[combatant_word::side] = 0;
    setup.combatants()[1U].words[combatant_word::side] = 1;
    setup.combatants()[0U].words[combatant_word::initial_mode] = 1;
    setup.combatants()[0U].words[combatant_word::action_done] = 0;
    actor.set_word(openlegend::model::role_word::hidden_weapon, 20);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::maximum_hp, 200);
    target.set_word(openlegend::model::role_word::hurt, 40);
    target.set_word(openlegend::model::role_word::poison, 10);
    target.set_word(openlegend::model::role_word::anti_poison, 5);
    ranger.header.set_inventory(0U, openlegend::model::ItemId{102}, 1);
    ranger.header.set_inventory(1U, openlegend::model::ItemId{97}, 2);
    const BattleAiChoice party_throwing_choice{
        .action = BattleAiAction::throwing_weapon,
        .item_source = BattleAiItemSource::inventory,
        .item_slot = 0,
        .action_code_written = true,
    };
    random.seed(1U);
    const auto ai_party_result = setup.apply_ai_throwing_weapon_target(
        0U, BattlePathCoord{26, 26}, party_throwing_choice, random);
    OL_CHECK(ai_party_result.has_value());
    OL_CHECK(ai_party_result->hit_count == 1);
    OL_CHECK(ai_party_result->effect_id == 30);
    OL_CHECK(ai_party_result->damage == 21);
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 79);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 45);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 50);
    OL_CHECK(ranger.header.inventory_item(0U).value == 97);
    OL_CHECK(ranger.header.inventory_count(0U) == 2);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::initial_mode] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);

    ranger.header.set_inventory(0U, openlegend::model::ItemId{96}, 1);
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::hurt, 0);
    target.set_word(openlegend::model::role_word::poison, 10);
    random.seed(2U);
    const auto ai_plain_result = setup.apply_ai_throwing_weapon_target(
        0U, BattlePathCoord{26, 26}, party_throwing_choice, random);
    OL_CHECK(ai_plain_result.has_value());
    OL_CHECK(ai_plain_result->damage == 16);
    OL_CHECK(random.state() == 2'207'042'835U);
    OL_CHECK(target.word(openlegend::model::role_word::hp) == 84);
    OL_CHECK(target.word(openlegend::model::role_word::hurt) == 4);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 10);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);

    setup.combatants()[0U].words[combatant_word::side] = 1;
    setup.combatants()[1U].words[combatant_word::side] = 0;
    target.set_word(openlegend::model::role_word::hp, 100);
    target.set_word(openlegend::model::role_word::hurt, 40);
    target.set_word(openlegend::model::role_word::poison, 10);
    actor.set_word(openlegend::model::role_word::taking_item_begin, 102);
    actor.set_word(openlegend::model::role_word::taking_item_count_begin, 1);
    actor.set_word(openlegend::model::role_word::taking_item_begin + 1U, 97);
    actor.set_word(openlegend::model::role_word::taking_item_count_begin + 1U, 2);
    const BattleAiChoice carried_throwing_choice{
        .action = BattleAiAction::throwing_weapon,
        .item_source = BattleAiItemSource::carried,
        .item_slot = 0,
        .action_code_written = true,
    };
    random.seed(1U);
    const auto ai_carried_result = setup.apply_ai_throwing_weapon_target(
        0U, BattlePathCoord{26, 26}, carried_throwing_choice, random);
    OL_CHECK(ai_carried_result.has_value());
    OL_CHECK(ai_carried_result->damage == 21);
    OL_CHECK(random.state() == 1'103'527'590U);
    OL_CHECK(target.word(openlegend::model::role_word::poison) == 50);
    OL_CHECK(actor.word(openlegend::model::role_word::taking_item_begin) == 97);
    OL_CHECK(actor.word(openlegend::model::role_word::taking_item_count_begin) == 2);
    OL_CHECK(actor.word(openlegend::model::role_word::taking_item_begin + 3U) == -1);
    OL_CHECK(actor.word(openlegend::model::role_word::taking_item_count_begin + 3U) == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);
}

void run_shared_menu_item_helper_test() {
    using namespace openlegend;
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 1, -1, -1, -1, -1});
    ranger.roles[0U].set_word(model::role_word::id, 0);
    ranger.roles[1U].set_word(model::role_word::id, 1);
    auto& role = ranger.roles[0U];
    role.set_word(model::role_word::mp_type, 0);
    role.set_word(model::role_word::mp, 50);
    role.set_word(model::role_word::attack, 40);
    role.set_word(model::role_word::iq, 80);

    auto& equipment = ranger.items[5U];
    equipment.set_word(model::item_word::id, 5);
    equipment.set_word(model::item_word::item_type, 1);
    equipment.set_word(model::item_word::need_mp_type, 0);
    equipment.set_word(model::item_word::only_suitable_role, -1);
    equipment.set_word(model::item_word::need_mp, 50);
    equipment.set_word(model::item_word::need_attack, 40);
    equipment.set_word(model::item_word::need_iq, 80);
    OL_CHECK(role_meets_item_requirements(ranger, 0, 5));
    equipment.set_word(model::item_word::need_attack, 41);
    OL_CHECK(!role_meets_item_requirements(ranger, 0, 5));
    equipment.set_word(model::item_word::need_attack, 40);
    equipment.set_word(model::item_word::need_iq, -79);
    OL_CHECK(!role_meets_item_requirements(ranger, 0, 5));
    equipment.set_word(model::item_word::need_iq, -80);
    OL_CHECK(role_meets_item_requirements(ranger, 0, 5));
    equipment.set_word(model::item_word::id, 93);
    equipment.set_word(model::item_word::item_type, 2);
    equipment.set_word(model::item_word::need_attack, 41);
    equipment.set_word(model::item_word::magic_id, -1);
    role.set_word(model::role_word::sexual, 1);
    OL_CHECK(!role_meets_item_requirements(ranger, 0, 5));
    equipment.set_word(model::item_word::magic_id, 77);
    role.set_word(model::role_word::magic_id_begin, 77);
    OL_CHECK(role_meets_item_requirements(ranger, 0, 5));
    equipment.set_word(model::item_word::id, 5);
    equipment.set_word(model::item_word::item_type, 1);
    equipment.set_word(model::item_word::magic_id, -1);
    equipment.set_word(model::item_word::need_attack, 40);
    role.set_word(model::role_word::mp_type, 2);
    equipment.set_word(model::item_word::need_mp_type, 0);
    OL_CHECK(role_meets_item_requirements(ranger, 0, 5));

    equipment.set_word(model::item_word::equipment_type, 0);
    equipment.set_word(model::item_word::user, 1);
    ranger.roles[1U].set_word(model::role_word::equipment_begin, 5);
    auto& previous_equipment = ranger.items[6U];
    previous_equipment.set_word(model::item_word::id, 6);
    previous_equipment.set_word(model::item_word::user, 0);
    role.set_word(model::role_word::equipment_begin, 6);
    OL_CHECK(equip_role_item(ranger, 0, 5));
    OL_CHECK(ranger.roles[1U].word(model::role_word::equipment_begin) == -1);
    OL_CHECK(previous_equipment.word(model::item_word::user) == -1);
    OL_CHECK(role.word(model::role_word::equipment_begin) == 5);
    OL_CHECK(equipment.word(model::item_word::user) == 0);

    auto& practice = ranger.items[20U];
    practice.set_word(model::item_word::id, 20);
    practice.set_word(model::item_word::item_type, 2);
    practice.set_word(model::item_word::user, 1);
    ranger.roles[1U].set_word(model::role_word::practice_item, 20);
    ranger.roles[1U].set_word(model::role_word::item_experience, 7);
    auto& previous_practice = ranger.items[21U];
    previous_practice.set_word(model::item_word::id, 21);
    previous_practice.set_word(model::item_word::user, 0);
    role.set_word(model::role_word::practice_item, 21);
    role.set_word(model::role_word::item_experience, 8);
    role.set_word(model::role_word::make_item_experience, 9);
    OL_CHECK(assign_role_practice_item(ranger, 0, 20));
    OL_CHECK(ranger.roles[1U].word(model::role_word::practice_item) == -1);
    OL_CHECK(ranger.roles[1U].word(model::role_word::item_experience) == 0);
    OL_CHECK(previous_practice.word(model::item_word::user) == -1);
    OL_CHECK(practice.word(model::item_word::user) == 0);
    OL_CHECK(role.word(model::role_word::practice_item) == 20);
    OL_CHECK(role.word(model::role_word::item_experience) == 0);
    OL_CHECK(role.word(model::role_word::make_item_experience) == 0);

    ranger.header.set_inventory(0U, model::ItemId{5}, 1);
    ranger.header.set_inventory(1U, model::ItemId{20}, 3);
    ranger.header.set_inventory(2U, model::ItemId{-1}, 0);
    OL_CHECK(consume_inventory_item_slot(ranger, 0U));
    OL_CHECK(ranger.header.inventory_item(0U).value == 20);
    OL_CHECK(ranger.header.inventory_count(0U) == 3);
    OL_CHECK(ranger.header.inventory_item(1U).value == -1);
    OL_CHECK(consume_inventory_item_slot(ranger, 0U));
    OL_CHECK(ranger.header.inventory_item(0U).value == 20);
    OL_CHECK(ranger.header.inventory_count(0U) == 2);
}

void run_ai_item_effect_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
    }
    auto& actor = ranger.roles[1U];
    actor.set_word(role_word::hp, 100);
    actor.set_word(role_word::maximum_hp, 200);
    actor.set_word(role_word::hurt, 40);
    actor.set_word(role_word::poison, 50);
    actor.set_word(role_word::anti_poison, 5);
    actor.set_word(role_word::physical_power, 30);
    actor.set_word(role_word::mp, 10);
    actor.set_word(role_word::maximum_mp, 100);
    actor.set_word(role_word::hidden_weapon, 20);
    for (std::size_t slot = 0U; slot < role_word::taking_item_count; ++slot) {
        actor.set_word(role_word::taking_item_begin + slot, -1);
        actor.set_word(role_word::taking_item_count_begin + slot, 0);
    }

    auto& item = ranger.items[19U];
    item.set_word(item_word::item_type, 3);
    item.set_word(item_word::add_hp, 5'000);
    item.set_word(item_word::add_poison, -100);
    item.set_word(item_word::add_physical_power, 100);
    item.set_word(item_word::add_mp, 5'000);
    ranger.header.set_inventory(0U, openlegend::model::ItemId{19}, 1);
    ranger.header.set_inventory(1U, openlegend::model::ItemId{2}, 3);

    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    setup.combatants()[0U].words[combatant_word::action_done] = 0;
    const BattleAiChoice party_choice{
        .action = BattleAiAction::item,
        .target_slot = 0,
        .item_source = BattleAiItemSource::inventory,
        .item_slot = 0,
        .action_code_written = true,
    };
    openlegend::random::LegacyRandom random{1U};
    const auto party_result = setup.apply_ai_item_effect(0U, party_choice, random);
    OL_CHECK(party_result.has_value());
    OL_CHECK(party_result->has_effect);
    OL_CHECK(party_result->effect_count == 4);
    OL_CHECK(party_result->panel_x == 70);
    OL_CHECK(party_result->panel_y == 18);
    OL_CHECK(party_result->panel_width == 148);
    OL_CHECK(party_result->panel_height == 110);
    OL_CHECK(party_result->post_effect_tick_changes == 9);
    OL_CHECK(party_result->battle_redraw_required);
    OL_CHECK(party_result->wait_for_input);
    OL_CHECK(party_result->deltas[0U] == 100);
    OL_CHECK(party_result->deltas[2U] == -50);
    OL_CHECK(party_result->deltas[3U] == 70);
    OL_CHECK(party_result->deltas[5U] == 90);
    OL_CHECK(party_result->item_consumed);
    OL_CHECK(random.state() == 662'824'084U);
    OL_CHECK(actor.word(role_word::hp) == 200);
    OL_CHECK(actor.word(role_word::hurt) == 0);
    OL_CHECK(actor.word(role_word::poison) == 0);
    OL_CHECK(actor.word(role_word::physical_power) == 100);
    OL_CHECK(actor.word(role_word::mp) == 100);
    OL_CHECK(ranger.header.inventory_item(0U).value == 2);
    OL_CHECK(ranger.header.inventory_count(0U) == 3);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);
    OL_CHECK(std::ranges::count(setup.attack_effects(), 1) == 1);

    actor.set_word(role_word::hp, 100);
    actor.set_word(role_word::hurt, 40);
    actor.set_word(role_word::poison, 50);
    actor.set_word(role_word::physical_power, 30);
    actor.set_word(role_word::mp, 10);
    actor.set_word(role_word::taking_item_begin, 19);
    actor.set_word(role_word::taking_item_count_begin, 1);
    actor.set_word(role_word::taking_item_begin + 1U, 2);
    actor.set_word(role_word::taking_item_count_begin + 1U, 3);
    setup.combatants()[0U].words[combatant_word::side] = 1;
    const BattleAiChoice carried_choice{
        .action = BattleAiAction::item,
        .target_slot = 0,
        .item_source = BattleAiItemSource::carried,
        .item_slot = 0,
        .action_code_written = true,
    };
    random.seed(1U);
    const auto carried_result = setup.apply_ai_item_effect(0U, carried_choice, random);
    OL_CHECK(carried_result.has_value());
    OL_CHECK(carried_result->effect_count == 4);
    OL_CHECK(actor.word(role_word::taking_item_begin) == 2);
    OL_CHECK(actor.word(role_word::taking_item_count_begin) == 3);
    OL_CHECK(actor.word(role_word::taking_item_begin + 3U) == -1);
    OL_CHECK(actor.word(role_word::taking_item_count_begin + 3U) == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);

    auto& display_only = ranger.items[151U];
    display_only.set_word(item_word::item_type, 3);
    display_only.set_word(item_word::add_morality, 7);
    display_only.set_word(item_word::add_attack_twice, 1);
    actor.set_word(role_word::morality, 25);
    actor.set_word(role_word::attack_twice, 0);
    actor.set_word(role_word::taking_item_begin, 151);
    actor.set_word(role_word::taking_item_count_begin, 1);
    const auto display_result = setup.apply_ai_item_effect(0U, carried_choice, random);
    OL_CHECK(display_result.has_value());
    OL_CHECK(display_result->has_effect);
    OL_CHECK(display_result->effect_count == 2);
    OL_CHECK(display_result->deltas[20U] == 7);
    OL_CHECK(display_result->deltas[21U] == 1);
    OL_CHECK(actor.word(role_word::morality) == 25);
    OL_CHECK(actor.word(role_word::attack_twice) == 0);

    auto& empty_item = ranger.items[152U];
    empty_item.set_word(item_word::item_type, 3);
    actor.set_word(role_word::taking_item_begin, 152);
    actor.set_word(role_word::taking_item_count_begin, 1);
    const auto empty_result = setup.apply_ai_item_effect(0U, carried_choice, random);
    OL_CHECK(empty_result.has_value());
    OL_CHECK(!empty_result->has_effect);
    OL_CHECK(empty_result->effect_count == 0);
    OL_CHECK(empty_result->panel_height == 30);
    OL_CHECK(empty_result->post_effect_tick_changes == 9);
    OL_CHECK(!empty_result->battle_redraw_required);
    OL_CHECK(!empty_result->wait_for_input);
    OL_CHECK(empty_result->item_consumed);
    OL_CHECK(actor.word(role_word::taking_item_begin) == -1);
    OL_CHECK(actor.word(role_word::taking_item_count_begin) == 0);

    auto& restorative = ranger.items[2U];
    restorative.set_word(item_word::item_type, 3);
    restorative.set_word(item_word::add_hp, 20);
    actor.set_word(role_word::hp, 100);
    actor.set_word(role_word::hurt, 100);
    actor.set_word(role_word::taking_item_begin, 2);
    actor.set_word(role_word::taking_item_count_begin, 1);
    random.seed(1U);
    const auto restorative_result = setup.apply_ai_item_effect(0U, carried_choice, random);
    OL_CHECK(restorative_result.has_value());
    OL_CHECK(restorative_result->deltas[0U] == 8);
    OL_CHECK(random.state() == 2'524'885'223U);
    OL_CHECK(actor.word(role_word::hp) == 108);
    OL_CHECK(actor.word(role_word::hurt) == 95);

    auto& harmful = ranger.items[96U];
    harmful.set_word(item_word::item_type, 4);
    harmful.set_word(item_word::add_hp, -30);
    actor.set_word(role_word::hp, 100);
    actor.set_word(role_word::hurt, 0);
    actor.set_word(role_word::taking_item_begin, 96);
    actor.set_word(role_word::taking_item_count_begin, 1);
    random.seed(1U);
    const auto harmful_result = setup.apply_ai_item_effect(0U, carried_choice, random);
    OL_CHECK(harmful_result.has_value());
    OL_CHECK(harmful_result->deltas[0U] == -22);
    OL_CHECK(random.state() == 2'524'885'223U);
    OL_CHECK(actor.word(role_word::hp) == 78);
    OL_CHECK(actor.word(role_word::hurt) == 2);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);

    auto& all_fields = ranger.items[153U];
    all_fields.set_word(item_word::item_type, 3);
    all_fields.set_word(item_word::add_maximum_hp, 50);
    all_fields.set_word(item_word::change_mp_type, 2);
    all_fields.set_word(item_word::add_maximum_mp, 30);
    for (std::size_t index = 0U; index < 13U; ++index) {
        all_fields.set_word(
            item_word::add_attack + index, static_cast<std::int16_t>(index + 1U));
        actor.set_word(role_word::attack + index, 10);
    }
    all_fields.set_word(item_word::add_morality, 7);
    all_fields.set_word(item_word::add_attack_twice, 1);
    all_fields.set_word(item_word::add_attack_with_poison, 5);
    actor.set_word(role_word::hp, 100);
    actor.set_word(role_word::maximum_hp, 200);
    actor.set_word(role_word::mp_type, 0);
    actor.set_word(role_word::mp, 50);
    actor.set_word(role_word::maximum_mp, 100);
    actor.set_word(role_word::morality, 25);
    actor.set_word(role_word::attack_twice, 0);
    actor.set_word(role_word::attack_with_poison, 4);
    actor.set_word(role_word::taking_item_begin, 153);
    actor.set_word(role_word::taking_item_count_begin, 2);
    random.seed(1U);
    const auto all_fields_result = setup.apply_ai_item_effect(0U, carried_choice, random);
    OL_CHECK(all_fields_result.has_value());
    OL_CHECK(all_fields_result->effect_count == 19);
    OL_CHECK(all_fields_result->deltas[1U] == 50);
    OL_CHECK(all_fields_result->deltas[4U] == 2);
    OL_CHECK(all_fields_result->deltas[6U] == 30);
    OL_CHECK(all_fields_result->deltas[20U] == 7);
    OL_CHECK(all_fields_result->deltas[21U] == 1);
    OL_CHECK(all_fields_result->deltas[22U] == 5);
    OL_CHECK(all_fields_result->panel_height == 410);
    OL_CHECK(random.state() == 1U);
    OL_CHECK(actor.word(role_word::maximum_hp) == 250);
    OL_CHECK(actor.word(role_word::mp_type) == 2);
    OL_CHECK(actor.word(role_word::maximum_mp) == 130);
    for (std::size_t index = 0U; index < 13U; ++index) {
        OL_CHECK(actor.word(role_word::attack + index) == static_cast<std::int16_t>(11U + index));
    }
    OL_CHECK(actor.word(role_word::morality) == 25);
    OL_CHECK(actor.word(role_word::attack_twice) == 0);
    OL_CHECK(actor.word(role_word::attack_with_poison) == 9);
    OL_CHECK(actor.word(role_word::taking_item_count_begin) == 1);
}

void run_ai_request_handler_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    setup.combatants()[0U].words[combatant_word::action_done] = 0;
    const BattleAiChoice medicine_request{
        .action = BattleAiAction::request_medicine,
        .target_slot = 1,
        .action_code_written = true,
    };
    const auto medicine_plan = setup.begin_ai_request_plan(0U, medicine_request);
    OL_CHECK(medicine_plan.has_value());
    OL_CHECK(medicine_plan->request_action == BattleAiAction::request_medicine);
    OL_CHECK(medicine_plan->target_slot == 1);
    OL_CHECK(medicine_plan->target.x ==
             setup.combatants()[1U].words[combatant_word::x]);
    OL_CHECK(medicine_plan->target.y ==
             setup.combatants()[1U].words[combatant_word::y]);
    OL_CHECK(medicine_plan->movement_mode == 0);
    OL_CHECK(medicine_plan->movement_value == 0);
    OL_CHECK(medicine_plan->next_step == BattleAiRequestNextStep::move);
    OL_CHECK(medicine_plan->restore_request_target_before_attack);
    OL_CHECK(medicine_plan->outer_marks_action_done_after_handler);

    auto resumed = setup.resume_ai_request_after_move(0U, *medicine_plan);
    OL_CHECK(resumed.has_value());
    OL_CHECK(resumed->next_step == BattleAiRequestNextStep::automatic_attack);
    OL_CHECK(resumed->target_slot == 1);
    OL_CHECK(resumed->target.x == setup.combatants()[1U].words[combatant_word::x]);
    OL_CHECK(resumed->target.y == setup.combatants()[1U].words[combatant_word::y]);
    OL_CHECK(!setup.resume_ai_request_after_move(0U, *resumed).has_value());

    setup.combatants()[0U].words[combatant_word::round_value] = 0;
    const BattleAiChoice detox_request{
        .action = BattleAiAction::request_detox,
        .target_slot = 1,
        .action_code_written = true,
    };
    const auto detox_plan = setup.begin_ai_request_plan(0U, detox_request);
    OL_CHECK(detox_plan.has_value());
    OL_CHECK(detox_plan->request_action == BattleAiAction::request_detox);
    OL_CHECK(detox_plan->next_step == BattleAiRequestNextStep::automatic_attack);
    OL_CHECK(detox_plan->movement_mode == 0);
    OL_CHECK(detox_plan->movement_value == 0);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);

    BattleAiChoice invalid = medicine_request;
    invalid.action = BattleAiAction::medicine;
    OL_CHECK(!setup.begin_ai_request_plan(0U, invalid).has_value());
}

void run_ai_support_handler_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    using namespace openlegend::model;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    const auto actor_slot = std::size_t{0U};
    const auto target_slot = std::size_t{1U};
    auto& actor = setup.combatants()[actor_slot].words;
    auto& target = setup.combatants()[target_slot].words;
    target[combatant_word::side] = actor[combatant_word::side];
    const auto actor_role_id = static_cast<std::size_t>(actor[combatant_word::role_id]);
    const auto target_role_id = static_cast<std::size_t>(target[combatant_word::role_id]);

    BattlePathing pathing{data};
    pathing.build(
        BattlePathCoord{actor[combatant_word::x], actor[combatant_word::y]},
        BattlePathMode::targeting);
    std::optional<BattlePathCoord> adjacent;
    std::optional<BattlePathCoord> distant;
    for (std::int16_t y = 0; y < 64; ++y) {
        for (std::int16_t x = 0; x < 64; ++x) {
            const auto value = pathing.value(BattlePathCoord{x, y});
            if (value == 1 && !adjacent.has_value()) {
                adjacent = BattlePathCoord{x, y};
            }
            if (value >= 5 && !distant.has_value()) {
                distant = BattlePathCoord{x, y};
            }
        }
    }
    OL_CHECK(adjacent.has_value());
    OL_CHECK(distant.has_value());

    ranger.roles[actor_role_id].set_word(role_word::medicine, 0);
    ranger.roles[actor_role_id].set_word(role_word::detoxification, 0);
    target[combatant_word::x] = adjacent->x;
    target[combatant_word::y] = adjacent->y;
    actor[combatant_word::round_value] = 3;
    actor[combatant_word::action_done] = 0;
    const BattleAiChoice medicine_choice{
        .action = BattleAiAction::medicine,
        .target_slot = static_cast<std::int16_t>(target_slot),
        .action_code_written = true,
    };
    auto plan = setup.begin_ai_support_plan(actor_slot, medicine_choice);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->support_action == BattleAiAction::medicine);
    OL_CHECK(plan->target_slot == static_cast<std::int16_t>(target_slot));
    OL_CHECK(plan->target.x == adjacent->x);
    OL_CHECK(plan->target.y == adjacent->y);
    OL_CHECK(plan->targeting_range == 1);
    OL_CHECK(plan->target_distance == 1);
    OL_CHECK(plan->range_check_count == 1);
    OL_CHECK(plan->movement_mode == 1);
    OL_CHECK(plan->movement_value == 1);
    OL_CHECK(plan->next_step == BattleAiSupportNextStep::apply_support);
    OL_CHECK(plan->restore_target_after_move);
    OL_CHECK(plan->outer_marks_action_done_after_handler);

    const BattleAiChoice detox_choice{
        .action = BattleAiAction::detox,
        .target_slot = static_cast<std::int16_t>(target_slot),
        .action_code_written = true,
    };
    plan = setup.begin_ai_support_plan(actor_slot, detox_choice);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->support_action == BattleAiAction::detox);
    OL_CHECK(plan->targeting_range == 1);
    OL_CHECK(plan->target_distance == 1);
    OL_CHECK(plan->next_step == BattleAiSupportNextStep::apply_support);

    for (const auto& combatant : setup.combatants().first(
             static_cast<std::size_t>(setup.combatant_count()))) {
        if (combatant.words[combatant_word::side] != actor[combatant_word::side]) {
            continue;
        }
        const auto role_id = static_cast<std::size_t>(combatant.words[combatant_word::role_id]);
        ranger.roles[role_id].set_word(role_word::attack, 0);
        ranger.roles[role_id].set_word(role_word::hp, 0);
    }
    ranger.roles[actor_role_id].set_word(role_word::attack, 300);
    target[combatant_word::x] = distant->x;
    target[combatant_word::y] = distant->y;
    actor[combatant_word::round_value] = 3;
    plan = setup.begin_ai_support_plan(actor_slot, medicine_choice);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->target_distance >= 5);
    OL_CHECK(plan->range_check_count == 1);
    OL_CHECK(plan->next_step == BattleAiSupportNextStep::move);
    OL_CHECK(plan->movement_mode == 1);
    OL_CHECK(plan->movement_value == plan->targeting_range);

    plan = setup.resume_ai_support_after_move(actor_slot, *plan);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->range_check_count == 2);
    OL_CHECK(plan->next_step == BattleAiSupportNextStep::automatic_attack);
    OL_CHECK(plan->doubled_actor_attack == 600);
    OL_CHECK(plan->doubled_actor_attack > plan->doubled_allied_average);
    OL_CHECK(!setup.resume_ai_support_after_move(actor_slot, *plan).has_value());

    ranger.roles[actor_role_id].set_word(role_word::attack, 0);
    actor[combatant_word::round_value] = 0;
    plan = setup.begin_ai_support_plan(actor_slot, detox_choice);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->range_check_count == 2);
    OL_CHECK(plan->next_step == BattleAiSupportNextStep::rest);
    OL_CHECK(plan->doubled_actor_attack == 0);
    OL_CHECK(plan->doubled_allied_average == 0);
    OL_CHECK(actor[combatant_word::action_done] == 0);

    OL_CHECK(actor_role_id != target_role_id);
    ranger.roles[actor_role_id].set_word(role_word::attack, 30'000);
    ranger.roles[actor_role_id].set_word(role_word::hp, 30'000);
    ranger.roles[target_role_id].set_word(role_word::attack, 10'000);
    ranger.roles[target_role_id].set_word(role_word::hp, 10'000);
    plan = setup.begin_ai_support_plan(actor_slot, medicine_choice);
    OL_CHECK(plan.has_value());
    OL_CHECK(plan->allied_total == 14'464);
    OL_CHECK(plan->allied_count == 2);
    OL_CHECK(plan->doubled_actor_attack == 60'000);
    OL_CHECK(plan->doubled_allied_average == 14'464);
    OL_CHECK(plan->next_step == BattleAiSupportNextStep::automatic_attack);

    BattleAiChoice invalid = medicine_choice;
    invalid.action = BattleAiAction::attack;
    OL_CHECK(!setup.begin_ai_support_plan(actor_slot, invalid).has_value());
}

void run_post_battle_progression_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    using namespace openlegend::model;

    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.valid());
        auto& role = ranger.roles[0U];
        role.set_word(role_word::level, 1);
        role.set_word(role_word::experience, 150);
        role.set_word(role_word::increased_life, 2);
        role.set_word(role_word::iq, 90);
        role.set_word(role_word::maximum_hp, 100);
        role.set_word(role_word::maximum_mp, 80);
        role.set_word(role_word::attack, 30);
        role.set_word(role_word::speed, 30);
        role.set_word(role_word::defence, 30);
        role.set_word(role_word::medicine, 21);
        role.set_word(role_word::use_poison, 20);
        role.set_word(role_word::detoxification, 22);
        role.set_word(role_word::fist, 23);
        role.set_word(role_word::sword, 24);
        role.set_word(role_word::knife, 25);
        role.set_word(role_word::hidden_weapon, 26);
        openlegend::random::LegacyRandom random{1U};
        const auto level_up = setup.apply_battle_level_up(0U, false, random);
        OL_CHECK(level_up.has_value());
        OL_CHECK(level_up->changed);
        OL_CHECK(level_up->old_level == 1);
        OL_CHECK(level_up->new_level == 3);
        OL_CHECK(level_up->levels_gained == 2);
        OL_CHECK(level_up->growth_roll == 3);
        OL_CHECK(level_up->maximum_hp == 118);
        OL_CHECK(level_up->maximum_mp == 128);
        OL_CHECK(level_up->message_required);
        OL_CHECK(level_up->present_required);
        OL_CHECK(level_up->wait_for_input);
        OL_CHECK(role.word(role_word::hp) == 118);
        OL_CHECK(role.word(role_word::mp) == 128);
        OL_CHECK(role.word(role_word::hurt) == 0);
        OL_CHECK(role.word(role_word::poison) == 0);
        OL_CHECK(role.word(role_word::physical_power) == 100);
        OL_CHECK(role.word(role_word::attack) == 36);
        OL_CHECK(role.word(role_word::speed) == 36);
        OL_CHECK(role.word(role_word::defence) == 36);
        OL_CHECK(role.word(role_word::medicine) == 21);
        OL_CHECK(role.word(role_word::use_poison) == 20);
        OL_CHECK(role.word(role_word::detoxification) == 23);
        OL_CHECK(role.word(role_word::fist) == 24);
        OL_CHECK(role.word(role_word::sword) == 26);
        OL_CHECK(role.word(role_word::knife) == 25);
        OL_CHECK(role.word(role_word::hidden_weapon) == 26);
        OL_CHECK(random.state() == 2'633'739'833U);

        auto& practice_item = ranger.items[5U];
        role.set_word(role_word::practice_item, 5);
        role.set_word(role_word::iq, 60);
        role.set_word(role_word::item_experience, 60);
        role.set_word(role_word::maximum_hp, 100);
        role.set_word(role_word::maximum_mp, 80);
        role.set_word(role_word::attack, 30);
        role.set_word(role_word::morality, 50);
        role.set_word(role_word::attack_twice, 0);
        role.set_word(role_word::attack_with_poison, 0);
        role.set_word(role_word::magic_id_begin, 2);
        role.set_word(role_word::magic_level_begin, 199);
        practice_item.set_word(item_word::magic_id, 2);
        practice_item.set_word(item_word::need_experience, 10);
        practice_item.set_word(item_word::add_maximum_hp, 10);
        practice_item.set_word(item_word::add_maximum_mp, 20);
        practice_item.set_word(item_word::add_attack, 80);
        practice_item.set_word(item_word::add_morality, -100);
        practice_item.set_word(item_word::add_attack_twice, 1);
        practice_item.set_word(item_word::add_attack_with_poison, 5);
        const auto practice = setup.apply_battle_practice(0U, false);
        OL_CHECK(practice.has_value());
        OL_CHECK(practice->practiced);
        OL_CHECK(practice->required_experience == 60);
        OL_CHECK(practice->magic_slot == 0);
        OL_CHECK(practice->increased_magic_level);
        OL_CHECK(practice->practice_message_required);
        OL_CHECK(practice->magic_message_required);
        OL_CHECK(role.word(role_word::item_experience) == 0);
        OL_CHECK(role.word(role_word::maximum_hp) == 110);
        OL_CHECK(role.word(role_word::maximum_mp) == 100);
        OL_CHECK(role.word(role_word::attack) == 100);
        OL_CHECK(role.word(role_word::morality) == 0);
        OL_CHECK(role.word(role_word::attack_twice) == 1);
        OL_CHECK(role.word(role_word::attack_with_poison) == 5);
        OL_CHECK(role.word(role_word::magic_level_begin) == 299);

        role.set_word(role_word::make_item_experience, 30);
        practice_item.set_word(item_word::need_make_item_experience, 10);
        practice_item.set_word(item_word::need_material, 10);
        practice_item.set_word(item_word::make_item_begin, 20);
        practice_item.set_word(item_word::make_item_count_begin, 2);
        for (std::size_t recipe = 1U; recipe < item_word::make_item_count; ++recipe) {
            practice_item.set_word(item_word::make_item_begin + recipe, -1);
        }
        ranger.header.set_inventory(0U, ItemId{10}, 3);
        ranger.header.set_inventory(1U, ItemId{20}, 4);
        ranger.header.set_inventory(2U, ItemId{-1}, 0);
        openlegend::random::LegacyRandom craft_random{1U};
        const auto craft = setup.apply_battle_crafting(0U, false, craft_random);
        OL_CHECK(craft.has_value());
        OL_CHECK(craft->recipe_available);
        OL_CHECK(craft->recipe_slot == 0);
        OL_CHECK(craft->product_item_id == 20);
        OL_CHECK(craft->product_count_added == 2);
        OL_CHECK(craft->material_count_removed == 2);
        OL_CHECK(craft->message_required);
        OL_CHECK(craft->crafted);
        OL_CHECK(!craft->created_inventory_slot);
        OL_CHECK(ranger.header.inventory_count(0U) == 1);
        OL_CHECK(ranger.header.inventory_count(1U) == 6);
        OL_CHECK(role.word(role_word::make_item_experience) == 0);
        OL_CHECK(craft_random.state() == 4'182'499'122U);
    }

    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        BattleData data{data_root, 2};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.valid());
        for (std::size_t step = 0U; step < setup.party_prefix_length(); ++step) {
            OL_CHECK(setup.apply(PartySelectionAction::next) ==
                     PartySelectionResult::changed);
        }
        OL_CHECK(setup.apply(PartySelectionAction::activate) ==
                 PartySelectionResult::complete);
        OL_CHECK(setup.combatant_count() >= 3);
        std::optional<std::size_t> first_party;
        std::optional<std::size_t> dead_party;
        std::optional<std::size_t> first_enemy;
        for (std::size_t slot = 0U;
             slot < static_cast<std::size_t>(setup.combatant_count());
             ++slot) {
            auto& words = setup.combatants()[slot].words;
            auto& role = ranger.roles[static_cast<std::size_t>(words[combatant_word::role_id])];
            role.set_word(role_word::level, 30);
            role.set_word(role_word::practice_item, -1);
            role.set_word(role_word::maximum_hp, 100);
            role.set_word(role_word::maximum_mp, 80);
            role.set_word(role_word::experience, 0);
            role.set_word(role_word::item_experience, 0);
            role.set_word(role_word::make_item_experience, 0);
            words[combatant_word::reward_experience] = 0;
            if (words[combatant_word::side] == 0) {
                if (!first_party) {
                    first_party = slot;
                    role.set_word(role_word::hp, 100);
                    words[combatant_word::reward_experience] = 5;
                } else {
                    if (!dead_party) {
                        dead_party = slot;
                        words[combatant_word::reward_experience] = 7;
                    }
                    role.set_word(role_word::hp, 0);
                    role.set_word(role_word::physical_power, 0);
                }
            } else {
                if (!first_enemy) {
                    first_enemy = slot;
                }
                role.set_word(role_word::hp, 1);
                role.set_word(role_word::mp, 1);
                role.set_word(role_word::hurt, 50);
                role.set_word(role_word::poison, 50);
                role.set_word(role_word::physical_power, 1);
            }
        }
        OL_CHECK(first_party.has_value());
        OL_CHECK(dead_party.has_value());
        OL_CHECK(first_enemy.has_value());
        openlegend::random::LegacyRandom random{1U};
        const auto settled = setup.settle_battle(BattleOutcome::victory, false, random);
        OL_CHECK(settled.has_value());
        OL_CHECK(settled->total_experience == data.definition()[7U]);
        OL_CHECK(settled->living_party_count == 1);
        OL_CHECK(settled->shared_experience == data.definition()[7U]);
        OL_CHECK(settled->render_required);
        OL_CHECK(settled->present_required);
        OL_CHECK(settled->wait_for_input);
        OL_CHECK(settled->roles.size() == static_cast<std::size_t>(setup.combatant_count()));
        const auto first_party_role = static_cast<std::size_t>(
            setup.combatants()[*first_party].words[combatant_word::role_id]);
        const auto dead_party_role = static_cast<std::size_t>(
            setup.combatants()[*dead_party].words[combatant_word::role_id]);
        const auto enemy_role = static_cast<std::size_t>(
            setup.combatants()[*first_enemy].words[combatant_word::role_id]);
        OL_CHECK(ranger.roles[first_party_role].word(role_word::experience) ==
                 static_cast<std::int16_t>(data.definition()[7U] + 5));
        OL_CHECK(ranger.roles[dead_party_role].word(role_word::experience) == 7);
        OL_CHECK(ranger.roles[dead_party_role].word(role_word::hp) == 20);
        OL_CHECK(ranger.roles[dead_party_role].word(role_word::physical_power) == 10);
        OL_CHECK(ranger.roles[enemy_role].word(role_word::hp) == 100);
        OL_CHECK(ranger.roles[enemy_role].word(role_word::mp) == 80);
        OL_CHECK(ranger.roles[enemy_role].word(role_word::hurt) == 0);
        OL_CHECK(ranger.roles[enemy_role].word(role_word::poison) == 0);
        OL_CHECK(ranger.roles[enemy_role].word(role_word::physical_power) == 100);
        OL_CHECK(random.state() == 1U);
    }

    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.valid());
        OL_CHECK(setup.combatant_count() == 2);
        auto& first = setup.combatants()[0U].words;
        auto& second = setup.combatants()[1U].words;
        auto& first_role = ranger.roles[static_cast<std::size_t>(
            first[combatant_word::role_id])];
        auto& second_role = ranger.roles[static_cast<std::size_t>(
            second[combatant_word::role_id])];
        first_role.set_word(role_word::hp, 0);
        first_role.set_word(role_word::hurt, 20);
        first_role.set_word(role_word::poison, 0);
        first_role.set_word(role_word::physical_power, -1);
        first[combatant_word::occupancy_hidden] = 1;
        second_role.set_word(role_word::hp, 100);
        second_role.set_word(role_word::hurt, 0);
        second_role.set_word(role_word::poison, 20);
        second_role.set_word(role_word::physical_power, 100);
        second[combatant_word::occupancy_hidden] = 0;
        const auto status_damage = setup.apply_round_status_damage();
        OL_CHECK(status_damage.has_value());
        OL_CHECK(status_damage->entries.size() == 2U);
        OL_CHECK(status_damage->entries[0U].hurt_damage == 1);
        OL_CHECK(status_damage->entries[0U].poison_damage == 0);
        OL_CHECK(status_damage->entries[0U].physical_power_floored);
        OL_CHECK(status_damage->entries[0U].hp_floored);
        OL_CHECK(first_role.word(role_word::hp) == 1);
        OL_CHECK(first_role.word(role_word::physical_power) == 1);
        OL_CHECK(status_damage->entries[1U].hurt_damage == 0);
        OL_CHECK(status_damage->entries[1U].poison_damage == 2);
        OL_CHECK(second_role.word(role_word::hp) == 98);

        first[combatant_word::ai_target] = 1;
        first[combatant_word::ai_poison_target] = 1;
        second[combatant_word::occupancy_hidden] = 1;
        const auto cleanup = setup.clear_hidden_ai_targets();
        OL_CHECK(cleanup.has_value());
        OL_CHECK(cleanup->attack_targets_cleared == 1);
        OL_CHECK(cleanup->poison_targets_cleared == 1);
        OL_CHECK(first[combatant_word::ai_target] == -1);
        OL_CHECK(first[combatant_word::ai_poison_target] == -1);
        first[combatant_word::ai_target] = 1;
        second[combatant_word::occupancy_hidden] = 2;
        const auto exact_cleanup = setup.clear_hidden_ai_targets();
        OL_CHECK(exact_cleanup.has_value());
        OL_CHECK(exact_cleanup->attack_targets_cleared == 0);
        OL_CHECK(first[combatant_word::ai_target] == 1);
        first[combatant_word::ai_target] = setup.combatant_count();
        setup.combatants()[static_cast<std::size_t>(setup.combatant_count())]
            .words[combatant_word::occupancy_hidden] = 1;
        const auto inactive_cleanup = setup.clear_hidden_ai_targets();
        OL_CHECK(inactive_cleanup.has_value());
        OL_CHECK(inactive_cleanup->attack_targets_cleared == 1);
        OL_CHECK(first[combatant_word::ai_target] == -1);

        std::fill_n(
            first_role.bytes.begin() + static_cast<std::ptrdiff_t>(role_word::name_byte),
            role_word::name_bytes,
            static_cast<std::uint8_t>(0));
        first_role.bytes[role_word::name_byte] = 'A';
        first_role.bytes[role_word::name_byte + 1U] = 'B';
        first_role.set_word(role_word::head_id, 123);
        first_role.set_word(role_word::physical_power, 77);
        first_role.set_word(role_word::hp, 55);
        first_role.set_word(role_word::maximum_hp, 100);
        first_role.set_word(role_word::hurt, 34);
        first_role.set_word(role_word::poison, 50);
        first_role.set_word(role_word::mp_type, 3);
        first_role.set_word(role_word::mp, 22);
        first_role.set_word(role_word::maximum_mp, 80);
        const auto party_panel = setup.status_panel_plan(0U);
        OL_CHECK(party_panel.has_value());
        OL_CHECK(party_panel->side_offset == 0);
        OL_CHECK(party_panel->panel_x == 220);
        OL_CHECK(party_panel->portrait_x == 242);
        OL_CHECK(party_panel->portrait_id == 123);
        OL_CHECK(party_panel->name_x == 262);
        OL_CHECK(party_panel->hurt_color == 3'600);
        OL_CHECK(party_panel->poison_color == 13'623);
        OL_CHECK(party_panel->mp_color == 13'623);
        OL_CHECK(party_panel->render_required);
        const auto enemy_panel = setup.status_panel_plan(1U);
        OL_CHECK(enemy_panel.has_value());
        OL_CHECK(enemy_panel->side_offset == 220);
        OL_CHECK(enemy_panel->panel_x == 0);
        OL_CHECK(enemy_panel->portrait_x == 22);
    }
}

void run_player_movement_selection_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    using namespace openlegend::model;

    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    auto& actor = setup.combatants()[0U].words;
    const auto& occupied_target = setup.combatants()[1U].words;
    const auto role_id = static_cast<std::size_t>(actor[combatant_word::role_id]);
    actor[combatant_word::round_value] = 2;
    ranger.roles[role_id].set_word(role_word::speed, 20);
    ranger.roles[role_id].set_word(role_word::physical_power, 10);

    auto selection = setup.begin_player_movement_selection(0U);
    OL_CHECK(selection.has_value());
    OL_CHECK((selection->source == BattlePathCoord{26, 24}));
    OL_CHECK(selection->cursor == selection->source);
    OL_CHECK(selection->path_limit == 2);
    OL_CHECK(selection->mode == BattleCursorSelectionMode::movement);
    OL_CHECK(selection->render_required);
    OL_CHECK(selection->present_required);
    OL_CHECK(
        setup.apply_cursor_selection(*selection, BattleCursorSelectionAction::activate) ==
        BattleCursorSelectionResult::unchanged);
    OL_CHECK(setup.apply_cursor_selection(*selection, BattleCursorSelectionAction::down) ==
             BattleCursorSelectionResult::moved);
    OL_CHECK((selection->cursor == BattlePathCoord{26, 25}));
    OL_CHECK(setup.apply_cursor_selection(*selection, BattleCursorSelectionAction::down) ==
             BattleCursorSelectionResult::moved);
    OL_CHECK((selection->cursor == BattlePathCoord{
                                       occupied_target[combatant_word::x],
                                       occupied_target[combatant_word::y]}));
    OL_CHECK(selection->pathing.value(selection->cursor) == kBattlePathBlocked);
    OL_CHECK(
        setup.apply_cursor_selection(*selection, BattleCursorSelectionAction::activate) ==
        BattleCursorSelectionResult::unchanged);
    OL_CHECK(setup.apply_cursor_selection(*selection, BattleCursorSelectionAction::up) ==
             BattleCursorSelectionResult::moved);
    OL_CHECK(
        setup.apply_cursor_selection(*selection, BattleCursorSelectionAction::activate) ==
        BattleCursorSelectionResult::selected);
    OL_CHECK(selection->selected);
    OL_CHECK(selection->complete);

    auto movement = setup.finish_player_movement_selection(*selection);
    OL_CHECK(movement.has_value());
    OL_CHECK(movement->path_marked);
    OL_CHECK((movement->destination == BattlePathCoord{26, 25}));
    const auto step = setup.advance_player_movement(*movement);
    OL_CHECK(step.has_value());
    OL_CHECK((step->from == BattlePathCoord{26, 24}));
    OL_CHECK((step->to == BattlePathCoord{26, 25}));
    OL_CHECK(step->remaining_round_value == 1);
    OL_CHECK(step->physical_power == 9);
    OL_CHECK(step->wait_ticks == 40);
    OL_CHECK(step->render_required);
    OL_CHECK(step->present_required);
    OL_CHECK(step->complete);
    OL_CHECK(movement->complete);

    auto cancelled = setup.begin_player_movement_selection(0U);
    OL_CHECK(cancelled.has_value());
    OL_CHECK(
        setup.apply_cursor_selection(*cancelled, BattleCursorSelectionAction::cancel) ==
        BattleCursorSelectionResult::cancelled);
    OL_CHECK(cancelled->path_limit == 0);
    OL_CHECK(cancelled->cancelled);
    OL_CHECK(!setup.finish_player_movement_selection(*cancelled).has_value());

    auto targeting = setup.begin_cursor_selection(
        0U, 1, BattleCursorSelectionMode::targeting);
    OL_CHECK(targeting.has_value());
    OL_CHECK(setup.apply_cursor_selection(*targeting, BattleCursorSelectionAction::down) ==
             BattleCursorSelectionResult::moved);
    OL_CHECK((targeting->cursor == BattlePathCoord{26, 26}));
    OL_CHECK(targeting->pathing.value(targeting->cursor) == 1);
    OL_CHECK(
        setup.apply_cursor_selection(*targeting, BattleCursorSelectionAction::activate) ==
        BattleCursorSelectionResult::selected);
}

void run_ai_movement_continuation_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    using namespace openlegend::model;
    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.valid());
        auto& actor = setup.combatants()[0U].words;
        const auto& target = setup.combatants()[1U].words;
        const auto role_id = static_cast<std::size_t>(actor[combatant_word::role_id]);
        actor[combatant_word::round_value] = 8;
        ranger.roles[role_id].set_word(role_word::speed, 80);
        ranger.roles[role_id].set_word(role_word::physical_power, 10);
        const BattlePathCoord requested_target{
            target[combatant_word::x],
            target[combatant_word::y],
        };
        auto plan = setup.begin_ai_movement_plan(0U, 1, requested_target, 1, 1);
        OL_CHECK(plan.has_value());
        OL_CHECK(plan->selection == BattleAiMovementSelection::generic_reachable_neighbor);
        OL_CHECK(plan->requested_target == requested_target);
        OL_CHECK((plan->source == BattlePathCoord{26, 24}));
        OL_CHECK((plan->destination == BattlePathCoord{26, 25}));
        OL_CHECK(plan->preliminary_target_distance == 2);
        OL_CHECK(plan->preliminary_within_turn_range);
        OL_CHECK(plan->movement_map_build_count == 3);
        OL_CHECK(plan->first_reachability_passed);
        OL_CHECK(plan->second_reachability_passed);
        OL_CHECK(plan->path_marked);
        OL_CHECK(!plan->complete);

        const auto source = plan->source;
        const auto source_index = static_cast<std::size_t>(source.y) * kBattleExtent +
            static_cast<std::size_t>(source.x);
        const auto first_step = setup.advance_ai_movement(*plan);
        OL_CHECK(first_step.has_value());
        OL_CHECK(first_step->moved);
        OL_CHECK(first_step->from == source);
        OL_CHECK((first_step->to == BattlePathCoord{26, 25}));
        OL_CHECK(first_step->remaining_round_value == 7);
        OL_CHECK(first_step->physical_power == 9);
        OL_CHECK(first_step->view_center_x == first_step->to.x);
        OL_CHECK(first_step->view_center_y == first_step->to.y);
        OL_CHECK(first_step->view_x == std::clamp<std::int16_t>(
            static_cast<std::int16_t>(first_step->to.x - 11), 0, 32));
        OL_CHECK(first_step->view_y == std::clamp<std::int16_t>(
            static_cast<std::int16_t>(first_step->to.y - 11), 0, 32));
        OL_CHECK(first_step->wait_ticks == 40);
        OL_CHECK(first_step->render_required);
        OL_CHECK(first_step->present_required);
        OL_CHECK(plan->pathing.value(source) == kBattlePathConsumed);
        OL_CHECK(data.occupancy()[source_index] == -1);
        const auto first_index = static_cast<std::size_t>(first_step->to.y) * kBattleExtent +
            static_cast<std::size_t>(first_step->to.x);
        OL_CHECK(data.occupancy()[first_index] == 0);

        for (std::size_t step = 0U; step < 16U && !plan->complete; ++step) {
            OL_CHECK(setup.advance_ai_movement(*plan).has_value());
        }
        OL_CHECK(plan->complete);
        OL_CHECK(!setup.advance_ai_movement(*plan).has_value());
    }

    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.valid());
        auto& actor = setup.combatants()[0U].words;
        const auto& target = setup.combatants()[1U].words;
        actor[combatant_word::round_value] = 20;
        const BattlePathCoord requested_target{
            target[combatant_word::x],
            target[combatant_word::y],
        };
        const auto aligned = setup.begin_ai_movement_plan(0U, 1, requested_target, 2, 3);
        OL_CHECK(aligned.has_value());
        OL_CHECK(aligned->preliminary_within_turn_range);
        OL_CHECK(aligned->selection == BattleAiMovementSelection::aligned_range_layer);
        OL_CHECK(aligned->selected_distance_layer == 3);
        OL_CHECK((aligned->destination == BattlePathCoord{23, 26}));
        OL_CHECK(aligned->movement_map_build_count == 2);
        OL_CHECK(aligned->first_reachability_passed);
        OL_CHECK(aligned->second_reachability_passed);
        OL_CHECK(aligned->path_marked);

        const auto radial = setup.begin_ai_movement_plan(0U, 1, requested_target, 3, 3);
        OL_CHECK(radial.has_value());
        OL_CHECK(radial->selection == BattleAiMovementSelection::range_layer);
        OL_CHECK(radial->selected_distance_layer == 3);
        OL_CHECK((radial->destination == BattlePathCoord{25, 24}));
        OL_CHECK(radial->movement_map_build_count == 2);
        OL_CHECK(radial->first_reachability_passed);
        OL_CHECK(radial->second_reachability_passed);
        OL_CHECK(radial->path_marked);
    }

    {
        auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
        BattleData data{data_root, 4};
        BattleSetup setup{data, ranger};
        OL_CHECK(setup.valid());
        auto& actor = setup.combatants()[0U].words;
        const auto& target = setup.combatants()[1U].words;
        actor[combatant_word::round_value] = 1;
        const BattlePathCoord requested_target{
            target[combatant_word::x],
            target[combatant_word::y],
        };
        auto plan = setup.begin_ai_movement_plan(0U, -1, requested_target, 0, 0);
        OL_CHECK(plan.has_value());
        OL_CHECK(plan->path_marked);
        OL_CHECK((plan->destination == BattlePathCoord{26, 25}));
        OL_CHECK(plan->movement_map_build_count == 3);
        const auto step = setup.advance_ai_movement(*plan);
        OL_CHECK(step.has_value());
        OL_CHECK(step->remaining_round_value == 0);
        OL_CHECK(step->complete);
        OL_CHECK(plan->complete);
    }
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

    BattleRenderer renderer{data_root, render_data.battlefield_id()};
    openlegend::render::IndexedFramebuffer framebuffer;
    OL_CHECK(renderer.valid());
    OL_CHECK(!renderer.render(*plan, framebuffer));
    OL_CHECK(renderer.load_battlefield_assets());
    OL_CHECK(!renderer.render(*plan, framebuffer));
    OL_CHECK(renderer.load_effect_assets());
    OL_CHECK(renderer.load_battle_assets());
    OL_CHECK(renderer.render(*plan, framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == 0x7d8a5211fe8c4eb0ULL);
    const auto status_panel = render_setup.status_panel_plan(0U);
    OL_CHECK(status_panel.has_value());
    OL_CHECK(renderer.render_status_panel(*status_panel, framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == 0x630a82d57e1d8715ULL);

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

void run_player_action_availability_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = make_ranger({0, 2, 3, -1, -1, -1});
    auto& role = ranger.roles[1U];
    role.set_word(role_word::magic_id_begin, 5);
    ranger.magics[5U].set_word(magic_word::need_mp, 25);
    BattleData data{data_root, 4};
    BattleSetup setup{data, ranger};
    OL_CHECK(setup.valid());
    auto& actor = setup.combatants()[0U].words;

    role.set_word(role_word::physical_power, 5);
    actor[combatant_word::round_value] = 0;
    auto availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK((availability->available ==
              std::array<std::int16_t, 10>{0, 0, 0, 0, 0, 1, 1, 1, 1, 1}));
    OL_CHECK(availability->available_count == 5);

    role.set_word(role_word::physical_power, 6);
    actor[combatant_word::round_value] = 1;
    availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK(availability->available[0U] == 1);
    OL_CHECK(availability->available_count == 6);

    role.set_word(role_word::physical_power, 11);
    role.set_word(role_word::mp, 24);
    role.set_word(role_word::use_poison, 19);
    availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK(availability->available[1U] == 0);
    OL_CHECK(availability->available[2U] == 0);
    role.set_word(role_word::mp, 25);
    role.set_word(role_word::use_poison, 20);
    availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK(availability->available[1U] == 1);
    OL_CHECK(availability->available[2U] == 1);

    role.set_word(role_word::physical_power, 50);
    role.set_word(role_word::detoxification, 20);
    role.set_word(role_word::medicine, 20);
    availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK(availability->available[3U] == 0);
    OL_CHECK(availability->available[4U] == 0);
    role.set_word(role_word::physical_power, 51);
    availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK((availability->available ==
              std::array<std::int16_t, 10>{1, 1, 1, 1, 1, 1, 1, 1, 1, 1}));
    OL_CHECK(availability->available_count == 10);

    role.set_word(role_word::magic_id_begin, 0);
    role.set_word(role_word::mp, 999);
    availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK(availability->available[1U] == 0);
    role.set_word(role_word::mp, 1'000);
    availability = setup.player_action_availability(0U);
    OL_CHECK(availability.has_value());
    OL_CHECK(availability->available[1U] == 1);
}

void run_player_support_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-player-effects.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    auto& actor = ranger->roles[1U];
    auto& enemy = ranger->roles[3U];
    actor.set_word(role_word::magic_id_begin, 0);
    actor.set_word(role_word::frame_begin, 2);
    actor.set_word(role_word::frame_begin + 5U, 1);
    actor.set_word(role_word::frame_begin + 10U, 1);
    ranger->magics[0U].set_word(magic_word::sound_id, 7);

    const auto reach_player_action = [&](BattleSession& session) {
        OL_CHECK(session.valid());
        finish_battle_entry_fade(session);
        OL_CHECK(session.render(*framebuffer));
        session.finish_presented_tick(400U);
        for (std::size_t frame = 0U; frame < session.fade_frame_count(); ++frame) {
            OL_CHECK(session.render(*framebuffer));
            session.finish_presented_tick(400U);
        }
        OL_CHECK(session.phase() == BattleSessionPhase::round_start);
        session.advance(400U);
        OL_CHECK(session.phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session.render(*framebuffer));
        session.finish_presented_tick(400U);
        finish_player_menu_redraw(session);
        OL_CHECK(session.phase() == BattleSessionPhase::player_action);
    };

    const auto run_effect = [&](const BattlePlayerAction action,
                                const std::size_t ordinal,
                                const std::size_t target_down_steps,
                                const std::int16_t effect_id,
                                const std::size_t expected_magic_frames,
                                const std::uint32_t initial_tick) {
        openlegend::random::LegacyRandom action_random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, action_random, 4, false);
        reach_player_action(*session);
        for (std::size_t step = 0U; step < ordinal; ++step) {
            OL_CHECK(session->handle_key(0x98U) ==
                     BattleSessionInputResult::action_changed);
        }
        OL_CHECK(session->handle_key(0x20U) ==
                 BattleSessionInputResult::action_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::player_targeting_select);
        for (std::size_t step = 0U; step < target_down_steps; ++step) {
            OL_CHECK(session->handle_key(0x98U) ==
                     BattleSessionInputResult::cursor_changed);
        }
        OL_CHECK(session->handle_key(0x0DU) ==
                 BattleSessionInputResult::cursor_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::player_magic_frame_present);
        OL_CHECK(session->player_action_menu().selected_action ==
                 static_cast<std::int16_t>(action));
        if (action == BattlePlayerAction::use_poison) {
            OL_CHECK(enemy.word(role_word::poison) == 7);
            OL_CHECK(actor.word(role_word::physical_power) == 100);
        } else if (action == BattlePlayerAction::detoxification) {
            OL_CHECK(actor.word(role_word::poison) == 10);
            OL_CHECK(actor.word(role_word::physical_power) == 100);
        } else {
            OL_CHECK(actor.word(role_word::hp) == 77);
            OL_CHECK(actor.word(role_word::hurt) == 0);
            OL_CHECK(actor.word(role_word::physical_power) == 98);
        }
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::sprite] >=
                 2 * kBattleFightPointerBase);
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{
                      {BattleAudioBank::attack, 7},
                      {BattleAudioBank::effect, effect_id}}));

        const auto effect_plan = BattleSetup::effect_animation_plan(effect_id);
        OL_CHECK(effect_plan.has_value());
        std::uint32_t tick = initial_tick;
        std::size_t magic_frames = 0U;
        while (session->phase() == BattleSessionPhase::player_magic_frame_present &&
               magic_frames < 100U) {
            OL_CHECK(session->render(*framebuffer));
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::player_magic_wait);
            session->advance(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::player_magic_wait);
            session->advance(++tick);
            ++magic_frames;
        }
        OL_CHECK(effect_plan->frames.size() == expected_magic_frames);
        OL_CHECK(magic_frames == expected_magic_frames);
        OL_CHECK(session->phase() == BattleSessionPhase::player_damage_frame_present);
        OL_CHECK(session->take_audio_commands().empty());

        std::size_t damage_frames = 0U;
        while (session->phase() == BattleSessionPhase::player_damage_frame_present &&
               damage_frames < 20U) {
            OL_CHECK(session->render(*framebuffer));
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::player_damage_wait);
            session->advance(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::player_damage_wait);
            session->advance(++tick);
            ++damage_frames;
        }
        OL_CHECK(damage_frames == 10U);
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 1);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::attack_counter] == 1);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::sprite] <
                 2 * kBattleEffectPointerBase);
    };

    actor.set_word(role_word::hp, 100);
    actor.set_word(role_word::maximum_hp, 100);
    actor.set_word(role_word::physical_power, 100);
    actor.set_word(role_word::use_poison, 30);
    actor.set_word(role_word::detoxification, 30);
    actor.set_word(role_word::medicine, 30);
    enemy.set_word(role_word::hp, 100);
    enemy.set_word(role_word::maximum_hp, 100);
    enemy.set_word(role_word::anti_poison, 0);
    enemy.set_word(role_word::poison, 0);
    run_effect(BattlePlayerAction::use_poison, 0U, 2U, 30, 11U, 500U);
    OL_CHECK(enemy.word(role_word::poison) == 7);
    OL_CHECK(actor.word(role_word::physical_power) == 98);

    actor.set_word(role_word::hp, 100);
    actor.set_word(role_word::maximum_hp, 100);
    actor.set_word(role_word::poison, 20);
    actor.set_word(role_word::physical_power, 100);
    run_effect(BattlePlayerAction::detoxification, 1U, 0U, 36, 9U, 600U);
    OL_CHECK(actor.word(role_word::poison) == 10);
    OL_CHECK(actor.word(role_word::physical_power) == 98);

    actor.set_word(role_word::hp, 50);
    actor.set_word(role_word::maximum_hp, 100);
    actor.set_word(role_word::hurt, 20);
    actor.set_word(role_word::physical_power, 100);
    run_effect(BattlePlayerAction::medicine, 2U, 0U, 0, 10U, 700U);
    OL_CHECK(actor.word(role_word::hp) == 77);
    OL_CHECK(actor.word(role_word::hurt) == 0);
    OL_CHECK(actor.word(role_word::physical_power) == 96);

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find(
                 "battle player target effect ready id=4 slot=0 action=2 target=26,26 effect=30 damage_kind=2 hits=1") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle player target effect ready id=4 slot=0 action=3 target=26,24 effect=36 damage_kind=3 hits=1") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle player target effect ready id=4 slot=0 action=4 target=26,24 effect=0 damage_kind=4 hits=1") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player magic frame presented id=4") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player damage frame presented id=4") !=
             std::string::npos);
    const auto count_occurrences = [&](const std::string_view needle) {
        std::size_t count = 0U;
        for (std::size_t offset = 0U;
             (offset = log_text.find(needle, offset)) != std::string::npos;
             offset += needle.size()) {
            ++count;
        }
        return count;
    };
    OL_CHECK(count_occurrences("battle player magic frame presented id=4") == 30U);
    OL_CHECK(count_occurrences("battle player damage frame presented id=4") == 30U);
    OL_CHECK(count_occurrences(" flash=true") == 4U);
    OL_CHECK(count_occurrences(" flash=false") == 26U);
    OL_CHECK(log_text.find("battle player target effect complete id=4 slot=0 action=2") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player target effect complete id=4 slot=0 action=3") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player target effect complete id=4 slot=0 action=4") !=
             std::string::npos);
}

void run_player_item_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-player-items.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    const auto reach_player_action = [&](BattleSession& session) {
        OL_CHECK(session.valid());
        finish_battle_entry_fade(session);
        OL_CHECK(session.render(*framebuffer));
        session.finish_presented_tick(800U);
        for (std::size_t frame = 0U; frame < session.fade_frame_count(); ++frame) {
            OL_CHECK(session.render(*framebuffer));
            session.finish_presented_tick(800U);
        }
        OL_CHECK(session.phase() == BattleSessionPhase::round_start);
        session.advance(800U);
        OL_CHECK(session.phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session.render(*framebuffer));
        session.finish_presented_tick(800U);
        finish_player_menu_redraw(session);
        OL_CHECK(session.phase() == BattleSessionPhase::player_action);
    };
    const auto clear_inventory = [](openlegend::model::RangerState& ranger) {
        for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
            ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
        }
    };
    const auto set_item_text = [](
                                   openlegend::model::ItemRecord& item,
                                   const std::span<const std::uint8_t> name,
                                   const std::span<const std::uint8_t> introduction) {
        std::copy(name.begin(), name.end(), item.bytes.begin() + 2);
        std::copy(name.begin(), name.end(), item.bytes.begin() + 22);
        std::copy(introduction.begin(), introduction.end(), item.bytes.begin() + 42);
    };

    std::uint64_t item_menu_hash = 0U;
    std::uint64_t item_menu_single_hash = 0U;
    std::uint64_t item_effect_hash = 0U;
    std::uint64_t throwing_prelude_hash = 0U;
    std::uint64_t throwing_effect_hash = 0U;
    std::uint64_t throwing_damage_hash = 0U;
    {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        clear_inventory(*ranger);
        auto& actor = ranger->roles[1U];
        actor.set_word(role_word::hp, 50);
        actor.set_word(role_word::maximum_hp, 100);
        actor.set_word(role_word::hurt, 0);
        actor.set_word(role_word::physical_power, 5);
        ranger->roles[3U].set_word(role_word::hp, 100);
        ranger->roles[3U].set_word(role_word::maximum_hp, 100);
        auto& item = ranger->items[10U];
        item.set_word(item_word::show_introduction, 1);
        item.set_word(item_word::item_type, 3);
        item.set_word(item_word::add_hp, 20);
        constexpr std::array<std::uint8_t, 8> kName{
            0xA5U, 0xD5U, 0xB4U, 0x50U, 0xA4U, 0xF3U, 0xC2U, 0xFBU};
        constexpr std::array<std::uint8_t, 8> kIntroduction{
            0xA5U, 0xCDU, 0xA9U, 0x52U, 0xB5U, 0xEAU, 0xAEU, 0x7AU};
        set_item_text(item, kName, kIntroduction);
        ranger->header.set_inventory(0U, openlegend::model::ItemId{10}, 2);
        for (std::size_t slot = 1U; slot < 16U; ++slot) {
            const auto item_id = static_cast<std::size_t>(10U + slot);
            ranger->items[item_id].set_word(item_word::show_introduction, 1);
            ranger->items[item_id].set_word(item_word::item_type, 3);
            ranger->header.set_inventory(
                slot,
                openlegend::model::ItemId{static_cast<std::int16_t>(item_id)},
                1);
        }

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        reach_player_action(*session);
        OL_CHECK(session->handle_key(0x0DU) ==
                 BattleSessionInputResult::action_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::player_item_selection);
        OL_CHECK(session->player_item_selection() != nullptr);
        OL_CHECK(session->player_item_selection()->count == 16);
        OL_CHECK(session->player_item_page() == 0);
        OL_CHECK(session->player_item_row() == 0);
        OL_CHECK(session->player_item_column() == 0);
        OL_CHECK(session->handle_key(0x9CU) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->player_item_column() == 1);
        OL_CHECK(session->handle_key(0x9AU) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->player_item_column() == 0);
        OL_CHECK(session->handle_key(0x99U) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->player_item_page() == 3);
        OL_CHECK(session->handle_key(0x9FU) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->player_item_page() == 0);
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->player_item_page() == 1);
        OL_CHECK(session->player_item_row() == 2);
        OL_CHECK(session->handle_key(0x9EU) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->handle_key(0x9EU) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->handle_key(0x9EU) == BattleSessionInputResult::item_changed);
        OL_CHECK(session->player_item_page() == 0);
        OL_CHECK(session->player_item_row() == 0);
        OL_CHECK(session->render(*framebuffer));
        item_menu_hash = fnv1a_bytes(framebuffer->pixels());
        ranger->header.set_inventory(0U, openlegend::model::ItemId{10}, 1);
        OL_CHECK(session->render(*framebuffer));
        item_menu_single_hash = fnv1a_bytes(framebuffer->pixels());
        ranger->header.set_inventory(0U, openlegend::model::ItemId{10}, 2);
        session->finish_presented_tick(800U);

        OL_CHECK(session->handle_key(0x20U) == BattleSessionInputResult::item_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::player_item_effect_present);
        OL_CHECK(std::ranges::count(session->setup().attack_effects(), 1) == 0);
        OL_CHECK(actor.word(role_word::hp) > 50);
        OL_CHECK(ranger->header.inventory_item(0U).value == 10);
        OL_CHECK(ranger->header.inventory_count(0U) == 2);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 0);
        OL_CHECK(session->render(*framebuffer));
        item_effect_hash = fnv1a_bytes(framebuffer->pixels());
        session->finish_presented_tick(801U);
        OL_CHECK(session->phase() == BattleSessionPhase::player_item_effect_wait);
        OL_CHECK(ranger->header.inventory_item(0U).value == 10);
        OL_CHECK(ranger->header.inventory_count(0U) == 1);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 0);
        OL_CHECK(session->handle_key(0x98U) ==
                 BattleSessionInputResult::item_effect_acknowledged);
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 1);
    }

    {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        clear_inventory(*ranger);
        auto& actor = ranger->roles[1U];
        actor.set_word(role_word::hp, 100);
        actor.set_word(role_word::maximum_hp, 100);
        actor.set_word(role_word::physical_power, 5);
        ranger->roles[3U].set_word(role_word::hp, 100);
        ranger->roles[3U].set_word(role_word::maximum_hp, 100);
        auto& item = ranger->items[11U];
        item.set_word(item_word::show_introduction, 1);
        item.set_word(item_word::item_type, 3);
        ranger->header.set_inventory(0U, openlegend::model::ItemId{11}, 1);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        reach_player_action(*session);
        OL_CHECK(session->handle_key(0x0DU) ==
                 BattleSessionInputResult::action_selected);
        OL_CHECK(session->handle_key(0x20U) == BattleSessionInputResult::item_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 1);
        OL_CHECK(std::ranges::count(session->setup().attack_effects(), 1) == 0);
        OL_CHECK(ranger->header.inventory_item(0U).value == 11);
        OL_CHECK(ranger->header.inventory_count(0U) == 1);
        OL_CHECK(random.state() == 1U);
    }

    {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        clear_inventory(*ranger);
        auto& actor = ranger->roles[1U];
        auto& target = ranger->roles[3U];
        actor.set_word(role_word::hp, 100);
        actor.set_word(role_word::maximum_hp, 100);
        actor.set_word(role_word::physical_power, 5);
        actor.set_word(role_word::hidden_weapon, 20);
        target.set_word(role_word::hp, 100);
        target.set_word(role_word::maximum_hp, 200);
        target.set_word(role_word::hurt, 40);
        target.set_word(role_word::poison, 10);
        target.set_word(role_word::anti_poison, 5);
        auto& item = ranger->items[102U];
        item.set_word(item_word::show_introduction, 1);
        item.set_word(item_word::item_type, 4);
        item.set_word(item_word::hidden_weapon_effect_id, 30);
        item.set_word(item_word::add_hp, -40);
        item.set_word(item_word::add_poison, 40);
        constexpr std::array<std::uint8_t, 8> kName{
            0xACU, 0x72U, 0xE3U, 0xB0U, 0xBEU, 0xA4U, 0x00U, 0x00U};
        constexpr std::array<std::uint8_t, 8> kIntroduction{
            0xA7U, 0x74U, 0xACU, 0x72U, 0xA4U, 0xA7U, 0x00U, 0x00U};
        set_item_text(item, kName, kIntroduction);
        ranger->header.set_inventory(0U, openlegend::model::ItemId{102}, 1);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        reach_player_action(*session);
        OL_CHECK(session->handle_key(0x0DU) ==
                 BattleSessionInputResult::action_selected);
        OL_CHECK(session->handle_key(0x0DU) == BattleSessionInputResult::item_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::player_targeting_select);
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::cursor_changed);
        OL_CHECK((session->active_cursor() == BattlePathCoord{26, 25}));
        OL_CHECK(session->handle_key(0x20U) == BattleSessionInputResult::cursor_selected);
        finish_player_menu_redraw(*session);
        OL_CHECK(session->phase() == BattleSessionPhase::player_action);
        OL_CHECK(session->player_item_selection() == nullptr);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 0);
        OL_CHECK(ranger->header.inventory_count(0U) == 1);

        OL_CHECK(session->handle_key(0x0DU) ==
                 BattleSessionInputResult::action_selected);
        OL_CHECK(session->handle_key(0x0DU) == BattleSessionInputResult::item_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::player_targeting_select);
        OL_CHECK(session->handle_key(0x1BU) == BattleSessionInputResult::cursor_cancelled);
        finish_player_menu_redraw(*session);
        OL_CHECK(session->phase() == BattleSessionPhase::player_action);
        OL_CHECK(session->player_item_selection() == nullptr);
        OL_CHECK(ranger->header.inventory_count(0U) == 1);

        OL_CHECK(session->handle_key(0x0DU) ==
                 BattleSessionInputResult::action_selected);
        OL_CHECK(session->handle_key(0x0DU) == BattleSessionInputResult::item_selected);
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::cursor_changed);
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::cursor_changed);
        OL_CHECK((session->active_cursor() == BattlePathCoord{26, 26}));
        OL_CHECK(session->handle_key(0x20U) == BattleSessionInputResult::cursor_selected);
        OL_CHECK(session->phase() == BattleSessionPhase::player_effect_prelude_present);
        OL_CHECK(target.word(role_word::hp) == 79);
        OL_CHECK(target.word(role_word::hurt) == 45);
        OL_CHECK(target.word(role_word::poison) == 12);
        OL_CHECK(ranger->header.inventory_count(0U) == 1);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 0);
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{{BattleAudioBank::attack, 13}}));

        std::uint32_t tick = 900U;
        OL_CHECK(session->render(*framebuffer));
        throwing_prelude_hash = fnv1a_bytes(framebuffer->pixels());
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::player_effect_prelude_wait);
        session->advance(tick);
        std::size_t prelude_tick_changes = 0U;
        while (session->phase() == BattleSessionPhase::player_effect_prelude_wait &&
               prelude_tick_changes < 20U) {
            session->advance(++tick);
            ++prelude_tick_changes;
        }
        OL_CHECK(prelude_tick_changes == 3U);
        OL_CHECK(session->phase() == BattleSessionPhase::player_magic_frame_present);
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{{BattleAudioBank::effect, 30}}));

        std::size_t effect_frames = 0U;
        while (session->phase() == BattleSessionPhase::player_magic_frame_present &&
               effect_frames < 100U) {
            OL_CHECK(session->render(*framebuffer));
            if (effect_frames == 0U) {
                throwing_effect_hash = fnv1a_bytes(framebuffer->pixels());
            }
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::player_magic_wait);
            session->advance(tick);
            session->advance(++tick);
            ++effect_frames;
        }
        OL_CHECK(effect_frames == 11U);
        OL_CHECK(session->phase() == BattleSessionPhase::player_damage_frame_present);

        std::size_t damage_frames = 0U;
        while (session->phase() == BattleSessionPhase::player_damage_frame_present &&
               damage_frames < 20U) {
            OL_CHECK(session->render(*framebuffer));
            if (damage_frames == 0U) {
                throwing_damage_hash = fnv1a_bytes(framebuffer->pixels());
            }
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::player_damage_wait);
            session->advance(tick);
            session->advance(++tick);
            ++damage_frames;
        }
        OL_CHECK(damage_frames == 10U);
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 1);
        OL_CHECK(ranger->header.inventory_item(0U).value == -1);
        OL_CHECK(ranger->header.inventory_count(0U) == 0);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::sprite] <
                 2 * kBattleEffectPointerBase);
    }

    const auto hash_path = log_path.parent_path() / "b8-battle-player-items.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    hash_file << std::hex << item_menu_hash << '\n' << item_menu_single_hash << '\n'
              << item_effect_hash << '\n' << throwing_prelude_hash << '\n'
              << throwing_effect_hash << '\n' << throwing_damage_hash << '\n';
    hash_file.close();
    OL_CHECK(item_menu_hash == 0x68c3b70dfec20bbaULL);
    OL_CHECK(item_menu_single_hash == 0x17b6845a718a6cf8ULL);
    OL_CHECK(item_effect_hash == 0xd518fb664f3e0e3cULL);
    OL_CHECK(throwing_prelude_hash == 0xdbee20f394fd7219ULL);
    OL_CHECK(throwing_effect_hash == 0x370a4078e9de6172ULL);
    OL_CHECK(throwing_damage_hash == 0xd41fa068222d444aULL);

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle player item selection ready id=4 slot=0 items=16") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player item effect presented id=4 slot=0 item=10 effects=1") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player item effect acknowledged id=4 slot=0 item=10 effects=1") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player item had no visible effect id=4 slot=0 item=11 consumed=false action_complete=true") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player throwing-weapon target rejected id=4 slot=0 target=26,25") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player throwing-weapon effect ready id=4 slot=0 inventory_slot=0 target=26,26 effect=30 damage=21 frames=11") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player effect prelude presented id=4 slot=0 wait_tick_changes=3") !=
             std::string::npos);
}

void run_player_status_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-player-status.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*ranger, {1, 2, -1, -1, -1, -1});
    auto& actor = ranger->roles[1U];
    auto& status_role = ranger->roles[2U];
    constexpr std::array<std::uint8_t, 5U> kActorName{0xA4U, 0x40U, 0xA4U, 0x42U, 0U};
    constexpr std::array<std::uint8_t, 7U> kStatusName{
        0xA4U, 0x44U, 0xA4U, 0x46U, 0xA4U, 0x48U, 0U};
    std::ranges::copy(kActorName, actor.bytes.begin() + role_word::name_byte);
    std::ranges::copy(kStatusName, status_role.bytes.begin() + role_word::name_byte);
    actor.set_word(role_word::physical_power, 0);
    actor.set_word(role_word::mp, 0);
    actor.set_word(role_word::use_poison, 0);
    actor.set_word(role_word::detoxification, 0);
    actor.set_word(role_word::medicine, 0);
    for (std::size_t slot = 0U; slot < role_word::magic_count; ++slot) {
        actor.set_word(role_word::magic_id_begin + slot, 0);
    }

    status_role.set_word(role_word::level, 5);
    status_role.set_word(role_word::experience, 1'234);
    status_role.set_word(role_word::hp, 87);
    status_role.set_word(role_word::maximum_hp, 123);
    status_role.set_word(role_word::hurt, 67);
    status_role.set_word(role_word::poison, 50);
    status_role.set_word(role_word::physical_power, 88);
    status_role.set_word(role_word::mp_type, 3);
    status_role.set_word(role_word::mp, 66);
    status_role.set_word(role_word::maximum_mp, 99);
    status_role.set_word(role_word::attack, 101);
    status_role.set_word(role_word::defence, 102);
    status_role.set_word(role_word::speed, 103);
    status_role.set_word(role_word::medicine, 104);
    status_role.set_word(role_word::use_poison, 105);
    status_role.set_word(role_word::detoxification, 106);
    status_role.set_word(role_word::fist, 107);
    status_role.set_word(role_word::sword, 108);
    status_role.set_word(role_word::knife, 109);
    status_role.set_word(role_word::unusual, 110);
    status_role.set_word(role_word::hidden_weapon, 111);
    status_role.set_word(role_word::iq, 75);
    status_role.set_word(role_word::equipment_begin, 10);
    status_role.set_word(role_word::equipment_begin + 1U, 11);
    status_role.set_word(role_word::practice_item, 12);
    status_role.set_word(role_word::item_experience, 15);
    status_role.set_word(role_word::magic_id_begin, 5);
    status_role.set_word(role_word::magic_level_begin, 800);
    ranger->items[10U].set_word(item_word::add_attack, 7);
    ranger->items[10U].set_word(item_word::add_defence, 5);
    ranger->items[10U].set_word(item_word::add_speed, 3);
    ranger->items[11U].set_word(item_word::add_attack, 11);
    ranger->items[11U].set_word(item_word::add_defence, 13);
    ranger->items[11U].set_word(item_word::add_speed, 17);
    ranger->items[12U].set_word(item_word::magic_id, 5);
    ranger->items[12U].set_word(item_word::need_experience, 30);
    constexpr std::array<std::uint8_t, 5U> kEquipmentName0{
        0xA4U, 0x4AU, 0xA4U, 0x4CU, 0U};
    constexpr std::array<std::uint8_t, 5U> kEquipmentName1{
        0xA4U, 0x4EU, 0xA4U, 0x50U, 0U};
    constexpr std::array<std::uint8_t, 5U> kPracticeName{
        0xA4U, 0x52U, 0xA4U, 0x54U, 0U};
    constexpr std::array<std::uint8_t, 5U> kMagicName{
        0xA4U, 0x56U, 0xA4U, 0x58U, 0U};
    std::ranges::copy(
        kEquipmentName0,
        ranger->items[10U].bytes.begin() + 2U * item_word::secondary_name_begin);
    std::ranges::copy(
        kEquipmentName1,
        ranger->items[11U].bytes.begin() + 2U * item_word::secondary_name_begin);
    std::ranges::copy(
        kPracticeName,
        ranger->items[12U].bytes.begin() + 2U * item_word::secondary_name_begin);
    std::ranges::copy(
        kMagicName,
        ranger->magics[5U].bytes.begin() + magic_word::name_byte);

    status_role.set_word(role_word::experience, -32'767);
    status_role.set_word(role_word::item_experience, -1);
    BattleRenderer unsigned_status_renderer{data_root, 0};
    openlegend::render::IndexedFramebuffer unsigned_status_page_0;
    openlegend::render::IndexedFramebuffer unsigned_status_page_1;
    OL_CHECK(unsigned_status_renderer.valid());
    OL_CHECK(unsigned_status_renderer.render_character_status(
        *ranger, 2, 0U, unsigned_status_page_0));
    OL_CHECK(unsigned_status_renderer.render_character_status(
        *ranger, 2, 1U, unsigned_status_page_1));
    const auto unsigned_status_page_0_hash = fnv1a_bytes(unsigned_status_page_0.pixels());
    const auto unsigned_status_page_1_hash = fnv1a_bytes(unsigned_status_page_1.pixels());
    if (unsigned_status_page_0_hash != 0x77C7894D66E05F7CULL) {
        std::cerr << "unsigned_status_page_0_hash=0x" << std::hex
                  << unsigned_status_page_0_hash << std::dec << '\n';
    }
    if (unsigned_status_page_1_hash != 0x943CBB1B8EE2C566ULL) {
        std::cerr << "unsigned_status_page_1_hash=0x" << std::hex
                  << unsigned_status_page_1_hash << std::dec << '\n';
    }
    OL_CHECK(unsigned_status_page_0_hash == 0x77C7894D66E05F7CULL);
    OL_CHECK(unsigned_status_page_1_hash == 0x943CBB1B8EE2C566ULL);
    status_role.set_word(role_word::experience, 1'234);
    status_role.set_word(role_word::item_experience, 15);
    const auto status_role_before = status_role.bytes;

    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    openlegend::random::LegacyRandom random{1U};
    auto session = std::make_unique<BattleSession>(
        data_root, *ranger, random, 4, false);
    OL_CHECK(session->valid());
    finish_battle_entry_fade(*session);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(900U);
    for (std::size_t frame = 0U; frame < session->fade_frame_count(); ++frame) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(900U);
    }
    session->advance(900U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(900U);
    finish_player_menu_redraw(*session);
    OL_CHECK(session->phase() == BattleSessionPhase::player_action);
    OL_CHECK(session->player_action_menu().available_count == 5U);
    OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::action_changed);
    OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::action_changed);
    OL_CHECK(session->player_action_menu().cursor == 2U);
    OL_CHECK(session->handle_key(0x20U) == BattleSessionInputResult::action_selected);
    OL_CHECK(session->phase() == BattleSessionPhase::player_status_selection);
    OL_CHECK(session->player_status_count() == 2U);
    OL_CHECK(session->player_status_cursor() == 0U);
    OL_CHECK(session->handle_key(0x9EU) == BattleSessionInputResult::status_changed);
    OL_CHECK(session->player_status_cursor() == 1U);
    OL_CHECK(session->render(*framebuffer));
    const auto status_selection_hash = fnv1a_bytes(framebuffer->pixels());
    OL_CHECK(session->handle_key(0x1BU) == BattleSessionInputResult::status_cancelled);
    finish_player_menu_redraw(*session);
    OL_CHECK(session->phase() == BattleSessionPhase::player_action);
    OL_CHECK(session->player_action_menu().cursor == 2U);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 0);

    OL_CHECK(session->handle_key(0x20U) == BattleSessionInputResult::action_selected);
    OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::status_changed);
    OL_CHECK(session->handle_key(0x96U) == BattleSessionInputResult::status_selected);
    OL_CHECK(session->phase() == BattleSessionPhase::player_status_page_present);
    OL_CHECK(session->player_status_role_id() == 2);
    OL_CHECK(session->player_status_page() == 0U);
    OL_CHECK(session->render(*framebuffer));
    const auto status_page_0_hash = fnv1a_bytes(framebuffer->pixels());
    session->finish_presented_tick(901U);
    OL_CHECK(session->phase() == BattleSessionPhase::player_status_page_wait);
    OL_CHECK(session->handle_key(0U) == BattleSessionInputResult::ignored);
    OL_CHECK(session->handle_key('A') == BattleSessionInputResult::status_page_advanced);
    OL_CHECK(session->phase() == BattleSessionPhase::player_status_page_present);
    OL_CHECK(session->player_status_page() == 1U);
    OL_CHECK(session->render(*framebuffer));
    const auto status_page_1_hash = fnv1a_bytes(framebuffer->pixels());
    OL_CHECK(status_selection_hash == 0xfa1b21403051335cULL);
    OL_CHECK(status_page_0_hash == 0x1c5e879ce61d5b34ULL);
    OL_CHECK(status_page_1_hash == 0x9592da33a3c151d4ULL);
    session->finish_presented_tick(902U);
    OL_CHECK(session->phase() == BattleSessionPhase::player_status_page_wait);
    OL_CHECK(session->handle_key('B') == BattleSessionInputResult::status_closed);
    finish_player_menu_redraw(*session);
    OL_CHECK(session->phase() == BattleSessionPhase::player_action);
    OL_CHECK(session->player_action_menu().cursor == 2U);
    OL_CHECK(session->player_action_menu().selected_action == -1);
    OL_CHECK(session->current_actor_slot() == 0U);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 0);

    constexpr std::array<std::uint8_t, 2U> kAdditionalStatusConfirmKeys{0x0DU, 0x20U};
    std::uint32_t status_tick = 903U;
    for (const auto confirm_key : kAdditionalStatusConfirmKeys) {
        OL_CHECK(session->handle_key(0x20U) == BattleSessionInputResult::action_selected);
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::status_changed);
        OL_CHECK(session->handle_key(confirm_key) == BattleSessionInputResult::status_selected);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(status_tick++);
        OL_CHECK(session->handle_key('C') ==
                 BattleSessionInputResult::status_page_advanced);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(status_tick++);
        OL_CHECK(session->handle_key('D') == BattleSessionInputResult::status_closed);
        finish_player_menu_redraw(*session);
        OL_CHECK(session->phase() == BattleSessionPhase::player_action);
        OL_CHECK(session->player_action_menu().cursor == 2U);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::action_done] == 0);
    }
    OL_CHECK(status_role.bytes == status_role_before);
    OL_CHECK(random.state() == 1U);

    const auto hash_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-player-status.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "status_selection=0x" << std::hex << status_selection_hash << '\n';
    hash_file << "status_page_0=0x" << std::hex << status_page_0_hash << '\n';
    hash_file << "status_page_1=0x" << std::hex << status_page_1_hash << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle player status selection ready id=4 slot=0 party=2") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player status cursor id=4 slot=0 cursor=1") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player status selection cancelled id=4 slot=0") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player status selected id=4 slot=0 party_slot=1 role=2") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player status page presented id=4 slot=0 role=2 page=0") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player status page advanced id=4 slot=0 role=2") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player status page presented id=4 slot=0 role=2 page=1") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player status closed id=4 slot=0 role=2") !=
             std::string::npos);
}

void run_player_attack_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-player-attacks.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    const auto run_case = [&](const std::int16_t area_type,
                              const std::uint32_t initial_tick,
                              const bool attack_twice,
                              const std::int16_t initial_experience) {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        auto& actor = ranger->roles[1U];
        auto& enemy = ranger->roles[3U];
        actor.set_word(role_word::hp, 500);
        actor.set_word(role_word::maximum_hp, 500);
        actor.set_word(role_word::mp, 20);
        actor.set_word(role_word::maximum_mp, 20);
        actor.set_word(role_word::physical_power, 100);
        actor.set_word(role_word::attack, 50);
        actor.set_word(role_word::speed, 0);
        actor.set_word(role_word::magic_id_begin, 5);
        actor.set_word(role_word::magic_level_begin, initial_experience);
        actor.set_word(role_word::attack_twice, attack_twice ? 1 : 0);
        actor.set_word(role_word::frame_begin, 2);
        actor.set_word(role_word::frame_begin + 5U, 1);
        actor.set_word(role_word::frame_begin + 10U, 1);
        enemy.set_word(role_word::hp, 5'000);
        enemy.set_word(role_word::maximum_hp, 5'000);
        enemy.set_word(role_word::defence, 0);
        enemy.set_word(role_word::anti_poison, 100);
        auto& magic = ranger->magics[5U];
        magic.set_word(magic_word::sound_id, 7);
        magic.set_word(magic_word::magic_type, 0);
        magic.set_word(magic_word::effect_id, 0);
        magic.set_word(magic_word::hurt_type, 0);
        magic.set_word(magic_word::attack_area_type, area_type);
        magic.set_word(magic_word::need_mp, 5);
        magic.set_word(magic_word::attack_begin + 2U, 20);
        magic.set_word(magic_word::attack_begin + 3U, 20);
        magic.set_word(magic_word::select_distance_begin + 2U, 2);
        magic.set_word(magic_word::attack_distance_begin + 2U, 0);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        OL_CHECK(session->valid());
        finish_battle_entry_fade(*session);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(initial_tick);
        for (std::size_t frame = 0U; frame < session->fade_frame_count(); ++frame) {
            OL_CHECK(session->render(*framebuffer));
            session->finish_presented_tick(initial_tick);
        }
        session->advance(initial_tick);
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(initial_tick);
        finish_player_menu_redraw(*session);
        OL_CHECK(session->phase() == BattleSessionPhase::player_action);
        OL_CHECK(session->player_action_menu().available[0U] == 0);
        OL_CHECK(session->player_action_menu().available[1U] == 1);
        OL_CHECK(session->player_action_menu().cursor == 0U);
        OL_CHECK(session->handle_key(0x0DU) ==
                 BattleSessionInputResult::action_selected);

        if (area_type == 0 || area_type == 3) {
            OL_CHECK(session->phase() == BattleSessionPhase::player_targeting_select);
            if (area_type == 0) {
                OL_CHECK(session->handle_key(0x1BU) ==
                         BattleSessionInputResult::cursor_cancelled);
                finish_player_menu_redraw(*session);
                OL_CHECK(session->phase() == BattleSessionPhase::player_action);
                OL_CHECK(session->player_action_menu().cursor == 0U);
                OL_CHECK(session->handle_key(0x0DU) ==
                         BattleSessionInputResult::action_selected);
                OL_CHECK(session->phase() ==
                         BattleSessionPhase::player_targeting_select);
            }
            OL_CHECK(session->handle_key(0x98U) ==
                     BattleSessionInputResult::cursor_changed);
            OL_CHECK(session->handle_key(0x98U) ==
                     BattleSessionInputResult::cursor_changed);
            OL_CHECK((session->active_cursor() == BattlePathCoord{26, 26}));
            OL_CHECK(session->handle_key(0x20U) ==
                     BattleSessionInputResult::cursor_selected);
            OL_CHECK((session->selected_player_target() == BattlePathCoord{26, 26}));
        } else if (area_type == 1) {
            OL_CHECK(session->phase() == BattleSessionPhase::player_attack_direction);
            OL_CHECK(session->render(*framebuffer));
            OL_CHECK(fnv1a_bytes(framebuffer->pixels()) == 0x5e46c805f42677b0ULL);
            OL_CHECK(session->handle_key(0x1BU) == BattleSessionInputResult::ignored);
            OL_CHECK(session->handle_key(0x98U) ==
                     BattleSessionInputResult::direction_selected);
            OL_CHECK(session->setup().combatants()[0U]
                         .words[combatant_word::initial_mode] == 3);
        } else {
            OL_CHECK(area_type == 2);
        }

        OL_CHECK(session->phase() == BattleSessionPhase::player_magic_frame_present);
        OL_CHECK(enemy.word(role_word::hp) < 5'000);
        std::uint32_t tick = initial_tick;
        std::size_t iterations = 0U;
        std::size_t level_wait_tick_changes = 0U;
        while (session->phase() != BattleSessionPhase::actor_present &&
               iterations < 3U) {
            OL_CHECK(session->phase() == BattleSessionPhase::player_magic_frame_present);
            OL_CHECK((session->take_audio_commands() ==
                      std::vector<BattleAudioCommand>{
                          {BattleAudioBank::attack, 7},
                          {BattleAudioBank::effect, 0}}));
            std::size_t magic_frames = 0U;
            while (session->phase() ==
                       BattleSessionPhase::player_magic_frame_present &&
                   magic_frames < 20U) {
                OL_CHECK(session->render(*framebuffer));
                session->finish_presented_tick(tick);
                OL_CHECK(session->phase() == BattleSessionPhase::player_magic_wait);
                session->advance(tick);
                session->advance(++tick);
                ++magic_frames;
            }
            OL_CHECK(magic_frames == 10U);
            OL_CHECK(session->phase() ==
                     BattleSessionPhase::player_damage_frame_present);
            std::size_t damage_frames = 0U;
            while (session->phase() ==
                       BattleSessionPhase::player_damage_frame_present &&
                   damage_frames < 20U) {
                OL_CHECK(session->render(*framebuffer));
                session->finish_presented_tick(tick);
                OL_CHECK(session->phase() == BattleSessionPhase::player_damage_wait);
                session->advance(tick);
                session->advance(++tick);
                ++damage_frames;
            }
            OL_CHECK(damage_frames == 10U);
            OL_CHECK(session->phase() ==
                     BattleSessionPhase::player_attack_commit_present);
            OL_CHECK(session->render(*framebuffer));
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() ==
                     BattleSessionPhase::player_attack_commit_wait);
            session->advance(tick);
            OL_CHECK(session->phase() ==
                     BattleSessionPhase::player_attack_commit_wait);
            session->advance(++tick);
            ++iterations;
            if (session->phase() == BattleSessionPhase::player_attack_level_present) {
                OL_CHECK(session->render(*framebuffer));
                OL_CHECK(fnv1a_bytes(framebuffer->pixels()) ==
                         0x1f0048d1945a4948ULL);
                session->finish_presented_tick(tick);
                OL_CHECK(session->phase() ==
                         BattleSessionPhase::player_attack_level_wait);
                session->advance(tick);
                while (session->phase() ==
                           BattleSessionPhase::player_attack_level_wait &&
                       level_wait_tick_changes < 20U) {
                    session->advance(++tick);
                    ++level_wait_tick_changes;
                }
            }
        }
        const auto expected_iterations = attack_twice ? 2U : 1U;
        OL_CHECK(iterations == expected_iterations);
        OL_CHECK(level_wait_tick_changes ==
                 (initial_experience == 299 ? 13U : 0U));
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(actor.word(role_word::mp) ==
                 static_cast<std::int16_t>(attack_twice ? 5 : 15));
        OL_CHECK(actor.word(role_word::physical_power) == 97);
        OL_CHECK(actor.word(role_word::magic_level_begin) >=
                 initial_experience + static_cast<std::int16_t>(expected_iterations));
        OL_CHECK(actor.word(role_word::magic_level_begin) <=
                 initial_experience +
                     static_cast<std::int16_t>(2U * expected_iterations));
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::action_done] == 1);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::attack_counter] >=
                 static_cast<std::int16_t>(2U * expected_iterations));
    };

    run_case(0, 800U, false, 250);
    run_case(1, 900U, false, 250);
    run_case(2, 1'000U, true, 299);
    run_case(3, 1'100U, false, 250);
    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle player attack ready id=4") != std::string::npos);
    OL_CHECK(log_text.find("battle player attack direction selected id=4") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player attack iteration ready id=4") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player attack commit frame presented id=4") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "iteration=1 area_type=2 hits=1 damage_kind=1 frames=10") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "iteration=1 cost_scale=4 level_up=false") != std::string::npos);
    OL_CHECK(log_text.find("battle player attack iteration committed id=4") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player attack complete id=4") != std::string::npos);
}

void run_ai_attack_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-ai-attack.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
    auto& actor = ranger->roles[1U];
    auto& enemy = ranger->roles[3U];
    actor.set_word(role_word::hp, 500);
    actor.set_word(role_word::maximum_hp, 500);
    actor.set_word(role_word::mp, 20);
    actor.set_word(role_word::maximum_mp, 20);
    actor.set_word(role_word::physical_power, 100);
    actor.set_word(role_word::attack, 50);
    actor.set_word(role_word::speed, 0);
    actor.set_word(role_word::magic_id_begin, 5);
    actor.set_word(role_word::magic_level_begin, 299);
    actor.set_word(role_word::frame_begin, 2);
    actor.set_word(role_word::frame_begin + 5U, 1);
    actor.set_word(role_word::frame_begin + 10U, 1);
    enemy.set_word(role_word::hp, 5'000);
    enemy.set_word(role_word::maximum_hp, 5'000);
    enemy.set_word(role_word::defence, 0);
    enemy.set_word(role_word::anti_poison, 100);
    auto& magic = ranger->magics[5U];
    magic.set_word(magic_word::sound_id, 7);
    magic.set_word(magic_word::magic_type, 0);
    magic.set_word(magic_word::effect_id, 0);
    magic.set_word(magic_word::hurt_type, 0);
    magic.set_word(magic_word::attack_area_type, 1);
    magic.set_word(magic_word::need_mp, 5);
    magic.set_word(magic_word::attack_begin + 2U, 20);
    magic.set_word(magic_word::attack_begin + 3U, 20);
    magic.set_word(magic_word::select_distance_begin + 2U, 2);
    magic.set_word(magic_word::attack_distance_begin + 2U, 0);

    openlegend::random::LegacyRandom random{1U};
    auto session = std::make_unique<BattleSession>(
        data_root, *ranger, random, 4, false);
    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    OL_CHECK(session->valid());
    finish_battle_entry_fade(*session);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'200U);
    for (std::size_t frame = 0U; frame < session->fade_frame_count(); ++frame) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(1'200U);
    }
    session->setup().enable_automatic_mode();
    session->advance(1'200U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::role_id] == 1);
    const auto initial_attack_counter = session->setup().combatants()[0U]
                                            .words[combatant_word::attack_counter];
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'200U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_action);
    session->advance(1'200U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(session->render(*framebuffer));
    const auto ai_prelude_hash = fnv1a_bytes(framebuffer->pixels());
    session->finish_presented_tick(1'200U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_wait);
    const auto actor_hp_before_ai_wait = actor.word(role_word::hp);
    actor.set_word(role_word::hp, 1);
    OL_CHECK(session->render(*framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer->pixels()) == ai_prelude_hash);
    actor.set_word(role_word::hp, actor_hp_before_ai_wait);
    for (std::uint32_t tick = 1'201U; tick < 1'208U; ++tick) {
        session->advance(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_wait);
    }
    session->advance(1'208U);
    OL_CHECK(session->valid());
    OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_frame_present);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(enemy.word(role_word::hp) < 5'000);
    OL_CHECK((session->take_audio_commands() ==
              std::vector<BattleAudioCommand>{
                  {BattleAudioBank::attack, 7},
                  {BattleAudioBank::effect, 0}}));

    std::uint32_t tick = 1'208U;
    std::size_t magic_frames = 0U;
    std::uint64_t first_magic_hash = 0U;
    while (session->phase() == BattleSessionPhase::ai_magic_frame_present &&
           magic_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        if (magic_frames == 0U) {
            first_magic_hash = fnv1a_bytes(framebuffer->pixels());
        }
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_wait);
        session->advance(tick);
        session->advance(++tick);
        ++magic_frames;
    }
    OL_CHECK(magic_frames == 10U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_frame_present);

    std::size_t damage_frames = 0U;
    std::uint64_t first_damage_hash = 0U;
    while (session->phase() == BattleSessionPhase::ai_damage_frame_present &&
           damage_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        if (damage_frames == 0U) {
            first_damage_hash = fnv1a_bytes(framebuffer->pixels());
        }
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_wait);
        session->advance(tick);
        session->advance(++tick);
        ++damage_frames;
    }
    OL_CHECK(damage_frames == 10U);
    const auto attack_counter_after_damage = session->setup().combatants()[0U]
                                                 .words[combatant_word::attack_counter];
    OL_CHECK(attack_counter_after_damage > initial_attack_counter);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_attack_commit_present);
    OL_CHECK(session->render(*framebuffer));
    const auto commit_hash = fnv1a_bytes(framebuffer->pixels());
    session->finish_presented_tick(tick);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_attack_commit_wait);
    session->advance(tick);
    session->advance(++tick);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_attack_level_present);
    OL_CHECK(session->render(*framebuffer));
    const auto level_hash = fnv1a_bytes(framebuffer->pixels());
    session->finish_presented_tick(tick);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_attack_level_wait);
    session->advance(tick);
    std::size_t level_wait_tick_changes = 0U;
    while (session->phase() == BattleSessionPhase::ai_attack_level_wait &&
           level_wait_tick_changes < 20U) {
        session->advance(++tick);
        ++level_wait_tick_changes;
    }
    OL_CHECK(level_wait_tick_changes == 13U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->current_actor_slot() == 1U);
    OL_CHECK(actor.word(role_word::mp) == 15);
    OL_CHECK(actor.word(role_word::physical_power) == 97);
    OL_CHECK(actor.word(role_word::magic_level_begin) >= 300);
    OL_CHECK(actor.word(role_word::magic_level_begin) <= 301);
    OL_CHECK(session->setup().combatants()[0U]
                 .words[combatant_word::action_done] == 1);
    const auto final_attack_counter = session->setup().combatants()[0U]
                                          .words[combatant_word::attack_counter];
    OL_CHECK(final_attack_counter == attack_counter_after_damage + 2);
    OL_CHECK(random.state() == 3'655'513'600U);
    OL_CHECK(ai_prelude_hash == 0xb02104139829a80dULL);
    OL_CHECK(first_magic_hash == 0xe1d1b3cff84bc0c4ULL);
    OL_CHECK(first_damage_hash == 0x04c528de57fbffa0ULL);
    OL_CHECK(commit_hash == 0xdbee20f394fd7219ULL);
    OL_CHECK(level_hash == 0xed97f52f9bedb836ULL);

    auto movement_ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*movement_ranger, {0, 2, 3, -1, -1, -1});
    auto& movement_actor = movement_ranger->roles[1U];
    auto& movement_enemy = movement_ranger->roles[3U];
    movement_actor.set_word(role_word::hp, 500);
    movement_actor.set_word(role_word::maximum_hp, 500);
    movement_actor.set_word(role_word::mp, 20);
    movement_actor.set_word(role_word::maximum_mp, 20);
    movement_actor.set_word(role_word::physical_power, 100);
    movement_actor.set_word(role_word::attack, 50);
    movement_actor.set_word(role_word::speed, 30);
    movement_actor.set_word(role_word::magic_id_begin, 5);
    movement_actor.set_word(role_word::magic_level_begin, 250);
    movement_actor.set_word(role_word::frame_begin, 2);
    movement_actor.set_word(role_word::frame_begin + 5U, 1);
    movement_actor.set_word(role_word::frame_begin + 10U, 1);
    movement_enemy.set_word(role_word::hp, 5'000);
    movement_enemy.set_word(role_word::maximum_hp, 5'000);
    movement_enemy.set_word(role_word::defence, 0);
    movement_enemy.set_word(role_word::anti_poison, 100);
    auto& movement_magic = movement_ranger->magics[5U];
    movement_magic.set_word(magic_word::sound_id, 7);
    movement_magic.set_word(magic_word::magic_type, 0);
    movement_magic.set_word(magic_word::effect_id, 0);
    movement_magic.set_word(magic_word::hurt_type, 0);
    movement_magic.set_word(magic_word::attack_area_type, 0);
    movement_magic.set_word(magic_word::need_mp, 5);
    movement_magic.set_word(magic_word::attack_begin + 2U, 20);
    movement_magic.set_word(magic_word::attack_begin + 3U, 20);
    movement_magic.set_word(magic_word::select_distance_begin + 2U, 1);
    movement_magic.set_word(magic_word::attack_distance_begin + 2U, 0);

    openlegend::random::LegacyRandom movement_random{1U};
    auto movement_session = std::make_unique<BattleSession>(
        data_root, *movement_ranger, movement_random, 4, false);
    OL_CHECK(movement_session->valid());
    finish_battle_entry_fade(*movement_session);
    OL_CHECK(movement_session->render(*framebuffer));
    movement_session->finish_presented_tick(1'300U);
    for (std::size_t frame = 0U;
         frame < movement_session->fade_frame_count();
         ++frame) {
        OL_CHECK(movement_session->render(*framebuffer));
        movement_session->finish_presented_tick(1'300U);
    }
    movement_session->setup().enable_automatic_mode();
    movement_session->advance(1'300U);
    OL_CHECK(movement_session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(movement_session->setup().combatants()[0U]
                 .words[combatant_word::round_value] == 2);
    OL_CHECK(movement_session->render(*framebuffer));
    movement_session->finish_presented_tick(1'300U);
    movement_session->advance(1'300U);
    OL_CHECK(movement_session->phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(movement_session->render(*framebuffer));
    movement_session->finish_presented_tick(1'300U);
    for (std::uint32_t movement_tick = 1'301U;
         movement_tick < 1'308U;
         ++movement_tick) {
        movement_session->advance(movement_tick);
        OL_CHECK(movement_session->phase() == BattleSessionPhase::ai_wait);
    }
    movement_session->advance(1'308U);
    OL_CHECK(movement_session->phase() ==
             BattleSessionPhase::ai_movement_step_present);
    OL_CHECK((BattlePathCoord{
                  movement_session->setup().combatants()[0U]
                      .words[combatant_word::x],
                  movement_session->setup().combatants()[0U]
                      .words[combatant_word::y]} ==
              BattlePathCoord{26, 25}));
    OL_CHECK(movement_enemy.word(role_word::hp) == 5'000);
    OL_CHECK(movement_session->render(*framebuffer));
    movement_session->finish_presented_tick(1'308U);
    OL_CHECK(movement_session->phase() == BattleSessionPhase::ai_movement_wait);
    movement_session->advance(1'309U);
    OL_CHECK(movement_session->phase() == BattleSessionPhase::ai_movement_wait);
    movement_session->advance(1'310U);
    OL_CHECK(movement_session->phase() == BattleSessionPhase::ai_magic_frame_present);
    OL_CHECK(movement_session->setup().combatants()[0U]
                 .words[combatant_word::initial_mode] == 3);
    OL_CHECK(movement_session->setup().combatants()[0U]
                 .words[combatant_word::round_value] == 1);
    OL_CHECK(movement_actor.word(role_word::physical_power) == 100);
    OL_CHECK(movement_enemy.word(role_word::hp) < 5'000);
    OL_CHECK((movement_session->take_audio_commands() ==
              std::vector<BattleAudioCommand>{
                  {BattleAudioBank::attack, 7},
                  {BattleAudioBank::effect, 0}}));
    OL_CHECK(movement_session->render(*framebuffer));
    const auto moved_magic_hash = fnv1a_bytes(framebuffer->pixels());
    OL_CHECK(moved_magic_hash == 0xacc58834b066ca07ULL);

    const auto hash_path = log_path.parent_path() / "b8-battle-ai-attack.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "prelude=0x" << std::hex << ai_prelude_hash << '\n';
    hash_file << "first_magic=0x" << std::hex << first_magic_hash << '\n';
    hash_file << "first_damage=0x" << std::hex << first_damage_hash << '\n';
    hash_file << "commit=0x" << std::hex << commit_hash << '\n';
    hash_file << "level=0x" << std::hex << level_hash << '\n';
    hash_file << "moved_magic=0x" << std::hex << moved_magic_hash << '\n';
    hash_file << "random_state=" << std::dec << random.state() << '\n';
    hash_file << "enemy_hp=" << enemy.word(role_word::hp) << '\n';
    hash_file << "attack_counter_initial=" << initial_attack_counter << '\n';
    hash_file << "attack_counter_after_damage=" << attack_counter_after_damage << '\n';
    hash_file << "attack_counter_final=" << final_attack_counter << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle AI action selected id=4 slot=0 action=2") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI attack ready id=4 slot=0 magic_slot=0 target=1 ") !=
             std::string::npos);
    OL_CHECK(log_text.find("area_type=1 direction=3 attack_count=1") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI magic frame ready id=4 slot=0 frame=0") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI damage frame ready id=4 slot=0 frame=0") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI attack commit frame presented id=4 slot=0 iteration=0") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI attack iteration committed id=4 slot=0") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI attack complete id=4 slot=0 iterations=1") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI movement continuation ready id=4 slot=0 continuation=2") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "target_coordinate=26,26 area_type=0 direction=3 attack_count=1") !=
             std::string::npos);
}

void run_ai_poison_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-ai-poison.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
    auto& actor = ranger->roles[1U];
    auto& enemy = ranger->roles[3U];
    actor.set_word(role_word::hp, 500);
    actor.set_word(role_word::maximum_hp, 500);
    actor.set_word(role_word::mp, 0);
    actor.set_word(role_word::maximum_mp, 0);
    actor.set_word(role_word::physical_power, 100);
    actor.set_word(role_word::attack, 0);
    actor.set_word(role_word::speed, 0);
    actor.set_word(role_word::use_poison, 100);
    actor.set_word(role_word::medicine, 0);
    actor.set_word(role_word::detoxification, 0);
    actor.set_word(role_word::magic_id_begin, 5);
    actor.set_word(role_word::frame_begin, 2);
    actor.set_word(role_word::frame_begin + 5U, 1);
    actor.set_word(role_word::frame_begin + 10U, 1);
    enemy.set_word(role_word::hp, 5'000);
    enemy.set_word(role_word::maximum_hp, 5'000);
    enemy.set_word(role_word::poison, 0);
    enemy.set_word(role_word::anti_poison, 0);
    auto& magic = ranger->magics[5U];
    magic.set_word(magic_word::sound_id, 7);

    openlegend::random::LegacyRandom random{2U};
    auto session = std::make_unique<BattleSession>(
        data_root, *ranger, random, 4, false);
    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    OL_CHECK(session->valid());
    finish_battle_entry_fade(*session);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'400U);
    for (std::size_t frame = 0U; frame < session->fade_frame_count(); ++frame) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(1'400U);
    }
    session->setup().combatants()[0U]
        .words[combatant_word::ai_poison_target] = 1;
    session->setup().enable_automatic_mode();
    session->advance(1'400U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::role_id] == 1);
    const auto initial_attack_counter = session->setup().combatants()[0U]
                                            .words[combatant_word::attack_counter];
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'400U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_action);
    session->advance(1'400U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'400U);
    for (std::uint32_t tick = 1'401U; tick < 1'408U; ++tick) {
        session->advance(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_wait);
    }
    session->advance(1'408U);
    OL_CHECK(session->valid());
    OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_frame_present);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::ai_action] ==
             static_cast<std::int16_t>(BattleAiAction::use_poison));
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::initial_mode] == 3);
    OL_CHECK(enemy.word(role_word::poison) == 25);
    OL_CHECK((session->take_audio_commands() ==
              std::vector<BattleAudioCommand>{
                  {BattleAudioBank::attack, 7},
                  {BattleAudioBank::effect, 30}}));

    std::uint32_t tick = 1'408U;
    std::size_t magic_frames = 0U;
    std::uint64_t first_magic_hash = 0U;
    while (session->phase() == BattleSessionPhase::ai_magic_frame_present &&
           magic_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        if (magic_frames == 0U) {
            first_magic_hash = fnv1a_bytes(framebuffer->pixels());
        }
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_wait);
        session->advance(tick);
        session->advance(++tick);
        ++magic_frames;
    }
    OL_CHECK(magic_frames == 11U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_frame_present);

    std::size_t damage_frames = 0U;
    std::uint64_t first_damage_hash = 0U;
    while (session->phase() == BattleSessionPhase::ai_damage_frame_present &&
           damage_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        if (damage_frames == 0U) {
            first_damage_hash = fnv1a_bytes(framebuffer->pixels());
        }
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_wait);
        session->advance(tick);
        session->advance(++tick);
        ++damage_frames;
    }
    OL_CHECK(damage_frames == 10U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->current_actor_slot() == 1U);
    OL_CHECK(actor.word(role_word::physical_power) == 98);
    OL_CHECK(session->setup().combatants()[0U]
                 .words[combatant_word::action_done] == 1);
    OL_CHECK(session->setup().combatants()[0U]
                 .words[combatant_word::attack_counter] ==
             initial_attack_counter + 1);
    OL_CHECK(first_magic_hash == 0x47286fa4af30fce4ULL);
    OL_CHECK(first_damage_hash == 0xd76de7fa195a1ac3ULL);
    OL_CHECK(random.state() == 2'993'822'286U);

    auto fallback_ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*fallback_ranger, {0, 2, 3, -1, -1, -1});
    auto& fallback_actor = fallback_ranger->roles[1U];
    auto& fallback_enemy = fallback_ranger->roles[3U];
    fallback_actor.set_word(role_word::hp, 500);
    fallback_actor.set_word(role_word::maximum_hp, 500);
    fallback_actor.set_word(role_word::mp, 20);
    fallback_actor.set_word(role_word::maximum_mp, 20);
    fallback_actor.set_word(role_word::physical_power, 100);
    fallback_actor.set_word(role_word::attack, 0);
    fallback_actor.set_word(role_word::speed, 0);
    fallback_actor.set_word(role_word::use_poison, 100);
    fallback_actor.set_word(role_word::medicine, 0);
    fallback_actor.set_word(role_word::detoxification, 0);
    fallback_actor.set_word(role_word::magic_id_begin, 5);
    fallback_actor.set_word(role_word::magic_level_begin, 200);
    fallback_actor.set_word(role_word::frame_begin, 2);
    fallback_actor.set_word(role_word::frame_begin + 5U, 1);
    fallback_actor.set_word(role_word::frame_begin + 10U, 1);
    fallback_enemy.set_word(role_word::hp, 5'000);
    fallback_enemy.set_word(role_word::maximum_hp, 5'000);
    fallback_enemy.set_word(role_word::poison, 95);
    fallback_enemy.set_word(role_word::anti_poison, 0);
    auto& fallback_magic = fallback_ranger->magics[5U];
    fallback_magic.set_word(magic_word::sound_id, 7);
    fallback_magic.set_word(magic_word::magic_type, 0);
    fallback_magic.set_word(magic_word::effect_id, 0);
    fallback_magic.set_word(magic_word::hurt_type, 0);
    fallback_magic.set_word(magic_word::attack_area_type, 1);
    fallback_magic.set_word(magic_word::need_mp, 5);
    fallback_magic.set_word(magic_word::attack_begin + 2U, 20);
    fallback_magic.set_word(magic_word::select_distance_begin + 2U, 2);
    fallback_magic.set_word(magic_word::attack_distance_begin + 2U, 0);
    openlegend::random::LegacyRandom fallback_random{2U};
    auto fallback_session = std::make_unique<BattleSession>(
        data_root, *fallback_ranger, fallback_random, 4, false);
    OL_CHECK(fallback_session->valid());
    finish_battle_entry_fade(*fallback_session);
    OL_CHECK(fallback_session->render(*framebuffer));
    fallback_session->finish_presented_tick(1'500U);
    for (std::size_t frame = 0U;
         frame < fallback_session->fade_frame_count();
         ++frame) {
        OL_CHECK(fallback_session->render(*framebuffer));
        fallback_session->finish_presented_tick(1'500U);
    }
    fallback_session->setup().combatants()[0U]
        .words[combatant_word::ai_poison_target] = 1;
    fallback_session->setup().enable_automatic_mode();
    fallback_session->advance(1'500U);
    OL_CHECK(fallback_session->render(*framebuffer));
    fallback_session->finish_presented_tick(1'500U);
    fallback_session->advance(1'500U);
    OL_CHECK(fallback_session->phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(fallback_session->render(*framebuffer));
    fallback_session->finish_presented_tick(1'500U);
    for (std::uint32_t fallback_tick = 1'501U;
         fallback_tick < 1'508U;
         ++fallback_tick) {
        fallback_session->advance(fallback_tick);
        OL_CHECK(fallback_session->phase() == BattleSessionPhase::ai_wait);
    }
    fallback_session->advance(1'508U);
    OL_CHECK(fallback_session->valid());
    OL_CHECK(fallback_session->phase() == BattleSessionPhase::ai_magic_frame_present);
    OL_CHECK(fallback_session->setup().combatants()[0U]
                 .words[combatant_word::ai_action] ==
             static_cast<std::int16_t>(BattleAiAction::use_poison));
    OL_CHECK((fallback_session->take_audio_commands() ==
              std::vector<BattleAudioCommand>{
                  {BattleAudioBank::attack, 7},
                  {BattleAudioBank::effect, 0}}));

    const auto hash_path = log_path.parent_path() / "b8-battle-ai-poison.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "first_magic=0x" << std::hex << first_magic_hash << '\n';
    hash_file << "first_damage=0x" << std::hex << first_damage_hash << '\n';
    hash_file << "random_state=" << std::dec << random.state() << '\n';
    hash_file << "enemy_poison=" << enemy.word(role_word::poison) << '\n';
    hash_file << "actor_physical_power=" << actor.word(role_word::physical_power) << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle AI action selected id=4 slot=0 action=3") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI poison effect ready id=4 slot=0 target=1 ") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "target_coordinate=26,26 hits=1 frames=11") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI magic frame ready id=4 slot=0 frame=0") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI damage frame ready id=4 slot=0 frame=0") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI target effect complete id=4 slot=0 action=2 magic_frames=11 damage_frames=10") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI poison fallback attack id=4 slot=0 target=-1") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI attack ready id=4 slot=0 magic_slot=0 target=1") !=
             std::string::npos);
}

void run_ai_item_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-ai-items.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    const auto clear_inventory = [](openlegend::model::RangerState& ranger) {
        for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
            ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
        }
    };
    const auto prepare_actor = [](openlegend::model::RangerState& ranger) {
        auto& actor = ranger.roles[1U];
        actor.set_word(role_word::hp, 500);
        actor.set_word(role_word::maximum_hp, 500);
        actor.set_word(role_word::hurt, 0);
        actor.set_word(role_word::poison, 0);
        actor.set_word(role_word::physical_power, 100);
        actor.set_word(role_word::speed, 0);
        actor.set_word(role_word::attack, 0);
        actor.set_word(role_word::medicine, 0);
        actor.set_word(role_word::use_poison, 0);
        actor.set_word(role_word::detoxification, 0);
        for (std::size_t slot = 0U; slot < role_word::magic_count; ++slot) {
            actor.set_word(role_word::magic_id_begin + slot, 0);
            actor.set_word(role_word::magic_level_begin + slot, 0);
        }
        for (std::size_t slot = 0U; slot < role_word::taking_item_count; ++slot) {
            actor.set_word(role_word::taking_item_begin + slot, -1);
            actor.set_word(role_word::taking_item_count_begin + slot, 0);
        }
    };
    const auto reach_ai_action = [&](BattleSession& session, const std::uint32_t tick) {
        OL_CHECK(session.valid());
        finish_battle_entry_fade(session);
        OL_CHECK(session.render(*framebuffer));
        session.finish_presented_tick(tick);
        for (std::size_t frame = 0U; frame < session.fade_frame_count(); ++frame) {
            OL_CHECK(session.render(*framebuffer));
            session.finish_presented_tick(tick);
        }
        session.setup().enable_automatic_mode();
        session.advance(tick);
        OL_CHECK(session.phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session.setup().combatants()[0U].words[combatant_word::role_id] == 1);
        OL_CHECK(session.render(*framebuffer));
        session.finish_presented_tick(tick);
        OL_CHECK(session.phase() == BattleSessionPhase::ai_action);
        session.advance(tick);
        OL_CHECK(session.phase() == BattleSessionPhase::ai_prelude_present);
        OL_CHECK(session.render(*framebuffer));
        session.finish_presented_tick(tick);
        OL_CHECK(session.phase() == BattleSessionPhase::ai_wait);
        for (std::uint32_t change = 1U; change < 8U; ++change) {
            session.advance(tick + change);
            OL_CHECK(session.phase() == BattleSessionPhase::ai_wait);
        }
        session.advance(tick + 8U);
        OL_CHECK(session.valid());
        return tick + 8U;
    };
    const auto move_combatant_away = [](BattleSession& session,
                                         const std::size_t actor_slot,
                                         const std::size_t opponent_slot,
                                         const std::size_t steps) {
        auto& setup = session.setup();
        for (std::size_t step = 0U; step < steps; ++step) {
            auto& actor_words = setup.combatants()[actor_slot].words;
            const auto& opponent_words = setup.combatants()[opponent_slot].words;
            actor_words[combatant_word::round_value] = 1;
            auto selection = setup.begin_player_movement_selection(actor_slot);
            if (!selection) {
                return false;
            }
            auto best = selection->cursor;
            auto best_distance = std::abs(best.x - opponent_words[combatant_word::x]) +
                std::abs(best.y - opponent_words[combatant_word::y]);
            for (std::int16_t y = 0; y < 64; ++y) {
                for (std::int16_t x = 0; x < 64; ++x) {
                    const BattlePathCoord candidate{x, y};
                    if (selection->pathing.value(candidate) != 1) {
                        continue;
                    }
                    const auto distance =
                        std::abs(x - opponent_words[combatant_word::x]) +
                        std::abs(y - opponent_words[combatant_word::y]);
                    if (distance > best_distance) {
                        best = candidate;
                        best_distance = distance;
                    }
                }
            }
            if (best == selection->cursor) {
                return false;
            }
            selection->cursor = best;
            if (setup.apply_cursor_selection(
                    *selection, BattleCursorSelectionAction::activate) !=
                BattleCursorSelectionResult::selected) {
                return false;
            }
            auto plan = setup.finish_player_movement_selection(*selection);
            if (!plan) {
                return false;
            }
            const auto movement = setup.advance_player_movement(*plan);
            if (!movement || !movement->moved || !plan->complete) {
                return false;
            }
        }
        return true;
    };

    std::uint64_t item_effect_hash = 0U;
    std::uint32_t item_random_state = 0U;
    {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        clear_inventory(*ranger);
        prepare_actor(*ranger);
        auto& actor = ranger->roles[1U];
        auto& enemy = ranger->roles[3U];
        actor.set_word(role_word::mp, 0);
        actor.set_word(role_word::maximum_mp, 100);
        enemy.set_word(role_word::hp, 100);
        enemy.set_word(role_word::maximum_hp, 100);
        auto& item = ranger->items[10U];
        item.set_word(item_word::item_type, 3);
        item.set_word(item_word::add_mp, 20);
        constexpr std::array<std::uint8_t, 7U> kItemName{
            0xA4U, 0xA4U, 0xA4U, 0x46U, 0xA4U, 0x48U, 0U};
        std::ranges::copy(
            kItemName,
            item.bytes.begin() + 2U * item_word::secondary_name_begin);
        ranger->header.set_inventory(0U, openlegend::model::ItemId{10}, 1);
        ranger->header.set_inventory(1U, openlegend::model::ItemId{2}, 3);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        auto tick = reach_ai_action(*session, 1'600U);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_step_present);
        OL_CHECK((BattlePathCoord{
                      session->setup().combatants()[0U].words[combatant_word::x],
                      session->setup().combatants()[0U].words[combatant_word::y]} ==
                  BattlePathCoord{26, 23}));
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::round_value] == -1);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
        session->advance(++tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
        session->advance(++tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_item_effect_present);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::ai_action] ==
                 static_cast<std::int16_t>(BattleAiAction::item));
        OL_CHECK(actor.word(role_word::mp) > 0);
        OL_CHECK(ranger->header.inventory_item(0U).value == 10);
        OL_CHECK(ranger->header.inventory_count(0U) == 1);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::action_done] == 0);
        OL_CHECK(session->take_audio_commands().empty());
        OL_CHECK(session->render(*framebuffer));
        item_effect_hash = fnv1a_bytes(framebuffer->pixels());
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_item_effect_wait);
        OL_CHECK(ranger->header.inventory_item(0U).value == 10);
        OL_CHECK(ranger->header.inventory_count(0U) == 1);
        tick += 5U;
        OL_CHECK(session->handle_key(0x98U, tick) ==
                 BattleSessionInputResult::item_effect_acknowledged);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_item_post_effect_wait);
        session->advance(tick);
        for (std::uint32_t change = 1U; change < 9U; ++change) {
            session->advance(tick + change);
            OL_CHECK(session->phase() ==
                     BattleSessionPhase::ai_item_post_effect_wait);
            OL_CHECK(ranger->header.inventory_item(0U).value == 10);
            OL_CHECK(ranger->header.inventory_count(0U) == 1);
        }
        session->advance(tick + 9U);
        OL_CHECK(session->valid());
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(actor.word(role_word::mp) == 20);
        OL_CHECK(ranger->header.inventory_item(0U).value == 2);
        OL_CHECK(ranger->header.inventory_count(0U) == 3);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::action_done] == 1);
        item_random_state = random.state();
    }

    std::uint64_t throwing_prelude_hash = 0U;
    std::uint64_t throwing_effect_hash = 0U;
    std::uint64_t throwing_damage_hash = 0U;
    std::uint32_t throwing_random_state = 0U;
    {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        clear_inventory(*ranger);
        prepare_actor(*ranger);
        auto& actor = ranger->roles[1U];
        auto& target = ranger->roles[3U];
        actor.set_word(role_word::mp, 0);
        actor.set_word(role_word::maximum_mp, 0);
        actor.set_word(role_word::hidden_weapon, 20);
        actor.set_word(role_word::taking_item_begin, 102);
        actor.set_word(role_word::taking_item_count_begin, 1);
        actor.set_word(role_word::taking_item_begin + 1U, 97);
        actor.set_word(role_word::taking_item_count_begin + 1U, 2);
        target.set_word(role_word::hp, 100);
        target.set_word(role_word::maximum_hp, 200);
        target.set_word(role_word::hurt, 40);
        target.set_word(role_word::poison, 10);
        target.set_word(role_word::anti_poison, 5);
        auto& item = ranger->items[102U];
        item.set_word(item_word::item_type, 4);
        item.set_word(item_word::hidden_weapon_effect_id, 30);
        item.set_word(item_word::add_hp, -40);
        item.set_word(item_word::add_poison, 40);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        session->setup().combatants()[0U].words[combatant_word::side] = 1;
        session->setup().combatants()[1U].words[combatant_word::side] = 0;
        const auto tick_after_selection = reach_ai_action(*session, 1'700U);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_effect_prelude_present);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::ai_action] ==
                 static_cast<std::int16_t>(BattleAiAction::throwing_weapon));
        OL_CHECK(target.word(role_word::hp) == 80);
        OL_CHECK(target.word(role_word::hurt) == 45);
        OL_CHECK(target.word(role_word::poison) == 50);
        OL_CHECK(actor.word(role_word::taking_item_begin) == 102);
        OL_CHECK(actor.word(role_word::taking_item_count_begin) == 1);
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{{BattleAudioBank::attack, 13}}));

        std::uint32_t tick = tick_after_selection;
        OL_CHECK(session->render(*framebuffer));
        throwing_prelude_hash = fnv1a_bytes(framebuffer->pixels());
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_effect_prelude_wait);
        session->advance(tick);
        std::size_t prelude_tick_changes = 0U;
        while (session->phase() == BattleSessionPhase::ai_effect_prelude_wait &&
               prelude_tick_changes < 20U) {
            session->advance(++tick);
            ++prelude_tick_changes;
        }
        OL_CHECK(prelude_tick_changes == 3U);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_frame_present);
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{{BattleAudioBank::effect, 30}}));

        std::size_t effect_frames = 0U;
        while (session->phase() == BattleSessionPhase::ai_magic_frame_present &&
               effect_frames < 100U) {
            OL_CHECK(session->render(*framebuffer));
            if (effect_frames == 0U) {
                throwing_effect_hash = fnv1a_bytes(framebuffer->pixels());
            }
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_wait);
            session->advance(tick);
            session->advance(++tick);
            ++effect_frames;
        }
        OL_CHECK(effect_frames == 11U);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_frame_present);
        OL_CHECK(actor.word(role_word::taking_item_begin) == 102);
        OL_CHECK(actor.word(role_word::taking_item_count_begin) == 1);

        std::size_t damage_frames = 0U;
        while (session->phase() == BattleSessionPhase::ai_damage_frame_present &&
               damage_frames < 20U) {
            OL_CHECK(session->render(*framebuffer));
            if (damage_frames == 0U) {
                throwing_damage_hash = fnv1a_bytes(framebuffer->pixels());
            }
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_wait);
            session->advance(tick);
            session->advance(++tick);
            ++damage_frames;
        }
        OL_CHECK(damage_frames == 10U);
        OL_CHECK(session->valid());
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(actor.word(role_word::taking_item_begin) == 97);
        OL_CHECK(actor.word(role_word::taking_item_count_begin) == 2);
        OL_CHECK(actor.word(role_word::taking_item_begin + 3U) == -1);
        OL_CHECK(actor.word(role_word::taking_item_count_begin + 3U) == 0);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::action_done] == 1);
        throwing_random_state = random.state();
    }

    std::uint64_t moved_throwing_effect_hash = 0U;
    {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        clear_inventory(*ranger);
        prepare_actor(*ranger);
        auto& actor = ranger->roles[1U];
        auto& target = ranger->roles[3U];
        actor.set_word(role_word::mp, 0);
        actor.set_word(role_word::maximum_mp, 0);
        actor.set_word(role_word::speed, 15);
        actor.set_word(role_word::hidden_weapon, 20);
        actor.set_word(role_word::taking_item_begin, 102);
        actor.set_word(role_word::taking_item_count_begin, 1);
        target.set_word(role_word::hp, 100);
        target.set_word(role_word::maximum_hp, 200);
        target.set_word(role_word::hurt, 40);
        target.set_word(role_word::poison, 10);
        target.set_word(role_word::anti_poison, 5);
        target.set_word(role_word::physical_power, 100);
        auto& item = ranger->items[102U];
        item.set_word(item_word::item_type, 4);
        item.set_word(item_word::hidden_weapon_effect_id, 30);
        item.set_word(item_word::add_hp, -40);
        item.set_word(item_word::add_poison, 40);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        session->setup().combatants()[0U].words[combatant_word::side] = 1;
        auto& target_words = session->setup().combatants()[1U].words;
        target_words[combatant_word::side] = 0;
        OL_CHECK(move_combatant_away(*session, 1U, 0U, 1U));
        OL_CHECK(std::abs(target_words[combatant_word::x] - 26) +
                     std::abs(target_words[combatant_word::y] - 24) ==
                 3);

        std::uint32_t tick = reach_ai_action(*session, 1'800U);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_step_present);
        OL_CHECK(std::abs(
                     session->setup().combatants()[0U].words[combatant_word::x] -
                     target_words[combatant_word::x]) +
                     std::abs(
                         session->setup().combatants()[0U].words[combatant_word::y] -
                         target_words[combatant_word::y]) ==
                 2);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::round_value] == 0);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
        session->advance(++tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
        session->advance(++tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_effect_prelude_present);
        OL_CHECK(target.word(role_word::hp) == 80);
        OL_CHECK(actor.word(role_word::taking_item_begin) == 102);
        OL_CHECK(actor.word(role_word::taking_item_count_begin) == 1);
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{{BattleAudioBank::attack, 13}}));
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_effect_prelude_wait);
        session->advance(tick);
        for (std::size_t change = 0U;
             session->phase() == BattleSessionPhase::ai_effect_prelude_wait &&
             change < 20U;
             ++change) {
            session->advance(++tick);
        }
        OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_frame_present);
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{{BattleAudioBank::effect, 30}}));
        std::size_t effect_frames = 0U;
        while (session->phase() == BattleSessionPhase::ai_magic_frame_present &&
               effect_frames < 100U) {
            OL_CHECK(session->render(*framebuffer));
            if (effect_frames == 0U) {
                moved_throwing_effect_hash = fnv1a_bytes(framebuffer->pixels());
            }
            session->finish_presented_tick(tick);
            session->advance(tick);
            session->advance(++tick);
            ++effect_frames;
        }
        OL_CHECK(effect_frames == 11U);
        std::size_t damage_frames = 0U;
        while (session->phase() == BattleSessionPhase::ai_damage_frame_present &&
               damage_frames < 20U) {
            OL_CHECK(session->render(*framebuffer));
            session->finish_presented_tick(tick);
            session->advance(tick);
            session->advance(++tick);
            ++damage_frames;
        }
        OL_CHECK(damage_frames == 10U);
        OL_CHECK(session->valid());
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(actor.word(role_word::taking_item_begin) == -1);
        OL_CHECK(actor.word(role_word::taking_item_count_begin) == 0);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::action_done] == 1);
        OL_CHECK(random.state() == 2'516'284'547U);
    }

    {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        clear_inventory(*ranger);
        prepare_actor(*ranger);
        auto& actor = ranger->roles[1U];
        auto& target = ranger->roles[3U];
        actor.set_word(role_word::mp, 20);
        actor.set_word(role_word::maximum_mp, 20);
        actor.set_word(role_word::speed, 15);
        actor.set_word(role_word::hidden_weapon, 20);
        actor.set_word(role_word::magic_id_begin, 5);
        actor.set_word(role_word::magic_level_begin, 250);
        actor.set_word(role_word::frame_begin, 2);
        actor.set_word(role_word::frame_begin + 5U, 1);
        actor.set_word(role_word::frame_begin + 10U, 1);
        actor.set_word(role_word::taking_item_begin, 102);
        actor.set_word(role_word::taking_item_count_begin, 1);
        target.set_word(role_word::hp, 5'000);
        target.set_word(role_word::maximum_hp, 5'000);
        target.set_word(role_word::hurt, 0);
        target.set_word(role_word::poison, 0);
        target.set_word(role_word::defence, 0);
        target.set_word(role_word::anti_poison, 100);
        target.set_word(role_word::physical_power, 100);
        auto& item = ranger->items[102U];
        item.set_word(item_word::item_type, 4);
        item.set_word(item_word::hidden_weapon_effect_id, 30);
        item.set_word(item_word::add_hp, -40);
        item.set_word(item_word::add_poison, 40);
        auto& magic = ranger->magics[5U];
        magic.set_word(magic_word::sound_id, 7);
        magic.set_word(magic_word::magic_type, 0);
        magic.set_word(magic_word::effect_id, 0);
        magic.set_word(magic_word::hurt_type, 0);
        magic.set_word(magic_word::attack_area_type, 1);
        magic.set_word(magic_word::need_mp, 5);
        magic.set_word(magic_word::attack_begin + 2U, 20);
        magic.set_word(magic_word::select_distance_begin + 2U, 10);
        magic.set_word(magic_word::attack_distance_begin + 2U, 0);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        session->setup().combatants()[0U].words[combatant_word::side] = 1;
        auto& target_words = session->setup().combatants()[1U].words;
        target_words[combatant_word::side] = 0;
        OL_CHECK(move_combatant_away(*session, 1U, 0U, 4U));
        OL_CHECK(std::abs(target_words[combatant_word::x] - 26) +
                     std::abs(target_words[combatant_word::y] - 24) ==
                 6);

        std::uint32_t tick = reach_ai_action(*session, 1'900U);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_step_present);
        OL_CHECK((BattlePathCoord{
                      session->setup().combatants()[0U].words[combatant_word::x],
                      session->setup().combatants()[0U].words[combatant_word::y]} !=
                  BattlePathCoord{26, 24}));
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::round_value] == 0);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(tick);
        session->advance(++tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
        session->advance(++tick);
        OL_CHECK(session->valid());
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(actor.word(role_word::taking_item_begin) == 102);
        OL_CHECK(actor.word(role_word::taking_item_count_begin) == 1);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::action_done] == 1);
        OL_CHECK(session->take_audio_commands().empty());
    }

    const auto hash_path = log_path.parent_path() / "b8-battle-ai-items.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "item_effect=0x" << std::hex << item_effect_hash << '\n';
    hash_file << "throwing_prelude=0x" << std::hex << throwing_prelude_hash << '\n';
    hash_file << "throwing_effect=0x" << std::hex << throwing_effect_hash << '\n';
    hash_file << "throwing_damage=0x" << std::hex << throwing_damage_hash << '\n';
    hash_file << "moved_throwing_effect=0x" << std::hex
              << moved_throwing_effect_hash << '\n';
    hash_file << "item_random_state=" << std::dec << item_random_state << '\n';
    hash_file << "throwing_random_state=" << throwing_random_state << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());
    OL_CHECK(item_effect_hash == 0xa7542240e4172664ULL);
    OL_CHECK(throwing_prelude_hash == 0x49aac6569a28fe89ULL);
    OL_CHECK(throwing_effect_hash == 0xc65b523bd75389e2ULL);
    OL_CHECK(throwing_damage_hash == 0x335fd35ea7e3f367ULL);
    OL_CHECK(moved_throwing_effect_hash == 0x16a8f10ce319622bULL);
    OL_CHECK(item_random_state == 662'824'084U);
    OL_CHECK(throwing_random_state == 2'516'284'547U);

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find(
                 "battle AI item effect ready id=4 slot=0 target=0 item=10") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI item effect acknowledged id=4 slot=0 item=10 effects=1 wait_tick_changes=9") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI item consumed id=4 slot=0 item=10 use_mode=0") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI throwing-weapon effect ready id=4 slot=0 target=1") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "effect=30 damage=20 frames=11 consumed=false") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI effect prelude presented id=4 slot=0 wait_tick_changes=3") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI target effect complete id=4 slot=0 action=5 magic_frames=11 damage_frames=10") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI action selected id=4 slot=0 action=10 handler=9") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI movement continuation ready id=4 slot=0 continuation=7") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI throwing-weapon fallback attack id=4 slot=0 target=1 range_checks=2") !=
             std::string::npos);
}

void run_ai_support_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-ai-support.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    struct SupportResult {
        std::uint64_t first_magic_hash{};
        std::uint64_t first_damage_hash{};
        std::uint32_t random_state{};
        std::int16_t hp{};
        std::int16_t poison{};
        std::int16_t physical_power{};
        std::size_t magic_frames{};
        std::size_t damage_frames{};
    };
    auto run_case = [&](const bool medicine, const std::uint32_t initial_tick) {
        auto ranger = std::make_unique<openlegend::model::RangerState>();
        initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
        auto& actor = ranger->roles[1U];
        auto& enemy = ranger->roles[3U];
        actor.set_word(role_word::hp, medicine ? 10 : 500);
        actor.set_word(role_word::maximum_hp, 500);
        actor.set_word(role_word::hurt, 0);
        actor.set_word(role_word::poison, medicine ? 0 : 100);
        actor.set_word(role_word::mp, 0);
        actor.set_word(role_word::maximum_mp, 0);
        actor.set_word(role_word::physical_power, 100);
        actor.set_word(role_word::attack, 0);
        actor.set_word(role_word::speed, 0);
        actor.set_word(role_word::use_poison, 0);
        actor.set_word(role_word::medicine, medicine ? 100 : 0);
        actor.set_word(role_word::detoxification, medicine ? 0 : 100);
        actor.set_word(role_word::magic_id_begin, 5);
        actor.set_word(role_word::frame_begin, 2);
        actor.set_word(role_word::frame_begin + 5U, 1);
        actor.set_word(role_word::frame_begin + 10U, 1);
        enemy.set_word(role_word::hp, 5'000);
        enemy.set_word(role_word::maximum_hp, 5'000);
        auto& magic = ranger->magics[5U];
        magic.set_word(magic_word::sound_id, 7);

        openlegend::random::LegacyRandom random{1U};
        auto session = std::make_unique<BattleSession>(
            data_root, *ranger, random, 4, false);
        auto framebuffer =
            std::make_unique<openlegend::render::IndexedFramebuffer>();
        OL_CHECK(session->valid());
        finish_battle_entry_fade(*session);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(initial_tick);
        for (std::size_t frame = 0U;
             frame < session->fade_frame_count();
             ++frame) {
            OL_CHECK(session->render(*framebuffer));
            session->finish_presented_tick(initial_tick);
        }
        session->setup().enable_automatic_mode();
        session->advance(initial_tick);
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::role_id] == 1);
        const auto initial_attack_counter = session->setup().combatants()[0U]
                                                .words[combatant_word::attack_counter];
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(initial_tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_action);
        session->advance(initial_tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_prelude_present);
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(initial_tick);
        for (std::uint32_t tick = initial_tick + 1U;
             tick < initial_tick + 8U;
             ++tick) {
            session->advance(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::ai_wait);
        }
        session->advance(initial_tick + 8U);
        OL_CHECK(session->valid());
        OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_frame_present);
        OL_CHECK(session->setup().combatants()[0U].words[combatant_word::ai_action] ==
                 static_cast<std::int16_t>(
                     medicine ? BattleAiAction::medicine : BattleAiAction::detox));
        OL_CHECK((session->take_audio_commands() ==
                  std::vector<BattleAudioCommand>{
                      {BattleAudioBank::attack, 7},
                      {BattleAudioBank::effect,
                       static_cast<std::int16_t>(medicine ? 0 : 36)}}));
        if (medicine) {
            OL_CHECK(actor.word(role_word::hp) == 93);
            OL_CHECK(actor.word(role_word::physical_power) == 98);
        } else {
            OL_CHECK(actor.word(role_word::poison) == 62);
            OL_CHECK(actor.word(role_word::physical_power) == 100);
        }

        std::uint32_t tick = initial_tick + 8U;
        SupportResult result{};
        while (session->phase() == BattleSessionPhase::ai_magic_frame_present &&
               result.magic_frames < 20U) {
            OL_CHECK(session->render(*framebuffer));
            if (result.magic_frames == 0U) {
                result.first_magic_hash = fnv1a_bytes(framebuffer->pixels());
            }
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_wait);
            session->advance(tick);
            session->advance(++tick);
            ++result.magic_frames;
        }
        OL_CHECK(result.magic_frames == (medicine ? 10U : 9U));
        OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_frame_present);
        while (session->phase() == BattleSessionPhase::ai_damage_frame_present &&
               result.damage_frames < 20U) {
            OL_CHECK(session->render(*framebuffer));
            if (result.damage_frames == 0U) {
                result.first_damage_hash = fnv1a_bytes(framebuffer->pixels());
            }
            session->finish_presented_tick(tick);
            OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_wait);
            session->advance(tick);
            session->advance(++tick);
            ++result.damage_frames;
        }
        OL_CHECK(result.damage_frames == 10U);
        OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session->current_actor_slot() == 1U);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::action_done] == 1);
        OL_CHECK(session->setup().combatants()[0U]
                     .words[combatant_word::attack_counter] ==
                 initial_attack_counter + 1);
        result.random_state = random.state();
        result.hp = actor.word(role_word::hp);
        result.poison = actor.word(role_word::poison);
        result.physical_power = actor.word(role_word::physical_power);
        OL_CHECK(result.random_state ==
                 (medicine ? 1'103'527'590U : 662'824'084U));
        OL_CHECK(result.physical_power == (medicine ? 96 : 98));
        return result;
    };

    const auto medicine = run_case(true, 1'600U);
    const auto detox = run_case(false, 1'700U);
    OL_CHECK(medicine.hp == 93);
    OL_CHECK(medicine.poison == 0);
    OL_CHECK(detox.hp == 500);
    OL_CHECK(detox.poison == 62);
    OL_CHECK(medicine.first_magic_hash == 0xbec9ef2738ca79b4ULL);
    OL_CHECK(medicine.first_damage_hash == 0xa962994ff58c7064ULL);
    OL_CHECK(detox.first_magic_hash == 0xae0f13fbbc4c8083ULL);
    OL_CHECK(detox.first_damage_hash == 0xa9cb56cb87bcbf4dULL);

    const auto hash_path = log_path.parent_path() / "b8-battle-ai-support.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "medicine_magic=0x" << std::hex << medicine.first_magic_hash << '\n';
    hash_file << "medicine_damage=0x" << std::hex << medicine.first_damage_hash << '\n';
    hash_file << "detox_magic=0x" << std::hex << detox.first_magic_hash << '\n';
    hash_file << "detox_damage=0x" << std::hex << detox.first_damage_hash << '\n';
    hash_file << "medicine_random_state=" << std::dec << medicine.random_state << '\n';
    hash_file << "detox_random_state=" << std::dec << detox.random_state << '\n';
    hash_file << "medicine_hp=" << medicine.hp << '\n';
    hash_file << "detox_poison=" << detox.poison << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle AI action selected id=4 slot=0 action=5") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI action selected id=4 slot=0 action=4") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI support effect ready id=4 slot=0 action=4 target=0") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI support effect ready id=4 slot=0 action=3 target=0") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI target effect complete id=4 slot=0 action=4 magic_frames=10 damage_frames=10") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI target effect complete id=4 slot=0 action=3 magic_frames=9 damage_frames=10") !=
             std::string::npos);
}

void run_ai_support_movement_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-ai-support-movement.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*ranger, {0, 2, -1, -1, -1, -1});
    openlegend::random::LegacyRandom random{12U};
    auto session = std::make_unique<BattleSession>(
        data_root, *ranger, random, 2, false);
    auto framebuffer =
        std::make_unique<openlegend::render::IndexedFramebuffer>();
    OL_CHECK(session->valid());
    OL_CHECK(session->phase() == BattleSessionPhase::party_selection);
    for (std::size_t index = 0U;
         index < session->setup().party_prefix_length();
         ++index) {
        if (session->setup().selection_states()[index] == 0) {
            static_cast<void>(session->handle_key(0x0DU));
        }
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::changed);
    }
    OL_CHECK(session->handle_key(0x0DU) ==
             BattleSessionInputResult::selection_complete);
    finish_battle_entry_fade(*session);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'900U);
    for (std::size_t frame = 0U;
         frame < session->fade_frame_count();
         ++frame) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(1'900U);
    }
    OL_CHECK(session->phase() == BattleSessionPhase::round_start);

    auto combatants = session->setup().combatants();
    OL_CHECK(combatants.size() >= 4U);
    const BattlePathCoord source{
        combatants[0U].words[combatant_word::x],
        combatants[0U].words[combatant_word::y]};
    BattlePathing pathing{session->data()};
    pathing.build(source, BattlePathMode::targeting);
    std::size_t helper_slot = 0U;
    std::int16_t helper_distance = std::numeric_limits<std::int16_t>::max();
    for (std::size_t slot = 1U; slot < 4U; ++slot) {
        const BattlePathCoord candidate{
            combatants[slot].words[combatant_word::x],
            combatants[slot].words[combatant_word::y]};
        const auto distance = pathing.value(candidate);
        if (distance > 2 && distance < helper_distance) {
            helper_slot = slot;
            helper_distance = distance;
        }
    }
    OL_CHECK(helper_slot != 0U);
    OL_CHECK(helper_distance > 2);
    for (std::size_t slot = 0U; slot < 4U; ++slot) {
        combatants[slot].words[combatant_word::side] = slot == 0U || slot == helper_slot
            ? 0
            : 1;
    }

    const auto actor_role_id = static_cast<std::size_t>(
        combatants[0U].words[combatant_word::role_id]);
    const auto helper_role_id = static_cast<std::size_t>(
        combatants[helper_slot].words[combatant_word::role_id]);
    auto& actor = ranger->roles[actor_role_id];
    auto& helper = ranger->roles[helper_role_id];
    actor.set_word(role_word::hp, 500);
    actor.set_word(role_word::maximum_hp, 500);
    actor.set_word(role_word::hurt, 0);
    actor.set_word(role_word::poison, 0);
    actor.set_word(role_word::mp, 0);
    actor.set_word(role_word::maximum_mp, 0);
    actor.set_word(role_word::physical_power, 100);
    actor.set_word(role_word::attack, 0);
    actor.set_word(role_word::speed, 300);
    actor.set_word(role_word::use_poison, 0);
    actor.set_word(role_word::medicine, 20);
    actor.set_word(role_word::detoxification, 0);
    actor.set_word(role_word::magic_id_begin, 5);
    actor.set_word(role_word::frame_begin, 2);
    actor.set_word(role_word::frame_begin + 5U, 1);
    actor.set_word(role_word::frame_begin + 10U, 1);
    helper.set_word(role_word::hp, 10);
    helper.set_word(role_word::maximum_hp, 500);
    helper.set_word(role_word::hurt, 0);
    helper.set_word(role_word::speed, 0);
    for (std::size_t slot = 1U; slot < 4U; ++slot) {
        if (slot == helper_slot) {
            continue;
        }
        auto& enemy = ranger->roles[static_cast<std::size_t>(
            combatants[slot].words[combatant_word::role_id])];
        enemy.set_word(role_word::hp, 5'000);
        enemy.set_word(role_word::maximum_hp, 5'000);
        enemy.set_word(role_word::speed, 0);
    }
    auto& magic = ranger->magics[5U];
    magic.set_word(magic_word::sound_id, 7);

    session->setup().enable_automatic_mode();
    session->advance(1'900U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->current_actor_slot() == 0U);
    OL_CHECK(combatants[0U].words[combatant_word::round_value] > 0);
    const auto initial_attack_counter =
        combatants[0U].words[combatant_word::attack_counter];
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'900U);
    session->advance(1'900U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'900U);
    for (std::uint32_t wait_tick = 1'901U; wait_tick < 1'908U; ++wait_tick) {
        session->advance(wait_tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_wait);
    }
    session->advance(1'908U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_step_present);
    OL_CHECK(combatants[0U].words[combatant_word::ai_action] ==
             static_cast<std::int16_t>(BattleAiAction::medicine));

    std::uint32_t tick = 1'908U;
    std::size_t movement_steps = 0U;
    while (session->phase() == BattleSessionPhase::ai_movement_step_present &&
           movement_steps < 8U) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
        session->advance(++tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
        session->advance(++tick);
        ++movement_steps;
    }
    OL_CHECK(movement_steps > 0U);
    OL_CHECK(session->valid());
    OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_frame_present);
    OL_CHECK(helper.word(role_word::hp) == 28);
    OL_CHECK(actor.word(role_word::physical_power) == 98);
    OL_CHECK((session->take_audio_commands() ==
              std::vector<BattleAudioCommand>{
                  {BattleAudioBank::attack, 7},
                  {BattleAudioBank::effect, 0}}));

    std::size_t magic_frames = 0U;
    std::uint64_t first_magic_hash = 0U;
    while (session->phase() == BattleSessionPhase::ai_magic_frame_present &&
           magic_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        if (magic_frames == 0U) {
            first_magic_hash = fnv1a_bytes(framebuffer->pixels());
        }
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_wait);
        session->advance(tick);
        session->advance(++tick);
        ++magic_frames;
    }
    OL_CHECK(magic_frames == 10U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_frame_present);
    std::size_t damage_frames = 0U;
    while (session->phase() == BattleSessionPhase::ai_damage_frame_present &&
           damage_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_wait);
        session->advance(tick);
        session->advance(++tick);
        ++damage_frames;
    }
    OL_CHECK(damage_frames == 10U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->current_actor_slot() == 1U);
    OL_CHECK(actor.word(role_word::physical_power) == 96);
    OL_CHECK(combatants[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(combatants[0U].words[combatant_word::attack_counter] ==
             initial_attack_counter + 1);
    OL_CHECK(helper_distance == 6);
    OL_CHECK(movement_steps == 4U);
    OL_CHECK(first_magic_hash == 0x2138cfdf8041c6bbULL);
    OL_CHECK(random.state() == 2'900'951'131U);

    const auto hash_path =
        log_path.parent_path() / "b8-battle-ai-support-movement.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "first_magic=0x" << std::hex << first_magic_hash << '\n';
    hash_file << "movement_steps=" << std::dec << movement_steps << '\n';
    hash_file << "target_distance=" << helper_distance << '\n';
    hash_file << "random_state=" << random.state() << '\n';
    hash_file << "helper_hp=" << helper.word(role_word::hp) << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle AI action selected id=2 slot=0 action=5") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI movement ready id=2 slot=0 mode=1") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI movement continuation ready id=2 slot=0 continuation=6") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI support effect ready id=2 slot=0 action=4") !=
             std::string::npos);
}

void run_ai_request_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-ai-request.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*ranger, {0, 2, -1, -1, -1, -1});
    openlegend::random::LegacyRandom random{1U};
    auto session = std::make_unique<BattleSession>(
        data_root, *ranger, random, 2, false);
    auto framebuffer =
        std::make_unique<openlegend::render::IndexedFramebuffer>();
    OL_CHECK(session->valid());
    OL_CHECK(session->phase() == BattleSessionPhase::party_selection);
    for (std::size_t index = 0U;
         index < session->setup().party_prefix_length();
         ++index) {
        if (session->setup().selection_states()[index] == 0) {
            static_cast<void>(session->handle_key(0x0DU));
        }
        OL_CHECK(session->handle_key(0x98U) == BattleSessionInputResult::changed);
    }
    OL_CHECK(session->handle_key(0x0DU) ==
             BattleSessionInputResult::selection_complete);
    finish_battle_entry_fade(*session);
    OL_CHECK(session->setup().combatant_count() == 4);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'800U);
    for (std::size_t frame = 0U;
         frame < session->fade_frame_count();
         ++frame) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(1'800U);
    }
    OL_CHECK(session->phase() == BattleSessionPhase::round_start);

    auto combatants = session->setup().combatants();
    OL_CHECK(combatants.size() >= 4U);
    combatants[0U].words[combatant_word::side] = 0;
    combatants[1U].words[combatant_word::side] = 0;
    combatants[2U].words[combatant_word::side] = 1;
    combatants[3U].words[combatant_word::side] = 1;
    const auto actor_role_id = static_cast<std::size_t>(
        combatants[0U].words[combatant_word::role_id]);
    const auto helper_role_id = static_cast<std::size_t>(
        combatants[1U].words[combatant_word::role_id]);
    auto& actor = ranger->roles[actor_role_id];
    auto& helper = ranger->roles[helper_role_id];
    actor.set_word(role_word::hp, 10);
    actor.set_word(role_word::maximum_hp, 500);
    actor.set_word(role_word::hurt, 0);
    actor.set_word(role_word::poison, 0);
    actor.set_word(role_word::mp, 20);
    actor.set_word(role_word::maximum_mp, 20);
    actor.set_word(role_word::physical_power, 100);
    actor.set_word(role_word::attack, 50);
    actor.set_word(role_word::speed, 15);
    actor.set_word(role_word::use_poison, 0);
    actor.set_word(role_word::medicine, 0);
    actor.set_word(role_word::detoxification, 0);
    actor.set_word(role_word::magic_id_begin, 5);
    actor.set_word(role_word::magic_level_begin, 200);
    actor.set_word(role_word::frame_begin, 2);
    actor.set_word(role_word::frame_begin + 5U, 1);
    actor.set_word(role_word::frame_begin + 10U, 1);
    helper.set_word(role_word::hp, 500);
    helper.set_word(role_word::maximum_hp, 500);
    helper.set_word(role_word::physical_power, 100);
    helper.set_word(role_word::speed, 0);
    helper.set_word(role_word::medicine, 100);
    for (std::size_t slot = 2U; slot < 4U; ++slot) {
        auto& enemy = ranger->roles[static_cast<std::size_t>(
            combatants[slot].words[combatant_word::role_id])];
        enemy.set_word(role_word::hp, 5'000);
        enemy.set_word(role_word::maximum_hp, 5'000);
        enemy.set_word(role_word::defence, 0);
        enemy.set_word(role_word::anti_poison, 100);
        enemy.set_word(role_word::speed, 0);
    }
    auto& magic = ranger->magics[5U];
    magic.set_word(magic_word::sound_id, 7);
    magic.set_word(magic_word::magic_type, 0);
    magic.set_word(magic_word::effect_id, 0);
    magic.set_word(magic_word::hurt_type, 0);
    magic.set_word(magic_word::attack_area_type, 1);
    magic.set_word(magic_word::need_mp, 5);
    magic.set_word(magic_word::attack_begin + 2U, 20);
    magic.set_word(magic_word::select_distance_begin + 2U, 20);
    magic.set_word(magic_word::attack_distance_begin + 2U, 0);

    session->setup().enable_automatic_mode();
    session->advance(1'800U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->current_actor_slot() == 0U);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'800U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_action);
    session->advance(1'800U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'800U);
    for (std::uint32_t tick = 1'801U; tick < 1'808U; ++tick) {
        session->advance(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_wait);
    }
    session->advance(1'808U);
    OL_CHECK(session->valid());
    OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_step_present);
    OL_CHECK(combatants[0U].words[combatant_word::ai_action] ==
             static_cast<std::int16_t>(BattleAiAction::request_medicine));
    OL_CHECK(combatants[0U].words[combatant_word::round_value] == 0);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(1'808U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
    session->advance(1'809U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_movement_wait);
    session->advance(1'810U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_frame_present);
    OL_CHECK(actor.word(role_word::physical_power) == 99);
    OL_CHECK((session->take_audio_commands() ==
              std::vector<BattleAudioCommand>{
                  {BattleAudioBank::attack, 7},
                  {BattleAudioBank::effect, 0}}));

    std::uint32_t tick = 1'810U;
    std::size_t magic_frames = 0U;
    std::uint64_t first_magic_hash = 0U;
    while (session->phase() == BattleSessionPhase::ai_magic_frame_present &&
           magic_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        if (magic_frames == 0U) {
            first_magic_hash = fnv1a_bytes(framebuffer->pixels());
        }
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_magic_wait);
        session->advance(tick);
        session->advance(++tick);
        ++magic_frames;
    }
    OL_CHECK(magic_frames == 10U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_frame_present);
    std::size_t damage_frames = 0U;
    while (session->phase() == BattleSessionPhase::ai_damage_frame_present &&
           damage_frames < 20U) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(tick);
        OL_CHECK(session->phase() == BattleSessionPhase::ai_damage_wait);
        session->advance(tick);
        session->advance(++tick);
        ++damage_frames;
    }
    OL_CHECK(damage_frames == 10U);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_attack_commit_present);
    OL_CHECK(session->render(*framebuffer));
    session->finish_presented_tick(tick);
    OL_CHECK(session->phase() == BattleSessionPhase::ai_attack_commit_wait);
    session->advance(tick);
    session->advance(++tick);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->current_actor_slot() == 1U);
    OL_CHECK(combatants[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(combatants[0U].words[combatant_word::ai_action] ==
             static_cast<std::int16_t>(BattleAiAction::request_medicine));
    OL_CHECK(actor.word(role_word::mp) == 15);
    OL_CHECK(actor.word(role_word::physical_power) == 96);
    OL_CHECK(first_magic_hash == 0xcc6a249ebb919a23ULL);
    OL_CHECK(random.state() == 3'295'386'429U);

    const auto hash_path = log_path.parent_path() / "b8-battle-ai-request.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "first_magic=0x" << std::hex << first_magic_hash << '\n';
    hash_file << "random_state=" << std::dec << random.state() << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle AI action selected id=2 slot=0 action=8") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI movement ready id=2 slot=0 mode=0") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI movement continuation ready id=2 slot=0 continuation=5") !=
             std::string::npos);
    OL_CHECK(log_text.find(
                 "battle AI request attack ready id=2 slot=0 action=8 target=1") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI attack ready id=2 slot=0 magic_slot=0") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle AI attack complete id=2 slot=0 iterations=1") !=
             std::string::npos);
}

void run_battle_session_test(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    const auto log_path =
        openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-session.log";
    std::error_code log_error;
    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);

    auto ranger = make_ranger({0, 2, -1, -1, -1, -1});
    for (const auto role_id : {0U, 2U}) {
        auto& role = ranger.roles[role_id];
        auto name = std::span<std::uint8_t>{role.bytes}.subspan(
            openlegend::model::role_word::name_byte,
            openlegend::model::role_word::name_bytes);
        std::ranges::fill(name, std::uint8_t{0U});
        name[0U] = static_cast<std::uint8_t>('A' + role_id);
    }
    openlegend::random::LegacyRandom random{1U};
    BattleSession session{data_root, ranger, random, 2, true};
    OL_CHECK(session.valid());
    OL_CHECK(session.grants_experience());
    OL_CHECK(session.phase() == BattleSessionPhase::party_selection);
    OL_CHECK(session.setup().party_prefix_length() == 2U);

    openlegend::render::IndexedFramebuffer framebuffer;
    for (std::size_t index = 0U; index < framebuffer.pixels().size(); ++index) {
        framebuffer.pixels()[index] = static_cast<std::uint8_t>(index % 251U);
    }
    OL_CHECK(session.render(framebuffer));
    const auto selection_hash = fnv1a_bytes(framebuffer.pixels());
    OL_CHECK(selection_hash == 0x83f943240d14bb33ULL);
    OL_CHECK(session.render(framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == selection_hash);
    framebuffer.pixels()[0U] = 123U;
    OL_CHECK(session.render(framebuffer));
    OL_CHECK(framebuffer.pixels()[0U] == 0U);
    framebuffer.pixels()[0U] = 123U;
    OL_CHECK(session.render(framebuffer, true));
    OL_CHECK(framebuffer.pixels()[0U] == 123U);

    for (std::size_t index = 0U; index < session.setup().party_prefix_length(); ++index) {
        if (session.setup().selection_states()[index] == 0) {
            static_cast<void>(session.handle_key(0x0DU));
        }
        OL_CHECK(session.handle_key(0x98U) == BattleSessionInputResult::changed);
    }
    OL_CHECK(session.setup().cursor() == session.setup().party_prefix_length());
    OL_CHECK(session.handle_key(0x0DU) == BattleSessionInputResult::selection_complete);
    finish_battle_entry_fade(session);
    OL_CHECK(session.setup().combatant_count() == 4);
    constexpr std::array<std::int16_t, 4> kExpectedRoles{0, 1, 2, 4};
    for (std::size_t slot = 0U; slot < kExpectedRoles.size(); ++slot) {
        OL_CHECK(session.setup().combatants()[slot].words[combatant_word::role_id] ==
                 kExpectedRoles[slot]);
    }
    OL_CHECK(session.view_x() == 0 && session.view_y() == 0);
    OL_CHECK(session.render(framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == 0x568240847c97700cULL);

    session.finish_presented_tick();
    OL_CHECK(session.phase() == BattleSessionPhase::initial_fade);
    OL_CHECK(session.fade_frame_count() > 0U);
    for (std::size_t frame = 0U; frame < session.fade_frame_count(); ++frame) {
        OL_CHECK(session.fade_frame() == frame);
        OL_CHECK(session.render(framebuffer));
        session.finish_presented_tick();
    }
    OL_CHECK(session.phase() == BattleSessionPhase::round_start);
    ranger.roles[0U].set_word(role_word::speed, 100);
    ranger.roles[0U].set_word(role_word::physical_power, 60);
    ranger.roles[0U].set_word(role_word::mp, 25);
    ranger.roles[0U].set_word(role_word::use_poison, 20);
    ranger.roles[0U].set_word(role_word::detoxification, 20);
    ranger.roles[0U].set_word(role_word::medicine, 20);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 5);
    ranger.roles[0U].set_word(role_word::magic_id_begin + 2U, 6);
    ranger.magics[5U].set_word(magic_word::need_mp, 25);
    ranger.magics[6U].set_word(magic_word::need_mp, 20);
    auto magic5_name = std::span<std::uint8_t>{ranger.magics[5U].bytes}.subspan(
        magic_word::name_byte, magic_word::name_bytes);
    auto magic6_name = std::span<std::uint8_t>{ranger.magics[6U].bytes}.subspan(
        magic_word::name_byte, magic_word::name_bytes);
    std::ranges::fill(magic5_name, std::uint8_t{0U});
    std::ranges::fill(magic6_name, std::uint8_t{0U});
    std::ranges::copy(std::array<std::uint8_t, 4>{0xA7U, 0xF0U, 0xC0U, 0xBBU},
                      magic5_name.begin());
    std::ranges::copy(std::array<std::uint8_t, 6>{
                          0xADU, 0xB0U, 0xC0U, 0x59U, 0xAFU, 0xABU},
                      magic6_name.begin());
    session.advance();
    OL_CHECK(session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session.render(framebuffer));
    session.finish_presented_tick();
    finish_player_menu_redraw(session);
    OL_CHECK(session.phase() == BattleSessionPhase::player_action);
    OL_CHECK((session.player_action_menu().available ==
              std::array<std::int16_t, 10>{1, 1, 1, 1, 1, 1, 1, 1, 1, 1}));
    OL_CHECK(session.player_action_menu().available_count == 10U);
    OL_CHECK(session.render_state().secondary_cursor_visible);
    const auto& menu_actor = session.setup().combatants()[session.current_actor_slot()].words;
    OL_CHECK((session.render_state().secondary_cursor == BattlePathCoord{
        menu_actor[combatant_word::x], menu_actor[combatant_word::y]}));
    OL_CHECK(session.player_action_menu().cursor == 0U);
    OL_CHECK(session.player_action_menu().selected_action == -1);
    OL_CHECK(session.render(framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == 0x648d1a4f7c02fdbcULL);
    OL_CHECK(session.handle_key(0x9EU) == BattleSessionInputResult::action_changed);
    OL_CHECK(session.player_action_menu().cursor == 9U);
    OL_CHECK(session.handle_key(0x98U) == BattleSessionInputResult::action_changed);
    OL_CHECK(session.player_action_menu().cursor == 0U);
    OL_CHECK(session.handle_key(0x98U) == BattleSessionInputResult::action_changed);
    OL_CHECK(session.player_action_menu().cursor == 1U);
    OL_CHECK(session.handle_key(0x20U) == BattleSessionInputResult::action_selected);
    OL_CHECK(session.phase() == BattleSessionPhase::player_magic_selection);
    OL_CHECK(session.player_action_menu().selected_action ==
             static_cast<std::int16_t>(BattlePlayerAction::attack));
    OL_CHECK(!session.render_state().secondary_cursor_visible);
    OL_CHECK(session.player_magic_selection().has_value());
    OL_CHECK(session.player_magic_selection()->learned_count == 2);
    OL_CHECK(session.player_magic_selection()->available_count == 2);
    OL_CHECK(session.player_magic_selection()->available_slots[0U] == 0);
    OL_CHECK(session.player_magic_selection()->available_slots[1U] == 2);
    OL_CHECK(session.render(framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == 0x909332be9671b27cULL);
    OL_CHECK(session.handle_key(0x9CU) == BattleSessionInputResult::magic_changed);
    OL_CHECK(session.player_magic_selection()->cursor == 1);
    OL_CHECK(session.render(framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == 0x6977ba7a0c3172a6ULL);
    OL_CHECK(session.handle_key(0x9CU) == BattleSessionInputResult::magic_changed);
    OL_CHECK(session.player_magic_selection()->cursor == 0);
    OL_CHECK(session.handle_key(0x9AU) == BattleSessionInputResult::magic_changed);
    OL_CHECK(session.player_magic_selection()->cursor == 1);
    OL_CHECK(session.handle_key(0x1BU) == BattleSessionInputResult::magic_cancelled);
    finish_player_menu_redraw(session);
    OL_CHECK(session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(!session.render_state().secondary_cursor_visible);
    OL_CHECK(session.player_action_menu().cursor == 1U);
    OL_CHECK(session.player_action_menu().selected_action == -1);
    OL_CHECK(session.handle_key(0x20U) == BattleSessionInputResult::action_selected);
    OL_CHECK(session.phase() == BattleSessionPhase::player_magic_selection);
    OL_CHECK(session.handle_key(0x9AU) == BattleSessionInputResult::magic_changed);
    OL_CHECK(session.handle_key(0x0DU) == BattleSessionInputResult::magic_selected);
    OL_CHECK(session.phase() == BattleSessionPhase::player_targeting_select);
    OL_CHECK(session.selected_magic_slot() == 2);
    OL_CHECK(!session.player_magic_selection().has_value());

    openlegend::diagnostics::shutdown_logging();
    std::ifstream log_file{log_path, std::ios::binary};
    const std::string log_text{
        std::istreambuf_iterator<char>{log_file}, std::istreambuf_iterator<char>{}};
    OL_CHECK(log_text.find("battle session initialized id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle party selection complete id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle initial frame presented id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle initial fade complete id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle round actor ready id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle actor dispatch id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle player action menu ready id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle player action selected id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle player magic selection ready id=2") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player magic cursor id=2") != std::string::npos);
    OL_CHECK(log_text.find("battle player magic selection cancelled id=2") !=
             std::string::npos);
    OL_CHECK(log_text.find("battle player magic selected id=2") != std::string::npos);

    const auto reach_player_menu_initial = [&](BattleSession& target) {
        OL_CHECK(target.valid());
        finish_battle_entry_fade(target);
        OL_CHECK(target.render(framebuffer));
        target.finish_presented_tick();
        for (std::size_t frame = 0U; frame < target.fade_frame_count(); ++frame) {
            OL_CHECK(target.render(framebuffer));
            target.finish_presented_tick();
        }
        OL_CHECK(target.phase() == BattleSessionPhase::round_start);
        target.advance();
        OL_CHECK(target.phase() == BattleSessionPhase::actor_present);
        OL_CHECK(target.render(framebuffer));
        target.finish_presented_tick();
        OL_CHECK(target.phase() == BattleSessionPhase::player_action_initial_present);
    };
    const auto reach_player_action = [&](BattleSession& target) {
        reach_player_menu_initial(target);
        finish_player_menu_redraw(target);
        OL_CHECK(target.phase() == BattleSessionPhase::player_action);
    };

    auto state_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    state_ranger.roles[1U].set_word(role_word::hp, 100);
    state_ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    state_ranger.roles[1U].set_word(role_word::physical_power, 100);
    state_ranger.roles[1U].set_word(role_word::speed, 30);
    state_ranger.roles[3U].set_word(role_word::hp, 100);
    state_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom state_random{1U};
    BattleSession state_session{data_root, state_ranger, state_random, 4, false};
    reach_player_menu_initial(state_session);
    state_session.set_player_menu_direction_states(true, true);
    state_session.set_confirmation_state(true);
    OL_CHECK(state_session.render(framebuffer));
    const auto state_initial_menu_hash = fnv1a_bytes(framebuffer.pixels());
    state_session.finish_presented_tick();
    OL_CHECK(state_session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(state_session.player_action_menu().cursor == 0U);
    OL_CHECK(state_session.player_action_menu().selected_action == -1);
    OL_CHECK(state_session.take_clear_player_menu_direction_request() == 0U);
    OL_CHECK(!state_session.take_clear_confirmation_states_request());
    OL_CHECK(state_session.render(framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == state_initial_menu_hash);
    state_session.finish_presented_tick();
    OL_CHECK(state_session.player_action_menu().cursor == 1U);
    OL_CHECK(state_session.take_clear_player_menu_direction_request() == 0x98U);
    OL_CHECK(!state_session.take_clear_confirmation_states_request());
    state_session.set_player_menu_direction_states(false, true);
    OL_CHECK(state_session.render(framebuffer));
    state_session.finish_presented_tick();
    OL_CHECK(state_session.player_action_menu().cursor == 0U);
    OL_CHECK(state_session.take_clear_player_menu_direction_request() == 0x9EU);
    OL_CHECK(!state_session.take_clear_confirmation_states_request());
    state_session.set_player_menu_direction_states(false, false);
    OL_CHECK(state_session.render(framebuffer));
    state_session.finish_presented_tick();
    OL_CHECK(state_session.phase() == BattleSessionPhase::player_movement_select);
    OL_CHECK(state_session.take_clear_confirmation_states_request());
    OL_CHECK(!state_session.render_state().secondary_cursor_visible);

    auto released_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    released_ranger.roles[1U].set_word(role_word::hp, 100);
    released_ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    released_ranger.roles[1U].set_word(role_word::physical_power, 100);
    released_ranger.roles[1U].set_word(role_word::speed, 30);
    released_ranger.roles[3U].set_word(role_word::hp, 100);
    released_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom released_random{1U};
    BattleSession released_session{
        data_root, released_ranger, released_random, 4, false};
    reach_player_menu_initial(released_session);
    released_session.set_player_menu_direction_states(true, false);
    released_session.set_player_menu_direction_states(false, false);
    OL_CHECK(released_session.render(framebuffer));
    released_session.finish_presented_tick();
    OL_CHECK(released_session.render(framebuffer));
    released_session.finish_presented_tick();
    OL_CHECK(released_session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(released_session.player_action_menu().cursor == 0U);
    OL_CHECK(released_session.take_clear_player_menu_direction_request() == 0U);
    OL_CHECK(released_session.handle_key(0x1BU) == BattleSessionInputResult::ignored);
    OL_CHECK(released_session.phase() == BattleSessionPhase::player_action);

    auto non_one_done_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    non_one_done_ranger.roles[1U].set_word(role_word::hp, 100);
    non_one_done_ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    non_one_done_ranger.roles[3U].set_word(role_word::hp, 100);
    non_one_done_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom non_one_done_random{1U};
    BattleSession non_one_done_session{
        data_root, non_one_done_ranger, non_one_done_random, 4, false};
    reach_player_action(non_one_done_session);
    non_one_done_session.setup().combatants()[0U]
        .words[combatant_word::action_done] = 2;
    OL_CHECK(non_one_done_session.handle_key(0x98U) ==
             BattleSessionInputResult::action_changed);
    OL_CHECK(non_one_done_session.handle_key(0x98U) ==
             BattleSessionInputResult::action_changed);
    OL_CHECK(non_one_done_session.handle_key(0x20U) ==
             BattleSessionInputResult::action_selected);
    OL_CHECK(non_one_done_session.phase() == BattleSessionPhase::player_status_selection);
    OL_CHECK(non_one_done_session.handle_key(0x1BU) ==
             BattleSessionInputResult::status_cancelled);
    OL_CHECK(non_one_done_session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(non_one_done_session.player_action_menu().selected_action == -1);
    OL_CHECK(!non_one_done_session.render_state().secondary_cursor_visible);

    auto filtered_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    openlegend::random::LegacyRandom filtered_random{1U};
    BattleSession filtered_session{
        data_root, filtered_ranger, filtered_random, 4, false};
    reach_player_action(filtered_session);
    OL_CHECK((filtered_session.player_action_menu().available ==
              std::array<std::int16_t, 10>{0, 0, 0, 0, 0, 1, 1, 1, 1, 1}));
    OL_CHECK(filtered_session.player_action_menu().available_count == 5U);
    OL_CHECK(filtered_session.handle_key(0x0DU) == BattleSessionInputResult::action_selected);
    OL_CHECK(filtered_session.player_action_menu().selected_action ==
             static_cast<std::int16_t>(BattlePlayerAction::item));

    auto wait_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    wait_ranger.roles[1U].set_word(role_word::hp, 100);
    wait_ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    wait_ranger.roles[3U].set_word(role_word::hp, 100);
    wait_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom wait_random{1U};
    BattleSession wait_session{data_root, wait_ranger, wait_random, 4, false};
    reach_player_action(wait_session);
    OL_CHECK(wait_session.handle_key(0x98U) == BattleSessionInputResult::action_changed);
    OL_CHECK(wait_session.handle_key(0x0DU) == BattleSessionInputResult::action_selected);
    OL_CHECK(wait_session.phase() == BattleSessionPhase::player_action_return_present);
    finish_player_menu_redraw(wait_session);
    OL_CHECK(wait_session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(wait_session.current_actor_slot() == 0U);
    OL_CHECK(wait_session.setup().combatants()[0U].words[combatant_word::role_id] == 3);
    OL_CHECK(wait_session.setup().combatants()[1U].words[combatant_word::role_id] == 1);
    OL_CHECK(wait_session.setup().combatants()[1U].words[combatant_word::action_done] == 0);

    auto rest_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    rest_ranger.roles[1U].set_word(role_word::hp, 100);
    rest_ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    rest_ranger.roles[3U].set_word(role_word::hp, 100);
    rest_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom rest_random{1U};
    BattleSession rest_session{data_root, rest_ranger, rest_random, 4, false};
    reach_player_action(rest_session);
    for (std::size_t step = 0U; step < 3U; ++step) {
        OL_CHECK(rest_session.handle_key(0x98U) == BattleSessionInputResult::action_changed);
    }
    OL_CHECK(rest_session.handle_key(0x20U) == BattleSessionInputResult::action_selected);
    OL_CHECK(rest_session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(rest_session.current_actor_slot() == 1U);
    OL_CHECK(rest_session.setup().combatants()[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(rest_ranger.roles[1U].word(role_word::physical_power) == 5);

    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);
    auto movement_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    movement_ranger.roles[1U].set_word(role_word::hp, 100);
    movement_ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    movement_ranger.roles[1U].set_word(role_word::physical_power, 10);
    movement_ranger.roles[1U].set_word(role_word::speed, 30);
    movement_ranger.roles[3U].set_word(role_word::hp, 100);
    movement_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom movement_random{1U};
    BattleSession movement_session{
        data_root, movement_ranger, movement_random, 4, false};
    reach_player_action(movement_session);
    OL_CHECK(
        movement_session.setup().combatants()[0U].words[combatant_word::round_value] == 2);
    OL_CHECK(movement_session.handle_key(0x0DU) == BattleSessionInputResult::action_selected);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_movement_select);
    OL_CHECK((movement_session.active_cursor() == BattlePathCoord{26, 24}));
    OL_CHECK(movement_session.render(framebuffer));
    movement_session.finish_presented_tick(200U);
    OL_CHECK(movement_session.handle_key(0x1BU) ==
             BattleSessionInputResult::cursor_cancelled);
    finish_player_menu_redraw(movement_session);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(movement_session.player_action_menu().available[0U] == 1);

    OL_CHECK(movement_session.handle_key(0x20U) == BattleSessionInputResult::action_selected);
    OL_CHECK(movement_session.render(framebuffer));
    movement_session.finish_presented_tick(200U);
    OL_CHECK(movement_session.handle_key(0x98U) ==
             BattleSessionInputResult::cursor_changed);
    OL_CHECK((movement_session.active_cursor() == BattlePathCoord{26, 25}));
    OL_CHECK(movement_session.render(framebuffer));
    movement_session.finish_presented_tick(200U);
    OL_CHECK(movement_session.handle_key(0x0DU) ==
             BattleSessionInputResult::cursor_selected);
    OL_CHECK(movement_session.phase() ==
             BattleSessionPhase::player_movement_step_present);
    OL_CHECK(movement_session.setup().combatants()[0U].words[combatant_word::x] == 26);
    OL_CHECK(movement_session.setup().combatants()[0U].words[combatant_word::y] == 25);
    OL_CHECK(
        movement_session.setup().combatants()[0U].words[combatant_word::round_value] == 1);
    OL_CHECK(movement_session.render(framebuffer));
    movement_session.finish_presented_tick(200U);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_movement_wait);
    movement_session.advance(200U);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_movement_wait);
    movement_session.advance(201U);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_movement_wait);
    movement_session.advance(202U);
    finish_player_menu_redraw(movement_session);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(movement_session.player_action_menu().available[0U] == 1);

    OL_CHECK(movement_session.handle_key(0x96U) == BattleSessionInputResult::action_selected);
    OL_CHECK(movement_session.render(framebuffer));
    movement_session.finish_presented_tick(202U);
    OL_CHECK(movement_session.handle_key(0x9AU) ==
             BattleSessionInputResult::cursor_changed);
    OL_CHECK((movement_session.active_cursor() == BattlePathCoord{25, 25}));
    OL_CHECK(movement_session.handle_key(0x20U) ==
             BattleSessionInputResult::cursor_selected);
    OL_CHECK(movement_session.setup().combatants()[0U].words[combatant_word::x] == 25);
    OL_CHECK(movement_session.setup().combatants()[0U].words[combatant_word::y] == 25);
    OL_CHECK(
        movement_session.setup().combatants()[0U].words[combatant_word::round_value] == 0);
    OL_CHECK(movement_ranger.roles[1U].word(role_word::physical_power) == 10);
    OL_CHECK(movement_session.render(framebuffer));
    movement_session.finish_presented_tick(202U);
    movement_session.advance(203U);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_movement_wait);
    movement_session.advance(204U);
    finish_player_menu_redraw(movement_session);
    OL_CHECK(movement_session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(movement_session.player_action_menu().available[0U] == 0);
    OL_CHECK(movement_session.player_action_menu().available_count == 5U);
    openlegend::diagnostics::shutdown_logging();
    std::ifstream movement_log_file{log_path, std::ios::binary};
    const std::string movement_log_text{
        std::istreambuf_iterator<char>{movement_log_file},
        std::istreambuf_iterator<char>{}};
    OL_CHECK(movement_log_text.find("battle player movement selection ready id=4") !=
             std::string::npos);
    OL_CHECK(movement_log_text.find("battle player movement cursor id=4") !=
             std::string::npos);
    OL_CHECK(movement_log_text.find("battle player movement step id=4") !=
             std::string::npos);
    OL_CHECK(movement_log_text.find("battle player movement step presented id=4") !=
             std::string::npos);
    OL_CHECK(movement_log_text.find(
                 "battle player action menu rebuilt after movement id=4") !=
             std::string::npos);

    const auto targeting_log_path = log_path.parent_path() / "b8-battle-targeting.log";
    std::filesystem::remove(targeting_log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 targeting_log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);
    auto& targeting_actor = filtered_ranger.roles[1U];
    targeting_actor.set_word(role_word::hp, 100);
    targeting_actor.set_word(role_word::maximum_hp, 100);
    targeting_actor.set_word(role_word::physical_power, 100);
    targeting_actor.set_word(role_word::use_poison, 30);
    targeting_actor.set_word(role_word::detoxification, 30);
    targeting_actor.set_word(role_word::medicine, 30);
    filtered_ranger.roles[3U].set_word(role_word::hp, 100);
    filtered_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom targeting_random{1U};
    auto targeting_session = std::make_unique<BattleSession>(
        data_root, filtered_ranger, targeting_random, 4, false);
    reach_player_action(*targeting_session);
    OL_CHECK((targeting_session->player_action_menu().available ==
              std::array<std::int16_t, 10>{0, 0, 1, 1, 1, 1, 1, 1, 1, 1}));
    OL_CHECK(targeting_session->player_action_menu().available_count == 8U);
    OL_CHECK(targeting_session->handle_key(0x0DU) ==
             BattleSessionInputResult::action_selected);
    OL_CHECK(targeting_session->phase() == BattleSessionPhase::player_targeting_select);
    OL_CHECK((targeting_session->active_cursor() == BattlePathCoord{26, 24}));
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x1BU) ==
             BattleSessionInputResult::cursor_cancelled);
    finish_player_menu_redraw(*targeting_session);
    OL_CHECK(targeting_session->phase() == BattleSessionPhase::player_action);
    OL_CHECK(targeting_session->player_action_menu().cursor == 0U);
    OL_CHECK(targeting_session->player_action_menu().selected_action == -1);
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x98U) ==
             BattleSessionInputResult::action_changed);
    OL_CHECK(targeting_session->player_action_menu().cursor == 1U);
    OL_CHECK(targeting_session->handle_key(0x20U) ==
             BattleSessionInputResult::action_selected);
    OL_CHECK(targeting_session->phase() == BattleSessionPhase::player_targeting_select);
    OL_CHECK(targeting_session->player_action_menu().selected_action ==
             static_cast<std::int16_t>(BattlePlayerAction::detoxification));
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x1BU) ==
             BattleSessionInputResult::cursor_cancelled);
    OL_CHECK(targeting_session->player_action_menu().cursor == 1U);
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x98U) ==
             BattleSessionInputResult::action_changed);
    OL_CHECK(targeting_session->player_action_menu().cursor == 2U);
    OL_CHECK(targeting_session->handle_key(0x20U) ==
             BattleSessionInputResult::action_selected);
    OL_CHECK(targeting_session->phase() == BattleSessionPhase::player_targeting_select);
    OL_CHECK(targeting_session->player_action_menu().selected_action ==
             static_cast<std::int16_t>(BattlePlayerAction::medicine));
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x1BU) ==
             BattleSessionInputResult::cursor_cancelled);
    OL_CHECK(targeting_session->player_action_menu().cursor == 2U);
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x9EU) ==
             BattleSessionInputResult::action_changed);
    OL_CHECK(targeting_session->handle_key(0x9EU) ==
             BattleSessionInputResult::action_changed);
    OL_CHECK(targeting_session->player_action_menu().cursor == 0U);
    OL_CHECK(targeting_session->handle_key(0x20U) ==
             BattleSessionInputResult::action_selected);
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x98U) ==
             BattleSessionInputResult::cursor_changed);
    OL_CHECK((targeting_session->active_cursor() == BattlePathCoord{26, 25}));
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x98U) ==
             BattleSessionInputResult::cursor_changed);
    OL_CHECK((targeting_session->active_cursor() == BattlePathCoord{26, 26}));
    OL_CHECK(targeting_session->render(framebuffer));
    targeting_session->finish_presented_tick(250U);
    OL_CHECK(targeting_session->handle_key(0x20U) ==
             BattleSessionInputResult::cursor_selected);
    OL_CHECK(targeting_session->phase() ==
             BattleSessionPhase::player_magic_frame_present);
    OL_CHECK((targeting_session->selected_player_target() == BattlePathCoord{26, 26}));
    OL_CHECK(targeting_session->player_action_menu().selected_action ==
             static_cast<std::int16_t>(BattlePlayerAction::use_poison));
    openlegend::diagnostics::shutdown_logging();
    std::ifstream targeting_log_file{targeting_log_path, std::ios::binary};
    const std::string targeting_log_text{
        std::istreambuf_iterator<char>{targeting_log_file},
        std::istreambuf_iterator<char>{}};
    OL_CHECK(targeting_log_text.find(
                 "battle player targeting ready id=4 slot=0 action=2 source=26,24 path_limit=3") !=
             std::string::npos);
    OL_CHECK(targeting_log_text.find(
                 "battle player targeting ready id=4 slot=0 action=3 source=26,24 path_limit=3") !=
             std::string::npos);
    OL_CHECK(targeting_log_text.find(
                 "battle player targeting ready id=4 slot=0 action=4 source=26,24 path_limit=3") !=
             std::string::npos);
    OL_CHECK(targeting_log_text.find(
                 "battle player targeting cursor id=4 slot=0 cursor=26,26") !=
             std::string::npos);
    OL_CHECK(targeting_log_text.find("battle player targeting cancelled id=4 slot=0") !=
             std::string::npos);
    OL_CHECK(targeting_log_text.find(
                 "battle player target selected id=4 slot=0 action=2 target=26,26") !=
             std::string::npos);

    auto automatic_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    automatic_ranger.roles[1U].set_word(role_word::hp, 100);
    automatic_ranger.roles[1U].set_word(role_word::maximum_hp, 100);
    automatic_ranger.roles[3U].set_word(role_word::hp, 100);
    automatic_ranger.roles[3U].set_word(role_word::maximum_hp, 100);
    openlegend::random::LegacyRandom automatic_random{1U};
    auto automatic_session_storage = std::make_unique<BattleSession>(
        data_root, automatic_ranger, automatic_random, 4, false);
    auto& automatic_session = *automatic_session_storage;
    reach_player_action(automatic_session);
    for (std::size_t step = 0U; step < 4U; ++step) {
        OL_CHECK(
            automatic_session.handle_key(0x98U) == BattleSessionInputResult::action_changed);
    }
    OL_CHECK(
        automatic_session.handle_key(0x0DU) == BattleSessionInputResult::action_selected);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::automatic_present);
    OL_CHECK(!automatic_session.setup().automatic_enabled());
    OL_CHECK(automatic_session.render(framebuffer));
    automatic_session.finish_presented_tick(100U);
    OL_CHECK(automatic_session.setup().automatic_enabled());
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_action);
    automatic_session.advance(100U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(automatic_session.render(framebuffer));
    automatic_session.finish_presented_tick(100U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_wait);
    automatic_session.advance(100U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_wait);
    for (std::uint32_t tick = 101U; tick < 108U; ++tick) {
        automatic_session.advance(tick);
        OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_wait);
    }
    automatic_session.advance(108U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(automatic_session.current_actor_slot() == 1U);
    OL_CHECK(automatic_ranger.roles[1U].word(role_word::physical_power) == 5);
    OL_CHECK(
        automatic_session.setup().combatants()[0U].words[combatant_word::action_done] == 1);

    automatic_session.set_confirmation_state(true); // Arrives after the slot input poll.
    OL_CHECK(automatic_session.render(framebuffer));
    automatic_session.finish_presented_tick(108U);
    OL_CHECK(automatic_session.setup().automatic_enabled());
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_action);
    automatic_session.set_confirmation_state(false); // Released before the next slot.
    automatic_session.advance(108U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(automatic_session.render(framebuffer));
    automatic_session.finish_presented_tick(108U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_wait);
    for (std::uint32_t tick = 109U; tick < 116U; ++tick) {
        automatic_session.advance(tick);
        OL_CHECK(automatic_session.phase() == BattleSessionPhase::ai_wait);
    }
    automatic_session.advance(116U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::round_wait);
    OL_CHECK(automatic_ranger.roles[3U].word(role_word::physical_power) == 4);
    OL_CHECK(
        automatic_session.setup().combatants()[1U].words[combatant_word::action_done] == 1);
    automatic_session.set_confirmation_state(true);
    automatic_session.advance(0U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::round_wait);
    OL_CHECK(automatic_session.setup().automatic_enabled());
    automatic_session.advance(1U);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(automatic_session.current_actor_slot() == 0U);
    OL_CHECK(
        automatic_session.setup().combatants()[0U].words[combatant_word::action_done] == 1);
    OL_CHECK(!automatic_session.setup().automatic_enabled());
    OL_CHECK(automatic_session.take_clear_confirmation_states_request());
    OL_CHECK(!automatic_session.take_clear_confirmation_states_request());
    OL_CHECK(automatic_session.render(framebuffer));
    automatic_session.finish_presented_tick(1U);
    OL_CHECK(!automatic_session.setup().automatic_enabled());
    finish_player_menu_redraw(automatic_session);
    OL_CHECK(automatic_session.phase() == BattleSessionPhase::player_action);
    OL_CHECK(
        automatic_session.setup().combatants()[0U].words[combatant_word::action_done] == 0);
    OL_CHECK(automatic_session.setup().combatants()[0U].words[combatant_word::ai_action] == 0);

    std::filesystem::remove(log_path, log_error);
    OL_CHECK(openlegend::diagnostics::initialize_logging(
                 log_path, openlegend::diagnostics::LogLevel::debug) ==
             openlegend::diagnostics::LoggingInitializationStatus::initialized);
    auto escape_ranger = make_ranger({0, 2, -1, -1, -1, -1});
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        escape_ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
    }
    for (const auto role_id : {0U, 1U, 2U, 4U}) {
        auto& role = escape_ranger.roles[role_id];
        role.set_word(role_word::hp, 100);
        role.set_word(role_word::maximum_hp, 100);
        role.set_word(role_word::hurt, 0);
        role.set_word(role_word::poison, 0);
        role.set_word(role_word::physical_power, 100);
        role.set_word(role_word::mp, 0);
        role.set_word(role_word::maximum_mp, 0);
        role.set_word(role_word::attack, 10);
        role.set_word(role_word::medicine, 0);
        role.set_word(role_word::detoxification, 0);
        role.set_word(role_word::use_poison, 0);
        role.set_word(role_word::speed, 0);
        for (std::size_t magic_slot = 0U; magic_slot < role_word::magic_count; ++magic_slot) {
            role.set_word(role_word::magic_id_begin + magic_slot, 0);
        }
    }
    escape_ranger.roles[0U].set_word(role_word::hp, 19);
    escape_ranger.roles[0U].set_word(role_word::speed, 45);
    openlegend::random::LegacyRandom escape_session_random{10U};
    BattleSession escape_session{
        data_root, escape_ranger, escape_session_random, 2, false};
    OL_CHECK(escape_session.valid());
    for (std::size_t index = 0U;
         index < escape_session.setup().party_prefix_length();
         ++index) {
        if (escape_session.setup().selection_states()[index] == 0) {
            static_cast<void>(escape_session.handle_key(0x0DU));
        }
        OL_CHECK(escape_session.handle_key(0x98U) == BattleSessionInputResult::changed);
    }
    OL_CHECK(escape_session.handle_key(0x0DU) ==
             BattleSessionInputResult::selection_complete);
    finish_battle_entry_fade(escape_session);
    OL_CHECK(escape_session.render(framebuffer));
    escape_session.finish_presented_tick(300U);
    for (std::size_t frame = 0U; frame < escape_session.fade_frame_count(); ++frame) {
        OL_CHECK(escape_session.render(framebuffer));
        escape_session.finish_presented_tick(300U);
    }
    escape_session.setup().enable_automatic_mode();
    escape_session.advance(300U);
    OL_CHECK(escape_session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(escape_session.current_actor_slot() == 0U);
    OL_CHECK(escape_session.setup().combatants()[0U].words[combatant_word::role_id] == 0);
    OL_CHECK(
        escape_session.setup().combatants()[0U].words[combatant_word::round_value] == 3);
    const BattlePathCoord escape_source{
        escape_session.setup().combatants()[0U].words[combatant_word::x],
        escape_session.setup().combatants()[0U].words[combatant_word::y]};
    OL_CHECK(escape_session.render(framebuffer));
    escape_session.finish_presented_tick(300U);
    OL_CHECK(escape_session.phase() == BattleSessionPhase::ai_action);
    escape_session.advance(300U);
    OL_CHECK(escape_session.phase() == BattleSessionPhase::ai_prelude_present);
    OL_CHECK(escape_session.render(framebuffer));
    escape_session.finish_presented_tick(300U);
    for (std::uint32_t tick = 301U; tick < 308U; ++tick) {
        escape_session.advance(tick);
        OL_CHECK(escape_session.phase() == BattleSessionPhase::ai_wait);
    }
    escape_session.advance(308U);
    OL_CHECK(escape_session.phase() == BattleSessionPhase::ai_movement_step_present);
    OL_CHECK(escape_session.setup().combatants()[0U].words[combatant_word::ai_action] == 0);
    OL_CHECK((BattlePathCoord{
                  escape_session.setup().combatants()[0U].words[combatant_word::x],
                  escape_session.setup().combatants()[0U].words[combatant_word::y]} !=
              escape_source));
    OL_CHECK(
        escape_session.setup().combatants()[0U].words[combatant_word::round_value] == 2);
    std::uint32_t movement_tick = 308U;
    std::size_t movement_steps = 0U;
    while (escape_session.phase() == BattleSessionPhase::ai_movement_step_present &&
           movement_steps < 4U) {
        OL_CHECK(escape_session.render(framebuffer));
        escape_session.finish_presented_tick(movement_tick);
        OL_CHECK(escape_session.phase() == BattleSessionPhase::ai_movement_wait);
        escape_session.advance(++movement_tick);
        OL_CHECK(escape_session.phase() == BattleSessionPhase::ai_movement_wait);
        escape_session.advance(++movement_tick);
        ++movement_steps;
    }
    OL_CHECK(movement_steps == 3U);
    OL_CHECK(escape_session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(escape_session.current_actor_slot() == 1U);
    const auto& escaped_actor = escape_session.setup().combatants()[0U].words;
    OL_CHECK((BattlePathCoord{
                  escaped_actor[combatant_word::x], escaped_actor[combatant_word::y]} ==
              BattlePathCoord{31, 22}));
    OL_CHECK(escaped_actor[combatant_word::round_value] == 0);
    OL_CHECK(escape_ranger.roles[0U].word(role_word::physical_power) == 100);
    OL_CHECK(escaped_actor[combatant_word::ai_action] == 0);
    OL_CHECK(escaped_actor[combatant_word::action_done] == 1);
    openlegend::diagnostics::shutdown_logging();
    std::ifstream escape_log_file{log_path, std::ios::binary};
    const std::string escape_log_text{
        std::istreambuf_iterator<char>{escape_log_file},
        std::istreambuf_iterator<char>{}};
    OL_CHECK(
        escape_log_text.find("source=30,24 destination=31,21 continuation=1") !=
        std::string::npos);
    OL_CHECK(escape_log_text.find(
                 "from=30,24 to=30,23 round_value=2 physical_power=100") !=
             std::string::npos);
    OL_CHECK(escape_log_text.find(
                 "from=30,23 to=31,23 round_value=1 physical_power=100") !=
             std::string::npos);
    OL_CHECK(escape_log_text.find(
                 "from=31,23 to=31,22 round_value=0 physical_power=100") !=
             std::string::npos);
    const std::string movement_presented{"battle AI movement step presented id=2"};
    std::size_t presented_count = 0U;
    for (std::size_t offset = 0U;
         (offset = escape_log_text.find(movement_presented, offset)) != std::string::npos;
         offset += movement_presented.size()) {
        ++presented_count;
    }
    OL_CHECK(presented_count == 3U);
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
            record.set_word(role_word::equipment_begin, -1);
            record.set_word(role_word::mp, 0);
            record.set_word(role_word::maximum_mp, 0);
            record.set_word(role_word::attack, 10);
            record.set_word(role_word::medicine, 0);
            record.set_word(role_word::use_poison, 0);
            record.set_word(role_word::detoxification, 0);
            record.set_word(role_word::anti_poison, 0);
            record.set_word(role_word::hidden_weapon, 0);
            record.set_word(role_word::id, static_cast<std::int16_t>(role));
            record.set_word(role_word::morality, 50);
            record.set_word(role_word::iq, 0);
            for (std::size_t slot = 0U; slot < role_word::magic_count; ++slot) {
                record.set_word(role_word::magic_id_begin + slot, 0);
                record.set_word(role_word::magic_level_begin + slot, 0);
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
            combatant[combatant_word::round_value] = 0;
            combatant[combatant_word::action_done] = 0;
            combatant[combatant_word::ai_action] = -1;
            combatant[combatant_word::ai_target] = -1;
            combatant[combatant_word::ai_poison_target] = -1;
        }
    };

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 21);
    ranger.roles[0U].set_word(role_word::hurt, 50);
    ranger.roles[0U].set_word(role_word::physical_power, 50);
    auto choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::medicine);
    OL_CHECK(choice->target_slot == 0);
    OL_CHECK((choice->target == BattlePathCoord{10, 20}));
    OL_CHECK(setup.combatants()[0U].words[combatant_word::ai_action] == 5);

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 20);
    ranger.roles[0U].set_word(role_word::hurt, 49);
    ranger.roles[0U].set_word(role_word::physical_power, 50);
    choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::medicine);
    OL_CHECK(choice->target_slot == 0);

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 20);
    ranger.roles[0U].set_word(role_word::hurt, 50);
    ranger.roles[0U].set_word(role_word::physical_power, 50);
    setup.combatants()[0U].words[combatant_word::ai_action] = 77;
    choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::none);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::ai_action] == 77);

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 100);
    ranger.roles[0U].set_word(role_word::hurt, 50);
    ranger.roles[0U].set_word(role_word::physical_power, 49);
    choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::none);

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
    setup.combatants()[0U].words[combatant_word::side] = 1;
    ranger.roles[0U].set_word(role_word::taking_item_begin + 2U, 5);
    ranger.roles[0U].set_word(role_word::taking_item_count_begin + 2U, 0);
    ranger.items[5U].set_word(item_word::add_hp, 1);
    choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::item);
    OL_CHECK(choice->item_source == BattleAiItemSource::carried);
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
    ranger.roles[0U].set_word(role_word::hurt, 80);
    ranger.roles[1U].set_word(role_word::medicine, 51);
    ranger.roles[2U].set_word(role_word::medicine, 51);
    setup.combatants()[1U].words[combatant_word::occupancy_hidden] = 1;
    choice = setup.choose_ai_low_hp_action(0U);
    OL_CHECK(choice.has_value());
    OL_CHECK(choice->action == BattleAiAction::request_medicine);
    OL_CHECK(choice->target_slot == 2);
    OL_CHECK((choice->target == BattlePathCoord{12, 22}));

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
    for (std::size_t slot = 0U; slot < role_word::taking_item_count; ++slot) {
        ranger.roles[3U].set_word(
            role_word::taking_item_begin + slot,
            static_cast<std::int16_t>(5 + slot));
        ranger.roles[3U].set_word(
            role_word::taking_item_count_begin + slot,
            static_cast<std::int16_t>(1 + slot));
    }
    OL_CHECK(setup.remove_carried_item_slot(3U, 1U));
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_begin) == 5);
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_begin + 1U) == 7);
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_begin + 2U) == 8);
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_begin + 3U) == -1);
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_count_begin) == 1);
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_count_begin + 1U) == 3);
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_count_begin + 2U) == 4);
    OL_CHECK(ranger.roles[3U].word(role_word::taking_item_count_begin + 3U) == 0);
    OL_CHECK(!setup.remove_carried_item_slot(3U, role_word::taking_item_count));

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

    reset();
    const auto prelude = setup.begin_ai_turn(0U);
    OL_CHECK(prelude.has_value());
    OL_CHECK(prelude->allied_total == 330);
    OL_CHECK(prelude->opponent_total == 220);
    OL_CHECK(prelude->allied_count == 3);
    OL_CHECK(prelude->opponent_count == 2);
    OL_CHECK(prelude->render_required);
    OL_CHECK(prelude->present_required);
    OL_CHECK(prelude->wait_ticks == 300);
    const auto choose_turn = [&setup](openlegend::random::LegacyRandom& turn_random)
        -> std::optional<BattleAiTurnDecision> {
        const auto turn_prelude = setup.begin_ai_turn(0U);
        if (!turn_prelude.has_value()) {
            return std::nullopt;
        }
        return setup.choose_ai_turn_action(0U, *turn_prelude, turn_random);
    };

    ranger.roles[0U].set_word(role_word::physical_power, 9);
    openlegend::random::LegacyRandom wait_random{1U};
    auto decision = choose_turn(wait_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::wait);
    OL_CHECK(decision->handler == BattleAiHandler::rest);
    OL_CHECK(wait_random.state() == 1U);

    reset();
    ranger.roles[0U].set_word(role_word::physical_power, 9);
    ranger.roles[0U].set_word(role_word::hp, 10);
    openlegend::random::LegacyRandom cleared_wait_random{1U};
    decision = choose_turn(cleared_wait_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::none);
    OL_CHECK(decision->handler == BattleAiHandler::rest);
    OL_CHECK(cleared_wait_random.state() == 662'824'084U);

    reset();
    ranger.roles[0U].set_word(role_word::poison, 100);
    ranger.roles[0U].set_word(role_word::detoxification, 100);
    openlegend::random::LegacyRandom poisoned_entry_random{1U};
    decision = choose_turn(poisoned_entry_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::detox);
    OL_CHECK(decision->handler == BattleAiHandler::detox);
    OL_CHECK(poisoned_entry_random.state() == 1'103'527'590U);

    reset();
    ranger.roles[0U].set_word(role_word::mp, 0);
    ranger.roles[0U].set_word(role_word::maximum_mp, 100);
    ranger.items[7U].set_word(item_word::add_mp, 1);
    ranger.header.set_inventory(3U, openlegend::model::ItemId{7}, 0);
    openlegend::random::LegacyRandom low_mp_entry_random{1U};
    decision = choose_turn(low_mp_entry_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::item);
    OL_CHECK(decision->choice.item_slot == 3);
    OL_CHECK(decision->handler == BattleAiHandler::item);
    OL_CHECK(low_mp_entry_random.state() == 662'824'084U);

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 80);
    setup.combatants()[1U].words[combatant_word::ai_action] =
        static_cast<std::int16_t>(BattleAiAction::request_medicine);
    openlegend::random::LegacyRandom medicine_entry_random{1U};
    decision = choose_turn(medicine_entry_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::medicine);
    OL_CHECK(decision->choice.target_slot == 1);
    OL_CHECK(decision->handler == BattleAiHandler::medicine);
    OL_CHECK(medicine_entry_random.state() == 662'824'084U);

    reset();
    ranger.roles[0U].set_word(role_word::detoxification, 80);
    setup.combatants()[1U].words[combatant_word::ai_action] =
        static_cast<std::int16_t>(BattleAiAction::request_detox);
    openlegend::random::LegacyRandom detox_entry_random{1U};
    decision = choose_turn(detox_entry_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::detox);
    OL_CHECK(decision->choice.target_slot == 1);
    OL_CHECK(decision->handler == BattleAiHandler::detox);
    OL_CHECK(detox_entry_random.state() == 662'824'084U);

    reset();
    ranger.roles[0U].set_word(role_word::hp, 19);
    openlegend::random::LegacyRandom escape_random{10U};
    decision = choose_turn(escape_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::escape);
    OL_CHECK(decision->handler == BattleAiHandler::escape);
    OL_CHECK(escape_random.state() == 1'849'040'536U);

    reset();
    ranger.roles[0U].set_word(role_word::mp, 5);
    ranger.roles[0U].set_word(role_word::maximum_mp, 5);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::need_mp, 5);
    openlegend::random::LegacyRandom entry_attack_random{1U};
    decision = choose_turn(entry_attack_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::attack);
    OL_CHECK(decision->handler == BattleAiHandler::attack);
    OL_CHECK(!decision->choice.action_code_written);
    OL_CHECK(entry_attack_random.state() == 662'824'084U);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 0);
    OL_CHECK(setup.finish_ai_turn(0U));
    OL_CHECK(setup.combatants()[0U].words[combatant_word::action_done] == 1);

    reset();
    ranger.roles[0U].set_word(role_word::medicine, 20);
    ranger.roles[0U].set_word(role_word::mp, 5);
    ranger.roles[0U].set_word(role_word::maximum_mp, 5);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::need_mp, 5);
    const auto frozen_prelude = setup.begin_ai_turn(0U);
    OL_CHECK(frozen_prelude.has_value());
    ranger.roles[1U].set_word(role_word::hp, 700);
    ranger.roles[1U].set_word(role_word::maximum_hp, 1'000);
    ranger.roles[2U].set_word(role_word::hp, 700);
    ranger.roles[2U].set_word(role_word::maximum_hp, 1'000);
    ranger.roles[3U].set_word(role_word::hp, 300);
    ranger.roles[3U].set_word(role_word::maximum_hp, 300);
    ranger.roles[4U].set_word(role_word::hp, 300);
    ranger.roles[4U].set_word(role_word::maximum_hp, 300);
    openlegend::random::LegacyRandom frozen_prelude_random{1U};
    decision = setup.choose_ai_turn_action(
        0U, *frozen_prelude, frozen_prelude_random);
    OL_CHECK(decision.has_value());
    OL_CHECK(decision->choice.action == BattleAiAction::attack);
    OL_CHECK(decision->handler == BattleAiHandler::attack);
    OL_CHECK(frozen_prelude_random.state() == 3'295'386'429U);

    reset();
    std::ranges::fill(data.occupancy(), static_cast<std::int16_t>(-1));
    for (std::size_t slot = 0U; slot < 5U; ++slot) {
        const auto& combatant = setup.combatants()[slot].words;
        const auto index = static_cast<std::size_t>(combatant[combatant_word::y]) * 64U +
            static_cast<std::size_t>(combatant[combatant_word::x]);
        data.occupancy()[index] = static_cast<std::int16_t>(slot);
    }
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    const auto escape_plan = setup.ai_escape_plan(0U, true);
    OL_CHECK(escape_plan.has_value());
    OL_CHECK(escape_plan->destination.has_value());
    OL_CHECK((*escape_plan->destination == BattlePathCoord{7, 20}));
    OL_CHECK(escape_plan->maximum_enemy_distance_sum == 20);
    OL_CHECK(escape_plan->rest_after_move);
    const auto reposition_plan = setup.ai_escape_plan(0U, false);
    OL_CHECK(reposition_plan.has_value());
    OL_CHECK(reposition_plan->destination == escape_plan->destination);
    OL_CHECK(!reposition_plan->rest_after_move);

    reset();
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    openlegend::random::LegacyRandom strongest_random{9U};
    auto target = setup.choose_ai_attack_target(0U, strongest_random);
    OL_CHECK(target.has_value());
    OL_CHECK(target->strategy == BattleAiTargetStrategy::strongest_attack);
    OL_CHECK(target->target_slot == 4);
    OL_CHECK(target->target_written);
    OL_CHECK(strongest_random.state() == 1'341'714'958U);

    reset();
    ranger.roles[0U].set_word(role_word::morality, 25);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    openlegend::random::LegacyRandom weakest_random{9U};
    target = setup.choose_ai_attack_target(0U, weakest_random);
    OL_CHECK(target.has_value());
    OL_CHECK(target->strategy == BattleAiTargetStrategy::weakest_attack);
    OL_CHECK(target->target_slot == 3);
    OL_CHECK(weakest_random.state() == 1'341'714'958U);

    reset();
    ranger.roles[0U].set_word(role_word::iq, 70);
    ranger.roles[3U].set_word(role_word::medicine, 30);
    ranger.roles[4U].set_word(role_word::medicine, 10);
    openlegend::random::LegacyRandom specialist_random{9U};
    target = setup.choose_ai_attack_target(0U, specialist_random);
    OL_CHECK(target.has_value());
    OL_CHECK(target->strategy == BattleAiTargetStrategy::specialist);
    OL_CHECK(target->target_slot == 3);
    OL_CHECK(specialist_random.state() == 1'341'714'958U);

    reset();
    ranger.roles[0U].set_word(role_word::iq, 70);
    ranger.roles[1U].set_word(role_word::use_poison, 21);
    ranger.roles[3U].set_word(role_word::detoxification, 30);
    ranger.roles[3U].set_word(role_word::attack, 50);
    ranger.roles[4U].set_word(role_word::attack, 10);
    openlegend::random::LegacyRandom specialist_bug_random{9U};
    target = setup.choose_ai_attack_target(0U, specialist_bug_random);
    OL_CHECK(target.has_value());
    OL_CHECK(target->strategy == BattleAiTargetStrategy::specialist);
    OL_CHECK(target->target_slot == 4);
    OL_CHECK(specialist_bug_random.state() == 1'341'714'958U);

    reset();
    openlegend::random::LegacyRandom nearest_random{1U};
    target = setup.choose_ai_attack_target(0U, nearest_random);
    OL_CHECK(target.has_value());
    OL_CHECK(target->strategy == BattleAiTargetStrategy::nearest);
    OL_CHECK(target->target_slot == 3);
    OL_CHECK(nearest_random.state() == 1U);

    reset();
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[0U].set_word(role_word::iq, 70);
    openlegend::random::LegacyRandom cascade_random{1U};
    target = setup.choose_ai_attack_target(0U, cascade_random);
    OL_CHECK(target.has_value());
    OL_CHECK(target->strategy == BattleAiTargetStrategy::nearest);
    OL_CHECK(cascade_random.state() == 2'524'885'223U);

    reset();
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[3U].set_word(role_word::attack, 0);
    ranger.roles[4U].set_word(role_word::attack, 0);
    openlegend::random::LegacyRandom no_strongest_random{9U};
    target = setup.choose_ai_attack_target(0U, no_strongest_random);
    OL_CHECK(target.has_value());
    OL_CHECK(target->strategy == BattleAiTargetStrategy::strongest_attack);
    OL_CHECK(target->target_slot == -1);
    OL_CHECK(!target->target_written);
    OL_CHECK(no_strongest_random.state() == 1'341'714'958U);

    reset();
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.roles[0U].set_word(role_word::magic_id_begin + 1U, 2);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 8);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 0);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom ordered_attack_random{9U};
    auto attack_plan = setup.begin_ai_attack_plan(0U, ordered_attack_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->magic_slot == 0);
    OL_CHECK(attack_plan->magic_id == 1);
    OL_CHECK(attack_plan->target_strategy == BattleAiTargetStrategy::strongest_attack);
    OL_CHECK(attack_plan->target_slot == 4);
    OL_CHECK(attack_plan->target_distance == 8);
    OL_CHECK(attack_plan->movement_mode == 1);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::attack);
    OL_CHECK(attack_plan->automatic_attack);
    OL_CHECK(attack_plan->mark_action_done_after_step);
    OL_CHECK(ordered_attack_random.state() == 2'878'571'567U);

    reset();
    ranger.roles[0U].set_word(role_word::equipment_begin, 106);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 57);
    ranger.magics[57U].set_word(magic_word::select_distance_begin, 6);
    ranger.magics[57U].set_word(magic_word::attack_area_type, 0);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom bonus_random{1U};
    attack_plan = setup.begin_ai_attack_plan(0U, bonus_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->magic_slot == 0);
    OL_CHECK(attack_plan->magic_id == 57);
    OL_CHECK(attack_plan->special_attack_bonus == 100);
    OL_CHECK(attack_plan->target_slot == 3);
    OL_CHECK(attack_plan->target_distance == 6);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::attack);
    OL_CHECK(bonus_random.state() == 1U);

    reset();
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 6);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 1);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom aligned_random{1U};
    attack_plan = setup.begin_ai_attack_plan(0U, aligned_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->target_slot == 3);
    OL_CHECK(attack_plan->target_distance == 6);
    OL_CHECK(attack_plan->movement_mode == 2);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::move);
    setup.combatants()[0U].words[combatant_word::x] = 12;
    setup.combatants()[0U].words[combatant_word::y] = 23;
    auto resumed_attack_plan = setup.resume_ai_attack_after_move(0U, *attack_plan);
    OL_CHECK(resumed_attack_plan.has_value());
    OL_CHECK(resumed_attack_plan->target_slot == 3);
    OL_CHECK(resumed_attack_plan->target_distance == 1);
    OL_CHECK(!resumed_attack_plan->target_reselected);
    OL_CHECK(resumed_attack_plan->next_step == BattleAiAttackNextStep::attack);

    reset();
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 6);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 2);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom cross_random{1U};
    attack_plan = setup.begin_ai_attack_plan(0U, cross_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->movement_mode == 2);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::move);

    reset();
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 6);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 3);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom square_random{1U};
    attack_plan = setup.begin_ai_attack_plan(0U, square_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->movement_mode == 1);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::attack);

    reset();
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 100);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 4);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom unsupported_random{1U};
    attack_plan = setup.begin_ai_attack_plan(0U, unsupported_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->movement_mode == 0);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::move);

    reset();
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 1);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 0);
    openlegend::random::LegacyRandom no_move_random{1U};
    attack_plan = setup.begin_ai_attack_plan(0U, no_move_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->target_distance == 6);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::finish);

    reset();
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 1);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 0);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    setup.combatants()[3U].words[combatant_word::x] = 11;
    setup.combatants()[3U].words[combatant_word::y] = 20;
    openlegend::random::LegacyRandom reselect_random{9U};
    attack_plan = setup.begin_ai_attack_plan(0U, reselect_random);
    OL_CHECK(attack_plan.has_value());
    OL_CHECK(attack_plan->target_slot == 4);
    OL_CHECK(attack_plan->next_step == BattleAiAttackNextStep::move);
    resumed_attack_plan = setup.resume_ai_attack_after_move(0U, *attack_plan);
    OL_CHECK(resumed_attack_plan.has_value());
    OL_CHECK(resumed_attack_plan->target_strategy == BattleAiTargetStrategy::nearest);
    OL_CHECK(resumed_attack_plan->target_slot == 3);
    OL_CHECK(resumed_attack_plan->target_distance == 1);
    OL_CHECK(resumed_attack_plan->target_reselected);
    OL_CHECK(resumed_attack_plan->next_step == BattleAiAttackNextStep::attack);

    reset();
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[0U].set_word(role_word::magic_id_begin, 1);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    ranger.magics[1U].set_word(magic_word::select_distance_begin, 1);
    ranger.magics[1U].set_word(magic_word::attack_area_type, 0);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom rest_random{9U};
    attack_plan = setup.begin_ai_attack_plan(0U, rest_random);
    OL_CHECK(attack_plan.has_value());
    resumed_attack_plan = setup.resume_ai_attack_after_move(0U, *attack_plan);
    OL_CHECK(resumed_attack_plan.has_value());
    OL_CHECK(resumed_attack_plan->target_slot == 3);
    OL_CHECK(resumed_attack_plan->target_distance == 6);
    OL_CHECK(resumed_attack_plan->target_reselected);
    OL_CHECK(resumed_attack_plan->next_step == BattleAiAttackNextStep::rest);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    ranger.roles[0U].set_word(role_word::iq, 61);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    openlegend::random::LegacyRandom poison_strongest_random{9U};
    auto poison_target = setup.choose_ai_poison_target(0U, 3U, poison_strongest_random);
    OL_CHECK(poison_target.has_value());
    OL_CHECK(poison_target->strategy == BattleAiPoisonTargetStrategy::strongest_attack);
    OL_CHECK(poison_target->target_slot == 4);
    OL_CHECK(poison_target->target_written);
    OL_CHECK(poison_strongest_random.state() == 1'341'714'958U);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    ranger.roles[0U].set_word(role_word::iq, 61);
    ranger.roles[3U].set_word(role_word::attack, 0);
    ranger.roles[4U].set_word(role_word::attack, 0);
    openlegend::random::LegacyRandom poison_stale_random{9U};
    poison_target = setup.choose_ai_poison_target(0U, 4U, poison_stale_random);
    OL_CHECK(poison_target.has_value());
    OL_CHECK(
        poison_target->strategy == BattleAiPoisonTargetStrategy::first_eligible_stale_distance);
    OL_CHECK(poison_target->target_slot == 3);
    OL_CHECK(poison_target->stale_target_distance == 8);
    OL_CHECK(poison_stale_random.state() == 1'341'714'958U);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    ranger.roles[0U].set_word(role_word::iq, 60);
    openlegend::random::LegacyRandom poison_first_random{1U};
    poison_target = setup.choose_ai_poison_target(0U, 4U, poison_first_random);
    OL_CHECK(poison_target.has_value());
    OL_CHECK(poison_target->target_slot == 3);
    OL_CHECK(poison_target->stale_target_distance == 8);
    OL_CHECK(poison_first_random.state() == 1U);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    ranger.roles[3U].set_word(role_word::poison, 95);
    ranger.roles[4U].set_word(role_word::anti_poison, 80);
    setup.combatants()[0U].words[combatant_word::ai_poison_target] = 4;
    openlegend::random::LegacyRandom no_poison_target_random{1U};
    auto poison_plan = setup.begin_ai_poison_plan(0U, 99U, no_poison_target_random);
    OL_CHECK(poison_plan.has_value());
    OL_CHECK(poison_plan->target_slot == -1);
    OL_CHECK(poison_plan->target_strategy == BattleAiPoisonTargetStrategy::none);
    OL_CHECK(poison_plan->next_step == BattleAiPoisonNextStep::attack_fallback);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::ai_poison_target] == -1);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    openlegend::random::LegacyRandom immediate_poison_random{1U};
    poison_plan = setup.begin_ai_poison_plan(0U, 4U, immediate_poison_random);
    OL_CHECK(poison_plan.has_value());
    OL_CHECK(poison_plan->target_slot == 3);
    OL_CHECK(poison_plan->targeting_range == 6);
    OL_CHECK(poison_plan->target_distance == 6);
    OL_CHECK(poison_plan->range_check_count == 1);
    OL_CHECK(poison_plan->movement_mode == 3);
    OL_CHECK(poison_plan->next_step == BattleAiPoisonNextStep::poison);
    OL_CHECK(poison_plan->outer_marks_action_done_after_handler);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    setup.combatants()[0U].words[combatant_word::round_value] = -1;
    openlegend::random::LegacyRandom negative_round_poison_random{1U};
    poison_plan = setup.begin_ai_poison_plan(0U, 4U, negative_round_poison_random);
    OL_CHECK(poison_plan.has_value());
    OL_CHECK(poison_plan->target_distance == 6);
    OL_CHECK(poison_plan->range_check_count == 2);
    OL_CHECK(poison_plan->next_step == BattleAiPoisonNextStep::poison);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    ranger.roles[0U].set_word(role_word::iq, 61);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    openlegend::random::LegacyRandom zero_round_fallback_random{9U};
    poison_plan = setup.begin_ai_poison_plan(0U, 3U, zero_round_fallback_random);
    OL_CHECK(poison_plan.has_value());
    OL_CHECK(poison_plan->target_slot == 4);
    OL_CHECK(poison_plan->target_distance == 8);
    OL_CHECK(poison_plan->range_check_count == 2);
    OL_CHECK(poison_plan->doubled_target_attack == 100);
    OL_CHECK(poison_plan->doubled_allied_average == 220);
    OL_CHECK(poison_plan->next_step == BattleAiPoisonNextStep::rest);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom poison_move_random{1U};
    poison_plan = setup.begin_ai_poison_plan(0U, 4U, poison_move_random);
    OL_CHECK(poison_plan.has_value());
    OL_CHECK(poison_plan->target_distance == 6);
    OL_CHECK(poison_plan->range_check_count == 1);
    OL_CHECK(poison_plan->next_step == BattleAiPoisonNextStep::move);
    auto resumed_poison_plan = setup.resume_ai_poison_after_move(0U, *poison_plan);
    OL_CHECK(resumed_poison_plan.has_value());
    OL_CHECK(resumed_poison_plan->range_check_count == 2);
    OL_CHECK(resumed_poison_plan->next_step == BattleAiPoisonNextStep::poison);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    ranger.roles[0U].set_word(role_word::iq, 61);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom poison_rest_random{9U};
    poison_plan = setup.begin_ai_poison_plan(0U, 3U, poison_rest_random);
    OL_CHECK(poison_plan.has_value());
    OL_CHECK(poison_plan->target_slot == 4);
    OL_CHECK(poison_plan->target_distance == 8);
    OL_CHECK(poison_plan->next_step == BattleAiPoisonNextStep::move);
    resumed_poison_plan = setup.resume_ai_poison_after_move(0U, *poison_plan);
    OL_CHECK(resumed_poison_plan.has_value());
    OL_CHECK(resumed_poison_plan->allied_total == 330);
    OL_CHECK(resumed_poison_plan->allied_count == 3);
    OL_CHECK(resumed_poison_plan->doubled_target_attack == 100);
    OL_CHECK(resumed_poison_plan->doubled_allied_average == 220);
    OL_CHECK(resumed_poison_plan->next_step == BattleAiPoisonNextStep::rest);

    reset();
    ranger.roles[0U].set_word(role_word::use_poison, 80);
    ranger.roles[0U].set_word(role_word::iq, 61);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 200);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom poison_attack_random{9U};
    poison_plan = setup.begin_ai_poison_plan(0U, 3U, poison_attack_random);
    OL_CHECK(poison_plan.has_value());
    resumed_poison_plan = setup.resume_ai_poison_after_move(0U, *poison_plan);
    OL_CHECK(resumed_poison_plan.has_value());
    OL_CHECK(resumed_poison_plan->doubled_target_attack == 400);
    OL_CHECK(resumed_poison_plan->doubled_allied_average == 220);
    OL_CHECK(resumed_poison_plan->next_step == BattleAiPoisonNextStep::attack_fallback);

    reset();
    std::ranges::fill(data.occupancy(), static_cast<std::int16_t>(-1));
    for (std::size_t slot = 0U; slot < 5U; ++slot) {
        const auto& combatant = setup.combatants()[slot].words;
        const auto index = static_cast<std::size_t>(combatant[combatant_word::y]) * 64U +
            static_cast<std::size_t>(combatant[combatant_word::x]);
        data.occupancy()[index] = static_cast<std::int16_t>(slot);
    }
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    ranger.header.set_inventory(2U, openlegend::model::ItemId{5}, 0);
    const BattleAiChoice item_choice{
        .action = BattleAiAction::item,
        .target_slot = 0,
        .item_source = BattleAiItemSource::inventory,
        .item_slot = 2,
        .action_code_written = true,
    };
    const auto item_plan = setup.begin_ai_item_plan(0U, item_choice);
    OL_CHECK(item_plan.has_value());
    OL_CHECK(item_plan->item_source == BattleAiItemSource::inventory);
    OL_CHECK(item_plan->item_slot == 2);
    OL_CHECK(item_plan->item_id == 5);
    OL_CHECK(item_plan->use_mode == 0);
    OL_CHECK(item_plan->relocation_destination.has_value());
    OL_CHECK((*item_plan->relocation_destination == BattlePathCoord{7, 20}));
    OL_CHECK(item_plan->maximum_enemy_distance_sum == 20);
    OL_CHECK(item_plan->movement_mode == 0);
    OL_CHECK(item_plan->next_step == BattleAiItemNextStep::move);
    OL_CHECK(item_plan->outer_marks_action_done_after_handler);
    const auto resumed_item_plan = setup.resume_ai_item_after_relocation(0U, *item_plan);
    OL_CHECK(resumed_item_plan.has_value());
    OL_CHECK(resumed_item_plan->next_step == BattleAiItemNextStep::use_item);

    reset();
    ranger.header.set_inventory(4U, openlegend::model::ItemId{5}, 0);
    ranger.roles[0U].set_word(role_word::hidden_weapon, 80);
    const BattleAiChoice throwing_choice{
        .action = BattleAiAction::throwing_weapon,
        .item_source = BattleAiItemSource::inventory,
        .item_slot = 4,
        .action_code_written = true,
    };
    openlegend::random::LegacyRandom immediate_throwing_random{1U};
    auto throwing_plan = setup.begin_ai_throwing_weapon_plan(
        0U, throwing_choice, immediate_throwing_random);
    OL_CHECK(throwing_plan.has_value());
    OL_CHECK(throwing_plan->item_id == 5);
    OL_CHECK(throwing_plan->use_mode == 1);
    OL_CHECK(throwing_plan->target_slot == 3);
    OL_CHECK(throwing_plan->target_strategy == BattleAiTargetStrategy::nearest);
    OL_CHECK(throwing_plan->target_written);
    OL_CHECK(throwing_plan->targeting_range == 6);
    OL_CHECK(throwing_plan->target_distance == 6);
    OL_CHECK(throwing_plan->range_check_count == 1);
    OL_CHECK(throwing_plan->movement_mode == 1);
    OL_CHECK(throwing_plan->next_step == BattleAiItemNextStep::use_item);
    OL_CHECK(immediate_throwing_random.state() == 1U);

    reset();
    ranger.header.set_inventory(4U, openlegend::model::ItemId{5}, 0);
    ranger.roles[0U].set_word(role_word::hidden_weapon, 80);
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    setup.combatants()[0U].words[combatant_word::round_value] = 3;
    openlegend::random::LegacyRandom moving_throwing_random{9U};
    throwing_plan = setup.begin_ai_throwing_weapon_plan(
        0U, throwing_choice, moving_throwing_random);
    OL_CHECK(throwing_plan.has_value());
    OL_CHECK(throwing_plan->target_slot == 4);
    OL_CHECK(throwing_plan->target_strategy == BattleAiTargetStrategy::strongest_attack);
    OL_CHECK(throwing_plan->target_distance == 8);
    OL_CHECK(throwing_plan->range_check_count == 1);
    OL_CHECK(throwing_plan->next_step == BattleAiItemNextStep::move);
    auto resumed_throwing_plan = setup.resume_ai_throwing_weapon_after_move(0U, *throwing_plan);
    OL_CHECK(resumed_throwing_plan.has_value());
    OL_CHECK(resumed_throwing_plan->target_slot == 4);
    OL_CHECK(resumed_throwing_plan->target_distance == 8);
    OL_CHECK(resumed_throwing_plan->range_check_count == 2);
    OL_CHECK(resumed_throwing_plan->next_step == BattleAiItemNextStep::attack_fallback);
    OL_CHECK(moving_throwing_random.state() == 1'341'714'958U);

    setup.combatants()[0U].words[combatant_word::x] = 13;
    setup.combatants()[0U].words[combatant_word::y] = 23;
    resumed_throwing_plan = setup.resume_ai_throwing_weapon_after_move(0U, *throwing_plan);
    OL_CHECK(resumed_throwing_plan.has_value());
    OL_CHECK(resumed_throwing_plan->target_distance == 2);
    OL_CHECK(resumed_throwing_plan->range_check_count == 2);
    OL_CHECK(resumed_throwing_plan->next_step == BattleAiItemNextStep::use_item);

    reset();
    ranger.header.set_inventory(4U, openlegend::model::ItemId{5}, 0);
    ranger.roles[0U].set_word(role_word::hidden_weapon, 80);
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[3U].set_word(role_word::attack, 30);
    ranger.roles[4U].set_word(role_word::attack, 50);
    openlegend::random::LegacyRandom zero_round_throwing_random{9U};
    throwing_plan = setup.begin_ai_throwing_weapon_plan(
        0U, throwing_choice, zero_round_throwing_random);
    OL_CHECK(throwing_plan.has_value());
    OL_CHECK(throwing_plan->target_slot == 4);
    OL_CHECK(throwing_plan->target_distance == 8);
    OL_CHECK(throwing_plan->range_check_count == 2);
    OL_CHECK(throwing_plan->next_step == BattleAiItemNextStep::attack_fallback);

    reset();
    ranger.header.set_inventory(4U, openlegend::model::ItemId{5}, 0);
    ranger.roles[0U].set_word(role_word::hidden_weapon, 80);
    ranger.roles[0U].set_word(role_word::morality, 75);
    ranger.roles[3U].set_word(role_word::attack, 0);
    ranger.roles[4U].set_word(role_word::attack, 0);
    setup.combatants()[0U].words[combatant_word::ai_target] = 4;
    openlegend::random::LegacyRandom stale_throwing_random{9U};
    throwing_plan = setup.begin_ai_throwing_weapon_plan(
        0U, throwing_choice, stale_throwing_random);
    OL_CHECK(throwing_plan.has_value());
    OL_CHECK(!throwing_plan->target_written);
    OL_CHECK(throwing_plan->target_slot == 4);
    OL_CHECK(throwing_plan->target_distance == 8);
    OL_CHECK(throwing_plan->range_check_count == 2);
    OL_CHECK(throwing_plan->next_step == BattleAiItemNextStep::attack_fallback);
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
    OL_CHECK(setup.apply(PartySelectionAction::activate) == PartySelectionResult::changed);
    OL_CHECK(setup.selection_states()[0U] == 2);

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

void run_initial_presentation_order_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    auto ranger = std::make_unique<openlegend::model::RangerState>();
    initialize_ranger(*ranger, {0, 2, 3, -1, -1, -1});
    ranger->roles[1U].set_word(openlegend::model::role_word::speed, 1);
    ranger->roles[3U].set_word(openlegend::model::role_word::speed, 30'000);
    for (const auto role_id : {1U, 3U}) {
        for (std::size_t equipment = 0U;
             equipment < openlegend::model::role_word::equipment_count;
             ++equipment) {
            ranger->roles[role_id].set_word(
                openlegend::model::role_word::equipment_begin + equipment, -1);
        }
    }
    openlegend::random::LegacyRandom random{1U};
    BattleRenderState inherited_state{};
    inherited_state.view_x = 17;
    inherited_state.view_y = 19;
    inherited_state.primary_cursor = {7, 8};
    inherited_state.secondary_cursor = {-1, -2};
    inherited_state.path_limit = -5; // Disabled range; no path-map fixture is needed.
    inherited_state.effect_frame_offset = 6;
    inherited_state.effect_id = kBattleEffectPointerBase;
    inherited_state.effect_visible = true;
    inherited_state.highlight_mode = 3;
    auto session = std::make_unique<BattleSession>(
        data_root, *ranger, random, 4, false, inherited_state);
    OL_CHECK(session->valid());
    finish_battle_entry_fade(*session);
    OL_CHECK(session->view_x() == 17);
    OL_CHECK(session->view_y() == 19);
    OL_CHECK(session->setup().combatant_count() == 2);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::role_id] == 1);

    auto framebuffer = std::make_unique<openlegend::render::IndexedFramebuffer>();
    OL_CHECK(session->render(*framebuffer));
    OL_CHECK(std::ranges::all_of(
        framebuffer->palette(),
        [](const auto& color) {
            return color.red == 0U && color.green == 0U && color.blue == 0U;
        }));
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::role_id] == 1);
    session->finish_presented_tick(10U);
    OL_CHECK(session->phase() == BattleSessionPhase::initial_fade);
    OL_CHECK(session->setup().combatants()[0U].words[combatant_word::role_id] == 3);
    const auto& first = session->setup().combatants()[0U].words;
    OL_CHECK(session->view_x() ==
             std::clamp(static_cast<int>(first[combatant_word::x]) - 11, 0, 32));
    OL_CHECK(session->view_y() ==
             std::clamp(static_cast<int>(first[combatant_word::y]) - 11, 0, 32));
    for (std::size_t frame = 0U; frame < session->fade_frame_count(); ++frame) {
        OL_CHECK(session->render(*framebuffer));
        session->finish_presented_tick(10U);
    }
    OL_CHECK(session->phase() == BattleSessionPhase::round_start);
    session->advance(11U);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(session->render_state().path_limit == -5);
    OL_CHECK(session->render_state().effect_frame_offset == 0);
    OL_CHECK(!session->render_state().effect_visible);
    OL_CHECK(session->render_state().highlight_mode == 0);
    OL_CHECK((session->render_state().primary_cursor == BattlePathCoord{7, 8}));
    OL_CHECK((session->render_state().secondary_cursor == BattlePathCoord{
        first[combatant_word::x], first[combatant_word::y]}));
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
    OL_CHECK(setup.sort_by_effective_speed());
    OL_CHECK(setup.prepare_round());
    OL_CHECK(setup.combatants()[0U].words[combatant_word::role_id] == 1);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::round_value] == 2);
    OL_CHECK(setup.combatants()[0U].words[combatant_word::sprite] == 5118);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::role_id] == 3);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::round_value] == 0);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::sprite] == 5132);
    OL_CHECK(data.occupancy()[24U * 64U + 26U] == 0);
    OL_CHECK(data.occupancy()[26U * 64U + 26U] == -1);

    // Computing word6 is separate from sorting and must not change slot order.
    ranger.roles[3U].set_word(openlegend::model::role_word::speed, 100);
    OL_CHECK(setup.prepare_round());
    OL_CHECK(setup.combatants()[0U].words[combatant_word::role_id] == 1);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::round_value] == 1);
    setup.combatants()[1U].words[combatant_word::y] = 24;
    for (const auto word : {7U, 9U, 10U, 11U, 12U, 13U}) {
        setup.combatants()[0U].words[word] = static_cast<std::int16_t>(100U + word);
        setup.combatants()[1U].words[word] = static_cast<std::int16_t>(200U + word);
    }
    setup.combatants()[0U].words[combatant_word::sprite] = -123;
    setup.combatants()[1U].words[combatant_word::sprite] = -456;
    const auto before_first = setup.combatants()[0U].words;
    const auto before_second = setup.combatants()[1U].words;
    OL_CHECK(setup.sort_by_effective_speed());
    for (std::size_t word = 0U; word < kBattleCombatantWords; ++word) {
        if (word != combatant_word::sprite) {
            OL_CHECK(setup.combatants()[0U].words[word] == before_second[word]);
            OL_CHECK(setup.combatants()[1U].words[word] == before_first[word]);
        }
    }
    OL_CHECK(setup.combatants()[0U].words[combatant_word::sprite] == 5132);
    OL_CHECK(setup.combatants()[1U].words[combatant_word::sprite] == 5118);
    OL_CHECK(data.occupancy()[24U * 64U + 26U] == 1);
    ranger.roles[1U].set_word(openlegend::model::role_word::speed, 200);
    OL_CHECK(setup.sort_by_effective_speed());
    OL_CHECK(data.occupancy()[24U * 64U + 26U] == -1);
    ranger.roles[1U].set_word(openlegend::model::role_word::speed, 32760);
    OL_CHECK(setup.sort_by_effective_speed()); // 32760 + 30 wraps negative.
    OL_CHECK(setup.combatants()[0U].words[combatant_word::role_id] == 3);
    OL_CHECK(setup.prepare_round());
    OL_CHECK(setup.combatants()[1U].words[combatant_word::round_value] == 0);

    auto hidden_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    openlegend::random::LegacyRandom hidden_random{1U};
    BattleSession hidden_session{data_root, hidden_ranger, hidden_random, 51, false};
    OL_CHECK(hidden_session.valid());
    OL_CHECK(hidden_session.setup().combatant_count() >= 3);
    for (const auto& combatant : hidden_session.setup().combatants().first(
             static_cast<std::size_t>(hidden_session.setup().combatant_count()))) {
        const auto role_id = static_cast<std::size_t>(
            combatant.words[combatant_word::role_id]);
        auto& role = hidden_ranger.roles[role_id];
        role.set_word(openlegend::model::role_word::hp, 100);
        role.set_word(openlegend::model::role_word::speed, 0);
        for (std::size_t equipment = 0U;
             equipment < openlegend::model::role_word::equipment_count;
             ++equipment) {
            role.set_word(
                openlegend::model::role_word::equipment_begin + equipment, -1);
        }
    }
    const std::array stable_role_ids{
        hidden_session.setup().combatants()[0U].words[combatant_word::role_id],
        hidden_session.setup().combatants()[1U].words[combatant_word::role_id],
        hidden_session.setup().combatants()[2U].words[combatant_word::role_id],
    };
    hidden_ranger.roles[static_cast<std::size_t>(stable_role_ids[0U])].set_word(
        openlegend::model::role_word::speed, 10);
    hidden_ranger.roles[static_cast<std::size_t>(stable_role_ids[1U])].set_word(
        openlegend::model::role_word::speed, 10);
    hidden_ranger.roles[static_cast<std::size_t>(stable_role_ids[2U])].set_word(
        openlegend::model::role_word::speed, 20);
    OL_CHECK(hidden_session.setup().sort_by_effective_speed());
    OL_CHECK(hidden_session.setup().combatants()[0U].words[combatant_word::role_id] ==
             stable_role_ids[2U]);
    // Equal speeds are not swapped directly, but a faster later slot exchanges
    // with the first slot, reversing these equal-speed actors indirectly.
    OL_CHECK(hidden_session.setup().combatants()[1U].words[combatant_word::role_id] ==
             stable_role_ids[1U]);
    OL_CHECK(hidden_session.setup().combatants()[2U].words[combatant_word::role_id] ==
             stable_role_ids[0U]);
    const auto hidden_role_id = static_cast<std::size_t>(
        hidden_session.setup().combatants()[1U].words[combatant_word::role_id]);
    hidden_ranger.roles[hidden_role_id].set_word(
        openlegend::model::role_word::speed, 30'000);
    hidden_session.setup().combatants()[1U]
        .words[combatant_word::occupancy_hidden] = 1;
    const auto reach_round_start = [](BattleSession& session) {
        finish_battle_entry_fade(session);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render(framebuffer));
        session.finish_presented_tick();
        for (std::size_t frame = 0U; frame < session.fade_frame_count(); ++frame) {
            OL_CHECK(session.render(framebuffer));
            session.finish_presented_tick();
        }
        OL_CHECK(session.phase() == BattleSessionPhase::round_start);
    };
    reach_round_start(hidden_session);
    hidden_session.setup().enable_automatic_mode();
    hidden_session.set_confirmation_state(true);
    hidden_session.set_confirmation_state(false); // Released before the actor boundary.
    hidden_session.advance(7U);
    OL_CHECK(hidden_session.setup().automatic_enabled());
    OL_CHECK(!hidden_session.take_clear_confirmation_states_request());
    OL_CHECK(hidden_session.phase() == BattleSessionPhase::actor_present);
    OL_CHECK(hidden_session.setup().combatants()[0U]
                 .words[combatant_word::occupancy_hidden] == 1);
    OL_CHECK(hidden_session.current_actor_slot() == 1U);
    OL_CHECK(hidden_session.setup().combatants()[hidden_session.current_actor_slot()]
                 .words[combatant_word::occupancy_hidden] == 0);

    auto hidden_outcome = std::make_unique<BattleSession>(
        data_root, hidden_ranger, hidden_random, 51, false);
    OL_CHECK(hidden_outcome->valid());
    const auto outcome_role = static_cast<std::size_t>(
        hidden_outcome->setup().combatants()[0U].words[combatant_word::role_id]);
    for (auto& combatant : hidden_outcome->setup().combatants().first(
             static_cast<std::size_t>(hidden_outcome->setup().combatant_count()))) {
        combatant.words[combatant_word::occupancy_hidden] = 1;
        auto& role = hidden_ranger.roles[static_cast<std::size_t>(
            combatant.words[combatant_word::role_id])];
        role.set_word(openlegend::model::role_word::hp, 100);
        role.set_word(openlegend::model::role_word::hurt, 20);
        role.set_word(openlegend::model::role_word::poison, 0);
        role.set_word(openlegend::model::role_word::level, 30);
        role.set_word(openlegend::model::role_word::practice_item, -1);
        combatant.words[combatant_word::ai_target] = 0;
    }
    reach_round_start(*hidden_outcome);
    hidden_outcome->setup().enable_automatic_mode();
    hidden_outcome->set_confirmation_state(true);
    hidden_outcome->advance(0x1800AFU);
    OL_CHECK(!hidden_outcome->setup().automatic_enabled());
    OL_CHECK(hidden_outcome->take_clear_confirmation_states_request());
    OL_CHECK(hidden_outcome->phase() == BattleSessionPhase::battle_outcome);
    OL_CHECK(hidden_outcome->outcome() == BattleOutcome::victory);
    OL_CHECK(hidden_ranger.roles[outcome_role].word(openlegend::model::role_word::hp) == 100);
    OL_CHECK(hidden_outcome->setup().combatants()[0U].words[combatant_word::ai_target] == 0);
    openlegend::render::IndexedFramebuffer outcome_frame;
    OL_CHECK(hidden_outcome->render(outcome_frame));
    hidden_outcome->finish_presented_tick(0x1800AFU);
    OL_CHECK(hidden_outcome->handle_key(0x20U) ==
             BattleSessionInputResult::outcome_acknowledged);
    for (std::size_t guard = 0U;
         hidden_outcome->phase() == BattleSessionPhase::post_battle_message_present &&
             guard < 128U;
         ++guard) {
        OL_CHECK(hidden_ranger.roles[outcome_role].word(openlegend::model::role_word::hp) == 100);
        OL_CHECK(hidden_outcome->render(outcome_frame));
        hidden_outcome->finish_presented_tick(0x1800AFU);
        OL_CHECK(hidden_outcome->handle_key(0x20U) ==
                 BattleSessionInputResult::post_battle_message_acknowledged);
    }
    OL_CHECK(hidden_outcome->phase() == BattleSessionPhase::round_wait);
    OL_CHECK(hidden_outcome->setup().combatants()[0U].words[combatant_word::ai_target] == -1);
    OL_CHECK(hidden_ranger.roles[outcome_role].word(openlegend::model::role_word::hp) == 99);
    hidden_outcome->advance(0x1800AFU);
    OL_CHECK(hidden_outcome->phase() == BattleSessionPhase::round_wait);
    OL_CHECK(hidden_ranger.roles[outcome_role].word(openlegend::model::role_word::hp) == 99);
    hidden_outcome->advance(0U); // BIOS day rollover is still a tick change.
    OL_CHECK(hidden_outcome->phase() == BattleSessionPhase::complete);
    OL_CHECK(hidden_outcome->result() == BattleStepResult::victory);
    OL_CHECK(hidden_ranger.roles[outcome_role].word(openlegend::model::role_word::hp) == 99);
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

void run_battle_outcome_session_test(
    const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend::battle;
    using namespace openlegend::model;

    openlegend::render::IndexedFramebuffer framebuffer;
    const auto prepare_player_action = [&](BattleSession& session) {
        OL_CHECK(session.valid());
        finish_battle_entry_fade(session);
        OL_CHECK(session.render(framebuffer));
        session.finish_presented_tick();
        for (std::size_t frame = 0U; frame < session.fade_frame_count(); ++frame) {
            OL_CHECK(session.render(framebuffer));
            session.finish_presented_tick();
        }
        OL_CHECK(session.phase() == BattleSessionPhase::round_start);
        session.advance();
        OL_CHECK(session.phase() == BattleSessionPhase::actor_present);
        OL_CHECK(session.render(framebuffer));
        session.finish_presented_tick();
        finish_player_menu_redraw(session);
        OL_CHECK(session.phase() == BattleSessionPhase::player_action);
    };
    const auto select_wait = [](BattleSession& session) {
        const auto visible_count = [&session]() {
            return std::ranges::count_if(
                session.setup().combatants().first(
                    static_cast<std::size_t>(session.setup().combatant_count())),
                [](const BattleCombatant& combatant) {
                    return combatant.words[combatant_word::occupancy_hidden] == 0;
                });
        };
        const auto visible_before_wait = visible_count();
        OL_CHECK(visible_before_wait > 0);
        std::size_t wait_ordinal = 0U;
        for (std::size_t action = 0U;
             action < static_cast<std::size_t>(BattlePlayerAction::wait);
             ++action) {
            if (session.player_action_menu().available[action] == 1) {
                ++wait_ordinal;
            }
        }
        OL_CHECK(session.player_action_menu().available[
                     static_cast<std::size_t>(BattlePlayerAction::wait)] == 1);
        while (session.player_action_menu().cursor != wait_ordinal) {
            OL_CHECK(session.handle_key(0x98U) ==
                     BattleSessionInputResult::action_changed);
        }
        OL_CHECK(session.handle_key(0x0DU) ==
                 BattleSessionInputResult::action_selected);
        OL_CHECK(session.phase() == BattleSessionPhase::player_action_return_present);
        OL_CHECK(visible_count() == visible_before_wait);
        finish_player_menu_redraw(session);
    };
    const auto set_names = [](openlegend::model::RangerState& ranger) {
        for (std::size_t role_id = 0U; role_id < ranger.roles.size(); ++role_id) {
            auto name = std::span<std::uint8_t>{ranger.roles[role_id].bytes}.subspan(
                role_word::name_byte, role_word::name_bytes);
            std::ranges::fill(name, std::uint8_t{0U});
            name[0U] = static_cast<std::uint8_t>('A' + role_id % 26U);
        }
    };

    auto victory_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    set_names(victory_ranger);
    openlegend::random::LegacyRandom victory_random{1U};
    BattleSession victory{data_root, victory_ranger, victory_random, 4, false};
    prepare_player_action(victory);
    auto& practice_item = victory_ranger.items[5U];
    practice_item.set_word(item_word::magic_id, 2);
    practice_item.set_word(item_word::need_experience, 10);
    practice_item.set_word(item_word::need_make_item_experience, 10);
    practice_item.set_word(item_word::need_material, 10);
    practice_item.set_word(item_word::make_item_begin, 20);
    practice_item.set_word(item_word::make_item_count_begin, 2);
    for (std::size_t recipe = 1U; recipe < item_word::make_item_count; ++recipe) {
        practice_item.set_word(item_word::make_item_begin + recipe, -1);
    }
    auto practice_name = std::span<std::uint8_t>{practice_item.bytes}.subspan(
        2U * item_word::secondary_name_begin,
        2U * item_word::secondary_name_count);
    std::ranges::fill(practice_name, std::uint8_t{0U});
    std::ranges::copy(std::array<std::uint8_t, 4>{'B', 'O', 'O', 'K'},
                      practice_name.begin());
    auto product_name = std::span<std::uint8_t>{victory_ranger.items[20U].bytes}.subspan(
        2U * item_word::secondary_name_begin,
        2U * item_word::secondary_name_count);
    std::ranges::fill(product_name, std::uint8_t{0U});
    std::ranges::copy(std::array<std::uint8_t, 4>{'P', 'I', 'L', 'L'},
                      product_name.begin());
    auto magic_name = std::span<std::uint8_t>{victory_ranger.magics[2U].bytes}.subspan(
        magic_word::name_byte, magic_word::name_bytes);
    std::ranges::fill(magic_name, std::uint8_t{0U});
    std::ranges::copy(std::array<std::uint8_t, 5>{'M', 'A', 'G', 'I', 'C'},
                      magic_name.begin());
    victory_ranger.header.set_inventory(0U, ItemId{10}, 3);
    victory_ranger.header.set_inventory(1U, ItemId{20}, 4);
    victory_ranger.header.set_inventory(2U, ItemId{-1}, 0);
    for (const auto& combatant : victory.setup().combatants().first(
             static_cast<std::size_t>(victory.setup().combatant_count()))) {
        const auto role_id = combatant.words[combatant_word::role_id];
        auto& role = victory_ranger.roles[static_cast<std::size_t>(role_id)];
        if (combatant.words[combatant_word::side] == 0) {
            role.set_word(role_word::level, 1);
            role.set_word(role_word::experience, 150);
            role.set_word(role_word::increased_life, 2);
            role.set_word(role_word::iq, 60);
            role.set_word(role_word::practice_item, 5);
            role.set_word(role_word::item_experience, 60);
            role.set_word(role_word::make_item_experience, 30);
            role.set_word(role_word::magic_id_begin, 2);
            role.set_word(role_word::magic_level_begin, 199);
        } else {
            role.set_word(role_word::hp, 0);
        }
    }
    select_wait(victory);
    OL_CHECK(victory.phase() == BattleSessionPhase::battle_outcome);
    OL_CHECK(victory.outcome() == BattleOutcome::victory);
    OL_CHECK(victory.render(framebuffer));
    const auto victory_outcome_hash = fnv1a_bytes(framebuffer.pixels());
    victory.finish_presented_tick();
    OL_CHECK(victory.phase() == BattleSessionPhase::battle_outcome_wait);
    OL_CHECK(victory.handle_key(0x20U) ==
             BattleSessionInputResult::outcome_acknowledged);
    OL_CHECK(victory.post_battle_result().has_value());
    OL_CHECK(victory.post_battle_message_count() == 1U);
    const auto role_result = std::ranges::find_if(
        victory.post_battle_result()->roles,
        [](const BattlePostBattleRoleResult& role) {
            return role.experience_message_required;
        });
    OL_CHECK(role_result != victory.post_battle_result()->roles.end());
    const auto progress_role_id = static_cast<std::size_t>(role_result->role_id);
    OL_CHECK(!role_result->level_up.message_required);
    OL_CHECK(!role_result->practice.practice_message_required);
    OL_CHECK(!role_result->practice.magic_message_required);
    OL_CHECK(!role_result->craft.message_required);
    OL_CHECK(victory_ranger.roles[progress_role_id].word(role_word::level) == 1);
    OL_CHECK(victory_random.state() == 1U);
    OL_CHECK(victory_ranger.header.inventory_count(1U) == 4);
    OL_CHECK(victory.phase() == BattleSessionPhase::post_battle_message_present);
    std::vector<std::uint64_t> post_battle_hashes;
    for (std::size_t message = 0U; message < 5U; ++message) {
        OL_CHECK(victory.post_battle_message_count() == message + 1U);
        OL_CHECK(victory.phase() == BattleSessionPhase::post_battle_message_present);
        OL_CHECK(victory.post_battle_message_index() == message);
        if (message <= 1U) {
            OL_CHECK(victory_ranger.roles[progress_role_id].word(role_word::level) == 1);
            OL_CHECK(victory_random.state() == 1U);
        }
        if (message == 2U) {
            OL_CHECK(victory_ranger.roles[progress_role_id].word(
                         role_word::magic_level_begin) == 199);
        }
        if (message >= 2U) {
            OL_CHECK(victory_ranger.header.inventory_count(1U) == 4);
        }
        OL_CHECK(victory.render(framebuffer));
        post_battle_hashes.push_back(fnv1a_bytes(framebuffer.pixels()));
        victory.finish_presented_tick();
        OL_CHECK(victory.phase() == BattleSessionPhase::post_battle_message_wait);
        OL_CHECK(victory.handle_key(0x0DU) ==
                 BattleSessionInputResult::post_battle_message_acknowledged);
        if (message == 2U) {
            OL_CHECK(victory_ranger.roles[progress_role_id].word(
                         role_word::magic_level_begin) == 299);
        }
        if (message == 3U) {
            OL_CHECK(victory_ranger.header.inventory_count(1U) == 4);
        }
    }
    OL_CHECK(victory.post_battle_message_count() == 5U);
    OL_CHECK(role_result->level_up.message_required);
    OL_CHECK(role_result->practice.practice_message_required);
    OL_CHECK(role_result->practice.magic_message_required);
    OL_CHECK(role_result->craft.message_required);
    OL_CHECK(victory.phase() == BattleSessionPhase::round_wait);
    OL_CHECK(!victory.finished());
    victory.advance();
    OL_CHECK(victory.phase() == BattleSessionPhase::round_wait);
    OL_CHECK(victory.render(framebuffer));
    OL_CHECK(fnv1a_bytes(framebuffer.pixels()) == post_battle_hashes.back());
    victory.advance(1U);
    OL_CHECK(victory.phase() == BattleSessionPhase::complete);
    OL_CHECK(victory.finished());
    OL_CHECK(victory.result() == BattleStepResult::victory);
    for (const auto& combatant : victory.setup().combatants().first(
             static_cast<std::size_t>(victory.setup().combatant_count()))) {
        if (combatant.words[combatant_word::side] != 1) {
            continue;
        }
        const auto& role = victory_ranger.roles[static_cast<std::size_t>(
            combatant.words[combatant_word::role_id])];
        OL_CHECK(role.word(role_word::hp) == role.word(role_word::maximum_hp));
        OL_CHECK(role.word(role_word::mp) == role.word(role_word::maximum_mp));
        OL_CHECK(role.word(role_word::physical_power) == 100);
        OL_CHECK(role.word(role_word::hurt) == 0);
        OL_CHECK(role.word(role_word::poison) == 0);
    }

    auto defeat_ranger = make_ranger({0, 2, 3, -1, -1, -1});
    set_names(defeat_ranger);
    openlegend::random::LegacyRandom defeat_random{1U};
    BattleSession defeat{data_root, defeat_ranger, defeat_random, 4, false};
    prepare_player_action(defeat);
    for (const auto& combatant : defeat.setup().combatants().first(
             static_cast<std::size_t>(defeat.setup().combatant_count()))) {
        const auto role_id = combatant.words[combatant_word::role_id];
        auto& role = defeat_ranger.roles[static_cast<std::size_t>(role_id)];
        if (combatant.words[combatant_word::side] == 0) {
            role.set_word(role_word::hp, 0);
        } else {
            role.set_word(role_word::maximum_hp, 100);
            role.set_word(role_word::hp, 100);
        }
    }
    select_wait(defeat);
    OL_CHECK(defeat.phase() == BattleSessionPhase::battle_outcome);
    OL_CHECK(defeat.outcome() == BattleOutcome::defeat);
    OL_CHECK(defeat.render(framebuffer));
    const auto defeat_outcome_hash = fnv1a_bytes(framebuffer.pixels());
    defeat.finish_presented_tick();
    OL_CHECK(defeat.phase() == BattleSessionPhase::battle_outcome_wait);
    OL_CHECK(defeat.handle_key(0x20U) ==
             BattleSessionInputResult::outcome_acknowledged);
    OL_CHECK(defeat.post_battle_result().has_value());
    OL_CHECK(defeat.post_battle_message_count() == 0U);
    OL_CHECK(defeat.phase() == BattleSessionPhase::round_wait);
    OL_CHECK(!defeat.finished());
    defeat.advance();
    OL_CHECK(defeat.phase() == BattleSessionPhase::round_wait);
    defeat.advance(1U);
    OL_CHECK(defeat.phase() == BattleSessionPhase::complete);
    OL_CHECK(defeat.finished());
    OL_CHECK(defeat.result() == BattleStepResult::defeat);

    OL_CHECK(victory_outcome_hash == 0x52bc7b717bebdba9ULL);
    OL_CHECK(defeat_outcome_hash == 0xf3c79c485413dfe5ULL);
    OL_CHECK((post_battle_hashes == std::vector<std::uint64_t>{
        0xa699bf683f037936ULL,
        0xef2c8987fe26a127ULL,
        0xdd4c7e74171e8ee5ULL,
        0x0f4783440328986eULL,
        0xb980de17004d5b6cULL,
    }));
    const auto hash_path = openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
        "b8-battle-outcome.hash";
    std::ofstream hash_file{hash_path, std::ios::binary | std::ios::trunc};
    OL_CHECK(hash_file.good());
    hash_file << "victory=0x" << std::hex << victory_outcome_hash << '\n';
    hash_file << "defeat=0x" << std::hex << defeat_outcome_hash << '\n';
    hash_file << "experience=0x" << std::hex << post_battle_hashes[0U] << '\n';
    hash_file << "level=0x" << std::hex << post_battle_hashes[1U] << '\n';
    hash_file << "practice=0x" << std::hex << post_battle_hashes[2U] << '\n';
    hash_file << "magic=0x" << std::hex << post_battle_hashes[3U] << '\n';
    hash_file << "craft=0x" << std::hex << post_battle_hashes[4U] << '\n';
    hash_file.close();
    OL_CHECK(hash_file.good());
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
        OL_CHECK(data.music_id() >= 0 && data.music_id() < 24);
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
    run_shared_menu_item_helper_test();
    run_ai_item_effect_test(data_root);
    run_ai_request_handler_test(data_root);
    run_ai_support_handler_test(data_root);
    run_post_battle_progression_test(data_root);
    run_player_movement_selection_test(data_root);
    run_ai_movement_continuation_test(data_root);
    run_rest_action_test(data_root);
    run_wait_auto_render_test(data_root);
    run_player_action_availability_test(data_root);
    run_player_support_session_test(data_root);
    run_player_item_session_test(data_root);
    run_player_status_session_test(data_root);
    run_player_attack_session_test(data_root);
    run_ai_attack_session_test(data_root);
    run_ai_poison_session_test(data_root);
    run_ai_item_session_test(data_root);
    run_ai_support_session_test(data_root);
    run_ai_support_movement_session_test(data_root);
    run_ai_request_session_test(data_root);
    run_battle_session_test(data_root);
    run_ai_selector_test(data_root);
    run_damage_formula_test(data_root);
    run_attack_area_test(data_root);
    run_party_selection_test(data_root);
    run_fixed_and_duplicate_tests(data_root);
    run_initial_presentation_order_test(data_root);
    run_turn_order_test(data_root);
    run_outcome_test(data_root);
    run_battle_outcome_session_test(data_root);
    run_all_definition_tests(data_root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
