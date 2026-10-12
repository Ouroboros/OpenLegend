#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#include "openlegend/attributes.hpp"
#include "openlegend/battle/battle_session.hpp"
#include "openlegend/model/experience.hpp"
#include "openlegend/model/playthrough_transition.hpp"
#include "openlegend/model/practice.hpp"
#include "openlegend/persistence/toml_slot.hpp"
#include "openlegend/render/legacy_font_renderer.hpp"
#include "openlegend/scene/scene.hpp"
#include "test_support.hpp"

namespace {

NODISCARD bool finish_automatic_battle(
    openlegend::battle::BattleSession& session,
    const openlegend::resource::DataRoot& data_root,
    const openlegend::battle::BattleStepResult expected = openlegend::battle::BattleStepResult::victory,
    const bool wide_values = true) {
    using namespace openlegend;
    render::IndexedFramebuffer framebuffer;
    const auto ascii = data_root.read("FONT3.E16");
    const auto big5 = data_root.read("FONT3.C16");
    OL_CHECK(ascii && big5);
    if (!ascii || !big5) {
        return false;
    }
    render::Big5GlyphCache glyphs{big5.bytes};
    bool wide_experience_presented = false;
    bool damage_presented = false;
    bool settlement_presented = false;
    std::int64_t maximum_damage = 0;
    for (std::uint32_t step = 0U; step < 2000U && session.valid() && !session.finished(); ++step) {
        const auto phase = session.phase();
        if (phase == battle::BattleSessionPhase::initial_fade_to_black) {
            OL_CHECK(session.finish_initial_fade_to_black());
            continue;
        }
        damage_presented = damage_presented || phase == battle::BattleSessionPhase::ai_damage_frame_present;
        settlement_presented = settlement_presented || phase == battle::BattleSessionPhase::post_battle_message_present;
        for (const auto& combatant : session.setup().combatants().first(
                 static_cast<std::size_t>(session.setup().combatant_count()))) {
            maximum_damage = std::max(maximum_damage, combatant.damage_value);
        }
        if (!session.render(framebuffer)) {
            std::cerr << "battle render failed: phase=" << static_cast<int>(session.phase())
                      << " message=" << session.post_battle_message_index()
                      << " error=" << session.error() << '\n';
            return false;
        }
        if (phase == battle::BattleSessionPhase::post_battle_message_present &&
            session.post_battle_message_index() == 0U && session.post_battle_result()) {
            for (const auto& role : session.post_battle_result()->roles) {
                if (!role.experience_message_required || role.experience_gained <= 99999) {
                    continue;
                }
                const auto digits = std::to_string(role.experience_gained);
                const std::u8string number{digits.begin(), digits.end()};
                render::IndexedFramebuffer expected_number;
                expected_number.clear(255U);
                OL_CHECK(render::draw_text_utf8(expected_number, 152, 52, number,
                    ascii.bytes, glyphs, render::legacy_color::text::notice));
                bool matches = true;
                for (int row = 52; row < 69; ++row) {
                    for (int column = 152; column < 305; ++column) {
                        if (expected_number.row(row)[column] != 255U) {
                            matches = matches && framebuffer.row(row)[column] == expected_number.row(row)[column];
                        }
                    }
                }
                OL_CHECK(matches);
                wide_experience_presented = true;
                break;
            }
        }
        const auto tick = step * 300U;
        session.finish_presented_tick(tick);
        if (session.phase() == battle::BattleSessionPhase::player_action) {
            static_cast<void>(session.handle_key(0x9EU));
            OL_CHECK(session.handle_key(0x0DU) == battle::BattleSessionInputResult::action_selected);
        } else if (session.phase() == battle::BattleSessionPhase::party_selection) {
            if (session.setup().cursor() != session.setup().party_prefix_length()) {
                static_cast<void>(session.handle_key(0x98U));
            } else {
                static_cast<void>(session.handle_key(0x0DU));
            }
        } else if (session.phase() == battle::BattleSessionPhase::battle_outcome_wait ||
                   session.phase() == battle::BattleSessionPhase::post_battle_message_wait) {
            static_cast<void>(session.handle_key(0x0DU));
        }
        session.advance(tick);
    }
    if (!session.valid() || !session.finished()) {
        std::cerr << "battle did not finish: phase=" << static_cast<int>(session.phase())
                  << " error=" << session.error() << '\n';
    }
    OL_CHECK(damage_presented);
    OL_CHECK(settlement_presented == (expected == battle::BattleStepResult::victory));
    OL_CHECK(wide_experience_presented ==
        (wide_values && expected == battle::BattleStepResult::victory));
    OL_CHECK(maximum_damage > 0);
    if (wide_values) {
        OL_CHECK(maximum_damage > std::numeric_limits<std::int32_t>::max());
    }
    return session.valid() && session.finished() && session.result() == expected;
}

void check_session_and_persistence(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::GameSnapshot& baseline,
    const openlegend::persistence::AssetFingerprints& fingerprints,
    const std::int64_t playthrough,
    const bool wide_messages = false) {
    using namespace openlegend;
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    if (wide_messages) {
        configuration.battle_experience_percent_ng2 = 10'000'000;
    }
    auto decoded = model::decode_legacy_snapshot(baseline, configuration, &baseline.ranger);
    OL_CHECK(decoded.has_value());
    if (!decoded) {
        return;
    }
    auto& snapshot = *decoded;
    snapshot.origin = model::SnapshotOrigin::new_game_plus;
    snapshot.playthrough = playthrough;
    auto& ranger = snapshot.ranger;
    ranger.header.set_team_member(0U, model::CharacterId{0});
    ranger.header.set_team_member(1U, model::CharacterId{1});
    for (std::size_t slot = 2U; slot < model::kTeamMemberCount; ++slot) {
        ranger.header.set_team_member(slot, model::CharacterId{-1});
    }
    ranger.roles[0U].ever_joined = true;
    auto& actor = ranger.roles[1U];
    actor.ever_joined = true;
    if (wide_messages) {
        actor.name = u8"一二三四五";
    }
    actor.level = wide_messages ? 1'000'000'000 : 1'000'000;
    actor.experience = 0;
    actor.hp = actor.maximum_hp = 5'000'000'000'000;
    actor.mp = actor.maximum_mp = 4'000'000'000'000;
    actor.attack = actor.defence = 3'000'000'000'000;
    actor.speed = 100;
    actor.medicine = 100;
    actor.physical_power = 100;
    actor.hurt = actor.poison = 0;
    std::size_t manual_index = ranger.items.size();
    for (std::size_t index = 0U; index < ranger.items.size(); ++index) {
        const auto& item = ranger.items[index];
        if (item.word(model::item_word::item_type) == 2 && item.word(model::item_word::magic_id) == -1 &&
            item.word(model::item_word::user) == -1 && item.word(model::item_word::need_experience) > 0 &&
            battle::role_meets_item_requirements(ranger, 1, static_cast<std::int16_t>(index))) {
            manual_index = index;
            break;
        }
    }
    OL_CHECK(manual_index < ranger.items.size());
    if (manual_index >= ranger.items.size()) {
        return;
    }
    const auto manual_id = static_cast<std::int16_t>(manual_index);
    ranger.header.set_inventory(0U, model::ItemId{manual_id}, 1);
    OL_CHECK(battle::assign_role_practice_item(ranger, 1, manual_id));
    OL_CHECK(ranger.header.add_inventory(model::ItemId{174}, 5'000'000'000'000));
    const auto enemy_before = ranger.roles[3U];
    random::LegacyRandom random{1U};
    battle::BattleSession session{data_root, ranger, random, 4, true, {}, nullptr, nullptr, nullptr,
        configuration, playthrough, &baseline.ranger};
    OL_CHECK(session.valid());
    if (!session.valid() || !finish_automatic_battle(session, data_root)) {
        OL_CHECK(false);
        return;
    }
    OL_CHECK(session.post_battle_result().has_value());
    OL_CHECK(ranger.roles[3U] == enemy_before);
    const auto& settled_actor = ranger.roles[1U];
    OL_CHECK(settled_actor.experience > std::numeric_limits<std::int32_t>::max());
    if (wide_messages) {
        OL_CHECK(settled_actor.experience >= 10'000'000'000'000'000);
    }
    OL_CHECK(settled_actor.no_magic_count.contains(static_cast<std::int16_t>(manual_index)) &&
        settled_actor.no_magic_count.at(static_cast<std::int16_t>(manual_index)) == 1);
    OL_CHECK(settled_actor.item_experience == 0);
    OL_CHECK(settled_actor.hp > std::numeric_limits<std::int32_t>::max());
    OL_CHECK(snapshot.valid_for_persistence());
    const auto directory = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / std::to_string(playthrough);
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    OL_CHECK(!error);
    const persistence::TomlSnapshotContext context{baseline.ranger, fingerprints, configuration};
    const persistence::TomlSaveMetadata ordinary{
        persistence::TomlSaveKind::ordinary, persistence::SaveSlot::one, "2026-01-01T00:00:00Z"};
    const auto saved = persistence::write_toml_slot(directory, snapshot, ordinary, context);
    OL_CHECK(static_cast<bool>(saved));
    if (!saved) {
        std::cerr << "post-battle save failed: " << saved.detail << '\n';
        return;
    }
    const auto loaded = persistence::load_toml_slot(directory, ordinary.kind, ordinary.slot, context);
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    OL_CHECK(loaded.save->snapshot == snapshot);
    const persistence::TomlSaveMetadata completion{
        persistence::TomlSaveKind::completion, persistence::SaveSlot::one, ordinary.timestamp_utc};
    OL_CHECK(static_cast<bool>(persistence::write_toml_slot(directory, loaded.save->snapshot, completion, context)));
    const auto completed = persistence::load_toml_slot(directory, completion.kind, completion.slot, context);
    OL_CHECK(static_cast<bool>(completed));
    if (!completed) {
        return;
    }
    const auto inherited = model::prepare_next_playthrough(completed.save->snapshot, baseline, configuration);
    if (playthrough == 999) {
        OL_CHECK(!inherited);
    } else {
        OL_CHECK(static_cast<bool>(inherited));
        if (inherited) {
            const auto& next_actor = inherited.snapshot->ranger.roles[1U];
            OL_CHECK(inherited.snapshot->playthrough == playthrough + 1);
            OL_CHECK(next_actor.experience == settled_actor.experience);
            OL_CHECK(next_actor.attack == settled_actor.attack);
            OL_CHECK(next_actor.medicine == settled_actor.medicine);
            OL_CHECK(next_actor.no_magic_count == settled_actor.no_magic_count);
            OL_CHECK(next_actor.practice_item.value == -1 && next_actor.item_experience == 0);
            OL_CHECK(next_actor.hp == next_actor.maximum_hp && next_actor.mp == next_actor.maximum_mp);
        }
    }
    OL_CHECK(completed.save->snapshot == snapshot);
    std::filesystem::remove_all(directory, error);
    OL_CHECK(!error);
}

void check_ordinary_progression(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::GameSnapshot& baseline,
    const openlegend::persistence::AssetFingerprints& fingerprints) {
    using namespace openlegend;
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    auto decoded = model::decode_legacy_snapshot(baseline, configuration, &baseline.ranger);
    const auto progression = battle::load_progression_data(data_root);
    OL_CHECK(decoded && progression.error.empty());
    if (!decoded || !progression.error.empty()) {
        return;
    }
    auto snapshot = std::move(*decoded);
    snapshot.origin = model::SnapshotOrigin::new_game_plus;
    snapshot.ranger.header.set_team_member(0U, model::CharacterId{0});
    for (std::size_t slot = 1U; slot < model::kTeamMemberCount; ++slot) {
        snapshot.ranger.header.set_team_member(slot, model::CharacterId{-1});
    }
    auto& initial = snapshot.ranger.roles[0U];
    initial.ever_joined = true;
    initial.level = 30;
    const auto initial_experience = model::level_experience_requirement(progression.thresholds, initial.level);
    OL_CHECK(initial_experience.has_value());
    if (!initial_experience) {
        return;
    }
    initial.experience = *initial_experience;
    initial.hp = initial.maximum_hp = 999;
    initial.mp = initial.maximum_mp = 999;
    initial.attack = initial.defence = initial.speed = initial.iq = 100;
    initial.hurt = initial.poison = 0;
    initial.physical_power = 100;
    initial.magic_ids.fill(model::MagicId{-1});
    initial.magic_levels.fill(0);
    initial.magic_ids[0U] = model::MagicId{1};
    initial.magic_levels[0U] = 900;
    OL_CHECK(snapshot.valid_for_persistence());
    if (!snapshot.valid_for_persistence()) {
        return;
    }
    random::LegacyRandom random{1U};
    const persistence::TomlSnapshotContext context{baseline.ranger, fingerprints, configuration};
    const auto directory = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "ordinary-growth";
    const persistence::TomlSaveMetadata metadata{persistence::TomlSaveKind::ordinary,
        persistence::SaveSlot::one, "2026-01-01T00:00:00Z"};
    for (const std::int64_t playthrough : {2, 3, 4, 5}) {
        auto inherited = model::prepare_next_playthrough(snapshot, baseline, configuration);
        OL_CHECK(inherited);
        if (!inherited) {
            std::cerr << "ordinary growth inheritance failed: " << inherited.error << '\n';
            return;
        }
        snapshot = std::move(*inherited.snapshot);
        OL_CHECK(snapshot.playthrough == playthrough);
        OL_CHECK(snapshot.ranger.header.add_inventory(model::ItemId{93}, 1));
        OL_CHECK(battle::assign_role_practice_item(snapshot.ranger, 0, 93));
        const auto actor_before = snapshot.ranger.roles[0U];
        const auto enemy_before = snapshot.ranger.roles[1U];
        if (playthrough == 2) {
            OL_CHECK(persistence::write_toml_slot(directory, snapshot, metadata, context));
            const auto preflight = persistence::load_toml_slot(
                directory, metadata.kind, metadata.slot, context);
            OL_CHECK(preflight && preflight.save->snapshot == snapshot);
        }
        scene::SceneSession resting{data_root, snapshot, random, 70};
        OL_CHECK(resting.valid());
        if (!resting.valid()) {
            return;
        }
        render::IndexedFramebuffer framebuffer;
        for (std::size_t battle_index = 0U; battle_index < 32U; ++battle_index) {
            auto step = resting.begin_event(931);
            for (std::size_t rest_step = 0U; step.kind != scene::SceneStepKind::stay && rest_step < 128U;
                 ++rest_step) {
                OL_CHECK(resting.render(framebuffer));
                step = resting.resume(step.kind == scene::SceneStepKind::question
                    ? scene::SceneResponse::yes : scene::SceneResponse::acknowledge);
            }
            OL_CHECK(resting.valid() && step.kind == scene::SceneStepKind::stay);
            battle::BattleSession session{data_root, snapshot.ranger, random, 0, true, {},
                nullptr, nullptr, nullptr, configuration, playthrough, &baseline.ranger};
            OL_CHECK(session.valid());
            if (!session.valid() || !finish_automatic_battle(session, data_root,
                    battle::BattleStepResult::victory, false)) {
                std::cerr << "ordinary growth battle failed: NG" << playthrough
                          << " battle=" << battle_index << " error=" << session.error() << '\n';
                OL_CHECK(false);
                return;
            }
            OL_CHECK(snapshot.ranger.roles[1U] == enemy_before);
        }
        const auto& actor = snapshot.ranger.roles[0U];
        OL_CHECK(actor.experience > actor_before.experience && actor.level > actor_before.level);
        OL_CHECK(actor.magic_levels[0U] > actor_before.magic_levels[0U]);
        OL_CHECK(actor.no_magic_count.at(93) >
            (actor_before.no_magic_count.contains(93) ? actor_before.no_magic_count.at(93) : 0));
        OL_CHECK(actor.attack > actor_before.attack && actor.maximum_hp > actor_before.maximum_hp);
        OL_CHECK(snapshot.valid_for_persistence());
        std::cout << "ordinary growth NG" << playthrough << ": battles=32 level=" << actor.level
                  << " experience=" << actor.experience << " attack=" << actor.attack
                  << " max_hp=" << actor.maximum_hp << " proficiency=" << actor.magic_levels[0U]
                  << " manual_completions=" << actor.no_magic_count.at(93) << '\n';
        OL_CHECK(persistence::write_toml_slot(directory, snapshot, metadata, context));
        auto loaded = persistence::load_toml_slot(directory, metadata.kind, metadata.slot, context);
        OL_CHECK(loaded && loaded.save->snapshot == snapshot);
        if (!loaded) {
            return;
        }
        snapshot = std::move(loaded.save->snapshot);
    }
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    OL_CHECK(!error);
}

void check_defeat(
    const openlegend::resource::DataRoot& data_root,
    const openlegend::model::GameSnapshot& baseline,
    const openlegend::persistence::AssetFingerprints& fingerprints) {
    using namespace openlegend;
    model::NewGamePlusConfiguration configuration;
    configuration.enabled = true;
    configuration.attack_cap_step = 5'000'000'000'000;
    auto decoded = model::decode_legacy_snapshot(baseline, configuration, &baseline.ranger);
    OL_CHECK(decoded.has_value());
    if (!decoded) {
        return;
    }
    auto& snapshot = *decoded;
    snapshot.origin = model::SnapshotOrigin::new_game_plus;
    snapshot.playthrough = 2;
    auto& ranger = snapshot.ranger;
    auto& actor = ranger.roles[1U];
    actor.ever_joined = true;
    actor.hp = actor.maximum_hp = 50'000'000'000;
    actor.mp = actor.maximum_mp = 4'000'000'000'000;
    actor.attack = 1;
    actor.defence = actor.speed = 0;
    actor.hurt = actor.poison = 0;
    actor.physical_power = 100;
    const auto history_before = actor.no_magic_count;
    const auto enemy_before = ranger.roles[3U];
    random::LegacyRandom random{1U};
    battle::BattleSession session{data_root, ranger, random, 4, false, {}, nullptr, nullptr, nullptr,
        configuration, 2, &baseline.ranger};
    OL_CHECK(session.valid());
    if (!session.valid() || !finish_automatic_battle(session, data_root, battle::BattleStepResult::defeat)) {
        OL_CHECK(false);
        return;
    }
    OL_CHECK(session.post_battle_result().has_value());
    OL_CHECK(ranger.roles[1U].hp == 8'525'000'000);
    OL_CHECK(ranger.roles[1U].hurt == 59);
    OL_CHECK(ranger.roles[1U].no_magic_count == history_before);
    OL_CHECK(ranger.roles[3U] == enemy_before);
    OL_CHECK(snapshot.valid_for_persistence());
    const auto directory = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "defeat";
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    OL_CHECK(!error);
    const persistence::TomlSnapshotContext context{baseline.ranger, fingerprints, configuration};
    const persistence::TomlSaveMetadata metadata{
        persistence::TomlSaveKind::ordinary, persistence::SaveSlot::one, "2026-01-01T00:00:00Z"};
    const auto saved = persistence::write_toml_slot(directory, snapshot, metadata, context);
    OL_CHECK(static_cast<bool>(saved));
    const auto loaded = persistence::load_toml_slot(directory, metadata.kind, metadata.slot, context);
    OL_CHECK(static_cast<bool>(loaded));
    if (loaded) {
        OL_CHECK(loaded.save->snapshot == snapshot);
    }
    std::filesystem::remove_all(directory, error);
    OL_CHECK(!error);
}

}

int main() {
    using namespace openlegend;
    const resource::DataRoot data_root{test::game_data_root()};
    const auto baseline = persistence::load_baseline(data_root.path());
    const auto fingerprints = persistence::fingerprint_new_game_plus_assets(data_root.path());
    OL_CHECK(static_cast<bool>(baseline));
    OL_CHECK(static_cast<bool>(fingerprints));
    if (!baseline || !fingerprints) {
        return 1;
    }
    for (const std::int64_t playthrough : {1, 2, 3, 4, 5, 999}) {
        check_session_and_persistence(data_root, *baseline.snapshot, *fingerprints.fingerprints, playthrough);
    }
    check_session_and_persistence(data_root, *baseline.snapshot, *fingerprints.fingerprints, 2, true);
    check_defeat(data_root, *baseline.snapshot, *fingerprints.fingerprints);
    check_ordinary_progression(data_root, *baseline.snapshot, *fingerprints.fingerprints);
    return test::failures == 0 ? 0 : 1;
}
