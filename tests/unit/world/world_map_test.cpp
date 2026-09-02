#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>

#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/render/world_depth_order.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/resource/packed_archive.hpp"
#include "openlegend/world/world_map.hpp"
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

[[nodiscard]] std::uint64_t fnv1a64_words(const std::span<const std::int16_t> words) {
    std::uint64_t result = 0xCBF29CE484222325ULL;
    for (const auto word : words) {
        const auto bits = static_cast<std::uint16_t>(word);
        for (const auto byte : {static_cast<std::uint8_t>(bits & 0xFFU),
                                static_cast<std::uint8_t>(bits >> 8U)}) {
            result ^= byte;
            result *= 0x100000001B3ULL;
        }
    }
    return result;
}

[[nodiscard]] std::uint64_t fnv1a64_depth_entries(
    const std::span<const openlegend::render::LegacyDepthEntry> entries) {
    std::uint64_t result = 0xCBF29CE484222325ULL;
    for (const auto& entry : entries) {
        for (const auto word : {entry.world_x, entry.world_y, entry.sprite_id}) {
            const auto bits = static_cast<std::uint16_t>(word);
            for (const auto byte : {static_cast<std::uint8_t>(bits & 0xFFU),
                                    static_cast<std::uint8_t>(bits >> 8U)}) {
                result ^= byte;
                result *= 0x100000001B3ULL;
            }
        }
    }
    return result;
}

[[nodiscard]] bool sprite_has_visible_pixel(
    const openlegend::resource::PackedArchive& sprites,
    const std::int16_t legacy_id,
    const openlegend::render::IndexedFramebuffer& framebuffer,
    const int anchor_x,
    const int anchor_y) {
    const auto index = openlegend::render::legacy_sprite_index(
        static_cast<std::uint16_t>(legacy_id));
    if (!index.has_value() || *index >= sprites.entry_count()) {
        return false;
    }
    const auto frame = openlegend::resource::SpriteFrameView::parse(sprites.entry(*index));
    if (!frame.valid()) {
        return false;
    }
    const auto left = anchor_x - static_cast<int>(frame.x_offset());
    const auto top = anchor_y - static_cast<int>(frame.y_offset());
    for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
        const auto destination_y = top + static_cast<int>(row_index);
        auto destination_x = left;
        for (const auto& run : frame.rows()[row_index].runs) {
            destination_x += static_cast<int>(run.skip);
            for (const auto source_pixel : run.pixels) {
                if (destination_y >= 0 &&
                    destination_y < openlegend::render::IndexedFramebuffer::height &&
                    destination_x >= 0 &&
                    destination_x < openlegend::render::IndexedFramebuffer::width &&
                    framebuffer.row(destination_y)[destination_x] == source_pixel) {
                    return true;
                }
                ++destination_x;
            }
        }
    }
    return false;
}

[[nodiscard]] openlegend::model::GameSnapshot load_baseline(
    const std::filesystem::path& root) {
    auto loaded = openlegend::persistence::load_baseline(root);
    OL_CHECK(loaded);
    if (!loaded) {
        return {};
    }
    return std::move(*loaded.snapshot);
}

void check_layers_and_cache(const std::filesystem::path& root) {
    using namespace openlegend::world;
    const openlegend::resource::DataRoot data_root{root};
    const WorldMapData map{data_root};
    OL_CHECK(map.valid());
    OL_CHECK(map.layer(WorldLayer::earth).size() == kWorldCellCount);
    OL_CHECK(map.at(WorldLayer::earth, 357, 235) == 70);
    OL_CHECK(map.at(WorldLayer::surface, 358, 235) == 1544);
    OL_CHECK(map.at(WorldLayer::building, 356, 235) == 2828);
    OL_CHECK(map.at(WorldLayer::build_x, 356, 235) == 356);
    OL_CHECK(map.at(WorldLayer::build_y, 356, 235) == 235);

    WorldCache cache;
    OL_CHECK(cache.reload(map, 293, 171));
    OL_CHECK(cache.origin_x() == 293);
    OL_CHECK(cache.origin_y() == 171);
    OL_CHECK(cache.at(WorldLayer::earth, 64, 64) == 70);
    constexpr std::array<std::uint64_t, 5> expected{
        0x8243D1432C8E5541ULL,
        0x391764A21E1CEC00ULL,
        0xBE58B568D6F8C735ULL,
        0x8A4E260A5A0773EFULL,
        0xDDC443349D365081ULL};
    for (std::size_t layer = 0U; layer < expected.size(); ++layer) {
        const auto actual = fnv1a64_words(cache.layer(static_cast<WorldLayer>(layer)));
        if (actual != expected[layer]) {
            std::cerr << "world cache hash layer " << layer << ": expected 0x" << std::hex
                      << expected[layer] << ", actual 0x" << actual << std::dec << '\n';
        }
        OL_CHECK(actual == expected[layer]);
    }
    constexpr std::array<std::uint64_t, 5> upper_left_expected{
        0x61FEAFE5CF36484BULL,
        0xDF9288BFEC9D5955ULL,
        0x169640B60BC2F3D1ULL,
        0xA9DC45EBF02F6307ULL,
        0xFA85DB6848E033FEULL};
    OL_CHECK(cache.reload(map, 0, 0));
    for (std::size_t layer = 0U; layer < upper_left_expected.size(); ++layer) {
        OL_CHECK(fnv1a64_words(cache.layer(static_cast<WorldLayer>(layer))) ==
                 upper_left_expected[layer]);
    }

    constexpr std::array<std::uint64_t, 5> lower_right_expected{
        0x41D5DC540D8CDD8FULL,
        0x7258C267A7C00302ULL,
        0xE4BF994183591B46ULL,
        0xBE395614125BE83EULL,
        0x9ECCCAE589131829ULL};
    OL_CHECK(cache.reload(map, kWorldCacheMaximumOrigin, kWorldCacheMaximumOrigin));
    for (std::size_t layer = 0U; layer < lower_right_expected.size(); ++layer) {
        OL_CHECK(fnv1a64_words(cache.layer(static_cast<WorldLayer>(layer))) ==
                 lower_right_expected[layer]);
    }
    OL_CHECK(cache.origin_x() == 352);
    OL_CHECK(cache.origin_y() == 352);
    OL_CHECK(!cache.reload(map, 353, 0));
}

