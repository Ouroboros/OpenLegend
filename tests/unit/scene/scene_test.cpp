#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/scene/scene.hpp"
#include "test_support.hpp"

namespace {

[[nodiscard]] std::uint64_t fnv1a64(const std::span<const std::uint8_t> bytes) {
    std::uint64_t result = 0xCBF29CE484222325ULL;
    for (const auto byte : bytes) {
        result ^= byte;
        result *= 0x100000001B3ULL;
    }
    return result;
}

[[nodiscard]] openlegend::model::GameSnapshot load_baseline(
    const std::filesystem::path& root) {
    auto loaded = openlegend::persistence::load_baseline(root);
    OL_CHECK(loaded);
    return loaded ? std::move(*loaded.snapshot) : openlegend::model::GameSnapshot{};
}

[[nodiscard]] openlegend::scene::SceneStepResult finish_scene_title(
    openlegend::scene::SceneSession& session) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;
    if (session.pending().kind == SceneStepKind::fade_from_black) {
        const auto title = session.resume(SceneResponse::acknowledge);
        OL_CHECK(title.kind == SceneStepKind::scene_title);
    } else {
        OL_CHECK(session.pending().kind == SceneStepKind::scene_title);
    }
    const auto present = session.resume(SceneResponse::acknowledge);
    OL_CHECK(present.kind == SceneStepKind::present);
    return session.resume(SceneResponse::acknowledge);
}

[[nodiscard]] int inventory_count(
    const openlegend::model::RangerState& ranger,
    const std::int16_t item_id) {
    int total = 0;
    for (std::size_t index = 0U; index < openlegend::model::kInventoryCount; ++index) {
        if (ranger.header.inventory_item(index).value == item_id) {
            total += ranger.header.inventory_count(index);
        }
    }
    return total;
}

void check_assets(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};
    const openlegend::scene::SceneAssets assets{data_root};
    OL_CHECK(assets.valid());
    OL_CHECK(assets.talk_count() == 2'977U);
    OL_CHECK(assets.script_count() == 1'018U);
    const auto talk0 = assets.talk(0U);
    const auto talk2976 = assets.talk(2'976U);
    OL_CHECK(fnv1a64(talk0) == 0x3E3D5F420B6D0673ULL);
    OL_CHECK(fnv1a64(talk2976) == 0x5A746CF535EC1913ULL);
    OL_CHECK(!talk0.empty());
    OL_CHECK(talk0.back() == 0U);
    const auto pages = openlegend::scene::paginate_dialogue(assets.talk(14U));
    OL_CHECK(pages.size() == 5U);
    for (const auto& page : pages) {
        OL_CHECK(!page.empty());
        OL_CHECK(page.back() == 0U);
        OL_CHECK(std::count(page.begin(), page.end(), static_cast<std::uint8_t>('*')) <= 2);
    }

    constexpr std::array<std::size_t, 68> widths{
        1, 4, 3, 14, 4, 3, 5, 1, 2, 3, 2, 3, 1, 1, 1, 2, 4,
        6, 4, 3, 3, 2, 1, 3, 1, 5, 6, 4, 6, 6, 5, 4, 3, 4,
        3, 5, 4, 2, 5, 2, 2, 4, 3, 4, 7, 3, 3, 3, 3, 3, 8,
        1, 1, 1, 1, 5, 2, 1, 1, 1, 6, 3, 7, 3, 1, 1, 2, 2};
    constexpr std::array<std::size_t, 68> expected_counts{
        4108, 3561, 325, 2320, 167, 43, 145, 645, 15, 81, 80, 7, 7,
        346, 171, 114, 80, 127, 2, 15, 82, 35, 1, 1, 0, 52, 121, 43,
        22, 5, 7, 8, 160, 7, 4, 8, 1, 156, 3, 3, 12, 6, 2, 5, 6,
        3, 2, 5, 8, 3, 1, 1, 1, 1, 1, 28, 105, 1, 1, 1, 5, 14,
        1, 1, 1, 1, 1, 20};
    std::array<std::size_t, 68> actual_counts{};
    std::size_t instruction_count = 0U;
    for (std::size_t script_id = 0U; script_id < assets.script_count(); ++script_id) {
        const auto script = assets.script(script_id);
        std::size_t program_counter = 0U;
        bool terminated = false;
        while (program_counter < script.size()) {
            const auto opcode = script[program_counter];
            if (opcode == -1) {
                terminated = true;
                break;
            }
            OL_CHECK(opcode >= 0);
            OL_CHECK(opcode < static_cast<std::int16_t>(widths.size()));
            if (opcode < 0 || opcode >= static_cast<std::int16_t>(widths.size())) {
                break;
            }
            ++actual_counts[static_cast<std::size_t>(opcode)];
            ++instruction_count;
            program_counter += widths[static_cast<std::size_t>(opcode)];
        }
        OL_CHECK(terminated);
    }
    OL_CHECK(instruction_count == 13'315U);
    OL_CHECK(actual_counts == expected_counts);

    for (const auto& [name, expected] : std::array{
             std::pair{"ALLSIN.GRP", 0x2559740843552333ULL},
             std::pair{"ALLDEF.GRP", 0x6858A00A333C091DULL},
             std::pair{"TALK.GRP", 0x676F88CCC95035C9ULL},
             std::pair{"KDEF.GRP", 0x84830AB54B029419ULL}}) {
        const auto file = data_root.read(name);
        OL_CHECK(file);
        OL_CHECK(fnv1a64(file.bytes) == expected);
    }
}

void check_scene_render_and_movement(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[70];
    for (std::size_t index = 0U; index < openlegend::model::scene_metadata_word::exit_count; ++index) {
        metadata.set_word(openlegend::model::scene_metadata_word::exit_x_begin + index, -1);
        metadata.set_word(openlegend::model::scene_metadata_word::exit_y_begin + index, -1);
    }
    metadata.set_word(openlegend::model::scene_metadata_word::jump_scene, -1);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(session.valid());
    OL_CHECK(session.scene_id() == 70);
    OL_CHECK(session.scene_x() == 44);
    OL_CHECK(session.scene_y() == 29);
    OL_CHECK(session.view_origin_x() == 33);
    OL_CHECK(session.view_origin_y() == 18);
    OL_CHECK(session.direction() == openlegend::scene::SceneDirection::right);
    OL_CHECK(session.pending().kind == openlegend::scene::SceneStepKind::fade_from_black);

    openlegend::render::IndexedFramebuffer framebuffer;
    OL_CHECK(session.render_map(framebuffer));
    const auto frame_hash = fnv1a64(framebuffer.pixels());
    if (frame_hash != 0x38FBAA07B733AD79ULL) {
        std::cerr << "scene 70 frame: expected 0x38fbaa07b733ad79, actual 0x"
                  << std::hex << frame_hash << std::dec << '\n';
    }
    OL_CHECK(frame_hash == 0x38FBAA07B733AD79ULL);
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::scene_title);
    openlegend::render::IndexedFramebuffer title_framebuffer;
    OL_CHECK(session.render(title_framebuffer));
    OL_CHECK(fnv1a64(title_framebuffer.pixels()) == 0xC5A8777E049759F2ULL);

    OL_CHECK(finish_scene_title(session).kind ==
             openlegend::scene::SceneStepKind::stay);
    const auto right = session.move(openlegend::scene::SceneDirection::right);
    OL_CHECK(right.kind == openlegend::scene::SceneStepKind::moved);
    OL_CHECK(right.scene_x == 45 && right.scene_y == 29);
    const auto up = session.move(openlegend::scene::SceneDirection::up);
    OL_CHECK(up.kind == openlegend::scene::SceneStepKind::moved);
    OL_CHECK(up.scene_x == 45 && up.scene_y == 28);
    const auto left = session.move(openlegend::scene::SceneDirection::left);
    OL_CHECK(left.kind == openlegend::scene::SceneStepKind::stay);
    OL_CHECK(left.scene_x == 45 && left.scene_y == 28);
    const auto down = session.move(openlegend::scene::SceneDirection::down);
    OL_CHECK(down.kind == openlegend::scene::SceneStepKind::moved);
    OL_CHECK(down.scene_x == 45 && down.scene_y == 29);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::sub_map_x) == 45);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::sub_map_y) == 29);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::face_towards) == 3);
}

void check_scene_entry_state(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[70];
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 0);
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 63);
    metadata.set_word(openlegend::model::scene_metadata_word::jump_return_x, 63);
    metadata.set_word(openlegend::model::scene_metadata_word::jump_return_y, 0);
    snapshot.ranger.header.set_word(
        openlegend::model::header_word::face_towards,
        static_cast<std::int16_t>(openlegend::scene::SceneDirection::left));
    openlegend::random::LegacyRandom random{1U};

    const openlegend::scene::SceneSession entrance{data_root, snapshot, random, 70};
    OL_CHECK(entrance.valid());
    OL_CHECK(entrance.scene_x() == 0 && entrance.scene_y() == 63);
    OL_CHECK(entrance.view_origin_x() == 0 && entrance.view_origin_y() == 36);
    OL_CHECK(entrance.direction() == openlegend::scene::SceneDirection::left);
    OL_CHECK(entrance.player_frame() == 5030);

    const openlegend::scene::SceneSession jump{data_root, snapshot, random, 70, true};
    OL_CHECK(jump.valid());
    OL_CHECK(jump.scene_x() == 63 && jump.scene_y() == 0);
    OL_CHECK(jump.view_origin_x() == 36 && jump.view_origin_y() == 0);
    OL_CHECK(jump.direction() == openlegend::scene::SceneDirection::left);
    OL_CHECK(jump.player_frame() == 5030);
}

void check_scene_archive_ownership(const std::filesystem::path& root) {
    using openlegend::model::SceneEventField;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    const auto before_x = snapshot.event_value(7U, 6U, SceneEventField::x).value_or(-32768);
    const auto before_y = snapshot.event_value(7U, 6U, SceneEventField::y).value_or(-32768);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    const auto result = session.begin_event(436, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(session.scene_id() == 70);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::cannot_walk).value_or(-1) == 0);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::index).value_or(-1) == 0);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::event_1).value_or(0) == -1);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::event_2).value_or(0) == -1);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::event_3).value_or(0) == -1);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::current_picture).value_or(0) == -1);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::end_picture).value_or(0) == -1);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::begin_picture).value_or(0) == -1);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::picture_delay).value_or(-1) == 0);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::x).value_or(-32768) == before_x);
    OL_CHECK(snapshot.event_value(7U, 6U, SceneEventField::y).value_or(-32768) == before_y);
}

void check_scene_interaction_present(const std::filesystem::path& root) {
    using openlegend::model::SceneEventField;
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    constexpr std::size_t event = 199U;
    OL_CHECK(snapshot.set_scene_value(
        70U, SceneLayer::event_index, 29U * 64U + 45U, event));
    OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::event_1, 825));
    snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, 7);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
    OL_CHECK(session.interact().kind == SceneStepKind::present);
    const auto result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(result.style == 52);
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

    auto empty_snapshot = load_baseline(root);
    OL_CHECK(empty_snapshot.set_scene_value(
        70U, SceneLayer::event_index, 29U * 64U + 45U, -1));
    openlegend::random::LegacyRandom empty_random{1U};
    openlegend::scene::SceneSession empty_session{
        data_root, empty_snapshot, empty_random, 70};
    OL_CHECK(finish_scene_title(empty_session).kind == SceneStepKind::stay);
    OL_CHECK(empty_session.interact().kind == SceneStepKind::present);
    OL_CHECK(empty_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
}

