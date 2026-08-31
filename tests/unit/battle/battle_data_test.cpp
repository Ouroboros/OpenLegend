#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>

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
    run_party_selection_test(data_root);
    run_fixed_and_duplicate_tests(data_root);
    run_turn_order_test(data_root);
    run_outcome_test(data_root);
    run_all_definition_tests(data_root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