void check_initial_render_and_trace(const std::filesystem::path& root) {
    using namespace openlegend::world;
    const openlegend::resource::DataRoot data_root{root};
    const WorldMapData map{data_root};
    const auto sprites = openlegend::resource::PackedArchive::open(
        root / "MMAP.IDX", root / "MMAP.GRP");
    OL_CHECK(sprites.valid());
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    WorldSession session{data_root, map, snapshot.ranger, random};
    OL_CHECK(session.valid());
    OL_CHECK(session.world_x() == 357);
    OL_CHECK(session.world_y() == 235);
    OL_CHECK(session.cache().origin_x() == 293);
    OL_CHECK(session.cache().origin_y() == 171);
    OL_CHECK(session.cache_x() == 64);
    OL_CHECK(session.cache_y() == 64);
    OL_CHECK(session.direction() == WorldDirection::right);
    OL_CHECK(session.player_frame() == 5016);

    const auto ship_x = snapshot.ranger.header.word(openlegend::model::header_word::ship_x);
    const auto ship_y = snapshot.ranger.header.word(openlegend::model::header_word::ship_y);
    const openlegend::render::LegacyWorldDepthInput depth_input{
        session.cache().layer(WorldLayer::build_x),
        session.cache().layer(WorldLayer::build_y),
        session.cache().layer(WorldLayer::building),
        session.cache_x(),
        session.cache_y(),
        session.cache().origin_x(),
        session.cache().origin_y(),
        {static_cast<std::int16_t>(session.world_x()),
         static_cast<std::int16_t>(session.world_y()),
         session.cache_x(),
         session.cache_y(),
         5000},
        openlegend::render::LegacyDepthActor{
            ship_x,
            ship_y,
            static_cast<int>(ship_x) - session.cache().origin_x(),
            static_cast<int>(ship_y) - session.cache().origin_y(),
            6000}};
    const auto depth = openlegend::render::build_legacy_world_depth_list(depth_input);
    OL_CHECK(static_cast<bool>(depth));
    OL_CHECK(depth.entries.size() == 65U);
    OL_CHECK(fnv1a64_depth_entries(depth.entries) == 0x5A1FB9A1B7E72989ULL);

    openlegend::render::IndexedFramebuffer framebuffer;
    OL_CHECK(session.render(framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x6F6CF22B7C8CB4B8ULL);
    OL_CHECK(sprite_has_visible_pixel(sprites, session.player_frame(), framebuffer, 145, 117));

    session.periodic_tick();
    OL_CHECK(random.state() == 0xAF1CF0FBU);
    constexpr std::array<WorldDirection, 4> directions{
        WorldDirection::right, WorldDirection::up, WorldDirection::left, WorldDirection::down};
    constexpr std::array<std::array<std::int16_t, 2>, 4> positions{
        std::array<std::int16_t, 2>{358, 235},
        std::array<std::int16_t, 2>{358, 234},
        std::array<std::int16_t, 2>{357, 234},
        std::array<std::int16_t, 2>{357, 235}};
    constexpr std::array<std::int16_t, 4> movement_frames{5018, 5006, 5036, 5052};
    constexpr std::array<int, 4> cache_x_after_move{65, 65, 64, 64};
    constexpr std::array<int, 4> cache_y_after_move{64, 63, 63, 64};
    constexpr std::array<std::uint64_t, 4> weather_frame_hashes{
        0x125A8A4075AEAEA6ULL,
        0x4AC2406E66E4442AULL,
        0xE24E0A05CD54064EULL,
        0x0E14CD607B173F30ULL};
    for (std::size_t index = 0U; index < directions.size(); ++index) {
        const auto result = session.move(directions[index]);
        OL_CHECK(result.kind == WorldStepKind::moved);
        OL_CHECK(result.scene_id == -1);
        OL_CHECK(result.world_x == positions[index][0]);
        OL_CHECK(result.world_y == positions[index][1]);
        OL_CHECK(session.cache_x() == cache_x_after_move[index]);
        OL_CHECK(session.cache_y() == cache_y_after_move[index]);
        OL_CHECK(session.player_frame() == movement_frames[index]);
        OL_CHECK(session.render(framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == weather_frame_hashes[index]);
        OL_CHECK(sprite_has_visible_pixel(
            sprites, session.player_frame(), framebuffer, 145, 117));
    }
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::main_map_x) == 357);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::main_map_y) == 235);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::face_towards) == 1);
    session.sync_persistent_state(true);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::main_map_x) == 357);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::main_map_y) == 235);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::face_towards) == 3);

    auto entrance_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom entrance_random{1U};
    WorldSession entrance{data_root, map, entrance_snapshot.ranger, entrance_random};
    const auto entrance_result = entrance.move(WorldDirection::left);
    OL_CHECK(entrance_result.kind == WorldStepKind::enter_scene);
    OL_CHECK(entrance_result.scene_id == 70);
    OL_CHECK(entrance_result.world_x == 357);
    OL_CHECK(entrance_result.world_y == 235);
    OL_CHECK(entrance_result.continuation ==
             (std::optional{WorldMoveContinuation{WorldDirection::left, 356, 235}}));
    entrance.restore_direction_after_scene(WorldDirection::right);
    const auto resumed_entrance_result =
        entrance.resume_move_after_scene(*entrance_result.continuation);
    OL_CHECK(resumed_entrance_result.kind == WorldStepKind::stay);
    OL_CHECK(resumed_entrance_result.world_x == 357);
    OL_CHECK(resumed_entrance_result.world_y == 235);
    OL_CHECK(entrance.direction() == WorldDirection::right);
    OL_CHECK(entrance.player_frame() == 5016);

    auto blocked_snapshot = load_baseline(root);
    for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                            openlegend::model::scene_metadata_word::main_entrance_y_1,
                            openlegend::model::scene_metadata_word::main_entrance_x_2,
                            openlegend::model::scene_metadata_word::main_entrance_y_2}) {
        blocked_snapshot.ranger.scenes[70].set_word(word, -1);
    }
    openlegend::random::LegacyRandom blocked_random{1U};
    WorldSession blocked{data_root, map, blocked_snapshot.ranger, blocked_random};
    const auto blocked_result = blocked.move(WorldDirection::left);
    OL_CHECK(blocked_result.kind == WorldStepKind::stay);
    OL_CHECK(blocked_result.world_x == 357);
    OL_CHECK(blocked_result.world_y == 235);
    OL_CHECK(blocked.player_frame() == 5032);
    blocked.prepare_game_menu_frame();
    OL_CHECK(blocked.player_frame() == 5030);

    auto noncanonical_ship_snapshot = blocked_snapshot;
    noncanonical_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::in_ship, 2);
    openlegend::random::LegacyRandom noncanonical_ship_random{1U};
    WorldSession noncanonical_ship{
        data_root, map, noncanonical_ship_snapshot.ranger, noncanonical_ship_random};
    openlegend::render::IndexedFramebuffer noncanonical_ship_framebuffer;
    OL_CHECK(noncanonical_ship.render(noncanonical_ship_framebuffer));
    OL_CHECK(!noncanonical_ship.rendered_player_frame().has_value());
    OL_CHECK(noncanonical_ship.move(WorldDirection::left).kind == WorldStepKind::stay);
    OL_CHECK(noncanonical_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::in_ship) == 2);

    auto low_iq_snapshot = load_baseline(root);
    auto& conditional_scene = low_iq_snapshot.ranger.scenes[0];
    conditional_scene.set_word(openlegend::model::scene_metadata_word::entrance_condition, 2);
    conditional_scene.set_word(openlegend::model::scene_metadata_word::main_entrance_x_1, 358);
    conditional_scene.set_word(openlegend::model::scene_metadata_word::main_entrance_y_1, 235);
    low_iq_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::iq, 69);
    openlegend::random::LegacyRandom low_iq_random{1U};
    WorldSession low_iq{data_root, map, low_iq_snapshot.ranger, low_iq_random};
    OL_CHECK(low_iq.move(WorldDirection::right).kind == WorldStepKind::moved);

    auto high_iq_snapshot = load_baseline(root);
    auto& allowed_scene = high_iq_snapshot.ranger.scenes[0];
    allowed_scene.set_word(openlegend::model::scene_metadata_word::entrance_condition, 2);
    allowed_scene.set_word(openlegend::model::scene_metadata_word::main_entrance_x_1, 358);
    allowed_scene.set_word(openlegend::model::scene_metadata_word::main_entrance_y_1, 235);
    high_iq_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::iq, 70);
    openlegend::random::LegacyRandom high_iq_random{1U};
    WorldSession high_iq{data_root, map, high_iq_snapshot.ranger, high_iq_random};
    const auto high_iq_result = high_iq.move(WorldDirection::right);
    OL_CHECK(high_iq_result.kind == WorldStepKind::enter_scene);
    OL_CHECK(high_iq_result.scene_id == 0);
    OL_CHECK(high_iq_result.continuation ==
             (std::optional{WorldMoveContinuation{WorldDirection::right, 358, 235}}));
    high_iq.restore_direction_after_scene(WorldDirection::left);
    const auto resumed_high_iq_result =
        high_iq.resume_move_after_scene(*high_iq_result.continuation);
    OL_CHECK(resumed_high_iq_result.kind == WorldStepKind::moved);
    OL_CHECK(resumed_high_iq_result.world_x == 358);
    OL_CHECK(resumed_high_iq_result.world_y == 235);
    OL_CHECK(high_iq.direction() == WorldDirection::left);
    OL_CHECK(high_iq.player_frame() == 5030);

    auto party_gap_snapshot = load_baseline(root);
    auto& party_gap_scene = party_gap_snapshot.ranger.scenes[0];
    party_gap_scene.set_word(openlegend::model::scene_metadata_word::entrance_condition, 2);
    party_gap_scene.set_word(openlegend::model::scene_metadata_word::main_entrance_x_1, 358);
    party_gap_scene.set_word(openlegend::model::scene_metadata_word::main_entrance_y_1, 235);
    party_gap_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::iq, 69);
    party_gap_snapshot.ranger.roles[1].set_word(openlegend::model::role_word::iq, 100);
    party_gap_snapshot.ranger.header.set_team_member(
        1U, openlegend::model::CharacterId{-1});
    party_gap_snapshot.ranger.header.set_team_member(
        2U, openlegend::model::CharacterId{1});
    openlegend::random::LegacyRandom party_gap_random{1U};
    WorldSession party_gap{data_root, map, party_gap_snapshot.ranger, party_gap_random};
    OL_CHECK(party_gap.move(WorldDirection::right).kind == WorldStepKind::moved);

    auto first_allowed_snapshot = load_baseline(root);
    for (auto& scene : first_allowed_snapshot.ranger.scenes) {
        for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                openlegend::model::scene_metadata_word::main_entrance_y_1,
                                openlegend::model::scene_metadata_word::main_entrance_x_2,
                                openlegend::model::scene_metadata_word::main_entrance_y_2}) {
            scene.set_word(word, -1);
        }
    }
    for (std::size_t index = 0U; index < 3U; ++index) {
        auto& scene = first_allowed_snapshot.ranger.scenes[index];
        scene.set_word(openlegend::model::scene_metadata_word::main_entrance_x_1, 358);
        scene.set_word(openlegend::model::scene_metadata_word::main_entrance_y_1, 235);
        scene.set_word(
            openlegend::model::scene_metadata_word::entrance_condition,
            static_cast<std::int16_t>(index == 0U ? 1 : 0));
    }
    openlegend::random::LegacyRandom first_allowed_random{1U};
    WorldSession first_allowed{
        data_root, map, first_allowed_snapshot.ranger, first_allowed_random};
    const auto first_allowed_result = first_allowed.move(WorldDirection::right);
    OL_CHECK(first_allowed_result.kind == WorldStepKind::enter_scene);
    OL_CHECK(first_allowed_result.scene_id == 1);

    auto reload_snapshot = load_baseline(root);
    reload_snapshot.ranger.header.set_word(openlegend::model::header_word::main_map_x, 71);
    reload_snapshot.ranger.header.set_word(openlegend::model::header_word::main_map_y, 149);
    openlegend::random::LegacyRandom reload_random{1U};
    WorldSession reload{data_root, map, reload_snapshot.ranger, reload_random};
    OL_CHECK(reload.cache().origin_x() == 7);
    for (int step = 0; step < 34; ++step) {
        OL_CHECK(reload.move(WorldDirection::right).kind == WorldStepKind::moved);
        OL_CHECK(reload.render(framebuffer));
        OL_CHECK(sprite_has_visible_pixel(
            sprites, reload.player_frame(), framebuffer, 145, 117));
    }
    OL_CHECK(reload.world_x() == 105);
    OL_CHECK(reload.cache().origin_x() == 7);
    OL_CHECK(reload.cache_x() == 98);
    OL_CHECK(reload.move(WorldDirection::right).kind == WorldStepKind::moved);
    OL_CHECK(reload.world_x() == 106);
    OL_CHECK(reload.cache().origin_x() == 42);
    OL_CHECK(reload.cache_x() == 64);

    auto ship_snapshot = load_baseline(root);
    ship_snapshot.ranger.header.set_word(openlegend::model::header_word::main_map_x, 108);
    ship_snapshot.ranger.header.set_word(openlegend::model::header_word::main_map_y, 100);
    openlegend::random::LegacyRandom ship_random{1U};
    WorldSession ship{data_root, map, ship_snapshot.ranger, ship_random};
    OL_CHECK(ship.move(WorldDirection::right).kind == WorldStepKind::moved);
    OL_CHECK(ship.world_x() == 109);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::in_ship) == 0);
    ship.sync_persistent_state(true);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::in_ship) == 1);
    OL_CHECK(ship.move(WorldDirection::right).kind == WorldStepKind::moved);
    OL_CHECK(ship.world_x() == 110);
    OL_CHECK(ship.cache_x() == 66);
    ship.sync_persistent_state(true);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::ship_x) == 110);
    OL_CHECK(ship.move(WorldDirection::left).kind == WorldStepKind::moved);
    OL_CHECK(ship.move(WorldDirection::left).kind == WorldStepKind::moved);
    OL_CHECK(ship.world_x() == 108);
    OL_CHECK(ship.rendered_player_frame() == 5030);
    ship.sync_persistent_state(true);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::in_ship) == 0);
    OL_CHECK(ship.move(WorldDirection::left).kind == WorldStepKind::moved);
    OL_CHECK(ship.world_x() == 107);
    ship.sync_persistent_state(true);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::in_ship) == 0);
    OL_CHECK(ship.move(WorldDirection::right).kind == WorldStepKind::moved);
    OL_CHECK(ship.world_x() == 108);
    OL_CHECK(ship.rendered_player_frame() == 7438);
    ship.sync_persistent_state(true);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::in_ship) == 1);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::ship_x) == 108);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::ship_y) == 100);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::ship_x_1) == 109);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::ship_y_1) == 100);
    ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::face_towards,
        static_cast<std::int16_t>(WorldDirection::up));
    ship.sync_persistent_state(false);
    OL_CHECK(ship_snapshot.ranger.header.word(openlegend::model::header_word::face_towards) ==
             static_cast<std::int16_t>(WorldDirection::up));

    auto orthogonal_boarding_snapshot = load_baseline(root);
    for (auto& scene : orthogonal_boarding_snapshot.ranger.scenes) {
        for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                openlegend::model::scene_metadata_word::main_entrance_y_1,
                                openlegend::model::scene_metadata_word::main_entrance_x_2,
                                openlegend::model::scene_metadata_word::main_entrance_y_2}) {
            scene.set_word(word, -1);
        }
    }
    orthogonal_boarding_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_x, 109);
    orthogonal_boarding_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_y, 99);
    orthogonal_boarding_snapshot.ranger.header.set_word(
        openlegend::model::header_word::in_ship, 0);
    orthogonal_boarding_snapshot.ranger.header.set_word(
        openlegend::model::header_word::ship_x, 109);
    orthogonal_boarding_snapshot.ranger.header.set_word(
        openlegend::model::header_word::ship_y, 100);
    orthogonal_boarding_snapshot.ranger.header.set_word(
        openlegend::model::header_word::ship_x_1, 108);
    orthogonal_boarding_snapshot.ranger.header.set_word(
        openlegend::model::header_word::ship_y_1, 100);
    openlegend::random::LegacyRandom orthogonal_boarding_random{1U};
    WorldSession orthogonal_boarding{
        data_root, map, orthogonal_boarding_snapshot.ranger, orthogonal_boarding_random};
    OL_CHECK(orthogonal_boarding.move(WorldDirection::down).kind == WorldStepKind::moved);
    OL_CHECK(orthogonal_boarding.world_x() == 109);
    OL_CHECK(orthogonal_boarding.world_y() == 100);
    OL_CHECK(orthogonal_boarding.rendered_player_frame() == 7454);
    orthogonal_boarding.sync_persistent_state(true);
    OL_CHECK(orthogonal_boarding_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_x) == 109);
    OL_CHECK(orthogonal_boarding_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_y) == 100);
    OL_CHECK(orthogonal_boarding_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_x_1) == 108);
    OL_CHECK(orthogonal_boarding_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_y_1) == 101);

    auto forward_ship_snapshot = load_baseline(root);
    for (auto& scene : forward_ship_snapshot.ranger.scenes) {
        for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                openlegend::model::scene_metadata_word::main_entrance_y_1,
                                openlegend::model::scene_metadata_word::main_entrance_x_2,
                                openlegend::model::scene_metadata_word::main_entrance_y_2}) {
            scene.set_word(word, -1);
        }
    }
    forward_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_x, 50);
    forward_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_y, 12);
    forward_ship_snapshot.ranger.header.set_word(openlegend::model::header_word::in_ship, 1);
    openlegend::random::LegacyRandom forward_ship_random{1U};
    WorldSession forward_ship{
        data_root, map, forward_ship_snapshot.ranger, forward_ship_random};
    OL_CHECK(forward_ship.move(WorldDirection::right).kind == WorldStepKind::moved);
    OL_CHECK(forward_ship.world_x() == 51);
    forward_ship.sync_persistent_state(true);
    OL_CHECK(forward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::in_ship) == 1);
    OL_CHECK(forward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_x) == 51);
    OL_CHECK(forward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_x_1) == 52);

    auto backward_ship_snapshot = forward_ship_snapshot;
    backward_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_x, 52);
    backward_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_y, 12);
    backward_ship_snapshot.ranger.header.set_word(openlegend::model::header_word::in_ship, 1);
    openlegend::random::LegacyRandom backward_ship_random{1U};
    WorldSession backward_ship{
        data_root, map, backward_ship_snapshot.ranger, backward_ship_random};
    OL_CHECK(backward_ship.move(WorldDirection::left).kind == WorldStepKind::moved);
    OL_CHECK(backward_ship.world_x() == 51);
    backward_ship.sync_persistent_state(true);
    OL_CHECK(backward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::in_ship) == 0);

    auto vertical_scene_snapshot = load_baseline(root);
    for (auto& scene : vertical_scene_snapshot.ranger.scenes) {
        for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                openlegend::model::scene_metadata_word::main_entrance_y_1,
                                openlegend::model::scene_metadata_word::main_entrance_x_2,
                                openlegend::model::scene_metadata_word::main_entrance_y_2}) {
            scene.set_word(word, -1);
        }
    }
    vertical_scene_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_x, 11);
    vertical_scene_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_y, 100);
    auto& vertical_scene_entry = vertical_scene_snapshot.ranger.scenes[0];
    vertical_scene_entry.set_word(
        openlegend::model::scene_metadata_word::entrance_condition, 0);
    vertical_scene_entry.set_word(
        openlegend::model::scene_metadata_word::main_entrance_x_1, 11);
    vertical_scene_entry.set_word(
        openlegend::model::scene_metadata_word::main_entrance_y_1, 101);
    openlegend::random::LegacyRandom vertical_scene_random{1U};
    WorldSession vertical_scene{
        data_root, map, vertical_scene_snapshot.ranger, vertical_scene_random};
    const auto vertical_scene_result = vertical_scene.move(WorldDirection::down);
    OL_CHECK(vertical_scene_result.kind == WorldStepKind::enter_scene);
    OL_CHECK(vertical_scene_result.scene_id == 0);
    OL_CHECK(vertical_scene_result.continuation ==
             (std::optional{WorldMoveContinuation{WorldDirection::down, 11, 101}}));
    vertical_scene.restore_direction_after_scene(WorldDirection::up);
    const auto resumed_vertical_scene =
        vertical_scene.resume_move_after_scene(*vertical_scene_result.continuation);
    OL_CHECK(resumed_vertical_scene.kind == WorldStepKind::moved);
    OL_CHECK(resumed_vertical_scene.world_x == 11);
    OL_CHECK(resumed_vertical_scene.world_y == 101);
    OL_CHECK(vertical_scene.direction() == WorldDirection::up);
    OL_CHECK(vertical_scene.cache().origin_y() == 36);
    OL_CHECK(vertical_scene.cache_y() == 65);
    for (int step = 0; step < 33; ++step) {
        OL_CHECK(vertical_scene.move(WorldDirection::down).kind == WorldStepKind::moved);
    }
    OL_CHECK(vertical_scene.world_y() == 134);
    OL_CHECK(vertical_scene.cache().origin_y() == 36);
    OL_CHECK(vertical_scene.cache_y() == 98);
    OL_CHECK(vertical_scene.move(WorldDirection::down).kind == WorldStepKind::moved);
    OL_CHECK(vertical_scene.world_y() == 135);
    OL_CHECK(vertical_scene.cache().origin_y() == 71);
    OL_CHECK(vertical_scene.cache_y() == 64);

    auto downward_ship_snapshot = load_baseline(root);
    for (auto& scene : downward_ship_snapshot.ranger.scenes) {
        for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                openlegend::model::scene_metadata_word::main_entrance_y_1,
                                openlegend::model::scene_metadata_word::main_entrance_x_2,
                                openlegend::model::scene_metadata_word::main_entrance_y_2}) {
            scene.set_word(word, -1);
        }
    }
    downward_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_x, 11);
    downward_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_y, 172);
    downward_ship_snapshot.ranger.header.set_word(openlegend::model::header_word::in_ship, 1);
    openlegend::random::LegacyRandom downward_ship_random{1U};
    WorldSession downward_ship{
        data_root, map, downward_ship_snapshot.ranger, downward_ship_random};
    OL_CHECK(downward_ship.move(WorldDirection::down).kind == WorldStepKind::moved);
    OL_CHECK(downward_ship.world_y() == 173);
    OL_CHECK(downward_ship.cache_y() == 65);
    downward_ship.sync_persistent_state(true);
    OL_CHECK(downward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::in_ship) == 1);
    OL_CHECK(downward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_y) == 173);
    OL_CHECK(downward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_y_1) == 174);

    auto upward_ship_snapshot = downward_ship_snapshot;
    upward_ship_snapshot.ranger.header.set_word(
        openlegend::model::header_word::main_map_y, 174);
    openlegend::random::LegacyRandom upward_ship_random{1U};
    WorldSession upward_ship{
        data_root, map, upward_ship_snapshot.ranger, upward_ship_random};
    OL_CHECK(upward_ship.move(WorldDirection::up).kind == WorldStepKind::stay);
    OL_CHECK(upward_ship.world_y() == 174);
    upward_ship.sync_persistent_state(true);
    OL_CHECK(upward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::in_ship) == 1);
    OL_CHECK(upward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_y) == 174);
    OL_CHECK(upward_ship_snapshot.ranger.header.word(
                 openlegend::model::header_word::ship_y_1) == 173);

    const auto check_ship_collision = [&](const int world_x,
                                           const int world_y,
                                           const WorldDirection direction,
                                           const WorldStepKind expected) {
        auto collision_snapshot = load_baseline(root);
        for (auto& scene : collision_snapshot.ranger.scenes) {
            for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                    openlegend::model::scene_metadata_word::main_entrance_y_1,
                                    openlegend::model::scene_metadata_word::main_entrance_x_2,
                                    openlegend::model::scene_metadata_word::main_entrance_y_2}) {
                scene.set_word(word, -1);
            }
        }
        collision_snapshot.ranger.header.set_word(
            openlegend::model::header_word::main_map_x,
            static_cast<std::int16_t>(world_x));
        collision_snapshot.ranger.header.set_word(
            openlegend::model::header_word::main_map_y,
            static_cast<std::int16_t>(world_y));
        collision_snapshot.ranger.header.set_word(openlegend::model::header_word::in_ship, 1);
        openlegend::random::LegacyRandom collision_random{1U};
        WorldSession collision{
            data_root, map, collision_snapshot.ranger, collision_random};
        OL_CHECK(collision.move(direction).kind == expected);
    };
    check_ship_collision(52, 10, WorldDirection::down, WorldStepKind::moved);
    check_ship_collision(52, 11, WorldDirection::up, WorldStepKind::stay);
    check_ship_collision(457, 310, WorldDirection::right, WorldStepKind::moved);
    check_ship_collision(458, 310, WorldDirection::right, WorldStepKind::stay);
    check_ship_collision(48, 10, WorldDirection::right, WorldStepKind::moved);
    check_ship_collision(30, 271, WorldDirection::right, WorldStepKind::moved);
    check_ship_collision(49, 1, WorldDirection::right, WorldStepKind::moved);
    check_ship_collision(47, 1, WorldDirection::right, WorldStepKind::moved);

    const auto check_disembark = [&](const int world_x,
                                      const int world_y,
                                      const WorldDirection direction,
                                      const WorldStepKind expected,
                                      const std::int16_t expected_in_ship) {
        auto disembark_snapshot = load_baseline(root);
        for (auto& scene : disembark_snapshot.ranger.scenes) {
            for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                    openlegend::model::scene_metadata_word::main_entrance_y_1,
                                    openlegend::model::scene_metadata_word::main_entrance_x_2,
                                    openlegend::model::scene_metadata_word::main_entrance_y_2}) {
                scene.set_word(word, -1);
            }
        }
        disembark_snapshot.ranger.header.set_word(
            openlegend::model::header_word::main_map_x,
            static_cast<std::int16_t>(world_x));
        disembark_snapshot.ranger.header.set_word(
            openlegend::model::header_word::main_map_y,
            static_cast<std::int16_t>(world_y));
        disembark_snapshot.ranger.header.set_word(openlegend::model::header_word::in_ship, 1);
        openlegend::random::LegacyRandom disembark_random{1U};
        WorldSession disembark{
            data_root, map, disembark_snapshot.ranger, disembark_random};
        OL_CHECK(disembark.move(direction).kind == expected);
        if (expected_in_ship == 0) {
            constexpr std::array<std::int16_t, 4> player_frame_base{5002, 5016, 5030, 5044};
            OL_CHECK(disembark.rendered_player_frame() ==
                     player_frame_base[static_cast<std::size_t>(direction)]);
        }
        disembark.sync_persistent_state(true);
        OL_CHECK(disembark_snapshot.ranger.header.word(
                     openlegend::model::header_word::in_ship) == expected_in_ship);
    };
    check_disembark(61, 34, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(168, 162, WorldDirection::left, WorldStepKind::moved, 0);
    check_disembark(169, 163, WorldDirection::left, WorldStepKind::moved, 0);
    check_disembark(190, 172, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(182, 191, WorldDirection::left, WorldStepKind::moved, 0);
    check_disembark(89, 10, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(91, 19, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(26, 22, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(111, 31, WorldDirection::left, WorldStepKind::moved, 0);
    check_disembark(53, 348, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(45, 3, WorldDirection::right, WorldStepKind::stay, 1);
    check_disembark(9, 0, WorldDirection::right, WorldStepKind::stay, 1);
    check_disembark(10, 0, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(457, 313, WorldDirection::right, WorldStepKind::moved, 0);
    check_disembark(458, 311, WorldDirection::right, WorldStepKind::stay, 1);

    const auto check_land_collision = [&](const int world_x,
                                           const int world_y,
                                           const WorldDirection direction,
                                           const WorldStepKind expected) {
        auto collision_snapshot = load_baseline(root);
        for (auto& scene : collision_snapshot.ranger.scenes) {
            for (const auto word : {openlegend::model::scene_metadata_word::main_entrance_x_1,
                                    openlegend::model::scene_metadata_word::main_entrance_y_1,
                                    openlegend::model::scene_metadata_word::main_entrance_x_2,
                                    openlegend::model::scene_metadata_word::main_entrance_y_2}) {
                scene.set_word(word, -1);
            }
        }
        collision_snapshot.ranger.header.set_word(
            openlegend::model::header_word::main_map_x,
            static_cast<std::int16_t>(world_x));
        collision_snapshot.ranger.header.set_word(
            openlegend::model::header_word::main_map_y,
            static_cast<std::int16_t>(world_y));
        collision_snapshot.ranger.header.set_word(openlegend::model::header_word::in_ship, 0);
        for (const auto word : {openlegend::model::header_word::ship_x,
                                openlegend::model::header_word::ship_y,
                                openlegend::model::header_word::ship_x_1,
                                openlegend::model::header_word::ship_y_1}) {
            collision_snapshot.ranger.header.set_word(word, -1);
        }
        openlegend::random::LegacyRandom collision_random{1U};
        WorldSession collision{
            data_root, map, collision_snapshot.ranger, collision_random};
        OL_CHECK(collision.move(direction).kind == expected);
    };
    check_land_collision(165, 155, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(29, 251, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(186, 166, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(290, 174, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(48, 2, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(424, 29, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(54, 349, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(51, 0, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(10, 26, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(100, 19, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(1, 15, WorldDirection::down, WorldStepKind::stay);
    check_land_collision(11, 22, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(3, 15, WorldDirection::down, WorldStepKind::stay);
    check_land_collision(164, 154, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(35, 2, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(13, 5, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(0, 13, WorldDirection::down, WorldStepKind::stay);
    check_land_collision(10, 0, WorldDirection::right, WorldStepKind::moved);
    check_land_collision(9, 0, WorldDirection::right, WorldStepKind::stay);
    check_land_collision(458, 311, WorldDirection::right, WorldStepKind::stay);
}

void check_periodic_rng_and_recovery(const std::filesystem::path& root) {
    using namespace openlegend::world;
    const openlegend::resource::DataRoot data_root{root};
    const WorldMapData map{data_root};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    WorldSession session{data_root, map, snapshot.ranger, random};
    openlegend::render::IndexedFramebuffer palette_frame;
    OL_CHECK(session.render(palette_frame));
    const auto palette_before = palette_frame.palette();
    session.cycle_palette();
    OL_CHECK(session.render(palette_frame));
    const auto palette_after = palette_frame.palette();
    const auto same_color = [](const auto& left, const auto& right) {
        return left.red == right.red && left.green == right.green && left.blue == right.blue;
    };
    OL_CHECK(same_color(palette_after[223], palette_before[223]));
    OL_CHECK(same_color(palette_after[224], palette_before[231]));
    for (std::size_t index = 225U; index <= 231U; ++index) {
        OL_CHECK(same_color(palette_after[index], palette_before[index - 1U]));
    }
    OL_CHECK(same_color(palette_after[232], palette_before[232]));
    OL_CHECK(same_color(palette_after[243], palette_before[243]));
    OL_CHECK(same_color(palette_after[244], palette_before[252]));
    for (std::size_t index = 245U; index <= 252U; ++index) {
        OL_CHECK(same_color(palette_after[index], palette_before[index - 1U]));
    }
    OL_CHECK(same_color(palette_after[253], palette_before[253]));
    session.cycle_palette();
    OL_CHECK(session.render(palette_frame));
    const auto palette_after_six = palette_frame.palette();
    OL_CHECK(same_color(palette_after_six[224], palette_after[231]));
    OL_CHECK(same_color(palette_after_six[244], palette_after[252]));

    session.periodic_tick();
    OL_CHECK(random.state() == 0xAF1CF0FBU);
    session.periodic_tick();
    OL_CHECK(random.state() == 0xAF1CF0FBU);
    for (int tick = 2; tick < 791; ++tick) {
        session.periodic_tick();
    }
    OL_CHECK(random.state() == 0xAF1CF0FBU);
    session.periodic_tick();
    OL_CHECK(random.state() == 0x42877E5CU);

    auto weather_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom weather_random{1U};
    WorldSession weather{data_root, map, weather_snapshot.ranger, weather_random};
    for (int tick = 0; tick < 300; ++tick) {
        weather.periodic_tick();
    }
    openlegend::render::IndexedFramebuffer weather_frame;
    OL_CHECK(weather.render(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == 0xDFF4C0D05BD3426BULL);

    openlegend::random::LegacyRandom idle_random{1U};
    WorldSession idle{data_root, map, snapshot.ranger, idle_random};
    for (int tick = 0; tick < 51; ++tick) {
        idle.idle_tick();
    }
    OL_CHECK(idle_random.state() == 0x41C67EA6U);

    constexpr std::array<std::int16_t, 4> idle_frame_base{5002, 5016, 5030, 5044};
    constexpr std::array<std::int16_t, 4> idle_frame_offset{54, 52, 50, 48};
    constexpr std::array<std::int16_t, 24> idle_animation_steps{
        2, 2, 2, 4, 4, 4, 4, 6, 6, 6, 6, 8,
        8, 8, 8, 10, 10, 10, 10, 12, 12, 12, 12, 0};
    for (std::size_t direction = 0U; direction < idle_frame_base.size(); ++direction) {
        auto idle_frame_snapshot = load_baseline(root);
        idle_frame_snapshot.ranger.header.set_word(
            openlegend::model::header_word::face_towards,
            static_cast<std::int16_t>(direction));
        openlegend::random::LegacyRandom idle_frame_random{0U};
        WorldSession idle_frame{data_root, map, idle_frame_snapshot.ranger, idle_frame_random};
        for (int tick = 0; tick < 50; ++tick) {
            idle_frame.idle_tick();
            idle_frame.idle_animation_tick();
        }
        OL_CHECK(idle_frame.player_frame() == idle_frame_base[direction]);
        for (const auto step : idle_animation_steps) {
            idle_frame.idle_tick();
            idle_frame.idle_animation_tick();
            const auto expected = step == 0
                                      ? idle_frame_base[direction]
                                      : static_cast<std::int16_t>(
                                            idle_frame_base[direction] +
                                            idle_frame_offset[direction] + step);
            OL_CHECK(idle_frame.player_frame() == expected);
        }
        idle_frame.idle_tick();
        idle_frame.idle_animation_tick();
        OL_CHECK(idle_frame.player_frame() == idle_frame_base[direction]);
        OL_CHECK(idle_frame_random.state() == 0x00003039U);
    }

    auto interrupted_idle_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom interrupted_idle_random{0U};
    WorldSession interrupted_idle{
        data_root, map, interrupted_idle_snapshot.ranger, interrupted_idle_random};
    for (int tick = 0; tick < 50; ++tick) {
        interrupted_idle.idle_tick();
        interrupted_idle.idle_animation_tick();
    }
    for (int tick = 0; tick < 12; ++tick) {
        interrupted_idle.idle_tick();
        interrupted_idle.idle_animation_tick();
    }
    constexpr auto right_index = static_cast<std::size_t>(WorldDirection::right);
    OL_CHECK(interrupted_idle.player_frame() ==
             idle_frame_base[right_index] + idle_frame_offset[right_index] + 8);
    OL_CHECK(interrupted_idle.move(WorldDirection::right).kind == WorldStepKind::moved);
    OL_CHECK(interrupted_idle.player_frame() == idle_frame_base[right_index] + 2);
    for (int tick = 0; tick < 20; ++tick) {
        interrupted_idle.idle_tick();
        interrupted_idle.idle_animation_tick();
        OL_CHECK(interrupted_idle.player_frame() == idle_frame_base[right_index] + 2);
    }
    interrupted_idle.idle_tick();
    interrupted_idle.idle_animation_tick();
    OL_CHECK(interrupted_idle.player_frame() == idle_frame_base[right_index]);
    auto resumed_idle_animation = false;
    for (int tick = 0; tick < 512 && !resumed_idle_animation; ++tick) {
        interrupted_idle.idle_tick();
        interrupted_idle.idle_animation_tick();
        if (interrupted_idle.player_frame() > idle_frame_base[right_index] + 12) {
            resumed_idle_animation = true;
            OL_CHECK(interrupted_idle.player_frame() ==
                     idle_frame_base[right_index] + idle_frame_offset[right_index] + 10);
        }
    }
    OL_CHECK(resumed_idle_animation);

    auto recovery_snapshot = load_baseline(root);
    auto& protagonist = recovery_snapshot.ranger.roles[0];
    protagonist.set_word(openlegend::model::role_word::hurt, 51);
    protagonist.set_word(openlegend::model::role_word::poison, 0);
    protagonist.set_word(openlegend::model::role_word::hp, 10);
    protagonist.set_word(openlegend::model::role_word::mp, 20);
    protagonist.set_word(openlegend::model::role_word::physical_power, 30);
    openlegend::random::LegacyRandom recovery_random{1U};
    WorldSession recovery{data_root, map, recovery_snapshot.ranger, recovery_random};
    for (int attempt = 0; attempt < 49; ++attempt) {
        static_cast<void>(recovery.move(WorldDirection::left));
    }
    OL_CHECK(protagonist.word(openlegend::model::role_word::hp) == 10);
    OL_CHECK(protagonist.word(openlegend::model::role_word::mp) == 20);
    OL_CHECK(protagonist.word(openlegend::model::role_word::physical_power) == 30);
    static_cast<void>(recovery.move(WorldDirection::left));
    OL_CHECK(protagonist.word(openlegend::model::role_word::hurt) == 51);
    OL_CHECK(protagonist.word(openlegend::model::role_word::hp) == 9);
    OL_CHECK(protagonist.word(openlegend::model::role_word::mp) == 19);
    OL_CHECK(protagonist.word(openlegend::model::role_word::physical_power) == 29);

    auto recovery_threshold_snapshot = load_baseline(root);
    auto& recovery_threshold_role = recovery_threshold_snapshot.ranger.roles[0U];
    recovery_threshold_role.set_word(openlegend::model::role_word::hurt, 50);
    recovery_threshold_role.set_word(openlegend::model::role_word::poison, 50);
    recovery_threshold_role.set_word(openlegend::model::role_word::hp, 2);
    recovery_threshold_role.set_word(openlegend::model::role_word::mp, 2);
    recovery_threshold_role.set_word(openlegend::model::role_word::physical_power, 2);
    openlegend::random::LegacyRandom recovery_threshold_random{1U};
    WorldSession recovery_threshold{
        data_root, map, recovery_threshold_snapshot.ranger, recovery_threshold_random};
    for (int attempt = 0; attempt < 50; ++attempt) {
        static_cast<void>(recovery_threshold.move(WorldDirection::left));
    }
    OL_CHECK(recovery_threshold_role.word(openlegend::model::role_word::hp) == 2);
    OL_CHECK(recovery_threshold_role.word(openlegend::model::role_word::mp) == 2);
    OL_CHECK(recovery_threshold_role.word(openlegend::model::role_word::physical_power) == 2);

    auto recovery_floor_snapshot = load_baseline(root);
    auto& recovery_floor_role = recovery_floor_snapshot.ranger.roles[0U];
    recovery_floor_role.set_word(openlegend::model::role_word::hurt, 51);
    recovery_floor_role.set_word(openlegend::model::role_word::poison, 0);
    recovery_floor_role.set_word(openlegend::model::role_word::hp, 2);
    recovery_floor_role.set_word(openlegend::model::role_word::mp, 1);
    recovery_floor_role.set_word(openlegend::model::role_word::physical_power, 0);
    openlegend::random::LegacyRandom recovery_floor_random{1U};
    WorldSession recovery_floor{
        data_root, map, recovery_floor_snapshot.ranger, recovery_floor_random};
    for (int attempt = 0; attempt < 50; ++attempt) {
        static_cast<void>(recovery_floor.move(WorldDirection::left));
    }
    OL_CHECK(recovery_floor_role.word(openlegend::model::role_word::hp) == 1);
    OL_CHECK(recovery_floor_role.word(openlegend::model::role_word::mp) == 1);
    OL_CHECK(recovery_floor_role.word(openlegend::model::role_word::physical_power) == 0);

    auto poison_index_snapshot = load_baseline(root);
    poison_index_snapshot.ranger.header.set_team_member(
        1U, openlegend::model::CharacterId{10});
    auto& poison_gate_role = poison_index_snapshot.ranger.roles[1U];
    poison_gate_role.set_word(openlegend::model::role_word::poison, 51);
    auto& indexed_party_role = poison_index_snapshot.ranger.roles[10U];
    indexed_party_role.set_word(openlegend::model::role_word::hurt, 0);
    indexed_party_role.set_word(openlegend::model::role_word::poison, 0);
    indexed_party_role.set_word(openlegend::model::role_word::hp, 10);
    indexed_party_role.set_word(openlegend::model::role_word::mp, 20);
    indexed_party_role.set_word(openlegend::model::role_word::physical_power, 30);
    openlegend::random::LegacyRandom poison_index_random{1U};
    WorldSession poison_index{
        data_root, map, poison_index_snapshot.ranger, poison_index_random};
    for (int attempt = 0; attempt < 50; ++attempt) {
        static_cast<void>(poison_index.move(WorldDirection::left));
    }
    OL_CHECK(indexed_party_role.word(openlegend::model::role_word::hp) == 9);
    OL_CHECK(indexed_party_role.word(openlegend::model::role_word::mp) == 19);
    OL_CHECK(indexed_party_role.word(openlegend::model::role_word::physical_power) == 29);
    OL_CHECK(poison_gate_role.word(openlegend::model::role_word::poison) == 51);

    auto power_snapshot = load_baseline(root);
    power_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::physical_power, 99);
    openlegend::random::LegacyRandom power_random{1U};
    WorldSession power{data_root, map, power_snapshot.ranger, power_random};
    for (int tick = 0; tick < 200; ++tick) {
        power.idle_tick();
    }
    OL_CHECK(power_snapshot.ranger.roles[0].word(openlegend::model::role_word::physical_power) ==
             100);
}

}  // namespace

int main() {
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
    check_layers_and_cache(root);
    check_initial_render_and_trace(root);
    check_periodic_rng_and_recovery(root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