void check_scene_item_and_auto_event_present(const std::filesystem::path& root) {
    using openlegend::model::SceneEventField;
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    constexpr std::size_t event = 199U;
    constexpr std::size_t front = 29U * 64U + 45U;
    constexpr std::size_t current = 29U * 64U + 44U;

    auto item_snapshot = load_baseline(root);
    OL_CHECK(item_snapshot.set_scene_value(70U, SceneLayer::event_index, front, event));
    OL_CHECK(item_snapshot.set_event_value(70U, event, SceneEventField::event_2, 825));
    item_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, 7);
    openlegend::random::LegacyRandom item_random{1U};
    openlegend::scene::SceneSession item_session{
        data_root, item_snapshot, item_random, 70};
    OL_CHECK(finish_scene_title(item_session).kind == SceneStepKind::stay);
    OL_CHECK(item_session.use_item(123).kind == SceneStepKind::present);
    const auto item_notice = item_session.resume(SceneResponse::acknowledge);
    OL_CHECK(item_notice.kind == SceneStepKind::notice && item_notice.style == 52);

    auto menu_item_snapshot = load_baseline(root);
    OL_CHECK(menu_item_snapshot.set_scene_value(
        70U, SceneLayer::event_index, front, event));
    OL_CHECK(menu_item_snapshot.set_event_value(
        70U, event, SceneEventField::event_2, 825));
    menu_item_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, 7);
    openlegend::random::LegacyRandom menu_item_random{1U};
    openlegend::scene::SceneSession menu_item_session{
        data_root, menu_item_snapshot, menu_item_random, 70};
    OL_CHECK(finish_scene_title(menu_item_session).kind == SceneStepKind::stay);
    OL_CHECK(menu_item_session.tick(std::nullopt, false, true).kind ==
             SceneStepKind::open_ui);
    OL_CHECK(menu_item_session.use_menu_item(123).kind == SceneStepKind::present);
    OL_CHECK(menu_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);
    const auto menu_item_notice = menu_item_session.resume(SceneResponse::acknowledge);
    OL_CHECK(menu_item_notice.kind == SceneStepKind::notice && menu_item_notice.style == 52);
    OL_CHECK(menu_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::open_ui);
    OL_CHECK(menu_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);

    auto empty_menu_item_snapshot = load_baseline(root);
    OL_CHECK(empty_menu_item_snapshot.set_scene_value(
        70U, SceneLayer::event_index, front, -1));
    openlegend::random::LegacyRandom empty_menu_item_random{1U};
    openlegend::scene::SceneSession empty_menu_item_session{
        data_root, empty_menu_item_snapshot, empty_menu_item_random, 70};
    OL_CHECK(finish_scene_title(empty_menu_item_session).kind == SceneStepKind::stay);
    OL_CHECK(empty_menu_item_session.tick(std::nullopt, false, true).kind ==
             SceneStepKind::open_ui);
    OL_CHECK(empty_menu_item_session.use_menu_item(123).kind == SceneStepKind::present);
    OL_CHECK(empty_menu_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::open_ui);
    OL_CHECK(empty_menu_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);

    auto zero_item_snapshot = load_baseline(root);
    OL_CHECK(zero_item_snapshot.set_scene_value(
        70U, SceneLayer::event_index, front, event));
    OL_CHECK(zero_item_snapshot.set_event_value(
        70U, event, SceneEventField::event_2, 0));
    openlegend::random::LegacyRandom zero_item_random{1U};
    openlegend::scene::SceneSession zero_item_session{
        data_root, zero_item_snapshot, zero_item_random, 70};
    OL_CHECK(finish_scene_title(zero_item_session).kind == SceneStepKind::stay);
    OL_CHECK(zero_item_session.use_item(123).kind == SceneStepKind::present);
    OL_CHECK(zero_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::stay);

    auto disabled_auto_snapshot = load_baseline(root);
    OL_CHECK(disabled_auto_snapshot.set_scene_value(
        70U, SceneLayer::event_index, current, event));
    OL_CHECK(disabled_auto_snapshot.set_event_value(
        70U, event, SceneEventField::event_3, -1));
    openlegend::random::LegacyRandom disabled_auto_random{1U};
    openlegend::scene::SceneSession disabled_auto{
        data_root, disabled_auto_snapshot, disabled_auto_random, 70};
    OL_CHECK(disabled_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::scene_title);
    OL_CHECK(disabled_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);
    OL_CHECK(disabled_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::stay);

    auto zero_auto_snapshot = load_baseline(root);
    OL_CHECK(zero_auto_snapshot.set_scene_value(
        70U, SceneLayer::event_index, current, event));
    OL_CHECK(zero_auto_snapshot.set_event_value(
        70U, event, SceneEventField::event_3, 0));
    openlegend::random::LegacyRandom zero_auto_random{1U};
    openlegend::scene::SceneSession zero_auto{
        data_root, zero_auto_snapshot, zero_auto_random, 70};
    OL_CHECK(zero_auto.resume(SceneResponse::acknowledge).kind == SceneStepKind::scene_title);
    OL_CHECK(zero_auto.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(zero_auto.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(zero_auto.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

    auto active_auto_snapshot = load_baseline(root);
    OL_CHECK(active_auto_snapshot.set_scene_value(
        70U, SceneLayer::event_index, current, event));
    OL_CHECK(active_auto_snapshot.set_event_value(
        70U, event, SceneEventField::event_3, 825));
    active_auto_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, 7);
    openlegend::random::LegacyRandom active_auto_random{1U};
    openlegend::scene::SceneSession active_auto{
        data_root, active_auto_snapshot, active_auto_random, 70};
    OL_CHECK(active_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::scene_title);
    OL_CHECK(active_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);
    OL_CHECK(active_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);
    const auto auto_notice = active_auto.resume(SceneResponse::acknowledge);
    OL_CHECK(auto_notice.kind == SceneStepKind::notice && auto_notice.style == 52);
}

void check_scene_loop_transitions(const std::filesystem::path& root) {
    using openlegend::model::SceneEventField;
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneAudioCommand;
    using openlegend::scene::SceneDirection;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    constexpr std::size_t event = 199U;
    constexpr std::size_t target = 29U * 64U + 45U;

    auto exit_snapshot = load_baseline(root);
    auto& exit_metadata = exit_snapshot.ranger.scenes[70];
    for (std::size_t index = 0U; index < openlegend::model::scene_metadata_word::exit_count;
         ++index) {
        exit_metadata.set_word(openlegend::model::scene_metadata_word::exit_x_begin + index, -1);
        exit_metadata.set_word(openlegend::model::scene_metadata_word::exit_y_begin + index, -1);
    }
    exit_metadata.set_word(openlegend::model::scene_metadata_word::exit_x_begin, 45);
    exit_metadata.set_word(openlegend::model::scene_metadata_word::exit_y_begin, 29);
    exit_metadata.set_word(openlegend::model::scene_metadata_word::jump_scene, -1);
    exit_metadata.set_word(openlegend::model::scene_metadata_word::exit_music, 10);
    OL_CHECK(exit_snapshot.set_scene_value(70U, SceneLayer::event_index, target, event));
    OL_CHECK(exit_snapshot.set_event_value(70U, event, SceneEventField::event_3, 825));
    exit_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, 7);
    openlegend::random::LegacyRandom exit_random{1U};
    openlegend::scene::SceneSession exit_session{
        data_root, exit_snapshot, exit_random, 70};
    OL_CHECK(finish_scene_title(exit_session).kind == SceneStepKind::stay);
    static_cast<void>(exit_session.take_audio_commands());
    OL_CHECK(exit_session.tick(SceneDirection::right, false, false).kind ==
             SceneStepKind::present);
    OL_CHECK(exit_session.scene_x() == 45 && exit_session.scene_y() == 29);
    OL_CHECK(exit_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(exit_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::notice);
    OL_CHECK(exit_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::fade_to_black);
    OL_CHECK(exit_snapshot.ranger.header.word(openlegend::model::header_word::in_sub_map) == 1);
    OL_CHECK(exit_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::return_world);
    OL_CHECK(exit_snapshot.ranger.header.word(openlegend::model::header_word::in_sub_map) == 0);
    const auto exit_audio = exit_session.take_audio_commands();
    const std::vector<SceneAudioCommand> expected_exit_audio{
        {SceneAudioCommand::Kind::music, 10}};
    OL_CHECK(exit_audio == expected_exit_audio);

    auto jump_snapshot = load_baseline(root);
    auto& source = jump_snapshot.ranger.scenes[70];
    for (std::size_t index = 0U; index < openlegend::model::scene_metadata_word::exit_count;
         ++index) {
        source.set_word(openlegend::model::scene_metadata_word::exit_x_begin + index, -1);
        source.set_word(openlegend::model::scene_metadata_word::exit_y_begin + index, -1);
    }
    source.set_word(openlegend::model::scene_metadata_word::jump_scene, 71);
    source.set_word(openlegend::model::scene_metadata_word::jump_x, 45);
    source.set_word(openlegend::model::scene_metadata_word::jump_y, 29);
    source.set_word(openlegend::model::scene_metadata_word::main_entrance_x_1, 0);
    source.set_word(openlegend::model::scene_metadata_word::main_entrance_y_1, 0);
    auto& destination = jump_snapshot.ranger.scenes[71];
    destination.set_word(openlegend::model::scene_metadata_word::jump_return_x, 12);
    destination.set_word(openlegend::model::scene_metadata_word::jump_return_y, 13);
    destination.set_word(openlegend::model::scene_metadata_word::entrance_x, 20);
    destination.set_word(openlegend::model::scene_metadata_word::entrance_y, 21);
    jump_snapshot.set_scene_value(70U, SceneLayer::event_index, target, -1);
    jump_snapshot.set_scene_value(71U, SceneLayer::event_index, 13U * 64U + 12U, -1);
    openlegend::random::LegacyRandom jump_random{1U};
    openlegend::scene::SceneSession jump_session{
        data_root, jump_snapshot, jump_random, 70};
    OL_CHECK(finish_scene_title(jump_session).kind == SceneStepKind::stay);
    OL_CHECK(jump_session.tick(SceneDirection::right, false, false).kind ==
             SceneStepKind::present);
    OL_CHECK(jump_session.scene_id() == 70);
    OL_CHECK(jump_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::fade_to_black);
    OL_CHECK(jump_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::fade_from_black);
    OL_CHECK(jump_session.scene_id() == 71);
    OL_CHECK(jump_session.scene_x() == 12 && jump_session.scene_y() == 13);
    OL_CHECK(jump_session.player_frame() == 5016);
    OL_CHECK(jump_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::scene_title);
    OL_CHECK(finish_scene_title(jump_session).kind == SceneStepKind::stay);
}

void check_scene_exit_music_override(const std::filesystem::path& root) {
    using openlegend::scene::SceneAudioCommand;
    using openlegend::scene::SceneDirection;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    OL_CHECK(snapshot.ranger.scenes[70].word(
                 openlegend::model::scene_metadata_word::exit_music) == 19);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
    static_cast<void>(session.take_audio_commands());

    auto result = session.begin_event(494, 0, 44, 29);
    for (int step = 0; step < 128 && result.kind != SceneStepKind::stay; ++step) {
        result = session.resume(
            result.kind == SceneStepKind::question ? SceneResponse::yes
                                                   : SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(session.take_audio_commands().empty());

    result = session.tick(SceneDirection::right, false, false);
    for (int step = 0; step < 16 && result.kind != SceneStepKind::fade_to_black; ++step) {
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::return_world);
    const std::vector<SceneAudioCommand> expected{
        {SceneAudioCommand::Kind::music, 3, true}};
    OL_CHECK(session.take_audio_commands() == expected);
}

void check_scene_event_animation(const std::filesystem::path& root) {
    using openlegend::model::SceneEventField;
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    constexpr std::size_t event = 199U;
    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, 0U, event));
    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, 1U, event));
    OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::current_picture, 100));
    OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::end_picture, 110));
    OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::begin_picture, 102));
    OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::picture_delay, 99));
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
    session.idle_tick();
    session.idle_tick();
    session.idle_tick();
    session.idle_tick();
    OL_CHECK(snapshot.event_value(
                 70U, event, SceneEventField::current_picture).value_or(-1) == 100);
    OL_CHECK(snapshot.event_value(
                 70U, event, SceneEventField::begin_picture).value_or(-1) == 106);

    auto render_snapshot = load_baseline(root);
    OL_CHECK(render_snapshot.set_scene_value(
        70U, SceneLayer::event_index, 29U * 64U + 44U, event));
    OL_CHECK(render_snapshot.set_event_value(
        70U, event, SceneEventField::current_picture, 100));
    OL_CHECK(render_snapshot.set_event_value(
        70U, event, SceneEventField::end_picture, 110));
    OL_CHECK(render_snapshot.set_event_value(
        70U, event, SceneEventField::begin_picture, 102));
    OL_CHECK(render_snapshot.set_event_value(
        70U, event, SceneEventField::picture_delay, 99));
    openlegend::random::LegacyRandom render_random{1U};
    openlegend::scene::SceneSession render_session{
        data_root, render_snapshot, render_random, 70};
    openlegend::render::IndexedFramebuffer framebuffer;
    OL_CHECK(render_session.render_map(framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x606EAA33DA727537ULL);
}

void check_scene_weather(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};
    auto coverage_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom coverage_random{1U};
    constexpr std::array<std::int16_t, 11> weather_scene_ids{
        5, 7, 10, 41, 42, 46, 65, 66, 67, 72, 79};
    for (const auto scene_id : weather_scene_ids) {
        const openlegend::scene::SceneSession weather_scene{
            data_root, coverage_snapshot, coverage_random, scene_id};
        OL_CHECK(weather_scene.valid());
        OL_CHECK(weather_scene.weather_enabled());
    }
    const openlegend::scene::SceneSession clear_scene{
        data_root, coverage_snapshot, coverage_random, 70};
    OL_CHECK(clear_scene.valid());
    OL_CHECK(!clear_scene.weather_enabled());

    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 5};
    OL_CHECK(session.valid());
    OL_CHECK(session.scene_x() == 17);
    OL_CHECK(session.scene_y() == 48);

    openlegend::render::IndexedFramebuffer base_frame;
    OL_CHECK(session.render_map(base_frame));
    OL_CHECK(fnv1a64(base_frame.pixels()) == 0x52C8861F0349D6DBULL);
    OL_CHECK(finish_scene_title(session).kind == openlegend::scene::SceneStepKind::stay);
    for (int tick = 0; tick < 300; ++tick) {
        session.periodic_tick();
    }
    OL_CHECK(random.state() == 0xAF1CF0FBU);
    openlegend::render::IndexedFramebuffer weather_frame;
    OL_CHECK(session.render_map(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == 0xB3E2B127988E5690ULL);
}

void check_event_camera_pan(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);

    constexpr std::array<int, 7> expected_origins{30, 29, 28, 27, 26, 25, 24};
    constexpr std::array<std::uint64_t, 7> expected_hashes{
        0x9838F6A2B37AD75DULL,
        0xA58B51E27D8F5FE3ULL,
        0x6A876603FD1DCE87ULL,
        0x3D2C25F9165BD6B4ULL,
        0x8248A9B81EE91C88ULL,
        0x201C90B91AA11963ULL,
        0x2A895D743D76C127ULL,
    };
    auto result = session.begin_event(30, 0, 44, 29);
    for (std::size_t index = 0U; index < expected_origins.size(); ++index) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 2U);
        OL_CHECK(session.view_origin_x() == expected_origins[index]);
        OL_CHECK(session.view_origin_y() == 18);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 86);
    OL_CHECK(session.scene_x() == 44 && session.scene_y() == 29);

    auto diagonal_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom diagonal_random{1U};
    openlegend::scene::SceneSession diagonal{
        data_root, diagonal_snapshot, diagonal_random, 70};
    OL_CHECK(finish_scene_title(diagonal).kind == SceneStepKind::stay);
    auto diagonal_result = diagonal.begin_event(225, 0, 44, 29);
    constexpr std::array<std::array<int, 2>, 5> forward_origins{{
        {33, 18}, {34, 18}, {35, 18}, {36, 18}, {36, 36},
    }};
    for (const auto& origin : forward_origins) {
        OL_CHECK(diagonal_result.kind == SceneStepKind::present);
        OL_CHECK(diagonal_result.wait_ticks == 2U);
        OL_CHECK(diagonal.view_origin_x() == origin[0]);
        OL_CHECK(diagonal.view_origin_y() == origin[1]);
        diagonal_result = diagonal.resume(SceneResponse::acknowledge);
    }
    for (int page = 0; page < 8 && diagonal_result.kind == SceneStepKind::dialogue; ++page) {
        diagonal_result = diagonal.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(diagonal_result.kind == SceneStepKind::present);
    OL_CHECK(diagonal_result.wait_ticks == 1U);
    diagonal_result = diagonal.resume(SceneResponse::acknowledge);
    constexpr std::array<std::array<int, 2>, 5> reverse_origins{{
        {36, 36}, {36, 36}, {35, 36}, {34, 36}, {34, 36},
    }};
    for (const auto& origin : reverse_origins) {
        OL_CHECK(diagonal_result.kind == SceneStepKind::present);
        OL_CHECK(diagonal_result.wait_ticks == 2U);
        OL_CHECK(diagonal.view_origin_x() == origin[0]);
        OL_CHECK(diagonal.view_origin_y() == origin[1]);
        diagonal_result = diagonal.resume(SceneResponse::acknowledge);
    }
}

