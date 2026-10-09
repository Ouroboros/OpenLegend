#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <vector>

#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/model/runtime_snapshot.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "test_support.hpp"

namespace {

void check_baseline_and_damage(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::RangerState& baseline) {
    using namespace openlegend;
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    const auto original_enemy = model::decode_legacy_role(baseline.roles[3U], model::kItemCount);
    OL_CHECK(original_enemy.has_value());
    if (!original_enemy) {
        return;
    }
    for (const std::int64_t playthrough : {1, 2, 999}) {
        auto decoded = model::decode_legacy_ranger(baseline);
        OL_CHECK(decoded.has_value());
        if (!decoded) {
            return;
        }
        auto& ranger = *decoded;
        auto& inherited = ranger.roles[3U];
        inherited.name = u8"繼承";
        inherited.ever_joined = true;
        inherited.hp = inherited.maximum_hp = 5'000'000'000'000;
        inherited.mp = inherited.maximum_mp = 4'000'000'000'000;
        inherited.attack = 3'000'000'000'000;
        inherited.defence = 2'000'000'000'000;
        inherited.level = 500;
        inherited.magic_ids.fill(model::MagicId{0});
        inherited.magic_levels.fill(777'777);
        const auto saved_inherited = inherited;
        auto& actor = ranger.roles[1U];
        actor.attack = 1'000'000;
        actor.attack_with_poison = 0;
        actor.magic_ids[0U] = model::MagicId{1};
        actor.magic_levels[0U] = 0;
        auto& magic = ranger.magics[1U];
        magic.set_word(model::magic_word::need_mp, 0);
        magic.set_word(model::magic_word::with_poison, 0);
        magic.set_word(model::magic_word::attack_begin, 100);
        OL_CHECK(ranger.valid());
        battle::BattleData data{data_root, 4};
        battle::BattleSetup setup{data, ranger, nullptr, configuration, playthrough, &baseline};
        OL_CHECK(setup.valid());
        if (!setup.valid()) {
            return;
        }
        OL_CHECK(setup.combatant_count() == 2);
        OL_CHECK(setup.combatant_role(0U) == &ranger.roles[1U]);
        auto* enemy = setup.combatant_role(1U);
        OL_CHECK(enemy != nullptr && enemy != &ranger.roles[3U]);
        if (enemy == nullptr) {
            return;
        }
        OL_CHECK(enemy->id.value == 3);
        OL_CHECK(enemy->name == original_enemy->name);
        OL_CHECK(!enemy->ever_joined);
        OL_CHECK(enemy->hp == original_enemy->hp);
        OL_CHECK(enemy->mp == original_enemy->mp);
        OL_CHECK(enemy->maximum_hp == original_enemy->maximum_hp + (playthrough - 1) * 999);
        OL_CHECK(enemy->maximum_mp == original_enemy->maximum_mp + (playthrough - 1) * 999);
        OL_CHECK(enemy->attack == original_enemy->attack + (playthrough - 1) * 100);
        OL_CHECK(enemy->defence == original_enemy->defence + (playthrough - 1) * 100);
        OL_CHECK(enemy->level == original_enemy->level + (playthrough - 1) * 30);
        OL_CHECK(enemy->magic_ids == original_enemy->magic_ids);
        OL_CHECK(enemy->speed == original_enemy->speed);
        OL_CHECK(enemy->equipment == original_enemy->equipment);
        OL_CHECK(enemy->taking_counts == original_enemy->taking_counts);
        OL_CHECK(setup.combatant_role(2U) == nullptr);
        OL_CHECK(setup.prepare_round());
        const auto panel = setup.status_panel_plan(1U);
        OL_CHECK(panel.has_value());
        if (panel) {
            OL_CHECK(panel->hp == enemy->hp);
            OL_CHECK(panel->maximum_hp == enemy->maximum_hp);
        }
        random::LegacyRandom random{1U};
        const auto damage = setup.apply_hp_damage(0U, 1U, 0, 1, 0, random);
        OL_CHECK(damage.has_value());
        if (!damage) {
            return;
        }
        OL_CHECK(damage->damage > original_enemy->hp);
        OL_CHECK(enemy->hp == 0);
        OL_CHECK(damage->poison_overflow_damage == 0);
        OL_CHECK(setup.combatants()[0U].reward_experience == damage->damage / 5 + 10 * enemy->level);
        OL_CHECK(setup.evaluate_outcome() == battle::BattleOutcome::victory);
        OL_CHECK(ranger.roles[3U] == saved_inherited);
        battle::BattleData next_data{data_root, 4};
        battle::BattleSetup next_setup{next_data, ranger, nullptr, configuration, playthrough, &baseline};
        OL_CHECK(next_setup.valid());
        const auto* next_enemy = next_setup.combatant_role(1U);
        OL_CHECK(next_enemy != nullptr);
        if (next_enemy) {
            OL_CHECK(next_enemy->hp == original_enemy->hp);
            OL_CHECK(next_enemy->attack == original_enemy->attack + (playthrough - 1) * 100);
        }
    }
}

void check_identity_and_aliases(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::RangerState& baseline) {
    using namespace openlegend;
    auto decoded = model::decode_legacy_ranger(baseline);
    OL_CHECK(decoded.has_value());
    if (!decoded) {
        return;
    }
    auto& ranger = *decoded;
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    battle::BattleData data{data_root, 4};
    battle::BattleSetup setup{data, ranger, nullptr, configuration, 2, &baseline};
    OL_CHECK(setup.valid());
    if (!setup.valid()) {
        return;
    }
    setup.combatants()[0U].words[battle::combatant_word::role_id] = 3;
    auto* friendly = setup.combatant_role(0U);
    auto* enemy = setup.combatant_role(1U);
    OL_CHECK(friendly == &ranger.roles[3U] && enemy != nullptr && enemy != friendly);
    if (friendly == nullptr || enemy == nullptr) {
        return;
    }
    friendly->magic_ids[0U] = model::MagicId{1};
    friendly->magic_levels[0U] = 0;
    friendly->mp = 50;
    friendly->maximum_mp = 1000;
    enemy->mp = 80;
    enemy->maximum_mp = 1000;
    auto& magic = ranger.magics[1U];
    magic.set_word(model::magic_word::hurt_mp_begin, 20);
    magic.set_word(model::magic_word::add_mp_begin, 10);
    random::LegacyRandom random{1U};
    const auto damage = setup.apply_mp_damage(0U, 1U, 0, random);
    OL_CHECK(damage.has_value());
    if (damage) {
        OL_CHECK(*damage >= 18 && *damage <= 22);
        OL_CHECK(enemy->mp == 80 - *damage);
        OL_CHECK(friendly->mp >= 58 && friendly->mp <= 62);
    }
    const auto saved_roles = ranger.roles;
    setup.combatants()[0U].words[battle::combatant_word::side] = 1;
    OL_CHECK(setup.combatant_role(0U) == setup.combatant_role(1U));
    enemy->magic_ids[0U] = model::MagicId{1};
    enemy->magic_levels[0U] = 0;
    enemy->mp = 80;
    const auto alias_damage = setup.apply_mp_damage(0U, 1U, 0, random);
    OL_CHECK(alias_damage.has_value());
    if (alias_damage) {
        OL_CHECK(*alias_damage >= 6 && *alias_damage <= 14);
        OL_CHECK(enemy->mp == 80 - *alias_damage);
    }
    OL_CHECK(ranger.roles == saved_roles);
    enemy->hp = enemy->maximum_hp = 1000;
    enemy->hurt = 20;
    enemy->poison = 10;
    enemy->physical_power = 60;
    OL_CHECK(setup.apply_round_status_damage().has_value());
    OL_CHECK(enemy->hp == 978);
    OL_CHECK(ranger.roles == saved_roles);
}

void check_rejection(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::RangerState& baseline) {
    using namespace openlegend;
    auto decoded = model::decode_legacy_ranger(baseline);
    OL_CHECK(decoded.has_value());
    if (!decoded) {
        return;
    }
    auto& ranger = *decoded;
    const auto original = ranger;
    for (const bool overflow : {false, true}) {
        model::NewGamePlusConfiguration configuration;
        configuration.enabled = true;
        if (overflow) {
            configuration.attack_cap_step = std::numeric_limits<std::int64_t>::max();
        } else {
            configuration.hp_cap_step = -baseline.roles[3U].word(model::role_word::maximum_hp);
        }
        battle::BattleData data{data_root, 4};
        const std::vector occupancy(data.occupancy().begin(), data.occupancy().end());
        battle::BattleSetup setup{data, ranger, nullptr, configuration, 2, &baseline};
        OL_CHECK(!setup.valid());
        OL_CHECK(setup.combatant_count() == 0);
        OL_CHECK(ranger == original);
        OL_CHECK(std::ranges::equal(data.occupancy(), occupancy));
        OL_CHECK(setup.error() == (overflow
            ? "battle enemy attributes are invalid or overflow"
            : "battle enemy current HP or MP exceeds the reduced maximum"));
    }
}

void check_all_battles(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::RangerState& baseline) {
    using namespace openlegend;
    auto decoded = model::decode_legacy_ranger(baseline);
    OL_CHECK(decoded.has_value());
    if (!decoded) {
        return;
    }
    auto& ranger = *decoded;
    const auto original = ranger;
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    std::array<bool, model::kRoleCount> seen{};
    for (const std::int64_t playthrough : {1, 2, 999}) {
        for (std::int16_t battle_id = 0; battle_id < 140; ++battle_id) {
            battle::BattleData data{data_root, battle_id};
            battle::BattleSetup setup{data, ranger, nullptr, configuration, playthrough, &baseline};
            OL_CHECK(setup.valid());
            if (!setup.valid()) {
                return;
            }
            if (setup.waiting_for_party_selection()) {
                OL_CHECK(setup.apply(battle::PartySelectionAction::activate) ==
                    battle::PartySelectionResult::changed);
                for (std::size_t index = 0U; index < setup.party_prefix_length(); ++index) {
                    OL_CHECK(setup.apply(battle::PartySelectionAction::next) ==
                        battle::PartySelectionResult::changed);
                }
                OL_CHECK(setup.apply(battle::PartySelectionAction::activate) ==
                    battle::PartySelectionResult::complete);
            }
            for (std::size_t slot = 0U; slot < static_cast<std::size_t>(setup.combatant_count()); ++slot) {
                const auto* role = setup.combatant_role(slot);
                OL_CHECK(role != nullptr);
                if (role == nullptr) {
                    return;
                }
                const auto role_id = static_cast<std::size_t>(role->id.value);
                if (setup.combatants()[slot].words[battle::combatant_word::side] != 1) {
                    OL_CHECK(role == &ranger.roles[role_id]);
                    continue;
                }
                seen[role_id] = true;
                OL_CHECK(role != &ranger.roles[role_id]);
                const auto& source = original.roles[role_id];
                OL_CHECK(role->hp == source.hp && role->mp == source.mp);
                OL_CHECK(role->maximum_hp == source.maximum_hp + (playthrough - 1) * 999);
                OL_CHECK(role->maximum_mp == source.maximum_mp + (playthrough - 1) * 999);
                OL_CHECK(role->speed == source.speed && role->frames == source.frames);
                OL_CHECK(role->taking_items == source.taking_items);
                OL_CHECK(role->taking_counts == source.taking_counts);
                OL_CHECK(role->magic_ids == source.magic_ids);
                for (std::size_t magic_slot = 0U; magic_slot < model::role_word::magic_count; ++magic_slot) {
                    const auto increment = source.magic_ids[magic_slot].value > 0
                        ? 1000 * (playthrough - 1) : 0;
                    OL_CHECK(role->magic_levels[magic_slot] == source.magic_levels[magic_slot] + increment);
                }
                if (playthrough == 1 && (role_id == 83U || role_id == 90U)) {
                    OL_CHECK(role->mp == 95 && role->maximum_mp == 90);
                }
            }
        }
    }
    OL_CHECK(std::ranges::count(seen, true) == 273);
    OL_CHECK(seen[83U] && seen[90U]);
    OL_CHECK(ranger == original);
}

void check_enemy_actions_and_settlement(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::RangerState& baseline) {
    using namespace openlegend;
    auto decoded = model::decode_legacy_ranger(baseline);
    OL_CHECK(decoded.has_value());
    if (!decoded) {
        return;
    }
    auto& ranger = *decoded;
    ranger.roles[3U].medicine = 1000;
    ranger.roles[3U].taking_items.fill(model::ItemId{-1});
    const auto inherited = ranger.roles[3U];
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    battle::BattleData data{data_root, 4};
    battle::BattleSetup setup{data, ranger, nullptr, configuration, 2, &baseline};
    OL_CHECK(setup.valid());
    if (!setup.valid()) {
        return;
    }
    auto* enemy = setup.combatant_role(1U);
    OL_CHECK(enemy != nullptr);
    if (enemy == nullptr) {
        return;
    }
    enemy->hp = 100;
    enemy->maximum_hp = 999;
    enemy->medicine = 0;
    enemy->hurt = enemy->poison = 0;
    enemy->physical_power = 100;
    enemy->taking_items.fill(model::ItemId{-1});
    enemy->taking_counts.fill(0);
    enemy->taking_items[0U] = model::ItemId{0};
    enemy->taking_counts[0U] = 2;
    auto& medicine = ranger.items[0U];
    medicine.bytes.fill(0);
    medicine.set_word(model::item_word::item_type, 3);
    medicine.set_word(model::item_word::add_hp, 100);
    const auto choice = setup.choose_ai_low_hp_action(1U);
    OL_CHECK(choice.has_value());
    if (!choice) {
        return;
    }
    OL_CHECK(choice->action == battle::BattleAiAction::item);
    OL_CHECK(choice->item_source == battle::BattleAiItemSource::carried);
    OL_CHECK(choice->item_slot == 0 && choice->target_slot == 1);
    random::LegacyRandom random{1U};
    const auto effect = setup.apply_ai_item_effect(1U, *choice, random);
    OL_CHECK(effect.has_value());
    if (effect) {
        OL_CHECK(effect->deltas[0U] >= 100 && effect->deltas[0U] <= 109);
        OL_CHECK(enemy->hp == 100 + effect->deltas[0U]);
        OL_CHECK(enemy->taking_counts[0U] == 1);
    }
    enemy->taking_counts[0U] = std::numeric_limits<std::int64_t>::min();
    const auto saved_enemy = *enemy;
    const auto saved_random = random.state();
    OL_CHECK(!setup.apply_ai_item_effect(1U, *choice, random).has_value());
    OL_CHECK(*enemy == saved_enemy);
    OL_CHECK(random.state() == saved_random);
    OL_CHECK(ranger.roles[3U] == inherited);

    battle::BattleData treatment_data{data_root, 4};
    battle::BattleSetup treatment{treatment_data, ranger, nullptr, configuration, 2, &baseline};
    OL_CHECK(treatment.valid());
    if (!treatment.valid()) {
        return;
    }
    enemy = treatment.combatant_role(1U);
    OL_CHECK(enemy != nullptr);
    if (enemy == nullptr) {
        return;
    }
    enemy->hp = 100;
    enemy->maximum_hp = 999;
    enemy->hurt = 40;
    enemy->poison = 50;
    enemy->medicine = 100;
    enemy->detoxification = 90;
    enemy->physical_power = 100;
    const auto healing = treatment.apply_medicine_value(1U, 1U, random);
    OL_CHECK(healing.has_value());
    if (healing) {
        OL_CHECK(*healing > 0 && enemy->hp == 100 + *healing);
        OL_CHECK(enemy->hurt == 0 && enemy->physical_power == 98);
    }
    const auto detox = treatment.apply_detox_value(1U, 1U, random);
    OL_CHECK(detox.has_value());
    if (detox) {
        OL_CHECK(*detox > 0 && enemy->poison == 50 - *detox);
    }
    enemy->physical_power = 100;
    enemy->hp = 5'000'000'000'000;
    enemy->maximum_hp = enemy->hp + 20;
    enemy->mp = 4'000'000'000'000;
    enemy->maximum_mp = enemy->mp + 20;
    random.seed(1U);
    const auto rest = treatment.rest_actor(1U, random);
    OL_CHECK(rest.has_value());
    OL_CHECK(enemy->hp == 5'000'000'000'009);
    OL_CHECK(enemy->mp == 4'000'000'000'004);
    OL_CHECK(enemy->physical_power == 100);
    OL_CHECK(random.state() == 662'824'084U);
    OL_CHECK(ranger.roles[3U] == inherited);

    enemy->experience = 50;
    treatment.combatants()[1U].reward_experience = 77;
    OL_CHECK(treatment.prepare_battle_settlement(battle::BattleOutcome::victory).has_value());
    const auto experience = treatment.apply_post_battle_experience(
        1U, battle::BattleOutcome::victory, true);
    OL_CHECK(experience.has_value());
    enemy = treatment.combatant_role(1U);
    OL_CHECK(enemy != nullptr);
    if (enemy && experience) {
        OL_CHECK(experience->experience_gained == 115);
        OL_CHECK(!experience->experience_message_required);
        OL_CHECK(enemy->experience == 165);
        OL_CHECK(enemy->hp == enemy->maximum_hp && enemy->mp == enemy->maximum_mp);
        OL_CHECK(enemy->hurt == 0 && enemy->poison == 0 && enemy->physical_power == 100);
    }
    OL_CHECK(ranger.roles[3U] == inherited);
}

}

int main() {
    using namespace openlegend;
    const resource::DataRoot data_root{test::game_data_root()};
    const auto loaded = persistence::load_baseline_ranger(data_root.path());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return 1;
    }
    check_baseline_and_damage(data_root, *loaded.ranger);
    check_identity_and_aliases(data_root, *loaded.ranger);
    check_rejection(data_root, *loaded.ranger);
    check_all_battles(data_root, *loaded.ranger);
    check_enemy_actions_and_settlement(data_root, *loaded.ranger);
    return test::failures == 0 ? 0 : 1;
}
