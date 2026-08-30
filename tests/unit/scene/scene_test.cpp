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
    OL_CHECK(map_result.kind == openlegend::scene::SceneStepKind::stay);
    OL_CHECK(snapshot.scene_value(70U, openlegend::model::SceneLayer::building,
                                  22U * 64U + 13U).value_or(-1) == 0);
    const auto changed_building = snapshot.scene_value(
        70U, openlegend::model::SceneLayer::building, 22U * 64U + 12U).value_or(-1);
    if (changed_building != 2898) {
        std::cerr << "script 274 building value: " << changed_building << '\n';
    }
    OL_CHECK(changed_building == 2898);

    const auto dialogue = session.begin_event(69, 0, 44, 29);
    OL_CHECK(dialogue.kind == openlegend::scene::SceneStepKind::dialogue);
    OL_CHECK(dialogue.talk_id == 228);
    OL_CHECK(!session.pending_text().empty());
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);
}

}  // namespace

int main() {
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
    check_assets(root);
    check_scene_render_and_movement(root);
    check_event_execution(root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