void check_event_picture_animation(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[53];
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 23);
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 24);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 53};

    constexpr std::array<std::int16_t, 4> expected_pictures{6342, 6344, 6346, 6348};
    constexpr std::array<std::uint64_t, 4> expected_hashes{
        0x0AB33641B9ED5D60ULL,
        0x555593B96AA27757ULL,
        0x54DC5D457C749844ULL,
        0x473E57A3E9AC9B4FULL,
    };
    auto result = session.begin_event(535, 4, 23, 24);
    for (std::size_t index = 0U; index < expected_pictures.size(); ++index) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 2U);
        for (const auto field : {
                 openlegend::model::SceneEventField::current_picture,
                 openlegend::model::SceneEventField::end_picture,
                 openlegend::model::SceneEventField::begin_picture}) {
            OL_CHECK(snapshot.event_value(53U, 3U, field).value_or(-1) ==
                     expected_pictures[index]);
        }
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 2005);

    auto player_snapshot = load_baseline(root);
    auto& player_metadata = player_snapshot.ranger.scenes[50];
    player_metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 23);
    player_metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 21);
    openlegend::random::LegacyRandom player_random{1U};
    openlegend::scene::SceneSession player_session{
        data_root, player_snapshot, player_random, 50};
    auto player_result = player_session.begin_event(20, 0, 23, 21);
    while (player_result.kind == SceneStepKind::dialogue) {
        player_result = player_session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(player_result.kind == SceneStepKind::present);
    OL_CHECK(player_result.wait_ticks == 1U);
    player_result = player_session.resume(SceneResponse::acknowledge);
    for (std::int16_t frame = 5994; frame <= 6012; frame += 2) {
        OL_CHECK(player_result.kind == SceneStepKind::present);
        OL_CHECK(player_result.wait_ticks == 2U);
        OL_CHECK(player_session.player_frame() == frame);
        player_result = player_session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(player_result.kind == SceneStepKind::fade_to_black);
    OL_CHECK(player_session.player_frame() == 6012);
}

void check_event_scripted_walk(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[39];
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 28);
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 24);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 39};

    auto result = session.begin_event(343, 12, 28, 24);
    for (int step = 0; step < 64 &&
                       !(result.kind == SceneStepKind::present && result.wait_ticks == 3U);
         ++step) {
        const auto resumable = result.kind == SceneStepKind::present ||
                               result.kind == SceneStepKind::dialogue ||
                               result.kind == SceneStepKind::fade_from_black ||
                               result.kind == SceneStepKind::fade_to_black;
        OL_CHECK(resumable);
        if (!resumable) {
            break;
        }
        result = session.resume(SceneResponse::acknowledge);
    }

    constexpr std::array<int, 5> expected_y{23, 22, 21, 20, 19};
    constexpr std::array<std::int16_t, 5> expected_pictures{5046, 5048, 5050, 5052, 5054};
    constexpr std::array<std::uint64_t, 5> expected_hashes{
        0x875FB7391E30B9B0ULL,
        0x74B973721D47C71CULL,
        0xC6F9A603AA1FC9F1ULL,
        0xBBE8CD264878BE18ULL,
        0xA97E21B948CFC40FULL,
    };
    for (std::size_t index = 0U; index < expected_y.size(); ++index) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 3U);
        OL_CHECK(session.scene_x() == 28);
        OL_CHECK(session.scene_y() == expected_y[index]);
        OL_CHECK(session.direction() == openlegend::scene::SceneDirection::down);
        OL_CHECK(session.player_frame() == expected_pictures[index]);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        const auto actual_hash = fnv1a64(framebuffer.pixels());
        if (actual_hash != expected_hashes[index]) {
            std::cerr << "script 343 frame " << index << ": actual 0x"
                      << std::hex << actual_hash << std::dec << '\n';
        }
        OL_CHECK(actual_hash == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 1248);
    OL_CHECK(session.player_frame() == 5044);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::sub_map_x) == 28);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::sub_map_y) == 19);

    auto blocked_snapshot = load_baseline(root);
    auto& blocked_metadata = blocked_snapshot.ranger.scenes[39];
    blocked_metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 28);
    blocked_metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 24);
    OL_CHECK(blocked_snapshot.set_scene_value(
        39U,
        openlegend::model::SceneLayer::building,
        22U * 64U + 28U,
        2));
    openlegend::random::LegacyRandom blocked_random{1U};
    openlegend::scene::SceneSession blocked{
        data_root, blocked_snapshot, blocked_random, 39};
    auto blocked_result = blocked.begin_event(343, 12, 28, 24);
    for (int step = 0; step < 64 &&
                       !(blocked_result.kind == SceneStepKind::present &&
                         blocked_result.wait_ticks == 3U);
         ++step) {
        blocked_result = blocked.resume(SceneResponse::acknowledge);
    }
    for (std::int16_t frame = 5046; frame <= 5054; frame += 2) {
        OL_CHECK(blocked_result.kind == SceneStepKind::present);
        OL_CHECK(blocked_result.wait_ticks == 3U);
        OL_CHECK(blocked.scene_x() == 28);
        OL_CHECK(blocked.scene_y() == 23);
        OL_CHECK(blocked.player_frame() == frame);
        blocked_result = blocked.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(blocked_result.kind == SceneStepKind::dialogue);
    OL_CHECK(blocked_result.talk_id == 1248);
    OL_CHECK(blocked.scene_y() == 23);
}

void check_event_dual_picture_animation(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[53];
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 23);
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 24);
    snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{49});
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 53};

    auto result = session.begin_event(534, 0, 23, 24);
    for (int step = 0; step < 512; ++step) {
        const auto first = snapshot.event_value(
            53U, 1U, openlegend::model::SceneEventField::current_picture).value_or(-1);
        const auto second = snapshot.event_value(
            53U, 2U, openlegend::model::SceneEventField::current_picture).value_or(-1);
        if (result.kind == SceneStepKind::present && result.wait_ticks == 2U &&
            first == 6486 && second == 6450 &&
            session.view_origin_x() == 12 && session.view_origin_y() == 9) {
            result = session.resume(SceneResponse::acknowledge);
            break;
        }
        if (result.kind == SceneStepKind::question) {
            result = session.resume(SceneResponse::yes);
        } else if (result.kind == SceneStepKind::battle) {
            result = session.resume(SceneResponse::battle_victory);
        } else if (result.kind == SceneStepKind::shop) {
            result = session.resume(SceneResponse::cancel);
        } else if (result.kind == SceneStepKind::dialogue ||
                   result.kind == SceneStepKind::notice ||
                   result.kind == SceneStepKind::present ||
                   result.kind == SceneStepKind::fade_from_black ||
                   result.kind == SceneStepKind::fade_to_black) {
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }

    constexpr std::array<std::uint64_t, 18> expected_hashes{
        0xF0453734925D7D4BULL,
        0x83284B4E46EFD8B3ULL,
        0xDF52440BFF8700F6ULL,
        0x24FBF6707AE2232CULL,
        0x8EF2CCE87521AF73ULL,
        0x57D2BDA3374E89FFULL,
        0x53CACA1140353B1BULL,
        0x98024E3D5C7001DDULL,
        0xEF0115242C04D81BULL,
        0x911AF846FF69DF3DULL,
        0x202532675B2DB0EEULL,
        0x0DA6B7809F0139EEULL,
        0x300F40F7E7D30F53ULL,
        0xEF9C6361C44BE45FULL,
        0xF76FAAC632BA21C5ULL,
        0xEDA8A5BC6E3A5A87ULL,
        0xBC143D9337E610C6ULL,
        0xF0453734925D7D4BULL,
    };
    for (std::size_t index = 0U; index < expected_hashes.size(); ++index) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 2U);
        const auto first = static_cast<std::int16_t>(6486 + index * 2U);
        const auto second = static_cast<std::int16_t>(6450 + index * 2U);
        for (const auto field : {
                 openlegend::model::SceneEventField::current_picture,
                 openlegend::model::SceneEventField::end_picture,
                 openlegend::model::SceneEventField::begin_picture}) {
            OL_CHECK(snapshot.event_value(53U, 1U, field).value_or(-1) == first);
            OL_CHECK(snapshot.event_value(53U, 2U, field).value_or(-1) == second);
        }
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
}

void check_event_three_statue_animation(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[14];
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 32);
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 15);
    snapshot.ranger.roles[0].set_word(openlegend::model::role_word::attack, 2000);
    snapshot.ranger.header.set_inventory(0U, openlegend::model::ItemId{106}, 1);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 14};

    auto result = session.begin_event(655, 3, 33, 15);
    for (int step = 0; step < 32; ++step) {
        if (result.kind == SceneStepKind::present && result.wait_ticks == 2U &&
            session.player_frame() == 7664) {
            break;
        }
        if (result.kind == SceneStepKind::dialogue ||
            result.kind == SceneStepKind::notice ||
            result.kind == SceneStepKind::present ||
            result.kind == SceneStepKind::fade_from_black ||
            result.kind == SceneStepKind::fade_to_black) {
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }

    constexpr std::array<std::uint64_t, 35> expected_hashes{
        0x310B7251C11479C4ULL,
        0x5DDA4D227403997BULL,
        0x5CE5831A41F6660EULL,
        0x591A01311B415D26ULL,
        0x7152955C3B8C242FULL,
        0x5CE5831A41F6660EULL,
        0xA6194A871EE14761ULL,
        0x1D768B6E4C5EEBBCULL,
        0x1DD6D05034E6DC66ULL,
        0x80251CB6068DBC95ULL,
        0x8954DA17BAECA653ULL,
        0x5B0012E1172B4FB9ULL,
        0x7D434F000AD224BDULL,
        0x8B2882D4EC890A7DULL,
        0xD050BFCE553E4DB5ULL,
        0x0D2441DD86F179FEULL,
        0x5DDA4D227403997BULL,
        0x5DDA4D227403997BULL,
        0x664EBCE027362C73ULL,
        0x5ED2A1D842AA5AD5ULL,
        0x8B2BE8610C3745C2ULL,
        0xE895949267CE417FULL,
        0x96E75A216A8D511FULL,
        0xA4CE4369D5FF4E9DULL,
        0x0799CED98322657AULL,
        0xF7CED4E019B01ECDULL,
        0x6A31E5C28A92952BULL,
        0x7A4BC6B60B7C2ED0ULL,
        0xB5E2CA7C45A1F75CULL,
        0xE786D0E064D8264AULL,
        0xEB527D3369847414ULL,
        0xF2AF878C6437BDD5ULL,
        0xFF9227DA5FA68699ULL,
        0xA1B2AF3219F0C4CBULL,
        0x22E67F4D16E05DDCULL,
    };
    for (std::size_t index = 0U; index < expected_hashes.size(); ++index) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 2U);
        const auto player_picture = index < 6U
                                        ? static_cast<std::int16_t>(7664 + index * 2U)
                                        : static_cast<std::int16_t>(
                                              std::min<std::size_t>(7688, 7676 + (index - 6U) * 2U));
        OL_CHECK(session.player_frame() == player_picture);
        std::array<std::int16_t, 3> event_pictures{7690, 7748, 7806};
        if (index >= 6U) {
            const auto offset = static_cast<std::int16_t>((index - 6U) * 2U);
            event_pictures = {
                static_cast<std::int16_t>(7690 + offset),
                static_cast<std::int16_t>(7748 + offset),
                static_cast<std::int16_t>(7806 + offset),
            };
        }
        for (std::size_t event = 0U; event < event_pictures.size(); ++event) {
            for (const auto field : {
                     openlegend::model::SceneEventField::current_picture,
                     openlegend::model::SceneEventField::end_picture,
                     openlegend::model::SceneEventField::begin_picture}) {
                OL_CHECK(snapshot.event_value(14U, event + 2U, field).value_or(-1) ==
                         event_pictures[event]);
            }
        }
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(session.player_frame() == 7688);
}

