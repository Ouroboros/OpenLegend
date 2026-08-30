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
    OL_CHECK(session.pending().kind == openlegend::scene::SceneStepKind::scene_title);

    openlegend::render::IndexedFramebuffer framebuffer;
    OL_CHECK(session.render_map(framebuffer));
    const auto frame_hash = fnv1a64(framebuffer.pixels());
    if (frame_hash != 0x38FBAA07B733AD79ULL) {
        std::cerr << "scene 70 frame: expected 0x38fbaa07b733ad79, actual 0x"
                  << std::hex << frame_hash << std::dec << '\n';
    }
    OL_CHECK(frame_hash == 0x38FBAA07B733AD79ULL);

    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
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
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);
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
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

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
    OL_CHECK(diagonal.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
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
    constexpr std::array<std::int16_t, 5> expected_pictures{5004, 5006, 5008, 5010, 5012};
    constexpr std::array<std::uint64_t, 5> expected_hashes{
        0xFE3EC0DE318A5A82ULL,
        0x68C98BB78C98819AULL,
        0x5C20EA03384F66FBULL,
        0xAAA97B36975C12E9ULL,
        0xF2A1E35490A4F647ULL,
    };
    for (std::size_t index = 0U; index < expected_y.size(); ++index) {
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 3U);
        OL_CHECK(session.scene_x() == 28);
        OL_CHECK(session.scene_y() == expected_y[index]);
        OL_CHECK(session.direction() == openlegend::scene::SceneDirection::up);
        OL_CHECK(session.player_frame() == expected_pictures[index]);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 1248);
    OL_CHECK(session.player_frame() == 5002);
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
    for (std::int16_t frame = 5004; frame <= 5012; frame += 2) {
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

void check_event_state_side_effects(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};

    auto book_snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        book_snapshot.ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
    }
    for (std::int16_t item_id = 144; item_id <= 157; ++item_id) {
        book_snapshot.ranger.header.set_inventory(
            static_cast<std::size_t>(item_id - 144), openlegend::model::ItemId{item_id}, 1);
    }
    book_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::fame, 200);
    openlegend::random::LegacyRandom book_random{1U};
    openlegend::scene::SceneSession book_session{
        data_root, book_snapshot, book_random, 70};
    OL_CHECK(book_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);
    OL_CHECK(book_session.begin_event(36, 0, 44, 29).kind ==
             openlegend::scene::SceneStepKind::notice);
    OL_CHECK(book_snapshot.event_value(
                 70U, 11U, openlegend::model::SceneEventField::event_1).value_or(-1) == 932);
    OL_CHECK(book_snapshot.event_value(
                 70U, 11U, openlegend::model::SceneEventField::current_picture).value_or(-1) ==
             7968);

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
    rest_snapshot.ranger.header.set_team_member(0U, openlegend::model::CharacterId{0});
    rest_snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{1});
    rest_snapshot.ranger.header.set_team_member(2U, openlegend::model::CharacterId{-1});
    openlegend::random::LegacyRandom rest_random{1U};
    openlegend::scene::SceneSession rest_session{
        data_root, rest_snapshot, rest_random, 70};
    OL_CHECK(rest_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
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
    OL_CHECK(join_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
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
    OL_CHECK(leave_session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
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

void check_event_execution(const std::filesystem::path& root) {
    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
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
    check_scene_weather(root);
    check_event_camera_pan(root);
    check_event_picture_animation(root);
    check_event_scripted_walk(root);
    check_event_dual_picture_animation(root);
    check_event_state_side_effects(root);
    check_event_execution(root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