void check_event_ending_prelude_animation(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[83];
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 22);
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 41);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 83};

    auto result = session.begin_event(1017, 1, 23, 41);
    for (int step = 0; step < 8; ++step) {
        if (result.kind == SceneStepKind::present && result.wait_ticks == 2U &&
            session.player_frame() == -86) {
            break;
        }
        if (result.kind == SceneStepKind::dialogue || result.kind == SceneStepKind::present) {
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }

    constexpr std::array<std::uint64_t, 38> expected_hashes{
        0x349A815CD51123D6ULL,
        0x66510181CBABFD3EULL,
        0xD46AC66F3ACCA7C9ULL,
        0xAC466D89E1553ABBULL,
        0x1AB55396CF5BD6AEULL,
        0xC3E96A8676826496ULL,
        0x930AE0FE4AD48AFAULL,
        0x5B2A76F736F60484ULL,
        0x9ECEF333B94E100BULL,
        0x8BB8A21E953DF4F7ULL,
        0x29194BCF02693ED6ULL,
        0x816884637FF912BBULL,
        0x471F1388A19B9C0DULL,
        0xDA0BFD78BE881F0AULL,
        0x0201B4ED8B45C414ULL,
        0xC16EEE23D5BCEE6BULL,
        0xD4780BF2C01EEFD5ULL,
        0xCBF0E49D13455C57ULL,
        0xBE58F1C9E6D325BCULL,
        0x26ADDABC5B2B2E4BULL,
        0x32A4D581F536B2DFULL,
        0xA75E9B7070885A1CULL,
        0x3FAF09F7F7249332ULL,
        0x7C18BE436FE3DE66ULL,
        0xAEE105D1FB035EE6ULL,
        0x3BADB7ECEA360037ULL,
        0xCA17F1C632059426ULL,
        0xAB0393813638F5D4ULL,
        0xACA65670D582D328ULL,
        0x453F15BC68682123ULL,
        0x4622259D280E3CFAULL,
        0x9B56788F7E438258ULL,
        0x784023546501B76DULL,
        0xD4719EC49245B4F5ULL,
        0x4D18FDAC043F3B06ULL,
        0xCD0B344D7EA17A1DULL,
        0x1FFECC0EB4ACC776ULL,
        0x677CE26188BA524AULL,
    };
    for (std::size_t index = 0U; index < expected_hashes.size(); ++index) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 2U);
        OL_CHECK(session.player_frame() == -86);
        const std::array<std::int16_t, 2> pictures{
            static_cast<std::int16_t>(8054 + index * 2U),
            static_cast<std::int16_t>(8130 + index * 2U),
        };
        for (std::size_t event = 0U; event < pictures.size(); ++event) {
            for (const auto field : {
                     openlegend::model::SceneEventField::current_picture,
                     openlegend::model::SceneEventField::end_picture,
                     openlegend::model::SceneEventField::begin_picture}) {
                OL_CHECK(snapshot.event_value(83U, event, field).value_or(-1) ==
                         pictures[event]);
            }
        }
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);

    openlegend::render::IndexedFramebuffer ending_framebuffer;
    const auto ending_hash = [&]() {
        OL_CHECK(session.render(ending_framebuffer));
        return fnv1a64(ending_framebuffer.pixels());
    };
    OL_CHECK(ending_hash() == expected_hashes.back());

    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_from_black);
    OL_CHECK(result.wait_ticks == 51U);
    OL_CHECK(ending_hash() == 0x20420DB943FD8DEFULL);

    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_from_black);
    OL_CHECK(ending_hash() == 0xDD14FCC6528CAB25ULL);
    result = session.resume(SceneResponse::acknowledge);

    constexpr std::array<std::pair<std::size_t, std::uint64_t>, 6> word_samples{
        std::pair{0U, 0xDD14FCC6528CAB25ULL},
        std::pair{50U, 0xFA8B7F39E4E2A9EFULL},
        std::pair{150U, 0xDCD51CEC16CCFBCFULL},
        std::pair{250U, 0x7FDB3C4BE6FB067DULL},
        std::pair{350U, 0xC04F8C2FFE28965DULL},
        std::pair{442U, 0xDD14FCC6528CAB25ULL},
    };
    std::size_t word_sample = 0U;
    for (std::size_t frame = 0U; frame < 443U; ++frame) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 3U);
        if (word_sample < word_samples.size() && word_samples[word_sample].first == frame) {
            OL_CHECK(ending_hash() == word_samples[word_sample].second);
            ++word_sample;
        }
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(word_sample == word_samples.size());
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);

    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_from_black);
    OL_CHECK(ending_hash() == 0xA2186A3321F0153AULL);
    result = session.resume(SceneResponse::acknowledge);
    for (std::size_t frame = 1U; frame < 221U; ++frame) {
        OL_CHECK(result.kind == SceneStepKind::present);
        if (frame == 1U) {
            OL_CHECK(ending_hash() == 0x68BF029A91B5D73CULL);
        } else if (frame == 220U) {
            OL_CHECK(ending_hash() == 0x42C2240F8D7700C7ULL);
        }
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::wait_key);

    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_from_black);
    OL_CHECK(ending_hash() == 0xDD14FCC6528CAB25ULL);
    result = session.resume(SceneResponse::acknowledge);

    constexpr std::array<std::pair<std::size_t, std::uint64_t>, 7> credit_samples{
        std::pair{0U, 0xDD14FCC6528CAB25ULL},
        std::pair{100U, 0xFC2E6273806B7A87ULL},
        std::pair{500U, 0x45668C9FB96CBE4DULL},
        std::pair{1000U, 0x534970B9F979F4C7ULL},
        std::pair{2000U, 0x93036BE96FD45365ULL},
        std::pair{3000U, 0xC5D6F84BE1DC5847ULL},
        std::pair{3243U, 0x206B76FCA6006E95ULL},
    };
    std::size_t credit_sample = 0U;
    for (std::size_t frame = 0U; frame < 3'244U; ++frame) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 3U);
        if (credit_sample < credit_samples.size() &&
            credit_samples[credit_sample].first == frame) {
            OL_CHECK(ending_hash() == credit_samples[credit_sample].second);
            ++credit_sample;
        }
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(credit_sample == credit_samples.size());
    OL_CHECK(result.kind == SceneStepKind::wait_key);
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::quit);
    OL_CHECK(result.ending_complete);
}

void check_event_role_sexual_and_audio(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    const auto run_role_script = [&data_root](openlegend::model::GameSnapshot& snapshot) {
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 53};
        auto result = session.begin_event(289, 0, 0, 0);
        for (int step = 0; step < 128 && result.kind != SceneStepKind::stay; ++step) {
            if (result.kind == SceneStepKind::dialogue ||
                result.kind == SceneStepKind::notice ||
                result.kind == SceneStepKind::present ||
                result.kind == SceneStepKind::fade_from_black ||
                result.kind == SceneStepKind::fade_to_black) {
                result = session.resume(SceneResponse::acknowledge);
            } else {
                break;
            }
        }
        return result.kind;
    };

    auto role_snapshot = load_baseline(root);
    auto& role = role_snapshot.ranger.roles[36];
    role.set_word(openlegend::model::role_word::sexual, 0);
    role.set_word(openlegend::model::role_word::taking_item_begin, 78);
    role.set_word(openlegend::model::role_word::taking_item_count_begin, -1);
    OL_CHECK(run_role_script(role_snapshot) == SceneStepKind::stay);
    OL_CHECK(role.word(openlegend::model::role_word::sexual) == 2);
    OL_CHECK(role.word(openlegend::model::role_word::magic_id_begin) == 60);
    OL_CHECK(role.word(openlegend::model::role_word::magic_level_begin) == 100);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == 78);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin) == 0);

    auto exact_empty_snapshot = load_baseline(root);
    auto& exact_empty_role = exact_empty_snapshot.ranger.roles[36];
    exact_empty_role.set_word(openlegend::model::role_word::taking_item_begin, -2);
    exact_empty_role.set_word(openlegend::model::role_word::taking_item_begin + 1U, -1);
    OL_CHECK(run_role_script(exact_empty_snapshot) == SceneStepKind::stay);
    OL_CHECK(exact_empty_role.word(openlegend::model::role_word::taking_item_begin) == -2);
    OL_CHECK(exact_empty_role.word(openlegend::model::role_word::taking_item_begin + 1U) == 78);
    OL_CHECK(exact_empty_role.word(openlegend::model::role_word::taking_item_count_begin + 1U) == 1);

    auto wave_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom wave_random{1U};
    openlegend::scene::SceneSession wave_session{
        data_root, wave_snapshot, wave_random, 7};
    const std::vector<openlegend::scene::SceneAudioCommand> expected_entrance_audio{
        {openlegend::scene::SceneAudioCommand::Kind::music, 12}};
    OL_CHECK(wave_session.take_audio_commands() == expected_entrance_audio);
    const auto wave_result = wave_session.begin_event(389, 0, 25, 48);
    OL_CHECK(wave_result.kind == SceneStepKind::dialogue);
    OL_CHECK(wave_result.talk_id == 1255);
    OL_CHECK((wave_session.take_audio_commands() ==
              std::vector<openlegend::scene::SceneAudioCommand>{
                  {openlegend::scene::SceneAudioCommand::Kind::wave, 22}}));

    auto music_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom music_random{1U};
    openlegend::scene::SceneSession music_session{
        data_root, music_snapshot, music_random, 53};
    OL_CHECK(music_session.take_audio_commands().empty());
    auto music_result = music_session.begin_event(531, 0, 0, 0, 183);
    for (int step = 0; step < 128 && music_result.kind != SceneStepKind::fade_to_black; ++step) {
        if (music_result.kind == SceneStepKind::dialogue ||
            music_result.kind == SceneStepKind::notice ||
            music_result.kind == SceneStepKind::present) {
            music_result = music_session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(music_result.kind == SceneStepKind::fade_to_black);
    music_result = music_session.resume(SceneResponse::acknowledge);
    OL_CHECK(music_result.kind == SceneStepKind::fade_from_black);
    OL_CHECK((music_session.take_audio_commands() ==
              std::vector<openlegend::scene::SceneAudioCommand>{
                  {openlegend::scene::SceneAudioCommand::Kind::music, 9, true}}));
}

void check_event_shop_helpers(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    const auto open_shop = [](openlegend::scene::SceneSession& session) {
        auto result = session.begin_event(938, 0, 0, 0);
        for (int step = 0; step < 16 && result.kind == SceneStepKind::dialogue; ++step) {
            result = session.resume(SceneResponse::acknowledge);
        }
        return result;
    };
    struct ShopCase {
        std::int16_t scene;
        std::int16_t shop;
        std::vector<std::int16_t> close_events;
    };
    const std::array<ShopCase, 6> cases{
        ShopCase{0, 0, {}},
        {1, 0, {17, 18}},
        {3, 1, {15, 16}},
        {40, 2, {21, 22}},
        {60, 3, {17, 18}},
        {61, 4, {10, 11, 12}},
    };
    for (const auto& shop_case : cases) {
        auto snapshot = load_baseline(root);
        for (const auto event : shop_case.close_events) {
            static_cast<void>(snapshot.set_event_value(
                shop_case.scene, event, openlegend::model::SceneEventField::event_3, -1));
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{
            data_root, snapshot, random, shop_case.scene};
        auto result = open_shop(session);
        OL_CHECK(result.kind == SceneStepKind::shop);
        OL_CHECK(result.shop_id == shop_case.shop);
        result = session.resume(SceneResponse::cancel);
        OL_CHECK(result.kind == SceneStepKind::stay);
        for (const auto event : shop_case.close_events) {
            OL_CHECK(snapshot.event_value(
                         shop_case.scene, event,
                         openlegend::model::SceneEventField::event_3).value_or(-1) == 939);
        }
    }

    auto purchase_snapshot = load_baseline(root);
    auto& shop = purchase_snapshot.ranger.shops[1];
    shop.set_word(openlegend::model::shop_word::item_id_begin, 42);
    shop.set_word(openlegend::model::shop_word::total_begin, 2);
    shop.set_word(openlegend::model::shop_word::price_begin, 7);
    purchase_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{174}, 10);
    static_cast<void>(purchase_snapshot.set_event_value(
        3U, 15U, openlegend::model::SceneEventField::event_3, -1));
    static_cast<void>(purchase_snapshot.set_event_value(
        3U, 16U, openlegend::model::SceneEventField::event_3, -1));
    openlegend::random::LegacyRandom purchase_random{1U};
    openlegend::scene::SceneSession purchase_session{
        data_root, purchase_snapshot, purchase_random, 3};
    auto purchase = open_shop(purchase_session);
    OL_CHECK(purchase.kind == SceneStepKind::shop);
    purchase = purchase_session.resume(SceneResponse::yes, 0);
    OL_CHECK(purchase.kind == SceneStepKind::dialogue);
    OL_CHECK(purchase.talk_id == 2976);
    OL_CHECK(purchase_snapshot.event_value(
                 3U, 15U, openlegend::model::SceneEventField::event_3).value_or(-1) == -1);
    for (int step = 0; step < 16 && purchase.kind == SceneStepKind::dialogue; ++step) {
        purchase = purchase_session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(purchase.kind == SceneStepKind::stay);
    OL_CHECK(shop.word(openlegend::model::shop_word::total_begin) == 1);
    bool found_currency = false;
    bool found_item = false;
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        found_currency = found_currency ||
                         (purchase_snapshot.ranger.header.inventory_item(slot).value == 174 &&
                          purchase_snapshot.ranger.header.inventory_count(slot) == 3);
        found_item = found_item ||
                     (purchase_snapshot.ranger.header.inventory_item(slot).value == 42 &&
                      purchase_snapshot.ranger.header.inventory_count(slot) == 1);
    }
    OL_CHECK(found_currency);
    OL_CHECK(found_item);
    OL_CHECK(purchase_snapshot.event_value(
                 3U, 15U, openlegend::model::SceneEventField::event_3).value_or(-1) == 939);
    OL_CHECK(purchase_snapshot.event_value(
                 3U, 16U, openlegend::model::SceneEventField::event_3).value_or(-1) == 939);

    auto split_money_snapshot = load_baseline(root);
    auto& split_money_shop = split_money_snapshot.ranger.shops[1];
    split_money_shop.set_word(openlegend::model::shop_word::item_id_begin, 42);
    split_money_shop.set_word(openlegend::model::shop_word::total_begin, 2);
    split_money_shop.set_word(openlegend::model::shop_word::price_begin, 7);
    split_money_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{174}, 5);
    split_money_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{174}, 100);
    const auto item_count_before = inventory_count(split_money_snapshot.ranger, 42);
    openlegend::random::LegacyRandom split_money_random{1U};
    openlegend::scene::SceneSession split_money_session{
        data_root, split_money_snapshot, split_money_random, 3};
    auto split_purchase = open_shop(split_money_session);
    OL_CHECK(split_purchase.kind == SceneStepKind::shop);
    split_purchase = split_money_session.resume(SceneResponse::yes, 0);
    OL_CHECK(split_purchase.kind == SceneStepKind::dialogue);
    OL_CHECK(split_purchase.talk_id == 2975);
    OL_CHECK(split_money_shop.word(openlegend::model::shop_word::total_begin) == 2);
    OL_CHECK(split_money_snapshot.ranger.header.inventory_count(0U) == 5);
    OL_CHECK(split_money_snapshot.ranger.header.inventory_count(1U) == 100);
    OL_CHECK(inventory_count(split_money_snapshot.ranger, 42) == item_count_before);

    const std::array<std::pair<std::int16_t, std::vector<std::int16_t>>, 6> hide_cases{
        std::pair<std::int16_t, std::vector<std::int16_t>>{0, {}},
        {1, {16, 17, 18}},
        {3, {14, 15, 16}},
        {40, {20, 21, 22}},
        {60, {16, 17, 18}},
        {61, {9, 10, 11, 12}},
    };
    for (const auto& [scene, hidden_events] : hide_cases) {
        auto snapshot = load_baseline(root);
        for (const auto event : hidden_events) {
            for (std::size_t field = 0U; field < 8U; ++field) {
                static_cast<void>(snapshot.set_event_value(
                    scene, event,
                    static_cast<openlegend::model::SceneEventField>(field), 777));
            }
        }
        if (scene == 0) {
            for (std::size_t field = 0U; field < 8U; ++field) {
                static_cast<void>(snapshot.set_event_value(
                    0U, 16U,
                    static_cast<openlegend::model::SceneEventField>(field), 777));
            }
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, scene};
        OL_CHECK(session.begin_event(939, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(random.state() == 0x41C67EA6U);
        for (const auto event : hidden_events) {
            if (scene == 60 && event == 16) {
                continue;
            }
            const std::array<std::int16_t, 8> expected{0, 0, -1, -1, -1, -1, -1, -1};
            for (std::size_t field = 0U; field < expected.size(); ++field) {
                OL_CHECK(snapshot.event_value(
                             scene, event,
                             static_cast<openlegend::model::SceneEventField>(field)).value_or(-2) ==
                         expected[field]);
            }
        }
        if (scene == 0) {
            for (std::size_t field = 0U; field < 8U; ++field) {
                OL_CHECK(snapshot.event_value(
                             0U, 16U,
                             static_cast<openlegend::model::SceneEventField>(field)).value_or(-2) == 777);
            }
        }
        const std::array<std::int16_t, 8> activated{
            1, 1, 938, -1, -1, 8256, 8256, 8256};
        for (std::size_t field = 0U; field < activated.size(); ++field) {
            OL_CHECK(snapshot.event_value(
                         60U, 16U,
                         static_cast<openlegend::model::SceneEventField>(field)).value_or(-2) ==
                     activated[field]);
        }
    }
}

void check_event_presence_and_party_tail_conditions(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto item_snapshot = load_baseline(root);
    item_snapshot.ranger.header.set_inventory(0U, openlegend::model::ItemId{173}, 0);
    openlegend::random::LegacyRandom item_random{1U};
    openlegend::scene::SceneSession item_session{
        data_root, item_snapshot, item_random, 70};
    auto result = item_session.begin_event(37, 0, 0, 0);
    OL_CHECK(result.kind == SceneStepKind::stay);
    item_snapshot.ranger.header.set_inventory(0U, openlegend::model::ItemId{-1}, 0);
    result = item_session.begin_event(37, 0, 0, 0);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 139);

    const auto party_tail_dialogue = [&data_root, &root](const bool tail_positive) {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kTeamMemberCount; ++index) {
            snapshot.ranger.header.set_team_member(
                index, openlegend::model::CharacterId{
                           index == 2U
                               ? std::int16_t{-1}
                               : static_cast<std::int16_t>(index + 1U)});
        }
        snapshot.ranger.header.set_team_member(
            openlegend::model::kTeamMemberCount - 1U,
            openlegend::model::CharacterId{tail_positive ? std::int16_t{9} : std::int16_t{0}});
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        auto step = session.begin_event(11, 0, 0, 0);
        OL_CHECK(step.kind == SceneStepKind::dialogue);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::present);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::question);
        step = session.resume(SceneResponse::yes);
        OL_CHECK(step.kind == SceneStepKind::dialogue);
        OL_CHECK(step.talk_id == 29);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::present);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::dialogue);
        return step.talk_id;
    };
    OL_CHECK(party_tail_dialogue(false) == 30);
    OL_CHECK(party_tail_dialogue(true) == 175);

    auto books_snapshot = load_baseline(root);
    for (std::size_t index = 0U; index < 5U; ++index) {
        books_snapshot.ranger.header.set_inventory(
            index, openlegend::model::ItemId{static_cast<std::int16_t>(138 + index)}, 0);
    }
    openlegend::random::LegacyRandom books_random{1U};
    openlegend::scene::SceneSession books_session{
        data_root, books_snapshot, books_random, 70};
    result = books_session.begin_event(676, 0, 0, 0);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 2482);
    books_snapshot.ranger.header.set_inventory(4U, openlegend::model::ItemId{-1}, 0);
    result = books_session.begin_event(676, 0, 0, 0);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 2481);
}

void check_event_role_stat_conditions(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    const auto reaches_success_dialogue = [&data_root, &root](const std::int16_t morality) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.header.set_inventory(0U, openlegend::model::ItemId{110}, 0);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, morality);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        auto result = session.begin_event(636, 0, 0, 0);
        for (int step = 0; step < 64; ++step) {
            if (result.kind == SceneStepKind::dialogue && result.talk_id == 2383) {
                return true;
            }
            if (result.kind == SceneStepKind::battle || result.kind == SceneStepKind::stay ||
                result.kind == SceneStepKind::quit) {
                return false;
            }
            if (result.kind == SceneStepKind::dialogue ||
                result.kind == SceneStepKind::notice ||
                result.kind == SceneStepKind::present ||
                result.kind == SceneStepKind::fade_from_black ||
                result.kind == SceneStepKind::fade_to_black) {
                result = session.resume(SceneResponse::acknowledge);
            } else {
                return false;
            }
        }
        return false;
    };
    OL_CHECK(!reaches_success_dialogue(79));
    OL_CHECK(reaches_success_dialogue(80));
    OL_CHECK(reaches_success_dialogue(100));
    OL_CHECK(!reaches_success_dialogue(101));
}

void check_event_inventory_condition_edge_cases(const std::filesystem::path& root) {
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto money_snapshot = load_baseline(root);
    money_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{174}, 5);
    money_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{174}, 10);
    openlegend::random::LegacyRandom money_random{1U};
    openlegend::scene::SceneSession money_session{
        data_root, money_snapshot, money_random, 70};
    const auto split_money = money_session.begin_event(234, 0, 0, 0, 174);
    OL_CHECK(split_money.kind == SceneStepKind::dialogue);
    OL_CHECK(split_money.talk_id == 790);

    money_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{174}, 10);
    money_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{174}, 20);
    const auto first_money = money_session.begin_event(234, 0, 0, 0, 174);
    OL_CHECK(first_money.kind == SceneStepKind::dialogue);
    OL_CHECK(first_money.talk_id == 791);
    OL_CHECK(money_snapshot.ranger.header.inventory_item(0U).value == 174);
    OL_CHECK(money_snapshot.ranger.header.inventory_count(0U) == 20);

    auto duplicate_snapshot = load_baseline(root);
    duplicate_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{109}, 2);
    duplicate_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{109}, 3);
    duplicate_snapshot.ranger.header.set_inventory(
        2U, openlegend::model::ItemId{88}, 4);
    openlegend::random::LegacyRandom duplicate_random{1U};
    openlegend::scene::SceneSession duplicate_session{
        data_root, duplicate_snapshot, duplicate_random, 70};
    const auto duplicate_add = duplicate_session.begin_event(149, 0, 0, 0);
    OL_CHECK(duplicate_add.kind == SceneStepKind::notice);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_item(0U).value == 109);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_count(0U) == 3);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_item(1U).value == 109);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_count(1U) == 4);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_item(2U).value == 88);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_count(2U) == 4);

    auto residual_snapshot = load_baseline(root);
    for (std::size_t index = 0U; index < 5U; ++index) {
        residual_snapshot.ranger.header.set_inventory(
            index,
            openlegend::model::ItemId{static_cast<std::int16_t>(50 + index)},
            1);
    }
    residual_snapshot.ranger.header.set_inventory(
        5U, openlegend::model::ItemId{-1}, 9);
    openlegend::random::LegacyRandom residual_random{1U};
    openlegend::scene::SceneSession residual_session{
        data_root, residual_snapshot, residual_random, 70};
    const auto residual_add = residual_session.begin_event(497, 0, 0, 0);
    OL_CHECK(residual_add.kind == SceneStepKind::notice);
    OL_CHECK(residual_snapshot.ranger.header.inventory_item(5U).value == 57);
    OL_CHECK(residual_snapshot.ranger.header.inventory_count(5U) == 10);

    auto presence_snapshot = load_baseline(root);
    presence_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{110}, 0);
    openlegend::random::LegacyRandom presence_random{1U};
    openlegend::scene::SceneSession presence_session{
        data_root, presence_snapshot, presence_random, 70};
    const auto zero_count = presence_session.begin_event(636, 0, 0, 0);
    OL_CHECK(zero_count.kind == SceneStepKind::dialogue);
    OL_CHECK(zero_count.talk_id == 2381);

    presence_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{-1}, 0);
    const auto absent = presence_session.begin_event(636, 0, 0, 0);
    OL_CHECK(absent.kind == SceneStepKind::dialogue);
    OL_CHECK(absent.talk_id == 2380);
}

void check_event_all_book_pictures_condition(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto make_snapshot = [&root]() {
        auto snapshot = load_baseline(root);
        for (std::size_t event = 11U; event < 25U; ++event) {
            static_cast<void>(snapshot.set_event_value(
                70U, event, openlegend::model::SceneEventField::current_picture, 4664));
            static_cast<void>(snapshot.set_event_value(
                70U, event, openlegend::model::SceneEventField::end_picture, 100));
            static_cast<void>(snapshot.set_event_value(
                70U, event, openlegend::model::SceneEventField::begin_picture, 100));
        }
        return snapshot;
    };

    auto miss_snapshot = make_snapshot();
    static_cast<void>(miss_snapshot.set_event_value(
        70U, 12U, openlegend::model::SceneEventField::current_picture, 100));
    static_cast<void>(miss_snapshot.set_event_value(
        70U, 12U, openlegend::model::SceneEventField::end_picture, 4664));
    static_cast<void>(miss_snapshot.set_event_value(
        70U, 12U, openlegend::model::SceneEventField::begin_picture, 4664));
    openlegend::random::LegacyRandom miss_random{1U};
    openlegend::scene::SceneSession miss_session{
        data_root, miss_snapshot, miss_random, 70};
    auto miss = miss_session.begin_event(1001, 11, 0, 0, 144);
    OL_CHECK(miss.kind == SceneStepKind::present);
    miss = miss_session.resume(SceneResponse::acknowledge);
    OL_CHECK(miss.kind == SceneStepKind::stay);

    auto match_snapshot = make_snapshot();
    openlegend::random::LegacyRandom match_random{1U};
    openlegend::scene::SceneSession match_session{
        data_root, match_snapshot, match_random, 70};
    auto match = match_session.begin_event(1001, 11, 0, 0, 144);
    OL_CHECK(match.kind == SceneStepKind::present);
    match = match_session.resume(SceneResponse::acknowledge);
    OL_CHECK(match.kind == SceneStepKind::dialogue);
    OL_CHECK(match.talk_id == 2914);
    OL_CHECK((match_session.take_audio_commands() ==
              std::vector<openlegend::scene::SceneAudioCommand>{
                  {openlegend::scene::SceneAudioCommand::Kind::wave, 23}}));
}

void check_event_current_picture_condition(const std::filesystem::path& root) {
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto local_snapshot = load_baseline(root);
    local_snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{53});
    static_cast<void>(local_snapshot.set_event_value(
        52U, 2U, openlegend::model::SceneEventField::current_picture, 100));
    static_cast<void>(local_snapshot.set_event_value(
        52U, 2U, openlegend::model::SceneEventField::end_picture, 6298));
    static_cast<void>(local_snapshot.set_event_value(
        52U, 2U, openlegend::model::SceneEventField::begin_picture, 6298));
    openlegend::random::LegacyRandom local_random{1U};
    openlegend::scene::SceneSession local_session{
        data_root, local_snapshot, local_random, 52};
    OL_CHECK(local_session.begin_event(491, 0, 0, 0).kind == SceneStepKind::stay);

    static_cast<void>(local_snapshot.set_event_value(
        52U, 2U, openlegend::model::SceneEventField::current_picture, 6298));
    static_cast<void>(local_snapshot.set_event_value(
        52U, 2U, openlegend::model::SceneEventField::end_picture, 100));
    static_cast<void>(local_snapshot.set_event_value(
        52U, 2U, openlegend::model::SceneEventField::begin_picture, 100));
    const auto local_match = local_session.begin_event(491, 0, 0, 0);
    OL_CHECK(local_match.kind == SceneStepKind::dialogue);
    OL_CHECK(local_match.talk_id == 1742);

    auto external_snapshot = load_baseline(root);
    static_cast<void>(external_snapshot.set_event_value(
        80U, 1U, openlegend::model::SceneEventField::current_picture, 100));
    static_cast<void>(external_snapshot.set_event_value(
        80U, 1U, openlegend::model::SceneEventField::end_picture, 6068));
    static_cast<void>(external_snapshot.set_event_value(
        80U, 1U, openlegend::model::SceneEventField::begin_picture, 6068));
    openlegend::random::LegacyRandom external_random{1U};
    openlegend::scene::SceneSession external_session{
        data_root, external_snapshot, external_random, 70};
    const auto external_miss = external_session.begin_event(990, 0, 0, 0);
    OL_CHECK(external_miss.kind == SceneStepKind::dialogue);
    OL_CHECK(external_miss.talk_id == 2807);

    static_cast<void>(external_snapshot.set_event_value(
        80U, 1U, openlegend::model::SceneEventField::current_picture, 6068));
    static_cast<void>(external_snapshot.set_event_value(
        80U, 1U, openlegend::model::SceneEventField::end_picture, 100));
    static_cast<void>(external_snapshot.set_event_value(
        80U, 1U, openlegend::model::SceneEventField::begin_picture, 100));
    const auto external_match = external_session.begin_event(990, 0, 0, 0);
    OL_CHECK(external_match.kind == SceneStepKind::dialogue);
    OL_CHECK(external_match.talk_id == 2968);
}

void check_event_tournament_trial(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    constexpr std::array<std::int16_t, 15> expected_battles{
        104, 106, 105, 109, 113, 108, 117, 114, 115, 125, 120, 123, 129, 126, 128};

    auto snapshot = load_baseline(root);
    auto& metadata = snapshot.ranger.scenes[25];
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 33);
    metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 26);
    auto& role = snapshot.ranger.roles[0];
    role.set_word(openlegend::model::role_word::hurt, 49);
    role.set_word(openlegend::model::role_word::poison, 0);
    role.set_word(openlegend::model::role_word::hp, 1);
    role.set_word(openlegend::model::role_word::maximum_hp, 100);
    role.set_word(openlegend::model::role_word::mp, 2);
    role.set_word(openlegend::model::role_word::maximum_mp, 90);
    role.set_word(openlegend::model::role_word::physical_power, 3);
    std::array<std::int16_t, 49> delays{};
    std::array<std::pair<std::int16_t, std::int16_t>, 49> coordinates{};
    for (std::size_t index = 0U; index < delays.size(); ++index) {
        const auto event = index + 24U;
        delays[index] = snapshot.event_value(
            25U, event, openlegend::model::SceneEventField::picture_delay).value_or(-1);
        coordinates[index] = {
            snapshot.event_value(25U, event, openlegend::model::SceneEventField::x).value_or(-1),
            snapshot.event_value(25U, event, openlegend::model::SceneEventField::y).value_or(-1),
        };
    }

    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 25};
    auto result = session.begin_event(936, 72, 33, 26);
    std::size_t battle_index = 0U;
    int interround_holds = 0;
    SceneStepKind previous_kind = SceneStepKind::stay;
    std::int16_t last_talk_id = -1;
    for (int step = 0; step < 4096 && result.kind != SceneStepKind::stay; ++step) {
        if (result.kind == SceneStepKind::dialogue) {
            last_talk_id = result.talk_id;
            previous_kind = result.kind;
            result = session.resume(SceneResponse::acknowledge);
        } else if (result.kind == SceneStepKind::battle) {
            OL_CHECK(battle_index < expected_battles.size());
            OL_CHECK(result.battle_id == expected_battles[battle_index]);
            OL_CHECK(last_talk_id == result.battle_id + 2752);
            OL_CHECK(previous_kind == SceneStepKind::present);
            ++battle_index;
            previous_kind = result.kind;
            result = session.resume(SceneResponse::battle_victory);
        } else if (result.kind == SceneStepKind::fade_to_black) {
            if (result.wait_ticks == 9U) {
                ++interround_holds;
            }
            previous_kind = result.kind;
            result = session.resume(SceneResponse::acknowledge);
        } else if (result.kind == SceneStepKind::notice ||
                   result.kind == SceneStepKind::present ||
                   result.kind == SceneStepKind::fade_from_black) {
            previous_kind = result.kind;
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(battle_index == expected_battles.size());
    OL_CHECK(interround_holds == 4);
    OL_CHECK(random.state() == 0xE95678E2U);
    OL_CHECK(role.word(openlegend::model::role_word::hurt) == 0);
    OL_CHECK(role.word(openlegend::model::role_word::hp) == 100);
    OL_CHECK(role.word(openlegend::model::role_word::mp) == 90);
    OL_CHECK(role.word(openlegend::model::role_word::physical_power) == 100);
    for (std::size_t index = 0U; index < delays.size(); ++index) {
        const auto event = index + 24U;
        const std::array<std::int16_t, 8> expected{0, 0, -1, -1, -1, -1, -1, -1};
        for (std::size_t field = 0U; field < expected.size(); ++field) {
            OL_CHECK(snapshot.event_value(
                         25U, event,
                         static_cast<openlegend::model::SceneEventField>(field)).value_or(-2) ==
                     expected[field]);
        }
        OL_CHECK(snapshot.event_value(
                     25U, event, openlegend::model::SceneEventField::picture_delay).value_or(-1) ==
                 delays[index]);
        OL_CHECK(snapshot.event_value(
                     25U, event, openlegend::model::SceneEventField::x).value_or(-1) ==
                 coordinates[index].first);
        OL_CHECK(snapshot.event_value(
                     25U, event, openlegend::model::SceneEventField::y).value_or(-1) ==
                 coordinates[index].second);
    }
    bool found_reward = false;
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        found_reward = found_reward ||
                       (snapshot.ranger.header.inventory_item(slot).value == 143 &&
                        snapshot.ranger.header.inventory_count(slot) == 1);
    }
    OL_CHECK(found_reward);

    auto defeat_snapshot = load_baseline(root);
    defeat_snapshot.ranger.scenes[25].set_word(
        openlegend::model::scene_metadata_word::entrance_x, 33);
    defeat_snapshot.ranger.scenes[25].set_word(
        openlegend::model::scene_metadata_word::entrance_y, 26);
    openlegend::random::LegacyRandom defeat_random{1U};
    openlegend::scene::SceneSession defeat_session{
        data_root, defeat_snapshot, defeat_random, 25, false,
        openlegend::scene::SceneDate{1996, 1, 1}};
    auto defeat = defeat_session.begin_event(936, 72, 33, 26);
    for (int step = 0; step < 512 && defeat.kind != SceneStepKind::battle; ++step) {
        if (defeat.kind == SceneStepKind::dialogue ||
            defeat.kind == SceneStepKind::present ||
            defeat.kind == SceneStepKind::fade_from_black ||
            defeat.kind == SceneStepKind::fade_to_black) {
            defeat = defeat_session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(defeat.kind == SceneStepKind::battle);
    OL_CHECK(defeat.battle_id == 104);
    defeat = defeat_session.resume(SceneResponse::battle_defeat);
    OL_CHECK(defeat.kind == SceneStepKind::fade_from_black);
    openlegend::render::IndexedFramebuffer death_framebuffer;
    const auto death_hash = [&]() {
        OL_CHECK(defeat_session.render(death_framebuffer));
        return fnv1a64(death_framebuffer.pixels());
    };
    OL_CHECK(death_hash() == 0x9DA84526F6317A4AULL);
    defeat = defeat_session.resume(SceneResponse::acknowledge);
    OL_CHECK(defeat.kind == SceneStepKind::death_menu);
    OL_CHECK(defeat.menu_index == 0);
    constexpr std::array<std::uint64_t, 4> selected_hashes{
        0x9DA84526F6317A4AULL,
        0x5F72CF3141CE8B24ULL,
        0xB2DB9B6F5EA184DEULL,
        0x11F91FD0E5BECCEBULL,
    };
    for (int selection = 1; selection < 4; ++selection) {
        defeat = defeat_session.resume(SceneResponse::acknowledge, 0x98);
        OL_CHECK(defeat.kind == SceneStepKind::death_menu);
        OL_CHECK(defeat.menu_index == selection);
        OL_CHECK(death_hash() == selected_hashes[static_cast<std::size_t>(selection)]);
    }
    defeat = defeat_session.resume(SceneResponse::acknowledge, 0x98);
    OL_CHECK(defeat.menu_index == 0);
    defeat = defeat_session.resume(SceneResponse::acknowledge, 0x9E);
    OL_CHECK(defeat.menu_index == 3);
    defeat = defeat_session.resume(SceneResponse::acknowledge, 0x0D);
    OL_CHECK(defeat.kind == SceneStepKind::death_menu);
    OL_CHECK(defeat.death_confirm);
    OL_CHECK(death_hash() == 0x4BA394E637CC051EULL);
    defeat = defeat_session.resume(SceneResponse::acknowledge, static_cast<int>('N'));
    OL_CHECK(defeat.kind == SceneStepKind::death_menu);
    OL_CHECK(defeat.menu_index == 3);
    OL_CHECK(!defeat.death_confirm);
    OL_CHECK(death_hash() == selected_hashes[3]);
    defeat = defeat_session.resume(SceneResponse::acknowledge, 0x20);
    OL_CHECK(defeat.death_confirm);
    defeat = defeat_session.resume(SceneResponse::acknowledge, static_cast<int>('Y'));
    OL_CHECK(defeat.kind == SceneStepKind::quit);
    OL_CHECK(defeat_random.state() == 0x41C67EA6U);

    auto load_snapshot = load_baseline(root);
    load_snapshot.ranger.scenes[25].set_word(
        openlegend::model::scene_metadata_word::entrance_x, 33);
    load_snapshot.ranger.scenes[25].set_word(
        openlegend::model::scene_metadata_word::entrance_y, 26);
    openlegend::random::LegacyRandom load_random{1U};
    openlegend::scene::SceneSession load_session{
        data_root, load_snapshot, load_random, 25, false,
        openlegend::scene::SceneDate{1996, 1, 1}};
    auto load_result = load_session.begin_event(936, 72, 33, 26);
    for (int step = 0; step < 512 && load_result.kind != SceneStepKind::battle; ++step) {
        if (load_result.kind == SceneStepKind::dialogue ||
            load_result.kind == SceneStepKind::present ||
            load_result.kind == SceneStepKind::fade_from_black ||
            load_result.kind == SceneStepKind::fade_to_black) {
            load_result = load_session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(load_result.kind == SceneStepKind::battle);
    load_result = load_session.resume(SceneResponse::battle_defeat);
    OL_CHECK(load_result.kind == SceneStepKind::fade_from_black);
    load_result = load_session.resume(SceneResponse::acknowledge);
    load_result = load_session.resume(SceneResponse::acknowledge, 0x98);
    load_result = load_session.resume(SceneResponse::acknowledge, 0x98);
    OL_CHECK(load_result.menu_index == 2);
    load_result = load_session.resume(SceneResponse::acknowledge, 0x0D);
    OL_CHECK(load_result.kind == SceneStepKind::present);
    openlegend::render::IndexedFramebuffer cleared;
    OL_CHECK(load_session.render(cleared));
    OL_CHECK(fnv1a64(cleared.pixels()) == 0xDD14FCC6528CAB25ULL);
    load_result = load_session.resume(SceneResponse::acknowledge);
    OL_CHECK(load_result.kind == SceneStepKind::load_slot);
    OL_CHECK(load_result.save_slot == 2);
    load_result = load_session.resume(SceneResponse::cancel);
    OL_CHECK(load_result.kind == SceneStepKind::death_menu);
    OL_CHECK(load_result.menu_index == 2);
    OL_CHECK(load_session.render(cleared));
    OL_CHECK(fnv1a64(cleared.pixels()) == selected_hashes[2]);
}

void check_event_finale_party_cleanup(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kTeamMemberCount; ++slot) {
        snapshot.ranger.header.set_team_member(
            slot, openlegend::model::CharacterId{static_cast<std::int16_t>(slot)});
    }
    snapshot.ranger.header.set_inventory(0U, openlegend::model::ItemId{6}, 1);
    for (std::int16_t role_id = 1; role_id <= 6; ++role_id) {
        auto& role = snapshot.ranger.roles[static_cast<std::size_t>(role_id)];
        const std::array<std::int16_t, 3> items{
            static_cast<std::int16_t>(20 + role_id * 3),
            static_cast<std::int16_t>(21 + role_id * 3),
            static_cast<std::int16_t>(22 + role_id * 3),
        };
        role.set_word(openlegend::model::role_word::equipment_begin, items[0]);
        role.set_word(openlegend::model::role_word::equipment_begin + 1U, items[1]);
        role.set_word(openlegend::model::role_word::practice_item, items[2]);
        role.set_word(openlegend::model::role_word::item_experience, 123);
        for (const auto item_id : items) {
            snapshot.ranger.items[static_cast<std::size_t>(item_id)].set_word(
                openlegend::model::item_word::user, role_id);
        }
    }
    constexpr std::array<std::pair<std::int16_t, std::int16_t>, 36> targets{
        std::pair<std::int16_t, std::int16_t>{0, 0},
        {49, 2}, {4, 1}, {44, 0}, {44, 1}, {37, 5}, {30, 0}, {59, 0},
        {40, 3}, {56, 1}, {1, 7}, {1, 8}, {1, 10}, {40, 7}, {40, 8},
        {77, 0}, {54, 0}, {62, 3}, {62, 4}, {60, 2}, {60, 15}, {52, 1},
        {61, 0}, {61, 8}, {78, 0}, {18, 0}, {18, 1}, {69, 0}, {69, 1},
        {45, 0}, {52, 2}, {42, 6}, {42, 7}, {8, 8}, {7, 6}, {80, 1},
    };
    std::array<std::pair<std::int16_t, std::int16_t>, targets.size()> coordinates{};
    for (std::size_t index = 0U; index < targets.size(); ++index) {
        const auto [scene, event] = targets[index];
        coordinates[index] = {
            snapshot.event_value(scene, event, openlegend::model::SceneEventField::x).value_or(-1),
            snapshot.event_value(scene, event, openlegend::model::SceneEventField::y).value_or(-1),
        };
    }

    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    auto result = session.begin_event(932, 0, 0, 0);
    for (int step = 0; step < 16 && result.kind != SceneStepKind::stay; ++step) {
        if (result.kind == SceneStepKind::dialogue ||
            result.kind == SceneStepKind::notice ||
            result.kind == SceneStepKind::present) {
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(result.kind == SceneStepKind::stay);
    bool found_item_189 = false;
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        if (snapshot.ranger.header.inventory_item(slot).value == 189 &&
            snapshot.ranger.header.inventory_count(slot) == 1) {
            found_item_189 = true;
        }
    }
    OL_CHECK(found_item_189);

    OL_CHECK(snapshot.ranger.header.team_member(0U).value == 0);
    for (std::size_t slot = 1U; slot < openlegend::model::kTeamMemberCount; ++slot) {
        OL_CHECK(snapshot.ranger.header.team_member(slot).value == -1);
    }
    for (std::int16_t role_id = 1; role_id <= 6; ++role_id) {
        const auto& role = snapshot.ranger.roles[static_cast<std::size_t>(role_id)];
        for (const auto field : {
                 openlegend::model::role_word::equipment_begin,
                 openlegend::model::role_word::equipment_begin + 1U,
                 openlegend::model::role_word::practice_item}) {
            OL_CHECK(role.word(field) == -1);
        }
        OL_CHECK(role.word(openlegend::model::role_word::item_experience) == 0);
        for (std::int16_t offset = 0; offset < 3; ++offset) {
            const auto item_id = static_cast<std::size_t>(20 + role_id * 3 + offset);
            OL_CHECK(snapshot.ranger.items[item_id].word(openlegend::model::item_word::user) == -1);
        }
    }
    for (std::size_t index = 0U; index < targets.size(); ++index) {
        const auto [scene, event] = targets[index];
        const std::array<std::int16_t, 9> expected{0, 0, -1, -1, -1, -1, -1, -1, 0};
        for (std::size_t field = 0U; field < expected.size(); ++field) {
            OL_CHECK(snapshot.event_value(
                         scene, event,
                         static_cast<openlegend::model::SceneEventField>(field)).value_or(-2) ==
                     expected[field]);
        }
        OL_CHECK(snapshot.event_value(scene, event, openlegend::model::SceneEventField::x).value_or(-1) ==
                 coordinates[index].first);
        OL_CHECK(snapshot.event_value(scene, event, openlegend::model::SceneEventField::y).value_or(-1) ==
                 coordinates[index].second);
    }
}

void check_event_role_iq_clamp(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    for (const auto [before, after] :
         std::array<std::pair<std::int16_t, std::int16_t>, 2>{
             std::pair<std::int16_t, std::int16_t>{99, 100}, {32766, 0}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::iq, before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        auto result = session.begin_event(673, 0, 0, 0);
        for (int step = 0; step < 256 && result.kind != SceneStepKind::stay; ++step) {
            if (result.kind == SceneStepKind::dialogue ||
                result.kind == SceneStepKind::notice ||
                result.kind == SceneStepKind::present ||
                result.kind == SceneStepKind::fade_from_black ||
                result.kind == SceneStepKind::fade_to_black) {
                result = session.resume(SceneResponse::acknowledge);
            } else {
                break;
            }
        }
        OL_CHECK(result.kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::iq) == after);
    }
}

void check_event_open_all_scenes(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    for (auto& scene : snapshot.ranger.scenes) {
        scene.set_word(openlegend::model::scene_metadata_word::entrance_condition, 777);
    }
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    auto result = session.begin_event(821, 0, 0, 0);
    for (int step = 0; step < 256 && result.kind != SceneStepKind::stay; ++step) {
        if (result.kind == SceneStepKind::dialogue ||
            result.kind == SceneStepKind::notice ||
            result.kind == SceneStepKind::present ||
            result.kind == SceneStepKind::fade_from_black ||
            result.kind == SceneStepKind::fade_to_black) {
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(snapshot.ranger.scenes[0].word(
                 openlegend::model::scene_metadata_word::entrance_condition) == 0);
    OL_CHECK(snapshot.ranger.scenes[2].word(
                 openlegend::model::scene_metadata_word::entrance_condition) == 2);
    OL_CHECK(snapshot.ranger.scenes[38].word(
                 openlegend::model::scene_metadata_word::entrance_condition) == 2);
    OL_CHECK(snapshot.ranger.scenes[75].word(
                 openlegend::model::scene_metadata_word::entrance_condition) == 1);
    OL_CHECK(snapshot.ranger.scenes[80].word(
                 openlegend::model::scene_metadata_word::entrance_condition) == 1);
    OL_CHECK(snapshot.ranger.scenes.size() == openlegend::model::kSceneMetadataCount);
    OL_CHECK(snapshot.ranger.scenes.size() == 84U);
    OL_CHECK(snapshot.ranger.scenes[83].word(
                 openlegend::model::scene_metadata_word::entrance_condition) == 0);
}

void check_event_clear_party_mp(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    snapshot.ranger.header.set_team_member(0U, openlegend::model::CharacterId{1});
    snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{0});
    for (std::size_t slot = 2U; slot < openlegend::model::kTeamMemberCount; ++slot) {
        snapshot.ranger.header.set_team_member(slot, openlegend::model::CharacterId{-1});
    }
    snapshot.ranger.roles[0].set_word(openlegend::model::role_word::mp, 77);
    snapshot.ranger.roles[1].set_word(openlegend::model::role_word::mp, 88);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    auto result = session.begin_event(20, 0, 0, 0);
    for (int step = 0; step < 128 &&
                       snapshot.ranger.roles[1].word(openlegend::model::role_word::mp) != 0;
         ++step) {
        if (result.kind == SceneStepKind::dialogue ||
            result.kind == SceneStepKind::notice ||
            result.kind == SceneStepKind::present ||
            result.kind == SceneStepKind::fade_from_black ||
            result.kind == SceneStepKind::fade_to_black) {
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(snapshot.ranger.roles[1].word(openlegend::model::role_word::mp) == 0);
    OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::mp) == 77);
}

void check_event_join_helper(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{0});
    snapshot.ranger.header.set_team_member(2U, openlegend::model::CharacterId{2});
    snapshot.ranger.header.set_team_member(3U, openlegend::model::CharacterId{3});
    snapshot.ranger.header.set_team_member(4U, openlegend::model::CharacterId{4});
    snapshot.ranger.header.set_team_member(5U, openlegend::model::CharacterId{0});
    snapshot.ranger.header.set_inventory(0U, openlegend::model::ItemId{109}, 2);
    snapshot.ranger.header.set_inventory(1U, openlegend::model::ItemId{109}, 3);
    auto& role = snapshot.ranger.roles[1];
    role.set_word(openlegend::model::role_word::taking_item_begin, 109);
    role.set_word(openlegend::model::role_word::taking_item_count_begin, 0);
    role.set_word(openlegend::model::role_word::equipment_begin, 10);
    role.set_word(openlegend::model::role_word::equipment_begin + 1U, 11);
    role.set_word(openlegend::model::role_word::practice_item, 12);
    role.set_word(openlegend::model::role_word::item_experience, 77);
    snapshot.ranger.items[10].set_word(openlegend::model::item_word::user, 1);
    snapshot.ranger.items[11].set_word(openlegend::model::item_word::user, 1);
    snapshot.ranger.items[12].set_word(openlegend::model::item_word::user, 1);

    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    auto result = session.begin_event(11, 0, 0, 0);
    for (int step = 0; step < 64 && result.kind != SceneStepKind::notice; ++step) {
        if (result.kind == SceneStepKind::question) {
            result = session.resume(SceneResponse::yes);
        } else if (result.kind == SceneStepKind::dialogue ||
                   result.kind == SceneStepKind::present ||
                   result.kind == SceneStepKind::fade_from_black ||
                   result.kind == SceneStepKind::fade_to_black) {
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(snapshot.ranger.header.team_member(1U).value == 1);
    OL_CHECK(snapshot.ranger.header.inventory_count(0U) == 2);
    OL_CHECK(snapshot.ranger.header.inventory_count(1U) == 3);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin) == 0);
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin + 1U) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::practice_item) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::item_experience) == 0);
    OL_CHECK(snapshot.ranger.items[10].word(openlegend::model::item_word::user) == -1);
    OL_CHECK(snapshot.ranger.items[11].word(openlegend::model::item_word::user) == -1);
    OL_CHECK(snapshot.ranger.items[12].word(openlegend::model::item_word::user) == -1);
}

void check_event_state_side_effects(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};

    auto book_snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        book_snapshot.ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
    }
    for (std::int16_t item_id = 144; item_id <= 157; ++item_id) {
        book_snapshot.ranger.header.set_inventory(
            static_cast<std::size_t>(item_id - 144), openlegend::model::ItemId{item_id}, 0);
    }
    book_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::fame, 200);
    openlegend::random::LegacyRandom book_random{1U};
    openlegend::scene::SceneSession book_session{
        data_root, book_snapshot, book_random, 70};
    OL_CHECK(finish_scene_title(book_session).kind ==
             openlegend::scene::SceneStepKind::stay);
    OL_CHECK(book_session.begin_event(36, 0, 44, 29).kind ==
             openlegend::scene::SceneStepKind::notice);
    OL_CHECK(book_snapshot.event_value(
                 70U, 11U, openlegend::model::SceneEventField::event_1).value_or(-1) == 932);
    OL_CHECK(book_snapshot.event_value(
                 70U, 11U, openlegend::model::SceneEventField::current_picture).value_or(-1) ==
             7968);

    auto letter_snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        letter_snapshot.ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
    }
    for (std::int16_t item_id = 144; item_id <= 157; ++item_id) {
        letter_snapshot.ranger.header.set_inventory(
            static_cast<std::size_t>(item_id - 144), openlegend::model::ItemId{item_id}, 0);
    }
    letter_snapshot.ranger.header.set_inventory(
        14U, openlegend::model::ItemId{189}, 0);
    letter_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::fame, 200);
    static_cast<void>(letter_snapshot.set_event_value(
        70U, 11U, openlegend::model::SceneEventField::event_1, 111));
    openlegend::random::LegacyRandom letter_random{1U};
    openlegend::scene::SceneSession letter_session{
        data_root, letter_snapshot, letter_random, 70};
    OL_CHECK(letter_session.begin_event(36, 0, 44, 29).kind ==
             openlegend::scene::SceneStepKind::notice);
    OL_CHECK(letter_snapshot.event_value(
                 70U, 11U, openlegend::model::SceneEventField::event_1).value_or(-1) == 111);

    auto rest_snapshot = load_baseline(root);
    auto& resting_role = rest_snapshot.ranger.roles[0];
    resting_role.set_word(openlegend::model::role_word::hp, 1);
    resting_role.set_word(openlegend::model::role_word::maximum_hp, 91);
    resting_role.set_word(openlegend::model::role_word::mp, 2);
    resting_role.set_word(openlegend::model::role_word::maximum_mp, 82);
    resting_role.set_word(openlegend::model::role_word::hurt, 32);
    resting_role.set_word(openlegend::model::role_word::poison, 0);
    resting_role.set_word(openlegend::model::role_word::physical_power, 3);
    auto& poisoned_role = rest_snapshot.ranger.roles[1];
    poisoned_role.set_word(openlegend::model::role_word::hp, 4);
    poisoned_role.set_word(openlegend::model::role_word::maximum_hp, 94);
    poisoned_role.set_word(openlegend::model::role_word::mp, 5);
    poisoned_role.set_word(openlegend::model::role_word::maximum_mp, 85);
    poisoned_role.set_word(openlegend::model::role_word::hurt, 10);
    poisoned_role.set_word(openlegend::model::role_word::poison, 1);
    poisoned_role.set_word(openlegend::model::role_word::physical_power, 6);
    auto& after_gap_role = rest_snapshot.ranger.roles[2];
    after_gap_role.set_word(openlegend::model::role_word::hp, 7);
    after_gap_role.set_word(openlegend::model::role_word::maximum_hp, 97);
    after_gap_role.set_word(openlegend::model::role_word::mp, 8);
    after_gap_role.set_word(openlegend::model::role_word::maximum_mp, 88);
    after_gap_role.set_word(openlegend::model::role_word::hurt, 10);
    after_gap_role.set_word(openlegend::model::role_word::poison, 0);
    after_gap_role.set_word(openlegend::model::role_word::physical_power, 9);
    rest_snapshot.ranger.header.set_team_member(0U, openlegend::model::CharacterId{0});
    rest_snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{1});
    rest_snapshot.ranger.header.set_team_member(2U, openlegend::model::CharacterId{-1});
    rest_snapshot.ranger.header.set_team_member(3U, openlegend::model::CharacterId{2});
    openlegend::random::LegacyRandom rest_random{1U};
    openlegend::scene::SceneSession rest_session{
        data_root, rest_snapshot, rest_random, 70};
    OL_CHECK(finish_scene_title(rest_session).kind ==
             openlegend::scene::SceneStepKind::stay);
    OL_CHECK(rest_session.begin_event(931, 0, 44, 29).kind ==
             openlegend::scene::SceneStepKind::question);
    OL_CHECK(rest_session.resume(openlegend::scene::SceneResponse::yes).kind ==
             openlegend::scene::SceneStepKind::dialogue);
    OL_CHECK(rest_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::present);
    OL_CHECK(rest_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::fade_to_black);
    OL_CHECK(rest_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::present);
    OL_CHECK(resting_role.word(openlegend::model::role_word::hp) == 91);
    OL_CHECK(resting_role.word(openlegend::model::role_word::mp) == 82);
    OL_CHECK(resting_role.word(openlegend::model::role_word::hurt) == 0);
    OL_CHECK(resting_role.word(openlegend::model::role_word::physical_power) == 100);
    OL_CHECK(poisoned_role.word(openlegend::model::role_word::hp) == 4);
    OL_CHECK(poisoned_role.word(openlegend::model::role_word::mp) == 5);
    OL_CHECK(poisoned_role.word(openlegend::model::role_word::hurt) == 10);
    OL_CHECK(poisoned_role.word(openlegend::model::role_word::physical_power) == 6);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::hp) == 7);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::mp) == 8);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::hurt) == 10);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::physical_power) == 9);

    auto join_snapshot = load_baseline(root);
    for (std::size_t slot = 1U; slot < openlegend::model::kTeamMemberCount; ++slot) {
        join_snapshot.ranger.header.set_team_member(slot, openlegend::model::CharacterId{-1});
    }
    auto& joining_role = join_snapshot.ranger.roles[49];
    for (std::size_t slot = 0U; slot < openlegend::model::role_word::magic_count; ++slot) {
        joining_role.set_word(openlegend::model::role_word::magic_id_begin + slot,
                              static_cast<std::int16_t>(40 + slot));
        joining_role.set_word(openlegend::model::role_word::magic_level_begin + slot, 100);
    }
    joining_role.set_word(openlegend::model::role_word::maximum_mp, 900);
    joining_role.set_word(openlegend::model::role_word::mp, 100);
    joining_role.set_word(openlegend::model::role_word::maximum_hp, 900);
    joining_role.set_word(openlegend::model::role_word::hp, 100);
    joining_role.set_word(openlegend::model::role_word::attack, 90);
    joining_role.set_word(openlegend::model::role_word::speed, 90);
    joining_role.set_word(openlegend::model::role_word::mp_type, 0);
    joining_role.set_word(openlegend::model::role_word::equipment_begin, 1);
    joining_role.set_word(openlegend::model::role_word::equipment_begin + 1U, 2);
    joining_role.set_word(openlegend::model::role_word::practice_item, 3);
    joining_role.set_word(openlegend::model::role_word::item_experience, 123);
    for (std::int16_t item_id = 1; item_id <= 3; ++item_id) {
        join_snapshot.ranger.items[static_cast<std::size_t>(item_id)].set_word(
            openlegend::model::item_word::user, 49);
    }
    for (std::size_t slot = 0U; slot < openlegend::model::role_word::taking_item_count; ++slot) {
        joining_role.set_word(openlegend::model::role_word::taking_item_begin + slot, -1);
        joining_role.set_word(openlegend::model::role_word::taking_item_count_begin + slot, 0);
    }
    openlegend::random::LegacyRandom join_random{1U};
    openlegend::scene::SceneSession join_session{
        data_root, join_snapshot, join_random, 70};
    OL_CHECK(finish_scene_title(join_session).kind ==
             openlegend::scene::SceneStepKind::stay);
    auto join_result = join_session.begin_event(581, 0, 44, 29);
    for (int step = 0; step < 128 && join_result.kind != openlegend::scene::SceneStepKind::stay;
         ++step) {
        const auto resumable = join_result.kind == openlegend::scene::SceneStepKind::dialogue ||
                               join_result.kind == openlegend::scene::SceneStepKind::notice ||
                               join_result.kind == openlegend::scene::SceneStepKind::present ||
                               join_result.kind == openlegend::scene::SceneStepKind::fade_from_black ||
                               join_result.kind == openlegend::scene::SceneStepKind::fade_to_black;
        OL_CHECK(resumable);
        if (!resumable) {
            break;
        }
        join_result = join_session.resume(openlegend::scene::SceneResponse::acknowledge);
    }
    OL_CHECK(join_result.kind == openlegend::scene::SceneStepKind::stay);
    OL_CHECK(join_snapshot.ranger.header.team_member(1U).value == 49);
    OL_CHECK(joining_role.word(openlegend::model::role_word::maximum_mp) == 1200);
    OL_CHECK(joining_role.word(openlegend::model::role_word::mp) == 1200);
    OL_CHECK(joining_role.word(openlegend::model::role_word::maximum_hp) == 1100);
    OL_CHECK(joining_role.word(openlegend::model::role_word::hp) == 1100);
    OL_CHECK(joining_role.word(openlegend::model::role_word::attack) == 100);
    OL_CHECK(joining_role.word(openlegend::model::role_word::speed) == 100);
    OL_CHECK(joining_role.word(openlegend::model::role_word::mp_type) == 2);
    OL_CHECK(joining_role.word(openlegend::model::role_word::magic_id_begin) == 15);
    OL_CHECK(joining_role.word(openlegend::model::role_word::magic_level_begin) == 0);
    OL_CHECK(joining_role.word(openlegend::model::role_word::equipment_begin) == -1);
    OL_CHECK(joining_role.word(openlegend::model::role_word::equipment_begin + 1U) == -1);
    OL_CHECK(joining_role.word(openlegend::model::role_word::practice_item) == -1);
    OL_CHECK(joining_role.word(openlegend::model::role_word::item_experience) == 0);
    for (std::int16_t item_id = 1; item_id <= 3; ++item_id) {
        OL_CHECK(join_snapshot.ranger.items[static_cast<std::size_t>(item_id)].word(
                     openlegend::model::item_word::user) == -1);
    }

    auto leave_snapshot = load_baseline(root);
    leave_snapshot.ranger.header.set_team_member(0U, openlegend::model::CharacterId{0});
    leave_snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{1});
    leave_snapshot.ranger.header.set_team_member(2U, openlegend::model::CharacterId{2});
    leave_snapshot.ranger.header.set_team_member(3U, openlegend::model::CharacterId{-1});
    auto& leaving_role = leave_snapshot.ranger.roles[1];
    leaving_role.set_word(openlegend::model::role_word::equipment_begin, 4);
    leaving_role.set_word(openlegend::model::role_word::equipment_begin + 1U, 5);
    leaving_role.set_word(openlegend::model::role_word::practice_item, 6);
    leaving_role.set_word(openlegend::model::role_word::item_experience, 321);
    for (std::int16_t item_id = 4; item_id <= 6; ++item_id) {
        leave_snapshot.ranger.items[static_cast<std::size_t>(item_id)].set_word(
            openlegend::model::item_word::user, 1);
    }
    openlegend::random::LegacyRandom leave_random{1U};
    openlegend::scene::SceneSession leave_session{
        data_root, leave_snapshot, leave_random, 70};
    OL_CHECK(finish_scene_title(leave_session).kind ==
             openlegend::scene::SceneStepKind::stay);
    OL_CHECK(leave_session.begin_event(950, 0, 44, 29).kind ==
             openlegend::scene::SceneStepKind::dialogue);
    OL_CHECK(leave_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);
    OL_CHECK(leave_snapshot.ranger.header.team_member(1U).value == 2);
    OL_CHECK(leave_snapshot.ranger.header.team_member(2U).value == -1);
    OL_CHECK(leaving_role.word(openlegend::model::role_word::equipment_begin) == -1);
    OL_CHECK(leaving_role.word(openlegend::model::role_word::equipment_begin + 1U) == -1);
    OL_CHECK(leaving_role.word(openlegend::model::role_word::practice_item) == -1);
    OL_CHECK(leaving_role.word(openlegend::model::role_word::item_experience) == 0);
}

void check_event_basic_role_and_scene_helpers(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    for (const auto [sexual, talk_id] :
         std::array<std::pair<std::int16_t, std::int16_t>, 2>{
             std::pair<std::int16_t, std::int16_t>{2, 1123}, {1, 1122}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::sexual, sexual);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        const auto result = session.begin_event(328, 0, 44, 29);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == talk_id);
    }

    for (const auto [before, after] :
         std::array<std::pair<std::int16_t, std::int16_t>, 2>{
             std::pair<std::int16_t, std::int16_t>{3, 0}, {-32768, 100}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        const auto notice = session.begin_event(149, 0, 44, 29);
        OL_CHECK(notice.kind == SceneStepKind::notice);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::morality) == after);
    }

    for (const auto [event_1, expected_event_1] :
         std::array<std::pair<std::int16_t, std::int16_t>, 2>{
             std::pair<std::int16_t, std::int16_t>{-1, -1}, {999, 862}}) {
        auto snapshot = load_baseline(root);
        for (std::size_t event = 2U; event <= 5U; ++event) {
            OL_CHECK(snapshot.set_event_value(
                70U, event, openlegend::model::SceneEventField::event_1,
                event == 2U ? event_1 : -1));
        }
        OL_CHECK(snapshot.set_event_value(
            70U, 2U, openlegend::model::SceneEventField::current_picture, 1234));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(464, 0, 44, 29).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.event_value(
                     70U, 2U, openlegend::model::SceneEventField::event_1).value_or(-2) ==
                 expected_event_1);
        OL_CHECK(snapshot.event_value(
                     70U, 2U, openlegend::model::SceneEventField::current_picture).value_or(-2) ==
                 1234);
    }

    const auto advance_to_stay = [](openlegend::scene::SceneSession& session,
                                    openlegend::scene::SceneStepResult result) {
        for (int step = 0; step < 256 && result.kind != SceneStepKind::stay; ++step) {
            if (result.kind == SceneStepKind::question) {
                result = session.resume(SceneResponse::yes);
            } else if (result.kind == SceneStepKind::battle) {
                result = session.resume(SceneResponse::battle_victory);
            } else if (result.kind == SceneStepKind::dialogue ||
                       result.kind == SceneStepKind::notice ||
                       result.kind == SceneStepKind::present ||
                       result.kind == SceneStepKind::fade_from_black ||
                       result.kind == SceneStepKind::fade_to_black) {
                result = session.resume(SceneResponse::acknowledge);
            } else {
                break;
            }
        }
        return result;
    };

    {
        auto snapshot = load_baseline(root);
        snapshot.ranger.header.set_inventory(0U, openlegend::model::ItemId{174}, 100);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(advance_to_stay(session, session.begin_event(235, 0, 44, 29)).kind ==
                 SceneStepKind::stay);
        OL_CHECK(session.scene_x() == 14 && session.scene_y() == 14);
        OL_CHECK(session.view_origin_x() == 3 && session.view_origin_y() == 3);
        OL_CHECK(session.direction() == openlegend::scene::SceneDirection::down);
        OL_CHECK(session.player_frame() == 5044);
    }

    {
        auto snapshot = load_baseline(root);
        snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{1});
        snapshot.ranger.roles[4].set_word(openlegend::model::role_word::use_poison, 0);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        auto result = session.begin_event(28, 0, 44, 29);
        for (int step = 0; step < 64 && result.kind != SceneStepKind::battle; ++step) {
            OL_CHECK(result.kind == SceneStepKind::dialogue ||
                     result.kind == SceneStepKind::present ||
                     result.kind == SceneStepKind::fade_from_black ||
                     result.kind == SceneStepKind::fade_to_black);
            result = session.resume(SceneResponse::acknowledge);
        }
        OL_CHECK(result.kind == SceneStepKind::battle);
        OL_CHECK(snapshot.ranger.roles[4].word(openlegend::model::role_word::use_poison) == 99);
    }

    {
        auto snapshot = load_baseline(root);
        snapshot.ranger.scenes[75].set_word(
            openlegend::model::scene_metadata_word::entrance_condition, 777);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(advance_to_stay(session, session.begin_event(420, 0, 44, 29)).kind ==
                 SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.scenes[75].word(
                     openlegend::model::scene_metadata_word::entrance_condition) == 0);
    }

    for (const auto [female_present, talk_id] :
         std::array<std::pair<bool, std::int16_t>, 2>{
             std::pair<bool, std::int16_t>{false, 1575}, {true, 1574}}) {
        auto snapshot = load_baseline(root);
        for (auto& role : snapshot.ranger.roles) {
            role.set_word(openlegend::model::role_word::sexual, 0);
        }
        snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{1});
        snapshot.ranger.header.set_team_member(5U, openlegend::model::CharacterId{-1});
        if (female_present) {
            snapshot.ranger.roles[1].set_word(openlegend::model::role_word::sexual, 1);
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        auto result = session.begin_event(445, 0, 44, 29);
        OL_CHECK(result.kind == SceneStepKind::dialogue && result.talk_id == 1578);
        while ((result.kind == SceneStepKind::dialogue && result.talk_id == 1578) ||
               result.kind == SceneStepKind::present) {
            result = session.resume(SceneResponse::acknowledge);
        }
        OL_CHECK(result.kind == SceneStepKind::question);
        result = session.resume(SceneResponse::yes);
        OL_CHECK(result.kind == SceneStepKind::dialogue && result.talk_id == 1573);
        while ((result.kind == SceneStepKind::dialogue && result.talk_id == 1573) ||
               result.kind == SceneStepKind::present) {
            result = session.resume(SceneResponse::acknowledge);
        }
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == talk_id);
    }

    {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::fame, 32767);
        for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
            snapshot.ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(advance_to_stay(session, session.begin_event(2, 0, 44, 29)).kind ==
                 SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::fame) == -32768);
    }

    {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::fame, 199);
        for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
            snapshot.ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
        }
        for (std::int16_t item_id = 144; item_id <= 157; ++item_id) {
            snapshot.ranger.header.set_inventory(
                static_cast<std::size_t>(item_id - 144), openlegend::model::ItemId{item_id}, 0);
        }
        OL_CHECK(snapshot.set_event_value(
            70U, 11U, openlegend::model::SceneEventField::event_1, -1));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(advance_to_stay(session, session.begin_event(2, 0, 44, 29)).kind ==
                 SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::fame) == 200);
        OL_CHECK(snapshot.event_value(
                     70U, 11U, openlegend::model::SceneEventField::event_1).value_or(-1) == 932);
    }
}

void check_event_status_notices(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    constexpr std::array<std::uint8_t, 24> morality_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xAAU, 0xBAU,
        0xABU, 0x7EU, 0xBCU, 0x77U, 0xABU, 0xFCU, 0xBCU, 0xC6U,
        0xACU, 0xB0U, 0x20U, 0x20U, 0x20U, 0x20U, 0x37U, 0x00U};
    constexpr std::array<std::uint8_t, 25> fame_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xADU, 0xD3U,
        0xA4U, 0x48U, 0xC1U, 0x6EU, 0xB1U, 0xE6U, 0xABU, 0xFCU,
        0xBCU, 0xC6U, 0xACU, 0xB0U, 0x20U, 0x31U, 0x32U, 0x33U, 0x00U};
    const openlegend::resource::DataRoot data_root{root};
    const auto check_notice = [&data_root, &root](
                                  const std::int16_t script_id,
                                  const std::size_t field,
                                  const std::int16_t value,
                                  const std::int16_t style,
                                  const std::span<const std::uint8_t> expected_text,
                                  const std::uint64_t expected_hash) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(field, value);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        const auto result = session.begin_event(script_id, 0, 44, 29);
        OL_CHECK(result.kind == SceneStepKind::notice);
        OL_CHECK(result.style == style);
        OL_CHECK(session.pending_text().size() == expected_text.size());
        OL_CHECK(std::equal(
            session.pending_text().begin(), session.pending_text().end(), expected_text.begin()));
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == expected_hash);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    };
    check_notice(
        825, openlegend::model::role_word::morality, 7, 52, morality_text,
        0x1CC47112086C10E7ULL);
    check_notice(
        828, openlegend::model::role_word::fame, 123, 53, fame_text,
        0x5678C57A93EC10C4ULL);
}

void check_event_map_replace_and_random_talk(const std::filesystem::path& root) {
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    {
        auto snapshot = load_baseline(root);
        constexpr std::array<std::size_t, 3> targets{0U, 63U, 4095U};
        for (const auto index : targets) {
            OL_CHECK(snapshot.set_scene_value(
                70U, openlegend::model::SceneLayer::earth, index, 990));
        }
        OL_CHECK(snapshot.set_scene_value(
            70U, openlegend::model::SceneLayer::earth, 64U, 123));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(434, 0, 44, 29).kind == SceneStepKind::stay);
        for (const auto index : targets) {
            OL_CHECK(snapshot.scene_value(
                         70U, openlegend::model::SceneLayer::earth, index).value_or(-1) == 994);
        }
        OL_CHECK(snapshot.scene_value(
                     70U, openlegend::model::SceneLayer::earth, 64U).value_or(-1) == 123);
    }

    {
        auto snapshot = load_baseline(root);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        const auto result = session.begin_event(692, 0, 44, 29);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == 2555);
        OL_CHECK(result.head_id == 114);
        OL_CHECK(result.style == 0);
        OL_CHECK(random.state() == 0x41C67EA6U);
    }
}

void check_event_execution(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind ==
             openlegend::scene::SceneStepKind::stay);

    const auto before = inventory_count(snapshot.ranger, 173);
    const auto item_result = session.begin_event(36, 0, 44, 29);
    OL_CHECK(item_result.kind == openlegend::scene::SceneStepKind::notice);
    OL_CHECK(inventory_count(snapshot.ranger, 173) == before + 1);
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);

    const openlegend::scene::SceneAssets assets{data_root};
    const auto script = assets.script(274U);
    OL_CHECK(script.size() == 14U);
    OL_CHECK(script[0] == 17 && script[1] == -2 && script[2] == 1);
    OL_CHECK(script[6] == 17 && script[7] == -2 && script[8] == 1);
    OL_CHECK(script[9] == 12 && script[10] == 22 && script[11] == 2898);
    const auto map_result = session.begin_event(274, 0, 44, 29);
    OL_CHECK(map_result.kind == openlegend::scene::SceneStepKind::present);
    OL_CHECK(snapshot.scene_value(70U, openlegend::model::SceneLayer::building,
                                  22U * 64U + 13U).value_or(-1) == 0);
    const auto changed_building = snapshot.scene_value(
        70U, openlegend::model::SceneLayer::building, 22U * 64U + 12U).value_or(-1);
    if (changed_building != 2898) {
        std::cerr << "script 274 building value: " << changed_building << '\n';
    }
    OL_CHECK(changed_building == 2898);
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);

    const auto dialogue = session.begin_event(69, 0, 44, 29);
    OL_CHECK(dialogue.kind == openlegend::scene::SceneStepKind::dialogue);
    OL_CHECK(dialogue.talk_id == 228);
    OL_CHECK(!session.pending_text().empty());
    const auto dialogue_tail = session.resume(openlegend::scene::SceneResponse::acknowledge);
    OL_CHECK(dialogue_tail.kind == openlegend::scene::SceneStepKind::stay ||
             dialogue_tail.kind == openlegend::scene::SceneStepKind::present);
    if (dialogue_tail.kind == openlegend::scene::SceneStepKind::present) {
        OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
                 openlegend::scene::SceneStepKind::stay);
    }
}

}  // namespace

int main() {
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
    check_assets(root);
    check_scene_render_and_movement(root);
    check_scene_entry_state(root);
    check_scene_archive_ownership(root);
    check_scene_interaction_present(root);
    check_scene_item_and_auto_event_present(root);
    check_scene_loop_transitions(root);
    check_scene_exit_music_override(root);
    check_scene_event_animation(root);
    check_scene_weather(root);
    check_event_camera_pan(root);
    check_event_picture_animation(root);
    check_event_scripted_walk(root);
    check_event_dual_picture_animation(root);
    check_event_three_statue_animation(root);
    check_event_ending_prelude_animation(root);
    check_event_role_sexual_and_audio(root);
    check_event_shop_helpers(root);
    check_event_presence_and_party_tail_conditions(root);
    check_event_role_stat_conditions(root);
    check_event_inventory_condition_edge_cases(root);
    check_event_all_book_pictures_condition(root);
    check_event_current_picture_condition(root);
    check_event_tournament_trial(root);
    check_event_finale_party_cleanup(root);
    check_event_role_iq_clamp(root);
    check_event_open_all_scenes(root);
    check_event_clear_party_mp(root);
    check_event_join_helper(root);
    check_event_state_side_effects(root);
    check_event_basic_role_and_scene_helpers(root);
    check_event_status_notices(root);
    check_event_map_replace_and_random_talk(root);
    check_event_execution(root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
