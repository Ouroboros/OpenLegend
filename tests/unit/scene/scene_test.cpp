#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
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

class SyntheticKdefDataRoot {
public:
    explicit SyntheticKdefDataRoot(const std::filesystem::path& source)
        : path_(std::filesystem::temp_directory_path() / "openlegend-scene-synthetic-kdef") {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        error.clear();
        std::filesystem::create_directories(path_, error);
        OL_CHECK(!error);
        for (const auto name : std::array<std::string_view, 13>{
                 "TALK.IDX", "TALK.GRP", "HDGRP.IDX", "HDGRP.GRP",
                 "CLOUD.IDX", "CLOUD.GRP", "MMAP.COL", "FONT.X16",
                 "FONT.C16", "3_shadow.msk", "4_shadow.msk", "SDX070", "SMP070"}) {
            error.clear();
            std::filesystem::copy_file(
                source / name, path_ / name,
                std::filesystem::copy_options::overwrite_existing, error);
            OL_CHECK(!error);
        }

        std::vector<std::uint8_t> index;
        std::vector<std::uint8_t> group;
        index.reserve(openlegend::scene::kEventScriptCount * 4U);
        for (std::size_t script = 0U; script < openlegend::scene::kEventScriptCount; ++script) {
            if (script == 1U) {
                append_i16(group, 24);
            } else if (script == 2U) {
                for (const auto word : std::array<std::int16_t, 4>{68, 2, 123, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 3U) {
                for (const auto word : std::array<std::int16_t, 7>{4, 123, 0, 3, 2, 200, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 4U) {
                for (const auto word : std::array<std::int16_t, 2>{13, 14}) {
                    append_i16(group, word);
                }
            } else if (script == 5U) {
                for (const auto word : std::array<std::int16_t, 7>{16, 55, 0, 3, 2, 201, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 6U) {
                for (const auto word : std::array<std::int16_t, 12>{
                         6, 77, 0, 4, 9, 2, 202, 1, -1, 2, 203, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 7U) {
                for (const auto word : std::array<std::int16_t, 6>{5, 0, 3, 2, 210, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 8U) {
                for (const auto word : std::array<std::int16_t, 6>{9, 0, 3, 2, 211, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 9U) {
                for (const auto word : std::array<std::int16_t, 6>{11, 0, 3, 2, 212, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 10U) {
                for (const auto word : std::array<std::int16_t, 14>{
                         3, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, 30, 31}) {
                    append_i16(group, word);
                }
            } else if (script == 11U) {
                for (const auto word : std::array<std::int16_t, 6>{26, -2, -2, 1, 2, 3}) {
                    append_i16(group, word);
                }
            } else if (script == 12U) {
                for (const auto word : std::array<std::int16_t, 6>{17, 69, 1, 2, 3, 456}) {
                    append_i16(group, word);
                }
            } else if (script == 13U) {
                for (const auto word : std::array<std::int16_t, 6>{26, 69, 5, -1, -2, -3}) {
                    append_i16(group, word);
                }
            } else if (script == 14U) {
                for (const auto word : std::array<std::int16_t, 5>{30, 10, 10, 10, 9}) {
                    append_i16(group, word);
                }
            } else if (script == 15U) {
                for (const auto word : std::array<std::int16_t, 14>{
                         3, -2, -1, 11, 12, 13, 14, 15, 16, 17, 18, 19, -2, -2}) {
                    append_i16(group, word);
                }
            } else if (script == 16U) {
                for (const auto word : std::array<std::int16_t, 14>{
                         3, 69, 5, 101, -2, 103, -2, 105, -2, 107, -2, 109, 30, 31}) {
                    append_i16(group, word);
                }
            } else if (script == 17U) {
                for (const auto word : std::array<std::int16_t, 8>{5, 0, 52, 6, 71, 0, 0, 0}) {
                    append_i16(group, word);
                }
                for (std::size_t program_counter = 8U; program_counter < 55U;
                     ++program_counter) {
                    append_i16(group, 7);
                }
                for (const auto word : std::array<std::int16_t, 5>{6, 72, 0, 0, 0}) {
                    append_i16(group, word);
                }
            } else if (script == 18U) {
                for (const auto word : std::array<std::int16_t, 16>{
                         6, 77, 8, 5, 1, 7, 7, 7, 7, 7, 2, 214, 1, 2, 215, 1}) {
                    append_i16(group, word);
                }
            } else if (script == 19U) {
                append_i16(group, 12);
            } else if (script == 20U) {
                for (const auto word : std::array<std::int16_t, 3>{19, -32768, 32767}) {
                    append_i16(group, word);
                }
            } else if (script == 21U) {
                for (const auto word : std::array<std::int16_t, 3>{19, 32767, -32768}) {
                    append_i16(group, word);
                }
            } else if (script == 22U) {
                for (const auto word : std::array<std::int16_t, 5>{
                         25, -32768, -32768, -32767, -32767}) {
                    append_i16(group, word);
                }
            } else if (script == 23U) {
                for (const auto word : std::array<std::int16_t, 12>{
                         27, -2, 32767, 32767,
                         27, -1, 11, 10,
                         27, -1, 10, 11}) {
                    append_i16(group, word);
                }
            } else if (script == 24U) {
                for (const auto word : std::array<std::int16_t, 10>{
                         29, 0, 90, 1000, 0, 4,
                         1, 100, 0, 0}) {
                    append_i16(group, word);
                }
            } else if (script >= 25U && script <= 28U) {
                constexpr std::array<std::array<std::int16_t, 5>, 4> walks{{
                    {30, 10, 10, 12, 12},
                    {30, 10, 10, 12, 8},
                    {30, 10, 10, 8, 12},
                    {30, 10, 10, 8, 8},
                }};
                for (const auto word : walks[script - 25U]) {
                    append_i16(group, word);
                }
            } else if (script == 29U) {
                for (const auto word : std::array<std::int16_t, 9>{
                         30, 10, 10, 10, 10,
                         27, -1, 10, 10}) {
                    append_i16(group, word);
                }
            } else if (script == 30U) {
                for (const auto word : std::array<std::int16_t, 5>{30, 10, 10, 18, 10}) {
                    append_i16(group, word);
                }
            } else if (script == 31U) {
                for (const auto word : std::array<std::int16_t, 5>{30, 0, 0, 2, 0}) {
                    append_i16(group, word);
                }
            } else if (script == 32U || script == 33U) {
                const auto required = script == 32U ? std::int16_t{0} : std::int16_t{-32768};
                for (const auto word : std::array<std::int16_t, 8>{
                         31, required, 0, 4,
                         1, 100, 0, 0}) {
                    append_i16(group, word);
                }
            } else if (script == 34U || script == 35U) {
                for (const auto word : std::array<std::int16_t, 3>{
                         32, 109, script == 34U ? std::int16_t{1} : std::int16_t{-1}}) {
                    append_i16(group, word);
                }
            } else if (script >= 36U && script <= 38U) {
                const auto silent = script == 36U ? std::int16_t{0}
                                  : script == 37U ? std::int16_t{1}
                                                  : std::int16_t{-32768};
                for (const auto word : std::array<std::int16_t, 4>{33, 49, 15, silent}) {
                    append_i16(group, word);
                }
            } else if (script >= 39U && script <= 43U) {
                constexpr std::array<std::int16_t, 5> deltas{3, 3, 0, -1, 1};
                for (const auto word :
                     std::array<std::int16_t, 3>{34, 0, deltas[script - 39U]}) {
                    append_i16(group, word);
                }
            } else if (script >= 44U && script <= 49U) {
                constexpr std::array<std::int16_t, 6> slots{9, -1, -1, -1, -2, 10};
                constexpr std::array<std::int16_t, 6> magics{-32768, 60, 60, 0, 60, 60};
                constexpr std::array<std::int16_t, 6> levels{32767, 100, 100, -32768, 100, 100};
                const auto index = script - 44U;
                for (const auto word : std::array<std::int16_t, 5>{
                         35, 0, slots[index], magics[index], levels[index]}) {
                    append_i16(group, word);
                }
            } else if (script == 50U) {
                for (const auto word : std::array<std::int16_t, 12>{
                         36, -32768, 4, 0,
                         1, 1122, 0, 0,
                         1, 1123, 0, 0}) {
                    append_i16(group, word);
                }
            } else if (script >= 51U && script <= 56U) {
                constexpr std::array<std::int16_t, 6> deltas{
                    5, 1, -1, 32767, -32768, 0};
                append_i16(group, 37);
                append_i16(group, deltas[script - 51U]);
            } else if (script == 57U) {
                for (const auto word : std::array<std::int16_t, 5>{
                         38, 7, 5, 123, -32768}) {
                    append_i16(group, word);
                }
            } else if (script >= 58U && script <= 61U) {
                constexpr std::array<std::int16_t, 4> scenes{0, 83, -1, 84};
                append_i16(group, 39);
                append_i16(group, scenes[script - 58U]);
            } else if (script >= 62U && script <= 67U) {
                constexpr std::array<std::int16_t, 6> directions{0, 1, 2, 3, -1, 4};
                append_i16(group, 40);
                append_i16(group, directions[script - 62U]);
            } else if (script >= 68U && script <= 74U) {
                constexpr std::array<std::array<std::int16_t, 3>, 7> arguments{{
                    {0, 78, 1},
                    {0, 78, -1},
                    {0, 99, 0},
                    {0, 99, -7},
                    {-1, 99, 1},
                    {0, 99, 1},
                    {0, -1, 5}}};
                append_i16(group, 41);
                for (const auto value : arguments[script - 68U]) {
                    append_i16(group, value);
                }
            } else if (script == 75U) {
                for (const auto word :
                     std::array<std::int16_t, 11>{42, 0, 4, 1, 1574, 0, 0, 1, 1575, 0, 0}) {
                    append_i16(group, word);
                }
            } else if (script == 76U || script == 77U) {
                for (const auto word : std::array<std::int16_t, 12>{
                         43, script == 76U ? std::int16_t{110} : std::int16_t{-1},
                         0, 4, 1, 1574, 0, 0, 1, 1575, 0, 0}) {
                    append_i16(group, word);
                }
            } else if (script >= 78U && script <= 81U) {
                constexpr std::array<std::array<std::int16_t, 6>, 4> arguments{{
                    {-1, 1, 0, -1, 100, 999},
                    {-1, 10, 12, -1, 100, -30000},
                    {-1, 32767, 32767, -1, -32768, 0},
                    {-1, 10, 13, -1, 200, 200}}};
                append_i16(group, 44);
                for (const auto value : arguments[script - 78U]) {
                    append_i16(group, value);
                }
            } else if (script >= 82U && script <= 85U) {
                constexpr std::array<std::array<std::int16_t, 2>, 4> arguments{{
                    {0, 1}, {0, -1}, {0, 0}, {-1, 1}}};
                append_i16(group, 45);
                for (const auto value : arguments[script - 82U]) {
                    append_i16(group, value);
                }
            } else if (script >= 86U && script <= 90U) {
                constexpr std::array<std::array<std::int16_t, 2>, 5> arguments{{
                    {0, 1}, {0, -1}, {0, 0}, {0, -10}, {-1, 1}}};
                append_i16(group, 46);
                for (const auto value : arguments[script - 86U]) {
                    append_i16(group, value);
                }
            } else if (script >= 91U && script <= 94U) {
                constexpr std::array<std::array<std::int16_t, 2>, 4> arguments{{
                    {0, 1}, {0, -1}, {0, 0}, {-1, 1}}};
                append_i16(group, 47);
                for (const auto value : arguments[script - 91U]) {
                    append_i16(group, value);
                }
            } else if (script >= 95U && script <= 100U) {
                constexpr std::array<std::array<std::int16_t, 2>, 6> arguments{{
                    {0, 1}, {0, -1}, {0, 0}, {0, -10}, {0, 1}, {-1, 1}}};
                append_i16(group, 48);
                for (const auto value : arguments[script - 95U]) {
                    append_i16(group, value);
                }
            } else if (script >= 101U && script <= 104U) {
                constexpr std::array<std::array<std::int16_t, 2>, 4> arguments{{
                    {0, 32767}, {0, -32768}, {0, 0}, {-1, 2}}};
                append_i16(group, 49);
                for (const auto value : arguments[script - 101U]) {
                    append_i16(group, value);
                }
            } else if (script >= 105U && script <= 107U) {
                constexpr std::array<std::array<std::int16_t, 5>, 3> item_ids{{
                    {110, 111, 112, 113, 114},
                    {110, 110, 110, 110, 110},
                    {-1, -1, -1, -1, -1}}};
                append_i16(group, 50);
                for (const auto value : item_ids[script - 105U]) {
                    append_i16(group, value);
                }
                append_i16(group, 0);
                append_i16(group, 4);
                for (const auto word :
                     std::array<std::int16_t, 8>{1, 1574, 0, 0, 1, 1575, 0, 0}) {
                    append_i16(group, word);
                }
            }
            append_i16(group, -1);
            append_u32(index, static_cast<std::uint32_t>(group.size()));
        }
        OL_CHECK(write(path_ / "KDEF.IDX", index));
        OL_CHECK(write(path_ / "KDEF.GRP", group));
    }

    ~SyntheticKdefDataRoot() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    static void append_i16(std::vector<std::uint8_t>& bytes, const std::int16_t value) {
        const auto bits = static_cast<std::uint16_t>(value);
        bytes.push_back(static_cast<std::uint8_t>(bits & 0xFFU));
        bytes.push_back(static_cast<std::uint8_t>(bits >> 8U));
    }

    static void append_u32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    static bool write(
        const std::filesystem::path& path,
        const std::span<const std::uint8_t> bytes) {
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return output.good();
    }

    std::filesystem::path path_;
};

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
        OL_CHECK(std::count(page.begin(), page.end(), static_cast<std::uint8_t>('*')) <= 3);
    }
    std::vector<std::uint8_t> overwide_line(30U, static_cast<std::uint8_t>('A'));
    overwide_line.push_back(0U);
    const auto overwide_pages = openlegend::scene::paginate_dialogue(overwide_line);
    OL_CHECK(overwide_pages.size() == 1U);
    OL_CHECK(overwide_pages.front() == overwide_line);
    constexpr std::array<std::uint8_t, 7> exact_page_boundary{
        'A', '*', 'B', '*', 'C', '*', 0U};
    const auto boundary_pages = openlegend::scene::paginate_dialogue(exact_page_boundary);
    OL_CHECK(boundary_pages.size() == 2U);
    OL_CHECK(boundary_pages[0] == std::vector<std::uint8_t>(exact_page_boundary.begin(), exact_page_boundary.end()));
    OL_CHECK(boundary_pages[1] == std::vector<std::uint8_t>{0U});

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

void check_event_dialogue_rendering(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession paired{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(paired).kind == SceneStepKind::stay);
    openlegend::render::IndexedFramebuffer style_0_frame;
    OL_CHECK(paired.render_map(style_0_frame));

    auto result = paired.begin_event(1, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 0 && result.head_id == 1 && result.style == 0);
    OL_CHECK(paired.render(style_0_frame));
    OL_CHECK(fnv1a64(style_0_frame.pixels()) == 0x1510F9342DE536C3ULL);

    result = paired.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(paired.render(style_0_frame));
    result = paired.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 1 && result.head_id == 0 && result.style == 1);
    OL_CHECK(paired.render(style_0_frame));
    OL_CHECK(fnv1a64(style_0_frame.pixels()) == 0xFD2A5CB6664410E1ULL);

    auto style_2_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom style_2_random{1U};
    openlegend::scene::SceneSession style_2{
        data_root, style_2_snapshot, style_2_random, 70};
    OL_CHECK(finish_scene_title(style_2).kind == SceneStepKind::stay);
    openlegend::render::IndexedFramebuffer style_2_frame;
    OL_CHECK(style_2.render_map(style_2_frame));
    result = style_2.begin_event(244, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 796 && result.head_id == 200 && result.style == 2);
    OL_CHECK(style_2.render(style_2_frame));
    OL_CHECK(fnv1a64(style_2_frame.pixels()) == 0x961BBDDC3FC79F35ULL);

    auto style_4_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom style_4_random{1U};
    openlegend::scene::SceneSession style_4{
        data_root, style_4_snapshot, style_4_random, 70};
    OL_CHECK(finish_scene_title(style_4).kind == SceneStepKind::stay);
    openlegend::render::IndexedFramebuffer style_4_frame;
    OL_CHECK(style_4.render_map(style_4_frame));
    result = style_4.begin_event(142, 0, 44, 29);
    for (int step = 0; step < 32 && result.kind == SceneStepKind::present; ++step) {
        OL_CHECK(style_4.render(style_4_frame));
        result = style_4.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 546 && result.style == 0);
    while (result.kind == SceneStepKind::dialogue && result.talk_id == 546) {
        OL_CHECK(style_4.render(style_4_frame));
        result = style_4.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(style_4.render(style_4_frame));
    result = style_4.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::dialogue);
    OL_CHECK(result.talk_id == 547 && result.head_id == 77 && result.style == 4);
    OL_CHECK(style_4.render(style_4_frame));
    const auto style_4_hash = fnv1a64(style_4_frame.pixels());
    if (style_4_hash != 0xD66E3B67975125A2ULL) {
        std::cerr << "style 4 dialogue frame: expected 0xd66e3b67975125a2, actual 0x"
                  << std::hex << style_4_hash << std::dec << '\n';
    }
    OL_CHECK(style_4_hash == 0xD66E3B67975125A2ULL);

    auto long_line_snapshot = load_baseline(root);
    long_line_snapshot.ranger.header.set_team_member(
        1U, openlegend::model::CharacterId{51});
    openlegend::random::LegacyRandom long_line_random{1U};
    openlegend::scene::SceneSession long_line{
        data_root, long_line_snapshot, long_line_random, 70};
    OL_CHECK(finish_scene_title(long_line).kind == SceneStepKind::stay);
    openlegend::render::IndexedFramebuffer long_line_frame;
    OL_CHECK(long_line.render_map(long_line_frame));
    result = long_line.begin_event(515, 0, 44, 29);
    for (int step = 0; step < 128 &&
                       !(result.kind == SceneStepKind::dialogue && result.talk_id == 1841);
         ++step) {
        OL_CHECK(result.kind == SceneStepKind::dialogue ||
                 result.kind == SceneStepKind::present);
        OL_CHECK(long_line.render(long_line_frame));
        result = long_line.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::dialogue && result.talk_id == 1841);
    OL_CHECK(long_line.render(long_line_frame));
    OL_CHECK(fnv1a64(long_line_frame.pixels()) == 0xF420561DCB42E981ULL);
}

void check_new_game_entry(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{
        data_root,
        snapshot,
        random,
        70,
        false,
        std::nullopt,
        0,
        openlegend::scene::SceneEntryOverride{
            19, 20, openlegend::scene::SceneDirection::right, 6890, 691}};
    OL_CHECK(session.valid());
    OL_CHECK(session.scene_x() == 19);
    OL_CHECK(session.scene_y() == 20);
    OL_CHECK(session.direction() == openlegend::scene::SceneDirection::right);
    OL_CHECK(session.player_frame() == 6890);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::in_sub_map) == 1);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::sub_map_x) == 19);
    OL_CHECK(snapshot.ranger.header.word(openlegend::model::header_word::sub_map_y) == 20);
    OL_CHECK(session.pending().kind == SceneStepKind::fade_from_black);

    const auto first_event_step = session.resume(SceneResponse::acknowledge);
    OL_CHECK(first_event_step.kind == SceneStepKind::dialogue);
    OL_CHECK(first_event_step.talk_id == 2520);
    OL_CHECK(first_event_step.head_id == 0);
    OL_CHECK(first_event_step.style == 1);

    auto menu_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom menu_random{1U};
    openlegend::scene::SceneSession menu_session{
        data_root,
        menu_snapshot,
        menu_random,
        70,
        false,
        std::nullopt,
        0,
        openlegend::scene::SceneEntryOverride{
            44, 29, openlegend::scene::SceneDirection::right, 6890, 0}};
    OL_CHECK(finish_scene_title(menu_session).kind == SceneStepKind::stay);
    OL_CHECK(menu_session.player_frame() == 6890);
    OL_CHECK(menu_session.open_ui().kind == SceneStepKind::open_ui);
    OL_CHECK(menu_session.player_frame() == 5016);

    auto idle_snapshot = load_baseline(root);
    idle_snapshot.ranger.roles[0U].set_word(
        openlegend::model::role_word::physical_power, 50);
    openlegend::random::LegacyRandom idle_random{1U};
    openlegend::scene::SceneSession idle_session{
        data_root,
        idle_snapshot,
        idle_random,
        70,
        false,
        std::nullopt,
        0,
        openlegend::scene::SceneEntryOverride{
            44, 29, openlegend::scene::SceneDirection::right, 6890, 0}};
    OL_CHECK(finish_scene_title(idle_session).kind == SceneStepKind::stay);
    const auto run_idle_tick = [&idle_session](const bool skip_player_idle = false) {
        auto step = idle_session.tick(
            std::nullopt, false, false, skip_player_idle);
        OL_CHECK(step.kind == SceneStepKind::present);
        step = idle_session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::stay);
    };
    for (int tick = 0; tick < 20; ++tick) {
        run_idle_tick();
    }
    OL_CHECK(idle_session.player_frame() == 6890);
    run_idle_tick(true);
    OL_CHECK(idle_session.player_frame() == 6890);
    run_idle_tick();
    OL_CHECK(idle_session.player_frame() != 6890);
    const auto reset_player_frame = idle_session.player_frame();
    for (int tick = 21; tick < 200; ++tick) {
        run_idle_tick();
    }
    OL_CHECK(idle_session.player_frame() == reset_player_frame);
    OL_CHECK(idle_snapshot.ranger.roles[0U].word(
                 openlegend::model::role_word::physical_power) == 51);
}

void check_event_load_menu(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);

    auto result = session.begin_event(1, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::fade_from_black);
    openlegend::render::IndexedFramebuffer frame;
    OL_CHECK(session.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0xDD14FCC6528CAB25ULL);

    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == 0);
    OL_CHECK(session.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0x65C8DA776EAC540FULL);

    result = session.resume(SceneResponse::acknowledge, 0x9E);
    OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == 2);
    OL_CHECK(session.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0x816757297E6CB14BULL);

    result = session.resume(SceneResponse::acknowledge, 0x98);
    OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == 3);
    OL_CHECK(session.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0x726A3CCCC0E9C1D2ULL);

    result = session.resume(SceneResponse::acknowledge, 0x0D);
    OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == 3);
    OL_CHECK(session.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0xB908953A6570B8A0ULL);

    result = session.resume(SceneResponse::acknowledge, static_cast<int>('N'));
    OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == 3);
    OL_CHECK(session.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0xB908953A6570B8A0ULL);

    result = session.resume(SceneResponse::acknowledge, 0x9E);
    OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == 2);
    result = session.resume(SceneResponse::acknowledge, 0x20);
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);
    OL_CHECK(session.exit_transition_pending());
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::return_world && result.save_slot == 2);

    auto quit_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom quit_random{1U};
    openlegend::scene::SceneSession quit_session{
        data_root, quit_snapshot, quit_random, 70};
    OL_CHECK(finish_scene_title(quit_session).kind == SceneStepKind::stay);
    result = quit_session.begin_event(1, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::fade_from_black);
    result = quit_session.resume(SceneResponse::acknowledge);
    for (int selection = 1; selection <= 3; ++selection) {
        result = quit_session.resume(SceneResponse::acknowledge, 0x98);
        OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == selection);
    }
    result = quit_session.resume(SceneResponse::acknowledge, 0x96);
    OL_CHECK(result.kind == SceneStepKind::load_menu && result.menu_index == 3);
    OL_CHECK(quit_session.resume(
                 SceneResponse::acknowledge, static_cast<int>('Y')).kind ==
             SceneStepKind::quit);

    auto invalid_snapshot = load_baseline(root);
    const auto item_count_before = inventory_count(invalid_snapshot.ranger, 123);
    openlegend::random::LegacyRandom invalid_random{1U};
    openlegend::scene::SceneSession invalid_opcode{
        data_root, invalid_snapshot, invalid_random, 70};
    OL_CHECK(finish_scene_title(invalid_opcode).kind == SceneStepKind::stay);
    result = invalid_opcode.begin_event(2, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(inventory_count(invalid_snapshot.ranger, 123) == item_count_before);
    result = invalid_opcode.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(inventory_count(invalid_snapshot.ranger, 123) == item_count_before);

    auto item_match_snapshot = load_baseline(root);
    const auto matched_count_before = inventory_count(item_match_snapshot.ranger, 200);
    openlegend::random::LegacyRandom item_match_random{1U};
    openlegend::scene::SceneSession item_match{
        data_root, item_match_snapshot, item_match_random, 70};
    OL_CHECK(finish_scene_title(item_match).kind == SceneStepKind::stay);
    result = item_match.begin_event(3, 0, 44, 29, 123);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(item_match_snapshot.ranger, 200) == matched_count_before + 1);

    auto item_miss_snapshot = load_baseline(root);
    const auto missed_count_before = inventory_count(item_miss_snapshot.ranger, 200);
    openlegend::random::LegacyRandom item_miss_random{1U};
    openlegend::scene::SceneSession item_miss{
        data_root, item_miss_snapshot, item_miss_random, 70};
    OL_CHECK(finish_scene_title(item_miss).kind == SceneStepKind::stay);
    result = item_miss.begin_event(3, 0, 44, 29, 124);
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(inventory_count(item_miss_snapshot.ranger, 200) == missed_count_before);

    auto fade_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom fade_random{1U};
    openlegend::scene::SceneSession fade_session{
        data_root, fade_snapshot, fade_random, 70};
    OL_CHECK(finish_scene_title(fade_session).kind == SceneStepKind::stay);
    result = fade_session.begin_event(4, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::fade_from_black);
    result = fade_session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::fade_to_black);
    OL_CHECK(fade_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

    auto party_match_snapshot = load_baseline(root);
    party_match_snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{-1});
    party_match_snapshot.ranger.header.set_team_member(5U, openlegend::model::CharacterId{55});
    const auto party_count_before = inventory_count(party_match_snapshot.ranger, 201);
    openlegend::random::LegacyRandom party_match_random{1U};
    openlegend::scene::SceneSession party_match{
        data_root, party_match_snapshot, party_match_random, 70};
    OL_CHECK(finish_scene_title(party_match).kind == SceneStepKind::stay);
    result = party_match.begin_event(5, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(party_match_snapshot.ranger, 201) == party_count_before + 1);

    for (std::size_t slot = 0U; slot < openlegend::model::kTeamMemberCount; ++slot) {
        party_match_snapshot.ranger.header.set_team_member(
            slot, openlegend::model::CharacterId{static_cast<std::int16_t>(slot)});
    }
    const auto party_miss_count_before = inventory_count(party_match_snapshot.ranger, 201);
    result = party_match.begin_event(5, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(inventory_count(party_match_snapshot.ranger, 201) == party_miss_count_before);

    auto battle_win_snapshot = load_baseline(root);
    const auto win_count_before = inventory_count(battle_win_snapshot.ranger, 202);
    openlegend::random::LegacyRandom battle_win_random{1U};
    openlegend::scene::SceneSession battle_win{
        data_root, battle_win_snapshot, battle_win_random, 70};
    OL_CHECK(finish_scene_title(battle_win).kind == SceneStepKind::stay);
    result = battle_win.begin_event(6, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::battle && result.battle_id == 77);
    OL_CHECK(result.battle_get_exp == 9);
    result = battle_win.resume(SceneResponse::battle_victory);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(battle_win_snapshot.ranger, 202) == win_count_before + 1);

    auto battle_loss_snapshot = load_baseline(root);
    const auto loss_count_before = inventory_count(battle_loss_snapshot.ranger, 203);
    openlegend::random::LegacyRandom battle_loss_random{1U};
    openlegend::scene::SceneSession battle_loss{
        data_root, battle_loss_snapshot, battle_loss_random, 70};
    OL_CHECK(finish_scene_title(battle_loss).kind == SceneStepKind::stay);
    result = battle_loss.begin_event(6, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::battle && result.battle_id == 77);
    OL_CHECK(result.battle_get_exp == 9);
    result = battle_loss.resume(SceneResponse::battle_defeat);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(battle_loss_snapshot.ranger, 203) == loss_count_before + 1);

    auto battle_offset_win_snapshot = load_baseline(root);
    const auto offset_win_count = inventory_count(battle_offset_win_snapshot.ranger, 215);
    openlegend::random::LegacyRandom battle_offset_win_random{1U};
    openlegend::scene::SceneSession battle_offset_win{
        data_root, battle_offset_win_snapshot, battle_offset_win_random, 70};
    OL_CHECK(finish_scene_title(battle_offset_win).kind == SceneStepKind::stay);
    result = battle_offset_win.begin_event(18, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::battle && result.battle_id == 77);
    OL_CHECK(result.battle_get_exp == 1);
    result = battle_offset_win.resume(SceneResponse::battle_victory);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(battle_offset_win_snapshot.ranger, 215) == offset_win_count + 1);

    auto battle_offset_loss_snapshot = load_baseline(root);
    const auto offset_loss_count = inventory_count(battle_offset_loss_snapshot.ranger, 214);
    openlegend::random::LegacyRandom battle_offset_loss_random{1U};
    openlegend::scene::SceneSession battle_offset_loss{
        data_root, battle_offset_loss_snapshot, battle_offset_loss_random, 70};
    OL_CHECK(finish_scene_title(battle_offset_loss).kind == SceneStepKind::stay);
    result = battle_offset_loss.begin_event(18, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::battle && result.battle_id == 77);
    OL_CHECK(result.battle_get_exp == 1);
    result = battle_offset_loss.resume(SceneResponse::battle_defeat);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(battle_offset_loss_snapshot.ranger, 214) == offset_loss_count + 1);

    auto battle_question_snapshot = load_baseline(root);
    const auto battle_question_count = inventory_count(battle_question_snapshot.ranger, 210);
    openlegend::random::LegacyRandom battle_question_random{1U};
    openlegend::scene::SceneSession battle_question{
        data_root, battle_question_snapshot, battle_question_random, 70};
    OL_CHECK(finish_scene_title(battle_question).kind == SceneStepKind::stay);
    result = battle_question.begin_event(7, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question &&
             result.question == openlegend::scene::SceneQuestion::battle);
    OL_CHECK(battle_question.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0x5D8FC752D48D9A98ULL);
    OL_CHECK(battle_question.resume(SceneResponse::no).kind == SceneStepKind::stay);
    OL_CHECK(inventory_count(battle_question_snapshot.ranger, 210) == battle_question_count);

    auto battle_yes_snapshot = load_baseline(root);
    const auto battle_yes_count = inventory_count(battle_yes_snapshot.ranger, 210);
    openlegend::random::LegacyRandom battle_yes_random{1U};
    openlegend::scene::SceneSession battle_yes{
        data_root, battle_yes_snapshot, battle_yes_random, 70};
    OL_CHECK(finish_scene_title(battle_yes).kind == SceneStepKind::stay);
    result = battle_yes.begin_event(7, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question);
    result = battle_yes.resume(SceneResponse::yes);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(battle_yes_snapshot.ranger, 210) == battle_yes_count + 1);

    auto exceptional_yes_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom exceptional_yes_random{1U};
    openlegend::scene::SceneSession exceptional_yes{
        data_root, exceptional_yes_snapshot, exceptional_yes_random, 70};
    OL_CHECK(finish_scene_title(exceptional_yes).kind == SceneStepKind::stay);
    result = exceptional_yes.begin_event(17, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question &&
             result.question == openlegend::scene::SceneQuestion::battle);
    result = exceptional_yes.resume(SceneResponse::yes);
    OL_CHECK(result.kind == SceneStepKind::battle && result.battle_id == 71);

    auto exceptional_no_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom exceptional_no_random{1U};
    openlegend::scene::SceneSession exceptional_no{
        data_root, exceptional_no_snapshot, exceptional_no_random, 70};
    OL_CHECK(finish_scene_title(exceptional_no).kind == SceneStepKind::stay);
    result = exceptional_no.begin_event(17, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question &&
             result.question == openlegend::scene::SceneQuestion::battle);
    result = exceptional_no.resume(SceneResponse::no);
    OL_CHECK(result.kind == SceneStepKind::battle && result.battle_id == 72);

    auto join_question_snapshot = load_baseline(root);
    const auto join_question_count = inventory_count(join_question_snapshot.ranger, 211);
    openlegend::random::LegacyRandom join_question_random{1U};
    openlegend::scene::SceneSession join_question{
        data_root, join_question_snapshot, join_question_random, 70};
    OL_CHECK(finish_scene_title(join_question).kind == SceneStepKind::stay);
    openlegend::render::IndexedFramebuffer bare_scene;
    OL_CHECK(join_question.render(bare_scene));
    const auto bare_scene_hash = fnv1a64(bare_scene.pixels());
    result = join_question.begin_event(8, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question &&
             result.question == openlegend::scene::SceneQuestion::join);
    OL_CHECK(join_question.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0xBEA93863A81CD9E0ULL);
    result = join_question.resume(SceneResponse::yes);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(join_question.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == bare_scene_hash);
    result = join_question.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(join_question_snapshot.ranger, 211) == join_question_count + 1);

    auto join_no_snapshot = load_baseline(root);
    const auto join_no_count = inventory_count(join_no_snapshot.ranger, 211);
    openlegend::random::LegacyRandom join_no_random{1U};
    openlegend::scene::SceneSession join_no{
        data_root, join_no_snapshot, join_no_random, 70};
    OL_CHECK(finish_scene_title(join_no).kind == SceneStepKind::stay);
    OL_CHECK(join_no.render(bare_scene));
    const auto join_no_bare_hash = fnv1a64(bare_scene.pixels());
    result = join_no.begin_event(8, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question &&
             result.question == openlegend::scene::SceneQuestion::join);
    result = join_no.resume(SceneResponse::no);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(join_no.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == join_no_bare_hash);
    OL_CHECK(join_no.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    OL_CHECK(inventory_count(join_no_snapshot.ranger, 211) == join_no_count);

    auto rest_question_snapshot = load_baseline(root);
    const auto rest_question_count = inventory_count(rest_question_snapshot.ranger, 212);
    openlegend::random::LegacyRandom rest_question_random{1U};
    openlegend::scene::SceneSession rest_question{
        data_root, rest_question_snapshot, rest_question_random, 70};
    OL_CHECK(finish_scene_title(rest_question).kind == SceneStepKind::stay);
    result = rest_question.begin_event(9, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question &&
             result.question == openlegend::scene::SceneQuestion::rest);
    OL_CHECK(rest_question.render(frame));
    OL_CHECK(fnv1a64(frame.pixels()) == 0xD070227A07492883ULL);
    result = rest_question.resume(SceneResponse::yes);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(inventory_count(rest_question_snapshot.ranger, 212) == rest_question_count + 1);

    auto rest_no_snapshot = load_baseline(root);
    const auto rest_no_count = inventory_count(rest_no_snapshot.ranger, 212);
    openlegend::random::LegacyRandom rest_no_random{1U};
    openlegend::scene::SceneSession rest_no{
        data_root, rest_no_snapshot, rest_no_random, 70};
    OL_CHECK(finish_scene_title(rest_no).kind == SceneStepKind::stay);
    result = rest_no.begin_event(9, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::question &&
             result.question == openlegend::scene::SceneQuestion::rest);
    OL_CHECK(rest_no.resume(SceneResponse::no).kind == SceneStepKind::stay);
    OL_CHECK(inventory_count(rest_no_snapshot.ranger, 212) == rest_no_count);

    auto full_rest_snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kTeamMemberCount; ++slot) {
        full_rest_snapshot.ranger.header.set_team_member(
            slot, openlegend::model::CharacterId{static_cast<std::int16_t>(slot)});
        auto& role = full_rest_snapshot.ranger.roles[slot];
        role.set_word(openlegend::model::role_word::hp, static_cast<std::int16_t>(10 + slot));
        role.set_word(openlegend::model::role_word::maximum_hp,
                      static_cast<std::int16_t>(90 + slot));
        role.set_word(openlegend::model::role_word::mp, static_cast<std::int16_t>(20 + slot));
        role.set_word(openlegend::model::role_word::maximum_mp,
                      static_cast<std::int16_t>(80 + slot));
        role.set_word(openlegend::model::role_word::hurt, 32);
        role.set_word(openlegend::model::role_word::poison, 0);
        role.set_word(openlegend::model::role_word::physical_power, 30);
    }
    openlegend::random::LegacyRandom full_rest_random{1U};
    openlegend::scene::SceneSession full_rest{
        data_root, full_rest_snapshot, full_rest_random, 70};
    OL_CHECK(finish_scene_title(full_rest).kind == SceneStepKind::stay);
    OL_CHECK(full_rest.begin_event(19, 0, 44, 29).kind == SceneStepKind::stay);
    for (std::size_t slot = 0U; slot < openlegend::model::kTeamMemberCount; ++slot) {
        const auto& role = full_rest_snapshot.ranger.roles[slot];
        OL_CHECK(role.word(openlegend::model::role_word::hp) ==
                 static_cast<std::int16_t>(90 + slot));
        OL_CHECK(role.word(openlegend::model::role_word::mp) ==
                 static_cast<std::int16_t>(80 + slot));
        OL_CHECK(role.word(openlegend::model::role_word::hurt) == 0);
        OL_CHECK(role.word(openlegend::model::role_word::physical_power) == 100);
    }
}

void check_event_state_write_helpers(const std::filesystem::path& root) {
    using openlegend::model::SceneEventField;
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    {
        auto snapshot = load_baseline(root);
        constexpr std::size_t event = 0U;
        constexpr std::size_t old_cell = 21U * 64U + 20U;
        constexpr std::size_t new_cell = 31U * 64U + 30U;
        OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::x, 20));
        OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::y, 21));
        OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, old_cell, 0));
        OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, new_cell, -1));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(10, 0, 44, 29).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.event_value(70U, event, SceneEventField::x).value_or(-1) == 30);
        OL_CHECK(snapshot.event_value(70U, event, SceneEventField::y).value_or(-1) == 31);
        OL_CHECK(snapshot.scene_value(
                     70U, SceneLayer::event_index, old_cell).value_or(0) == -1);
        OL_CHECK(snapshot.scene_value(
                     70U, SceneLayer::event_index, new_cell).value_or(-1) == 0);
    }

    {
        auto snapshot = load_baseline(root);
        constexpr std::size_t event = 5U;
        constexpr std::size_t trigger_cell = 29U * 64U + 44U;
        constexpr std::size_t record_cell = 21U * 64U + 20U;
        for (std::size_t field = 0U; field < 9U; ++field) {
            OL_CHECK(snapshot.set_event_value(
                70U, event, static_cast<SceneEventField>(field),
                static_cast<std::int16_t>(200U + field)));
        }
        OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::x, 20));
        OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::y, 21));
        OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, trigger_cell, event));
        OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, record_cell, event));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(15, event, 44, 29).kind == SceneStepKind::stay);
        for (std::size_t field = 0U; field < 9U; ++field) {
            OL_CHECK(snapshot.event_value(
                         70U, event, static_cast<SceneEventField>(field)).value_or(-1) ==
                     static_cast<std::int16_t>(11U + field));
        }
        OL_CHECK(snapshot.event_value(70U, event, SceneEventField::x).value_or(-1) == 20);
        OL_CHECK(snapshot.event_value(70U, event, SceneEventField::y).value_or(-1) == 21);
        OL_CHECK(snapshot.scene_value(
                     70U, SceneLayer::event_index, trigger_cell).value_or(0) == -1);
        OL_CHECK(snapshot.scene_value(
                     70U, SceneLayer::event_index, record_cell).value_or(-1) == event);
    }

    {
        auto snapshot = load_baseline(root);
        constexpr std::size_t event = 5U;
        constexpr std::size_t old_cell = 21U * 64U + 20U;
        constexpr std::size_t new_cell = 31U * 64U + 30U;
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        for (std::size_t field = 0U; field < 9U; ++field) {
            OL_CHECK(snapshot.set_event_value(
                69U, event, static_cast<SceneEventField>(field),
                static_cast<std::int16_t>(200U + field)));
            OL_CHECK(snapshot.set_event_value(
                70U, event, static_cast<SceneEventField>(field),
                static_cast<std::int16_t>(300U + field)));
        }
        for (const auto scene : {69U, 70U}) {
            OL_CHECK(snapshot.set_event_value(scene, event, SceneEventField::x, 20));
            OL_CHECK(snapshot.set_event_value(scene, event, SceneEventField::y, 21));
            OL_CHECK(snapshot.set_scene_value(scene, SceneLayer::event_index, old_cell, event));
            OL_CHECK(snapshot.set_scene_value(scene, SceneLayer::event_index, new_cell, -1));
        }
        OL_CHECK(session.begin_event(16, 0, 44, 29).kind == SceneStepKind::stay);
        for (std::size_t field = 0U; field < 9U; ++field) {
            const auto expected = static_cast<std::int16_t>(
                field % 2U == 0U ? 101U + field : 200U + field);
            OL_CHECK(snapshot.event_value(
                         69U, event, static_cast<SceneEventField>(field)).value_or(-1) == expected);
            OL_CHECK(snapshot.event_value(
                         70U, event, static_cast<SceneEventField>(field)).value_or(-1) ==
                     static_cast<std::int16_t>(300U + field));
        }
        OL_CHECK(snapshot.event_value(69U, event, SceneEventField::x).value_or(-1) == 30);
        OL_CHECK(snapshot.event_value(69U, event, SceneEventField::y).value_or(-1) == 31);
        OL_CHECK(snapshot.event_value(70U, event, SceneEventField::x).value_or(-1) == 20);
        OL_CHECK(snapshot.event_value(70U, event, SceneEventField::y).value_or(-1) == 21);
        OL_CHECK(snapshot.scene_value(
                     70U, SceneLayer::event_index, old_cell).value_or(0) == -1);
        OL_CHECK(snapshot.scene_value(
                     70U, SceneLayer::event_index, new_cell).value_or(-1) == event);
        OL_CHECK(snapshot.scene_value(
                     69U, SceneLayer::event_index, old_cell).value_or(-1) == event);
        OL_CHECK(snapshot.scene_value(
                     69U, SceneLayer::event_index, new_cell).value_or(0) == -1);
        OL_CHECK(session.scene_id() == 70);
    }

    {
        auto snapshot = load_baseline(root);
        constexpr std::size_t event = 5U;
        OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::event_1, 32767));
        OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::event_2, -2));
        OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::event_3, 3));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(11, 5, 44, 29).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.event_value(
                     70U, event, SceneEventField::event_1).value_or(0) == -32768);
        OL_CHECK(snapshot.event_value(
                     70U, event, SceneEventField::event_2).value_or(0) == 0);
        OL_CHECK(snapshot.event_value(
                     70U, event, SceneEventField::event_3).value_or(0) == 6);
    }

    {
        auto snapshot = load_baseline(root);
        constexpr std::size_t event = 5U;
        for (const auto& [scene, values] : std::array{
                 std::pair{69U, std::array<std::int16_t, 3>{10, 20, 30}},
                 std::pair{70U, std::array<std::int16_t, 3>{100, 200, 300}}}) {
            OL_CHECK(snapshot.set_event_value(scene, event, SceneEventField::event_1, values[0]));
            OL_CHECK(snapshot.set_event_value(scene, event, SceneEventField::event_2, values[1]));
            OL_CHECK(snapshot.set_event_value(scene, event, SceneEventField::event_3, values[2]));
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(13, 0, 44, 29).kind == SceneStepKind::stay);
        for (std::size_t field = 0U; field < 3U; ++field) {
            const auto field_id = static_cast<SceneEventField>(
                static_cast<std::size_t>(SceneEventField::event_1) + field);
            OL_CHECK(snapshot.event_value(69U, event, field_id).value_or(0) ==
                     static_cast<std::int16_t>(9 + field * 9));
            OL_CHECK(snapshot.event_value(70U, event, field_id).value_or(0) ==
                     static_cast<std::int16_t>(100 + field * 100));
        }
        OL_CHECK(session.scene_id() == 70);
    }

    {
        auto snapshot = load_baseline(root);
        constexpr std::size_t cell = 3U * 64U + 2U;
        OL_CHECK(snapshot.set_scene_value(69U, SceneLayer::building, cell, 0));
        OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building, cell, 123));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(12, 0, 44, 29).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.scene_value(
                     69U, SceneLayer::building, cell).value_or(-1) == 456);
        OL_CHECK(snapshot.scene_value(
                     70U, SceneLayer::building, cell).value_or(-1) == 123);
        OL_CHECK(session.scene_id() == 70);
    }

    {
        auto snapshot = load_baseline(root);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(20, 0, 44, 29).kind == SceneStepKind::stay);
        OL_CHECK(session.scene_x() == 0 && session.scene_y() == 63);
        OL_CHECK(session.view_origin_x() == 0 && session.view_origin_y() == 36);
        OL_CHECK(session.begin_event(21, 0, 44, 29).kind == SceneStepKind::stay);
        OL_CHECK(session.scene_x() == 63 && session.scene_y() == 0);
        OL_CHECK(session.view_origin_x() == 36 && session.view_origin_y() == 0);
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
    auto title_framebuffer = framebuffer;
    OL_CHECK(session.render(title_framebuffer));
    OL_CHECK(fnv1a64(title_framebuffer.pixels()) == 0xC5A8777E049759F2ULL);
    const auto title_hash = fnv1a64(title_framebuffer.pixels());
    OL_CHECK(session.render(title_framebuffer));
    OL_CHECK(fnv1a64(title_framebuffer.pixels()) == title_hash);

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

void check_scene_movement_guards(const std::filesystem::path& root) {
    using openlegend::model::SceneEventField;
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneDirection;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto prepare = [&]() {
        auto snapshot = load_baseline(root);
        auto& metadata = snapshot.ranger.scenes[70];
        metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 10);
        metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 10);
        for (const auto cell : std::array<std::size_t, 2>{10U * 64U + 10U, 10U * 64U + 11U}) {
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::earth, cell, 0));
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building, cell, 0));
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, cell, -1));
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building_height, cell, 0));
        }
        return snapshot;
    };

    auto snapshot = prepare();
    constexpr std::size_t source = 10U * 64U + 10U;
    constexpr std::size_t target = 10U * 64U + 11U;
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);

    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building, target, 1));
    OL_CHECK(session.move(SceneDirection::right).kind == SceneStepKind::stay);
    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building, target, 0));

    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building_height, source, -5));
    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building_height, target, 5));
    OL_CHECK(session.move(SceneDirection::right).kind == SceneStepKind::stay);
    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building_height, source, 0));
    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building_height, target, 0));

    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, target, 0));
    OL_CHECK(snapshot.set_event_value(70U, 0U, SceneEventField::cannot_walk, 1));
    OL_CHECK(session.move(SceneDirection::right).kind == SceneStepKind::stay);
    OL_CHECK(snapshot.set_event_value(70U, 0U, SceneEventField::cannot_walk, 0));

    constexpr std::array<std::int16_t, 9> blocked_low{
        358, 374, 458, 506, 818, 838, 934, 1016, 1324};
    constexpr std::array<std::int16_t, 9> blocked_high{
        362, 380, 464, 610, 824, 838, 936, 1022, 1348};
    for (std::size_t index = 0U; index < blocked_low.size(); ++index) {
        for (const auto earth : {blocked_low[index], blocked_high[index]}) {
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::earth, target, earth));
            const auto blocked = session.move(SceneDirection::right);
            OL_CHECK(blocked.kind == SceneStepKind::stay);
            OL_CHECK(blocked.scene_x == 10 && blocked.scene_y == 10);
            OL_CHECK(session.direction() == SceneDirection::right);
        }
    }
    OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::earth, target, 0));
    const auto moved = session.move(SceneDirection::right);
    OL_CHECK(moved.kind == SceneStepKind::moved);
    OL_CHECK(moved.scene_x == 11 && moved.scene_y == 10);
    OL_CHECK(session.view_origin_x() == 0 && session.view_origin_y() == 0);

    auto boundary_snapshot = prepare();
    auto& boundary_metadata = boundary_snapshot.ranger.scenes[70];
    boundary_metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 0);
    boundary_metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 63);
    constexpr std::size_t boundary_cell = 63U * 64U;
    OL_CHECK(boundary_snapshot.set_scene_value(70U, SceneLayer::earth, boundary_cell, 0));
    OL_CHECK(boundary_snapshot.set_scene_value(70U, SceneLayer::building, boundary_cell, 0));
    OL_CHECK(boundary_snapshot.set_scene_value(70U, SceneLayer::event_index, boundary_cell, -1));
    OL_CHECK(boundary_snapshot.set_scene_value(70U, SceneLayer::building_height, boundary_cell, 0));
    openlegend::random::LegacyRandom boundary_random{1U};
    openlegend::scene::SceneSession boundary{
        data_root, boundary_snapshot, boundary_random, 70};
    OL_CHECK(finish_scene_title(boundary).kind == SceneStepKind::stay);
    OL_CHECK(boundary.move(SceneDirection::left).kind == SceneStepKind::moved);
    OL_CHECK(boundary.scene_x() == 0 && boundary.scene_y() == 63);
    OL_CHECK(boundary.direction() == SceneDirection::left);
    OL_CHECK(boundary.move(SceneDirection::down).kind == SceneStepKind::moved);
    OL_CHECK(boundary.scene_x() == 0 && boundary.scene_y() == 63);
    OL_CHECK(boundary.direction() == SceneDirection::down);
    OL_CHECK(boundary.view_origin_x() == 0 && boundary.view_origin_y() == 36);

    auto opposite_snapshot = prepare();
    auto& opposite_metadata = opposite_snapshot.ranger.scenes[70];
    opposite_metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 63);
    opposite_metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 0);
    constexpr std::size_t opposite_cell = 63U;
    OL_CHECK(opposite_snapshot.set_scene_value(70U, SceneLayer::earth, opposite_cell, 0));
    OL_CHECK(opposite_snapshot.set_scene_value(70U, SceneLayer::building, opposite_cell, 0));
    OL_CHECK(opposite_snapshot.set_scene_value(70U, SceneLayer::event_index, opposite_cell, -1));
    OL_CHECK(opposite_snapshot.set_scene_value(70U, SceneLayer::building_height, opposite_cell, 0));
    openlegend::random::LegacyRandom opposite_random{1U};
    openlegend::scene::SceneSession opposite{
        data_root, opposite_snapshot, opposite_random, 70};
    OL_CHECK(finish_scene_title(opposite).kind == SceneStepKind::stay);
    OL_CHECK(opposite.move(SceneDirection::right).kind == SceneStepKind::moved);
    OL_CHECK(opposite.scene_x() == 63 && opposite.scene_y() == 0);
    OL_CHECK(opposite.direction() == SceneDirection::right);
    OL_CHECK(opposite.move(SceneDirection::up).kind == SceneStepKind::moved);
    OL_CHECK(opposite.scene_x() == 63 && opposite.scene_y() == 0);
    OL_CHECK(opposite.direction() == SceneDirection::up);
    OL_CHECK(opposite.view_origin_x() == 36 && opposite.view_origin_y() == 0);
}

void check_scene_movement_idle_state(const std::filesystem::path& root) {
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneDirection;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    auto prepare = [&]() {
        auto snapshot = load_baseline(root);
        auto& metadata = snapshot.ranger.scenes[70];
        metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 10);
        metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 10);
        metadata.set_word(openlegend::model::scene_metadata_word::jump_scene, -1);
        for (std::size_t index = 0U;
             index < openlegend::model::scene_metadata_word::exit_count;
             ++index) {
            metadata.set_word(openlegend::model::scene_metadata_word::exit_x_begin + index, -1);
            metadata.set_word(openlegend::model::scene_metadata_word::exit_y_begin + index, -1);
        }
        for (const auto cell : std::array<std::size_t, 3>{
                 10U * 64U + 10U, 10U * 64U + 11U, 9U * 64U + 10U}) {
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::earth, cell, 0));
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building, cell, 0));
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, cell, -1));
            OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building_height, cell, 0));
        }
        return snapshot;
    };
    auto idle_once = [](openlegend::scene::SceneSession& session) {
        OL_CHECK(session.tick(std::nullopt, false, false).kind == SceneStepKind::present);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    };

    auto walk_snapshot = prepare();
    openlegend::random::LegacyRandom walk_random{1U};
    openlegend::scene::SceneSession walk{
        data_root, walk_snapshot, walk_random, 70};
    OL_CHECK(finish_scene_title(walk).kind == SceneStepKind::stay);
    for (int tick = 0; tick < 20; ++tick) {
        idle_once(walk);
    }
    OL_CHECK(walk.move(SceneDirection::right).kind == SceneStepKind::moved);
    OL_CHECK(walk.player_frame() == 5018);
    idle_once(walk);
    OL_CHECK(walk.player_frame() == 5018);

    auto counter_snapshot = prepare();
    openlegend::random::LegacyRandom counter_random{1U};
    openlegend::scene::SceneSession counter{
        data_root, counter_snapshot, counter_random, 70};
    OL_CHECK(finish_scene_title(counter).kind == SceneStepKind::stay);
    for (int tick = 0; tick < 50; ++tick) {
        idle_once(counter);
    }
    OL_CHECK(counter_random.state() == 1U);
    const auto scripted = counter.begin_event(14, 0, 10, 10);
    OL_CHECK(scripted.kind == SceneStepKind::present);
    OL_CHECK(scripted.wait_ticks == 3U);
    OL_CHECK(counter.scene_y() == 9);
    OL_CHECK(counter.direction() == SceneDirection::up);
    OL_CHECK(counter.player_frame() == 5004);
    const auto standing = counter.resume(SceneResponse::acknowledge);
    OL_CHECK(standing.kind == SceneStepKind::present);
    OL_CHECK(standing.wait_ticks == 1U);
    OL_CHECK(counter.player_frame() == 5002);
    OL_CHECK(counter.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    OL_CHECK(counter_random.state() == 1U);
    idle_once(counter);
    OL_CHECK(counter_random.state() == 0x41C67EA6U);
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

    openlegend::random::LegacyRandom phase_random{1U};
    openlegend::scene::SceneSession phase{
        data_root, snapshot, phase_random, 70, false, std::nullopt, 4};
    OL_CHECK(phase.periodic_counter() == 4);
    OL_CHECK(finish_scene_title(phase).kind == openlegend::scene::SceneStepKind::stay);
    OL_CHECK(phase.periodic_counter() == 4);
    OL_CHECK(phase.tick(std::nullopt, false, false).kind ==
             openlegend::scene::SceneStepKind::present);
    OL_CHECK(phase.periodic_counter() == 4);
    static_cast<void>(phase.resume(openlegend::scene::SceneResponse::acknowledge));
    OL_CHECK(phase.periodic_counter() == 0);
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
    using openlegend::scene::SceneDirection;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    auto snapshot = load_baseline(root);
    constexpr std::size_t event = 199U;
    OL_CHECK(snapshot.set_scene_value(
        70U, SceneLayer::event_index, 29U * 64U + 45U, event));
    OL_CHECK(snapshot.set_event_value(70U, event, SceneEventField::event_1, 825));
    OL_CHECK(snapshot.set_scene_value(
        70U, SceneLayer::building, 29U * 64U + 45U, 1));
    snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, 7);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
    OL_CHECK(session.move(SceneDirection::right).kind == SceneStepKind::stay);
    OL_CHECK(session.player_frame() == 5018);
    OL_CHECK(session.tick(std::nullopt, true, false).kind == SceneStepKind::present);
    OL_CHECK(session.player_frame() == 5016);
    auto result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(result.style == 52);
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

    auto empty_snapshot = load_baseline(root);
    OL_CHECK(empty_snapshot.set_scene_value(
        70U, SceneLayer::event_index, 29U * 64U + 45U, -1));
    openlegend::random::LegacyRandom empty_random{1U};
    openlegend::scene::SceneSession empty_session{
        data_root, empty_snapshot, empty_random, 70};
    OL_CHECK(finish_scene_title(empty_session).kind == SceneStepKind::stay);
    OL_CHECK(empty_session.tick(std::nullopt, true, false).kind == SceneStepKind::present);
    OL_CHECK(empty_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

    constexpr std::array<SceneDirection, 4> directions{
        SceneDirection::up, SceneDirection::right, SceneDirection::left, SceneDirection::down};
    constexpr std::array<std::size_t, 4> front_cells{
        28U * 64U + 44U, 29U * 64U + 45U, 29U * 64U + 43U, 30U * 64U + 44U};
    for (std::size_t index = 0U; index < directions.size(); ++index) {
        auto direction_snapshot = load_baseline(root);
        direction_snapshot.ranger.header.set_word(
            openlegend::model::header_word::face_towards,
            static_cast<std::int16_t>(directions[index]));
        for (const auto cell : front_cells) {
            OL_CHECK(direction_snapshot.set_scene_value(
                70U, SceneLayer::event_index, cell, -1));
        }
        OL_CHECK(direction_snapshot.set_scene_value(
            70U, SceneLayer::event_index, front_cells[index], event));
        OL_CHECK(direction_snapshot.set_event_value(
            70U, event, SceneEventField::event_1, 0));
        openlegend::random::LegacyRandom direction_random{1U};
        openlegend::scene::SceneSession direction_session{
            data_root, direction_snapshot, direction_random, 70};
        OL_CHECK(finish_scene_title(direction_session).kind == SceneStepKind::stay);
        OL_CHECK(direction_session.tick(std::nullopt, true, false).kind ==
                 SceneStepKind::present);
        OL_CHECK(direction_session.resume(SceneResponse::acknowledge).kind ==
                 SceneStepKind::present);
        OL_CHECK(direction_session.resume(SceneResponse::acknowledge).kind ==
                 SceneStepKind::stay);
    }
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
    OL_CHECK(item_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(item_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

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
             SceneStepKind::present);
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
    OL_CHECK(empty_menu_item_session.event_item_id() == 123);
    OL_CHECK(empty_menu_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);

    auto boundary_item_snapshot = load_baseline(root);
    boundary_item_snapshot.ranger.scenes[70U].set_word(
        openlegend::model::scene_metadata_word::entrance_x, 63);
    boundary_item_snapshot.ranger.scenes[70U].set_word(
        openlegend::model::scene_metadata_word::entrance_y, 29);
    boundary_item_snapshot.ranger.header.set_word(
        openlegend::model::header_word::face_towards,
        static_cast<std::int16_t>(openlegend::scene::SceneDirection::right));
    OL_CHECK(boundary_item_snapshot.set_scene_value(
        70U, SceneLayer::event_index, 30U * 64U, event));
    OL_CHECK(boundary_item_snapshot.set_event_value(
        70U, event, SceneEventField::event_2, 0));
    openlegend::random::LegacyRandom boundary_item_random{1U};
    openlegend::scene::SceneSession boundary_item_session{
        data_root, boundary_item_snapshot, boundary_item_random, 70};
    OL_CHECK(finish_scene_title(boundary_item_session).kind == SceneStepKind::stay);
    static_cast<void>(boundary_item_session.move(openlegend::scene::SceneDirection::right));
    OL_CHECK(boundary_item_session.player_frame() == 5018);
    OL_CHECK(boundary_item_session.use_item(123).kind == SceneStepKind::present);
    OL_CHECK(boundary_item_session.player_frame() == 5016);
    OL_CHECK(boundary_item_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::stay);

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

    auto empty_auto_snapshot = load_baseline(root);
    OL_CHECK(empty_auto_snapshot.set_scene_value(
        70U, SceneLayer::event_index, current, -1));
    openlegend::random::LegacyRandom empty_auto_random{1U};
    openlegend::scene::SceneSession empty_auto{
        data_root, empty_auto_snapshot, empty_auto_random, 70};
    OL_CHECK(empty_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::scene_title);
    OL_CHECK(empty_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);
    OL_CHECK(empty_auto.resume(SceneResponse::acknowledge).kind ==
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

    auto negative_auto_snapshot = load_baseline(root);
    OL_CHECK(negative_auto_snapshot.set_scene_value(
        70U, SceneLayer::event_index, current, event));
    OL_CHECK(negative_auto_snapshot.set_event_value(
        70U, event, SceneEventField::event_3, -2));
    openlegend::random::LegacyRandom negative_auto_random{1U};
    openlegend::scene::SceneSession negative_auto{
        data_root, negative_auto_snapshot, negative_auto_random, 70};
    OL_CHECK(negative_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::scene_title);
    OL_CHECK(negative_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);
    OL_CHECK(negative_auto.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::present);
    OL_CHECK(negative_auto.resume(SceneResponse::acknowledge).kind ==
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
    OL_CHECK(active_auto.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(active_auto.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
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
    exit_session.set_event_item_id(123);
    OL_CHECK(exit_session.tick(SceneDirection::right, false, false).kind ==
             SceneStepKind::present);
    OL_CHECK(exit_session.scene_x() == 45 && exit_session.scene_y() == 29);
    OL_CHECK(exit_session.player_frame() == 5018);
    OL_CHECK(exit_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(exit_session.player_frame() == 5016);
    OL_CHECK(exit_session.event_item_id() == 123);
    OL_CHECK(exit_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::notice);
    OL_CHECK(exit_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
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
        data_root, jump_snapshot, jump_random, 70, false, std::nullopt, 3};
    OL_CHECK(finish_scene_title(jump_session).kind == SceneStepKind::stay);
    OL_CHECK(jump_session.tick(SceneDirection::right, false, false).kind ==
             SceneStepKind::present);
    OL_CHECK(jump_session.scene_id() == 70);
    OL_CHECK(jump_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::fade_to_black);
    OL_CHECK(jump_session.periodic_counter() == 4);
    OL_CHECK(jump_session.resume(SceneResponse::acknowledge).kind ==
             SceneStepKind::fade_from_black);
    OL_CHECK(jump_session.scene_id() == 71);
    OL_CHECK(jump_session.periodic_counter() == 4);
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
    const auto set_animation = [&](const std::size_t event_index,
                                   const std::size_t cell,
                                   const std::int16_t first,
                                   const std::int16_t end,
                                   const std::int16_t displayed,
                                   const std::int16_t delay) {
        OL_CHECK(snapshot.set_scene_value(
            70U, SceneLayer::event_index, cell, static_cast<std::int16_t>(event_index)));
        OL_CHECK(snapshot.set_event_value(
            70U, event_index, SceneEventField::current_picture, first));
        OL_CHECK(snapshot.set_event_value(
            70U, event_index, SceneEventField::end_picture, end));
        OL_CHECK(snapshot.set_event_value(
            70U, event_index, SceneEventField::begin_picture, displayed));
        OL_CHECK(snapshot.set_event_value(
            70U, event_index, SceneEventField::picture_delay, delay));
    };
    set_animation(195U, 2U, 0, 110, 108, -1);
    set_animation(196U, 3U, 200, 210, 210, 1);
    set_animation(197U, 4U, 300, 310, 310, -1);
    set_animation(198U, 5U, 400, 410, 400, 1);

    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(snapshot.event_value(
                 70U, event, SceneEventField::begin_picture).value_or(-1) == 106);
    OL_CHECK(snapshot.event_value(
                 70U, 195U, SceneEventField::begin_picture).value_or(-1) == 108);
    OL_CHECK(snapshot.event_value(
                 70U, 196U, SceneEventField::begin_picture).value_or(-1) == 200);
    OL_CHECK(snapshot.event_value(
                 70U, 197U, SceneEventField::begin_picture).value_or(-1) == 302);
    OL_CHECK(snapshot.event_value(
                 70U, 198U, SceneEventField::begin_picture).value_or(-1) == 400);
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
    session.idle_tick();
    OL_CHECK(snapshot.event_value(
                 70U, 196U, SceneEventField::begin_picture).value_or(-1) == 202);
    OL_CHECK(snapshot.event_value(
                 70U, 198U, SceneEventField::begin_picture).value_or(-1) == 402);
    session.idle_tick();
    session.idle_tick();
    session.idle_tick();
    OL_CHECK(snapshot.event_value(
                 70U, event, SceneEventField::current_picture).value_or(-1) == 100);
    OL_CHECK(snapshot.event_value(
                 70U, event, SceneEventField::begin_picture).value_or(-1) == 110);

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
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x3051F24E11C08DC5ULL);

    OL_CHECK(render_snapshot.set_event_value(
        70U, event, SceneEventField::current_picture, 0));
    openlegend::render::IndexedFramebuffer gated_framebuffer;
    OL_CHECK(render_session.render_map(gated_framebuffer));
    OL_CHECK(fnv1a64(gated_framebuffer.pixels()) == 0x38FBAA07B733AD79ULL);
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

    openlegend::render::IndexedFramebuffer weather_frame;
    OL_CHECK(session.render_map(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == 0x9EB8192A25F56019ULL);
    OL_CHECK(random.state() == 0x967EB0E7U);
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::scene_title);
    const auto title_random_state = random.state();
    OL_CHECK(session.render(weather_frame));
    const auto title_hash = fnv1a64(weather_frame.pixels());
    OL_CHECK(random.state() == title_random_state);
    OL_CHECK(session.render(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == title_hash);
    OL_CHECK(random.state() == title_random_state);
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::present);
    openlegend::random::LegacyRandom expected_after_title{title_random_state};
    static_cast<void>(expected_after_title.bounded(7));
    static_cast<void>(expected_after_title.bounded(7));
    OL_CHECK(session.render(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == 0x9EB8192A25F56019ULL);
    OL_CHECK(random.state() == expected_after_title.state());
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);

    auto dialogue = session.begin_event(18, 0, 17, 48);
    OL_CHECK(dialogue.kind == openlegend::scene::SceneStepKind::dialogue);
    OL_CHECK(dialogue.talk_id == 2960 && dialogue.head_id == 93 && dialogue.style == 0);
    const auto first_page_random_state = random.state();
    OL_CHECK(session.render(weather_frame));
    const auto first_page_hash = fnv1a64(weather_frame.pixels());
    OL_CHECK(first_page_hash == 0x8D9F538B1482E95EULL);
    OL_CHECK(random.state() == first_page_random_state);
    OL_CHECK(session.render(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == first_page_hash);
    OL_CHECK(random.state() == first_page_random_state);

    dialogue = session.resume(openlegend::scene::SceneResponse::acknowledge);
    OL_CHECK(dialogue.kind == openlegend::scene::SceneStepKind::dialogue);
    OL_CHECK(dialogue.talk_id == 2960);
    openlegend::random::LegacyRandom expected_second_page{first_page_random_state};
    static_cast<void>(expected_second_page.bounded(7));
    static_cast<void>(expected_second_page.bounded(7));
    OL_CHECK(session.render(weather_frame));
    const auto second_page_hash = fnv1a64(weather_frame.pixels());
    OL_CHECK(second_page_hash == 0x372FE4647B884671ULL);
    OL_CHECK(random.state() == expected_second_page.state());
    OL_CHECK(session.render(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == second_page_hash);
    OL_CHECK(random.state() == expected_second_page.state());

    dialogue = session.resume(openlegend::scene::SceneResponse::acknowledge);
    OL_CHECK(dialogue.kind == openlegend::scene::SceneStepKind::present);
    openlegend::random::LegacyRandom expected_after_dialogue{expected_second_page.state()};
    static_cast<void>(expected_after_dialogue.bounded(7));
    static_cast<void>(expected_after_dialogue.bounded(7));
    OL_CHECK(session.render(weather_frame));
    OL_CHECK(random.state() == expected_after_dialogue.state());
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);

    OL_CHECK(session.tick(std::nullopt, false, false, true).kind ==
             openlegend::scene::SceneStepKind::present);
    OL_CHECK(!session.weather_enabled());
    openlegend::render::IndexedFramebuffer disabled_frame;
    OL_CHECK(session.render_map(disabled_frame));
    OL_CHECK(fnv1a64(disabled_frame.pixels()) == 0x52C8861F0349D6DBULL);
    OL_CHECK(random.state() == expected_after_dialogue.state());

    auto preserved_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom preserved_random{1U};
    openlegend::scene::SceneSession preserved{
        data_root, preserved_snapshot, preserved_random, 4};
    OL_CHECK(preserved.valid());
    openlegend::render::IndexedFramebuffer preserved_frame;
    preserved_frame.clear(255U);
    OL_CHECK(preserved.render_map(preserved_frame));
    OL_CHECK(fnv1a64(preserved_frame.pixels()) == 0xA28918F3E9080082ULL);
    OL_CHECK(preserved_random.state() == 1U);
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

void check_event_camera_pan_word_wrap(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    auto snapshot = load_baseline(root);
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);

    auto result = session.begin_event(22, 0, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(result.wait_ticks == 2U);
    OL_CHECK(session.view_origin_x() == 36);
    OL_CHECK(session.view_origin_y() == 18);
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(result.wait_ticks == 2U);
    OL_CHECK(session.view_origin_x() == 36);
    OL_CHECK(session.view_origin_y() == 36);
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    OL_CHECK(session.scene_x() == 44 && session.scene_y() == 29);
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

void check_event_picture_animation_boundaries(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    auto snapshot = load_baseline(root);
    for (const auto field : {
             openlegend::model::SceneEventField::current_picture,
             openlegend::model::SceneEventField::end_picture,
             openlegend::model::SceneEventField::begin_picture}) {
        OL_CHECK(snapshot.set_event_value(70U, 4U, field, -123));
    }
    openlegend::random::LegacyRandom random{1U};
    openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
    OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);

    auto result = session.begin_event(23, 4, 44, 29);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(result.wait_ticks == 2U);
    for (const auto field : {
             openlegend::model::SceneEventField::current_picture,
             openlegend::model::SceneEventField::end_picture,
             openlegend::model::SceneEventField::begin_picture}) {
        OL_CHECK(snapshot.event_value(70U, 4U, field).value_or(-1) == 32767);
    }
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(result.wait_ticks == 2U);
    OL_CHECK(session.player_frame() == 10);
    OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
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
        0xB53215FF283C375AULL,
        0xA6CAE4420F245B6EULL,
        0x3F2F76D31C813BF3ULL,
        0xE1D227F227EF9D0EULL,
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
        const auto actual_hash = fnv1a64(framebuffer.pixels());
        if (actual_hash != expected_hashes[index]) {
            std::cerr << "script 343 frame " << index << ": actual 0x"
                      << std::hex << actual_hash << std::dec << '\n';
        }
        OL_CHECK(actual_hash == expected_hashes[index]);
        result = session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(result.wait_ticks == 1U);
    OL_CHECK(session.player_frame() == 5002);
    openlegend::render::IndexedFramebuffer standing_framebuffer;
    OL_CHECK(session.render_map(standing_framebuffer));
    OL_CHECK(fnv1a64(standing_framebuffer.pixels()) == 0x83BC0F8904252115ULL);
    result = session.resume(SceneResponse::acknowledge);
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
        OL_CHECK(blocked.direction() == openlegend::scene::SceneDirection::up);
        OL_CHECK(blocked.player_frame() == frame);
        blocked_result = blocked.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(blocked_result.kind == SceneStepKind::present);
    OL_CHECK(blocked_result.wait_ticks == 1U);
    OL_CHECK(blocked.player_frame() == 5002);
    blocked_result = blocked.resume(SceneResponse::acknowledge);
    OL_CHECK(blocked_result.kind == SceneStepKind::dialogue);
    OL_CHECK(blocked_result.talk_id == 1248);
    OL_CHECK(blocked.scene_y() == 23);
}

void check_event_scripted_walk_boundaries(const std::filesystem::path& root) {
    using openlegend::model::SceneLayer;
    using openlegend::scene::SceneDirection;
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    const auto check_walk = [&data_root, &root](
                                const int script,
                                const std::span<const std::array<int, 4>> expected) {
        auto snapshot = load_baseline(root);
        auto& metadata = snapshot.ranger.scenes[70];
        metadata.set_word(openlegend::model::scene_metadata_word::entrance_x, 10);
        metadata.set_word(openlegend::model::scene_metadata_word::entrance_y, 10);
        metadata.set_word(openlegend::model::scene_metadata_word::jump_scene, -1);
        for (std::size_t index = 0U;
             index < openlegend::model::scene_metadata_word::exit_count;
             ++index) {
            metadata.set_word(openlegend::model::scene_metadata_word::exit_x_begin + index, -1);
            metadata.set_word(openlegend::model::scene_metadata_word::exit_y_begin + index, -1);
        }
        for (int y = 8; y <= 12; ++y) {
            for (int x = 8; x <= 18; ++x) {
                const auto cell = static_cast<std::size_t>(y * 64 + x);
                OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::earth, cell, 0));
                OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building, cell, 0));
                OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::event_index, cell, -1));
                OL_CHECK(snapshot.set_scene_value(70U, SceneLayer::building_height, cell, 0));
            }
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
        if (script == 29) {
            for (int tick = 0; tick < 20; ++tick) {
                OL_CHECK(session.tick(std::nullopt, false, false).kind == SceneStepKind::present);
                OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
            }
        }
        auto result = session.begin_event(script, 0, 0, 0);
        std::vector<std::array<int, 4>> actual;
        while (result.kind == SceneStepKind::present && result.wait_ticks == 3U) {
            actual.push_back({
                session.scene_x(), session.scene_y(),
                static_cast<int>(session.direction()), session.player_frame(),
            });
            result = session.resume(SceneResponse::acknowledge);
        }
        OL_CHECK(actual.size() == expected.size());
        if (actual.size() == expected.size()) {
            OL_CHECK(std::equal(actual.begin(), actual.end(), expected.begin()));
        }
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 1U);
        if (script != 29) {
            OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
            return;
        }
        result = session.resume(SceneResponse::acknowledge);
        OL_CHECK(result.kind == SceneStepKind::present);
        OL_CHECK(result.wait_ticks == 2U);
        OL_CHECK(session.player_frame() == 10);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
        OL_CHECK(session.tick(std::nullopt, false, false).kind == SceneStepKind::present);
        OL_CHECK(session.player_frame() == 10);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    };

    check_walk(25, std::array<std::array<int, 4>, 4>{{
                       {11, 10, static_cast<int>(SceneDirection::right), 5018},
                       {12, 10, static_cast<int>(SceneDirection::right), 5020},
                       {12, 11, static_cast<int>(SceneDirection::down), 5050},
                       {12, 12, static_cast<int>(SceneDirection::down), 5052},
                   }});
    check_walk(26, std::array<std::array<int, 4>, 4>{{
                       {11, 10, static_cast<int>(SceneDirection::right), 5018},
                       {12, 10, static_cast<int>(SceneDirection::right), 5020},
                       {12, 9, static_cast<int>(SceneDirection::up), 5008},
                       {12, 8, static_cast<int>(SceneDirection::up), 5010},
                   }});
    check_walk(27, std::array<std::array<int, 4>, 4>{{
                       {9, 10, static_cast<int>(SceneDirection::left), 5032},
                       {8, 10, static_cast<int>(SceneDirection::left), 5034},
                       {8, 11, static_cast<int>(SceneDirection::down), 5050},
                       {8, 12, static_cast<int>(SceneDirection::down), 5052},
                   }});
    check_walk(28, std::array<std::array<int, 4>, 4>{{
                       {9, 10, static_cast<int>(SceneDirection::left), 5032},
                       {8, 10, static_cast<int>(SceneDirection::left), 5034},
                       {8, 9, static_cast<int>(SceneDirection::up), 5008},
                       {8, 8, static_cast<int>(SceneDirection::up), 5010},
                   }});
    check_walk(29, std::array<std::array<int, 4>, 0>{});
    check_walk(30, std::array<std::array<int, 4>, 8>{{
                       {11, 10, static_cast<int>(SceneDirection::right), 5018},
                       {12, 10, static_cast<int>(SceneDirection::right), 5020},
                       {13, 10, static_cast<int>(SceneDirection::right), 5022},
                       {14, 10, static_cast<int>(SceneDirection::right), 5024},
                       {15, 10, static_cast<int>(SceneDirection::right), 5026},
                       {16, 10, static_cast<int>(SceneDirection::right), 5028},
                       {17, 10, static_cast<int>(SceneDirection::right), 5018},
                       {18, 10, static_cast<int>(SceneDirection::right), 5020},
                   }});
    check_walk(31, std::array<std::array<int, 4>, 2>{{
                       {11, 10, static_cast<int>(SceneDirection::right), 5018},
                       {12, 10, static_cast<int>(SceneDirection::right), 5020},
                   }});
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
    using openlegend::model::SceneEventField;
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
            for (std::size_t field = 0U; field < 8U; ++field) {
                OL_CHECK(snapshot.set_event_value(
                    shop_case.scene, event, static_cast<SceneEventField>(field), 777));
            }
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{
            data_root, snapshot, random, shop_case.scene};
        auto result = open_shop(session);
        OL_CHECK(result.kind == SceneStepKind::shop);
        OL_CHECK(result.shop_id == shop_case.shop);
        result = session.resume(SceneResponse::cancel);
        OL_CHECK(result.kind == SceneStepKind::stay);
        constexpr std::array<std::int16_t, 8> expected_fields{
            0, 0, -1, -1, 939, -1, -1, -1};
        for (const auto event : shop_case.close_events) {
            for (std::size_t field = 0U; field < expected_fields.size(); ++field) {
                OL_CHECK(snapshot.event_value(
                             shop_case.scene, event,
                             static_cast<SceneEventField>(field)).value_or(-2) ==
                         expected_fields[field]);
            }
        }
    }

    auto purchase_snapshot = load_baseline(root);
    auto& shop = purchase_snapshot.ranger.shops[1];
    shop.set_word(openlegend::model::shop_word::item_id_begin, 42);
    shop.set_word(openlegend::model::shop_word::total_begin, 2);
    shop.set_word(openlegend::model::shop_word::price_begin, 7);
    purchase_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{174}, 10);
    purchase_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{42}, 32767);
    purchase_snapshot.ranger.header.set_inventory(
        openlegend::model::kInventoryCount - 1U,
        openlegend::model::ItemId{42},
        -32768);
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
    OL_CHECK(purchase.kind == SceneStepKind::present);
    purchase = purchase_session.resume(SceneResponse::acknowledge);
    OL_CHECK(purchase.kind == SceneStepKind::dialogue);
    OL_CHECK(purchase.talk_id == 2976);
    OL_CHECK(purchase_snapshot.event_value(
                 3U, 15U, openlegend::model::SceneEventField::event_3).value_or(-1) == -1);
    for (int step = 0; step < 16 && purchase.kind == SceneStepKind::dialogue; ++step) {
        purchase = purchase_session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(purchase.kind == SceneStepKind::present);
    purchase = purchase_session.resume(SceneResponse::acknowledge);
    OL_CHECK(purchase.kind == SceneStepKind::stay);
    OL_CHECK(shop.word(openlegend::model::shop_word::total_begin) == 1);
    OL_CHECK(purchase_snapshot.ranger.header.inventory_item(0U).value == 174);
    OL_CHECK(purchase_snapshot.ranger.header.inventory_count(0U) == 3);
    OL_CHECK(purchase_snapshot.ranger.header.inventory_item(1U).value == 42);
    OL_CHECK(purchase_snapshot.ranger.header.inventory_count(1U) == -32768);
    OL_CHECK(purchase_snapshot.ranger.header.inventory_item(
                 openlegend::model::kInventoryCount - 1U).value == 42);
    OL_CHECK(purchase_snapshot.ranger.header.inventory_count(
                 openlegend::model::kInventoryCount - 1U) == -32767);
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
    OL_CHECK(split_purchase.kind == SceneStepKind::present);
    split_purchase = split_money_session.resume(SceneResponse::acknowledge);
    OL_CHECK(split_purchase.kind == SceneStepKind::dialogue);
    OL_CHECK(split_purchase.talk_id == 2975);
    OL_CHECK(split_money_shop.word(openlegend::model::shop_word::total_begin) == 2);
    OL_CHECK(split_money_snapshot.ranger.header.inventory_count(0U) == 5);
    OL_CHECK(split_money_snapshot.ranger.header.inventory_count(1U) == 100);
    OL_CHECK(inventory_count(split_money_snapshot.ranger, 42) == item_count_before);
    split_purchase = split_money_session.resume(SceneResponse::acknowledge);
    OL_CHECK(split_purchase.kind == SceneStepKind::present);
    OL_CHECK(split_money_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

    auto absent_money_snapshot = load_baseline(root);
    auto& absent_money_shop = absent_money_snapshot.ranger.shops[1];
    absent_money_shop.set_word(openlegend::model::shop_word::item_id_begin, 42);
    absent_money_shop.set_word(openlegend::model::shop_word::total_begin, 2);
    absent_money_shop.set_word(openlegend::model::shop_word::price_begin, 0);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        absent_money_snapshot.ranger.header.set_inventory(
            slot, openlegend::model::ItemId{-1}, 0);
    }
    openlegend::random::LegacyRandom absent_money_random{1U};
    openlegend::scene::SceneSession absent_money_session{
        data_root, absent_money_snapshot, absent_money_random, 3};
    auto absent_purchase = open_shop(absent_money_session);
    OL_CHECK(absent_purchase.kind == SceneStepKind::shop);
    absent_purchase = absent_money_session.resume(SceneResponse::yes, 0);
    OL_CHECK(absent_purchase.kind == SceneStepKind::present);
    absent_purchase = absent_money_session.resume(SceneResponse::acknowledge);
    OL_CHECK(absent_purchase.kind == SceneStepKind::dialogue);
    OL_CHECK(absent_purchase.talk_id == 2975);
    OL_CHECK(absent_money_shop.word(openlegend::model::shop_word::total_begin) == 2);
    OL_CHECK(inventory_count(absent_money_snapshot.ranger, 42) == 0);

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

    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        item_snapshot.ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
    }
    item_snapshot.ranger.header.set_inventory(
        openlegend::model::kInventoryCount - 1U,
        openlegend::model::ItemId{173}, std::int16_t{-32768});
    result = item_session.begin_event(37, 0, 0, 0);
    OL_CHECK(result.kind == SceneStepKind::stay);

    const auto party_tail_dialogue = [&data_root, &root](
                                         const std::int16_t tail_value,
                                         const bool interior_gap) {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kTeamMemberCount; ++index) {
            snapshot.ranger.header.set_team_member(
                index, openlegend::model::CharacterId{
                           interior_gap && index == 2U
                               ? std::int16_t{-1}
                               : static_cast<std::int16_t>(index + 1U)});
        }
        snapshot.ranger.header.set_team_member(
            openlegend::model::kTeamMemberCount - 1U,
            openlegend::model::CharacterId{tail_value});
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        auto step = session.begin_event(11, 0, 0, 0);
        OL_CHECK(step.kind == SceneStepKind::dialogue);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::present);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::question);
        step = session.resume(SceneResponse::yes);
        OL_CHECK(step.kind == SceneStepKind::present);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::dialogue);
        OL_CHECK(step.talk_id == 29);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::present);
        step = session.resume(SceneResponse::acknowledge);
        OL_CHECK(step.kind == SceneStepKind::dialogue);
        return step.talk_id;
    };
    OL_CHECK(party_tail_dialogue(-1, false) == 30);
    OL_CHECK(party_tail_dialogue(0, false) == 30);
    OL_CHECK(party_tail_dialogue(9, true) == 175);

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
    OL_CHECK(!reaches_success_dialogue(-32768));
    OL_CHECK(!reaches_success_dialogue(79));
    OL_CHECK(reaches_success_dialogue(80));
    OL_CHECK(reaches_success_dialogue(100));
    OL_CHECK(!reaches_success_dialogue(101));
    OL_CHECK(!reaches_success_dialogue(32767));
}

void check_event_attack_condition_boundaries(const std::filesystem::path& root) {
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    const auto attack_succeeds = [&data_root, &root](const std::int16_t attack) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::attack, attack);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
        const auto result = session.begin_event(24, 0, 0, 0);
        return result.kind == SceneStepKind::dialogue && result.talk_id == 100;
    };
    OL_CHECK(!attack_succeeds(-32768));
    OL_CHECK(!attack_succeeds(89));
    OL_CHECK(attack_succeeds(90));
    OL_CHECK(attack_succeeds(1000));
    OL_CHECK(attack_succeeds(2000));
    OL_CHECK(attack_succeeds(32767));
}

void check_event_inventory_condition_edge_cases(const std::filesystem::path& root) {
    using openlegend::scene::SceneStepKind;

    const openlegend::resource::DataRoot data_root{root};
    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot synthetic_data_root{synthetic.path()};
    const auto money_condition_succeeds = [&synthetic_data_root, &root](
                                              const int script,
                                              const std::optional<std::int16_t> count) {
        auto snapshot = load_baseline(root);
        for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
            snapshot.ranger.header.set_inventory(slot, openlegend::model::ItemId{-1}, 0);
        }
        if (count.has_value()) {
            snapshot.ranger.header.set_inventory(
                0U, openlegend::model::ItemId{174}, *count);
        }
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{
            synthetic_data_root, snapshot, random, 70};
        const auto result = session.begin_event(script, 0, 0, 0);
        return result.kind == SceneStepKind::dialogue && result.talk_id == 100;
    };
    OL_CHECK(!money_condition_succeeds(32, std::nullopt));
    OL_CHECK(!money_condition_succeeds(32, -1));
    OL_CHECK(money_condition_succeeds(32, 0));
    OL_CHECK(!money_condition_succeeds(33, std::nullopt));
    OL_CHECK(money_condition_succeeds(33, -32768));
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

    const auto clear_inventory = [](openlegend::model::GameSnapshot& snapshot) {
        for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
            snapshot.ranger.header.set_inventory(
                slot, openlegend::model::ItemId{-1}, 0);
        }
    };
    auto absent_change_snapshot = load_baseline(root);
    clear_inventory(absent_change_snapshot);
    absent_change_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{88}, 4);
    absent_change_snapshot.ranger.header.set_inventory(
        openlegend::model::kInventoryCount - 1U,
        openlegend::model::ItemId{77}, 5);
    openlegend::random::LegacyRandom absent_change_random{1U};
    openlegend::scene::SceneSession absent_change_session{
        synthetic_data_root, absent_change_snapshot, absent_change_random, 70};
    OL_CHECK(absent_change_session.begin_event(34, 0, 0, 0).kind == SceneStepKind::stay);
    OL_CHECK(absent_change_snapshot.ranger.header.inventory_item(0U).value == 88);
    OL_CHECK(absent_change_snapshot.ranger.header.inventory_count(0U) == 4);
    OL_CHECK(absent_change_snapshot.ranger.header.inventory_item(
                 openlegend::model::kInventoryCount - 1U).value == 77);
    OL_CHECK(absent_change_snapshot.ranger.header.inventory_count(
                 openlegend::model::kInventoryCount - 1U) == 5);

    auto positive_change_snapshot = load_baseline(root);
    clear_inventory(positive_change_snapshot);
    positive_change_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{109}, 1);
    positive_change_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{109}, 7);
    positive_change_snapshot.ranger.header.set_inventory(
        2U, openlegend::model::ItemId{88}, 4);
    openlegend::random::LegacyRandom positive_change_random{1U};
    openlegend::scene::SceneSession positive_change_session{
        synthetic_data_root, positive_change_snapshot, positive_change_random, 70};
    OL_CHECK(positive_change_session.begin_event(34, 0, 0, 0).kind == SceneStepKind::stay);
    OL_CHECK(positive_change_snapshot.ranger.header.inventory_count(0U) == 2);
    OL_CHECK(positive_change_snapshot.ranger.header.inventory_count(1U) == 7);
    OL_CHECK(positive_change_snapshot.ranger.header.inventory_item(2U).value == 88);
    OL_CHECK(positive_change_snapshot.ranger.header.inventory_count(2U) == 4);

    auto compact_change_snapshot = load_baseline(root);
    clear_inventory(compact_change_snapshot);
    compact_change_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{109}, -1);
    compact_change_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{109}, 7);
    compact_change_snapshot.ranger.header.set_inventory(
        2U, openlegend::model::ItemId{88}, 4);
    compact_change_snapshot.ranger.header.set_inventory(
        3U, openlegend::model::ItemId{-1}, 9);
    compact_change_snapshot.ranger.header.set_inventory(
        openlegend::model::kInventoryCount - 1U,
        openlegend::model::ItemId{77}, 5);
    openlegend::random::LegacyRandom compact_change_random{1U};
    openlegend::scene::SceneSession compact_change_session{
        synthetic_data_root, compact_change_snapshot, compact_change_random, 70};
    OL_CHECK(compact_change_session.begin_event(34, 0, 0, 0).kind == SceneStepKind::stay);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_item(0U).value == 109);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_count(0U) == 7);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_item(1U).value == 88);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_count(1U) == 4);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_item(2U).value == -1);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_count(2U) == 9);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_item(
                 openlegend::model::kInventoryCount - 2U).value == 77);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_count(
                 openlegend::model::kInventoryCount - 2U) == 5);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_item(
                 openlegend::model::kInventoryCount - 1U).value == -1);
    OL_CHECK(compact_change_snapshot.ranger.header.inventory_count(
                 openlegend::model::kInventoryCount - 1U) == 0);

    auto wrapping_change_snapshot = load_baseline(root);
    clear_inventory(wrapping_change_snapshot);
    wrapping_change_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{109}, 32767);
    wrapping_change_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{88}, 4);
    openlegend::random::LegacyRandom wrapping_change_random{1U};
    openlegend::scene::SceneSession wrapping_change_session{
        synthetic_data_root, wrapping_change_snapshot, wrapping_change_random, 70};
    OL_CHECK(wrapping_change_session.begin_event(34, 0, 0, 0).kind == SceneStepKind::stay);
    OL_CHECK(wrapping_change_snapshot.ranger.header.inventory_item(0U).value == 88);
    OL_CHECK(wrapping_change_snapshot.ranger.header.inventory_count(0U) == 4);
    OL_CHECK(wrapping_change_snapshot.ranger.header.inventory_item(1U).value == -1);
    OL_CHECK(wrapping_change_snapshot.ranger.header.inventory_count(1U) == 0);

    auto negative_wrap_snapshot = load_baseline(root);
    clear_inventory(negative_wrap_snapshot);
    negative_wrap_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{109}, -32768);
    negative_wrap_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{109}, 7);
    openlegend::random::LegacyRandom negative_wrap_random{1U};
    openlegend::scene::SceneSession negative_wrap_session{
        synthetic_data_root, negative_wrap_snapshot, negative_wrap_random, 70};
    OL_CHECK(negative_wrap_session.begin_event(35, 0, 0, 0).kind == SceneStepKind::stay);
    OL_CHECK(negative_wrap_snapshot.ranger.header.inventory_item(0U).value == 109);
    OL_CHECK(negative_wrap_snapshot.ranger.header.inventory_count(0U) == 32767);
    OL_CHECK(negative_wrap_snapshot.ranger.header.inventory_count(1U) == 7);

    auto tail_change_snapshot = load_baseline(root);
    clear_inventory(tail_change_snapshot);
    tail_change_snapshot.ranger.header.set_inventory(
        openlegend::model::kInventoryCount - 2U,
        openlegend::model::ItemId{88}, 4);
    tail_change_snapshot.ranger.header.set_inventory(
        openlegend::model::kInventoryCount - 1U,
        openlegend::model::ItemId{109}, -1);
    openlegend::random::LegacyRandom tail_change_random{1U};
    openlegend::scene::SceneSession tail_change_session{
        synthetic_data_root, tail_change_snapshot, tail_change_random, 70};
    OL_CHECK(tail_change_session.begin_event(34, 0, 0, 0).kind == SceneStepKind::stay);
    OL_CHECK(tail_change_snapshot.ranger.header.inventory_item(
                 openlegend::model::kInventoryCount - 2U).value == 88);
    OL_CHECK(tail_change_snapshot.ranger.header.inventory_count(
                 openlegend::model::kInventoryCount - 2U) == 4);
    OL_CHECK(tail_change_snapshot.ranger.header.inventory_item(
                 openlegend::model::kInventoryCount - 1U).value == -1);
    OL_CHECK(tail_change_snapshot.ranger.header.inventory_count(
                 openlegend::model::kInventoryCount - 1U) == 0);

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
    OL_CHECK(duplicate_add.style == -1);
    constexpr std::array<std::uint8_t, 11> expected_item_109_notice{
        0xB1U, 0x6FU, 0xA8U, 0xECU, 0xADU, 0xCAU, 0xA4U, 0xD1U, 0xBCU, 0x43U, 0x00U};
    OL_CHECK(duplicate_session.pending_text().size() == expected_item_109_notice.size());
    OL_CHECK(std::equal(
        duplicate_session.pending_text().begin(), duplicate_session.pending_text().end(),
        expected_item_109_notice.begin()));
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_item(0U).value == 109);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_count(0U) == 3);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_item(1U).value == 109);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_count(1U) == 4);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_item(2U).value == 88);
    OL_CHECK(duplicate_snapshot.ranger.header.inventory_count(2U) == 4);
    openlegend::render::IndexedFramebuffer duplicate_frame;
    OL_CHECK(duplicate_session.render_map(duplicate_frame));
    OL_CHECK(duplicate_session.render(duplicate_frame));
    OL_CHECK(fnv1a64(duplicate_frame.pixels()) == 0x8397BA508B05051FULL);
    OL_CHECK(duplicate_session.render(duplicate_frame));
    OL_CHECK(fnv1a64(duplicate_frame.pixels()) == 0x8397BA508B05051FULL);

    auto wrapping_snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        wrapping_snapshot.ranger.header.set_inventory(
            slot, openlegend::model::ItemId{-1}, 0);
    }
    wrapping_snapshot.ranger.header.set_inventory(
        0U, openlegend::model::ItemId{109}, 32767);
    wrapping_snapshot.ranger.header.set_inventory(
        1U, openlegend::model::ItemId{109}, -32768);
    openlegend::random::LegacyRandom wrapping_random{1U};
    openlegend::scene::SceneSession wrapping_session{
        data_root, wrapping_snapshot, wrapping_random, 70};
    OL_CHECK(wrapping_session.begin_event(149, 0, 0, 0).kind == SceneStepKind::notice);
    OL_CHECK(wrapping_snapshot.ranger.header.inventory_count(0U) == -32768);
    OL_CHECK(wrapping_snapshot.ranger.header.inventory_count(1U) == -32767);

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

    auto full_snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        full_snapshot.ranger.header.set_inventory(
            slot, openlegend::model::ItemId{0}, 7);
    }
    openlegend::random::LegacyRandom full_random{1U};
    openlegend::scene::SceneSession full_session{
        data_root, full_snapshot, full_random, 70};
    OL_CHECK(full_session.begin_event(149, 0, 0, 0).kind == SceneStepKind::notice);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        OL_CHECK(full_snapshot.ranger.header.inventory_item(slot).value == 0);
        OL_CHECK(full_snapshot.ranger.header.inventory_count(slot) == 7);
    }

    auto weather_snapshot = load_baseline(root);
    openlegend::random::LegacyRandom weather_random{1U};
    openlegend::scene::SceneSession weather_session{
        data_root, weather_snapshot, weather_random, 5};
    openlegend::render::IndexedFramebuffer weather_frame;
    OL_CHECK(weather_session.render_map(weather_frame));
    const auto weather_state_before_notice = weather_random.state();
    OL_CHECK(weather_session.begin_event(149, 0, 0, 0).kind == SceneStepKind::notice);
    OL_CHECK(weather_session.render(weather_frame));
    const auto weather_notice_hash = fnv1a64(weather_frame.pixels());
    OL_CHECK(weather_random.state() == weather_state_before_notice);
    OL_CHECK(weather_session.render(weather_frame));
    OL_CHECK(fnv1a64(weather_frame.pixels()) == weather_notice_hash);
    OL_CHECK(weather_random.state() == weather_state_before_notice);

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

void check_event_learn_magic(const std::filesystem::path& root) {
    using openlegend::scene::SceneResponse;
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    const auto fill_magic_slots = [](openlegend::model::RoleRecord& role) {
        for (std::size_t slot = 0U; slot < openlegend::model::role_word::magic_count; ++slot) {
            role.set_word(
                openlegend::model::role_word::magic_id_begin + slot,
                static_cast<std::int16_t>(40 + slot));
            role.set_word(openlegend::model::role_word::magic_level_begin + slot, 100);
        }
    };
    constexpr std::array<std::uint8_t, 21> expected_notice{
        0xB5U, 0xEAU, 0xA6U, 0xCBU, 0x20U, 0xBEU, 0xC7U,
        0xB7U, 0x7CU, 0x20U, 0xA4U, 0xD1U, 0xA4U, 0x73U,
        0xA4U, 0xBBU, 0xB6U, 0xA7U, 0xB4U, 0x78U, 0x00U};

    auto empty_snapshot = load_baseline(root);
    auto& empty_role = empty_snapshot.ranger.roles[49];
    fill_magic_slots(empty_role);
    empty_role.set_word(openlegend::model::role_word::magic_id_begin + 2U, 0);
    openlegend::random::LegacyRandom empty_random{1U};
    openlegend::scene::SceneSession empty_session{
        data_root, empty_snapshot, empty_random, 70};
    OL_CHECK(finish_scene_title(empty_session).kind == SceneStepKind::stay);
    openlegend::render::IndexedFramebuffer empty_frame;
    OL_CHECK(empty_session.render_map(empty_frame));
    const auto random_before_notice = empty_random.state();
    auto empty_result = empty_session.begin_event(36, 0, 0, 0);
    OL_CHECK(empty_result.kind == SceneStepKind::notice);
    OL_CHECK(empty_result.style == -2);
    OL_CHECK(empty_session.pending_text().size() == expected_notice.size());
    OL_CHECK(std::equal(
        empty_session.pending_text().begin(), empty_session.pending_text().end(),
        expected_notice.begin()));
    OL_CHECK(empty_role.word(openlegend::model::role_word::magic_id_begin + 2U) == 15);
    OL_CHECK(empty_role.word(openlegend::model::role_word::magic_level_begin + 2U) == 0);
    OL_CHECK(empty_role.word(openlegend::model::role_word::magic_id_begin) == 40);
    OL_CHECK(empty_role.word(openlegend::model::role_word::magic_level_begin) == 100);
    OL_CHECK(empty_session.render(empty_frame));
    const auto notice_hash = fnv1a64(empty_frame.pixels());
    OL_CHECK(notice_hash == 0x7C2B94DA4729E581ULL);
    OL_CHECK(empty_random.state() == random_before_notice);
    OL_CHECK(empty_session.render(empty_frame));
    OL_CHECK(fnv1a64(empty_frame.pixels()) == notice_hash);
    OL_CHECK(empty_random.state() == random_before_notice);
    OL_CHECK(empty_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
    OL_CHECK(empty_session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);

    auto full_snapshot = load_baseline(root);
    auto& full_role = full_snapshot.ranger.roles[49];
    fill_magic_slots(full_role);
    openlegend::random::LegacyRandom full_random{1U};
    openlegend::scene::SceneSession full_session{data_root, full_snapshot, full_random, 70};
    OL_CHECK(finish_scene_title(full_session).kind == SceneStepKind::stay);
    const auto full_result = full_session.begin_event(36, 0, 0, 0);
    OL_CHECK(full_result.kind == SceneStepKind::notice);
    OL_CHECK(full_role.word(openlegend::model::role_word::magic_id_begin) == 15);
    OL_CHECK(full_role.word(openlegend::model::role_word::magic_level_begin) == 0);
    OL_CHECK(full_role.word(openlegend::model::role_word::magic_id_begin + 1U) == 41);
    OL_CHECK(full_role.word(openlegend::model::role_word::magic_level_begin + 1U) == 100);

    for (const auto [script, silent] : std::array<std::pair<int, std::int16_t>, 2>{
             std::pair{37, std::int16_t{1}},
             std::pair{38, std::int16_t{-32768}}}) {
        auto silent_snapshot = load_baseline(root);
        auto& silent_role = silent_snapshot.ranger.roles[49];
        fill_magic_slots(silent_role);
        silent_role.set_word(openlegend::model::role_word::magic_id_begin + 4U, 0);
        openlegend::random::LegacyRandom silent_random{1U};
        openlegend::scene::SceneSession silent_session{
            data_root, silent_snapshot, silent_random, 70};
        OL_CHECK(finish_scene_title(silent_session).kind == SceneStepKind::stay);
        OL_CHECK(silent_session.begin_event(script, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(silent_role.word(openlegend::model::role_word::magic_id_begin + 4U) == 15);
        OL_CHECK(silent_role.word(openlegend::model::role_word::magic_level_begin + 4U) == 0);
        OL_CHECK(silent != 0);
        OL_CHECK(silent_session.pending_text().empty());
    }
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
    role.set_word(openlegend::model::role_word::fame, 200);
    for (std::int16_t item_id = 144; item_id <= 157; ++item_id) {
        snapshot.ranger.header.set_inventory(
            static_cast<std::size_t>(item_id - 144), openlegend::model::ItemId{item_id}, 0);
    }
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
    int fade_from_black_steps = 0;
    int fade_to_black_steps = 0;
    SceneStepKind previous_kind = SceneStepKind::stay;
    std::int16_t last_talk_id = -1;
    bool expect_reward_restore = false;
    for (int step = 0; step < 4096 && result.kind != SceneStepKind::stay; ++step) {
        if (expect_reward_restore) {
            OL_CHECK(result.kind == SceneStepKind::present);
        }
        expect_reward_restore = result.kind == SceneStepKind::notice;
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
            ++fade_to_black_steps;
            if (result.wait_ticks == 9U) {
                ++interround_holds;
            } else {
                OL_CHECK(result.wait_ticks == 1U);
            }
            previous_kind = result.kind;
            result = session.resume(SceneResponse::acknowledge);
        } else if (result.kind == SceneStepKind::notice ||
                   result.kind == SceneStepKind::present ||
                   result.kind == SceneStepKind::fade_from_black) {
            if (result.kind == SceneStepKind::fade_from_black) {
                ++fade_from_black_steps;
            }
            if (result.kind == SceneStepKind::notice) {
                constexpr std::array<std::uint8_t, 9> expected_reward_notice{
                    0xB1U, 0x6FU, 0xA8U, 0xECU, 0xAFU, 0xABU, 0xA7U, 0xFAU, 0x00U};
                OL_CHECK(result.style == -1);
                OL_CHECK(session.pending_text().size() == expected_reward_notice.size());
                OL_CHECK(std::equal(
                    session.pending_text().begin(), session.pending_text().end(),
                    expected_reward_notice.begin()));
            }
            previous_kind = result.kind;
            result = session.resume(SceneResponse::acknowledge);
        } else {
            break;
        }
    }
    OL_CHECK(!expect_reward_restore);
    OL_CHECK(result.kind == SceneStepKind::stay);
    OL_CHECK(battle_index == expected_battles.size());
    OL_CHECK(interround_holds == 4);
    OL_CHECK(fade_from_black_steps == 20);
    OL_CHECK(fade_to_black_steps == 5);
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
    OL_CHECK(snapshot.event_value(
                 70U, 11U, openlegend::model::SceneEventField::event_1).value_or(-1) == 932);
    OL_CHECK(snapshot.event_value(
                 70U, 11U, openlegend::model::SceneEventField::current_picture).value_or(-1) ==
             7968);

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
    defeat = defeat_session.resume(SceneResponse::acknowledge, static_cast<int>('y'));
    OL_CHECK(defeat.kind == SceneStepKind::death_menu);
    OL_CHECK(defeat.menu_index == 3);
    OL_CHECK(!defeat.death_confirm);
    defeat = defeat_session.resume(SceneResponse::acknowledge, 0x96);
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

    auto opcode_15_result = load_session.begin_event(190, 0, 0, 0);
    for (int step = 0; step < 8 && opcode_15_result.kind != SceneStepKind::battle; ++step) {
        OL_CHECK(
            opcode_15_result.kind == SceneStepKind::dialogue ||
            opcode_15_result.kind == SceneStepKind::present);
        opcode_15_result = load_session.resume(SceneResponse::acknowledge);
    }
    OL_CHECK(opcode_15_result.kind == SceneStepKind::battle);
    OL_CHECK(opcode_15_result.battle_id == 28);
    opcode_15_result = load_session.resume(SceneResponse::battle_defeat);
    OL_CHECK(opcode_15_result.kind == SceneStepKind::fade_from_black);
    opcode_15_result = load_session.resume(SceneResponse::acknowledge);
    OL_CHECK(opcode_15_result.kind == SceneStepKind::death_menu);
    OL_CHECK(opcode_15_result.menu_index == 0);
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

    constexpr std::array<std::uint8_t, 16> notice_prefix{
        0xAEU, 0x7DU, 0xA4U, 0x70U, 0xABU, 0x4CU, 0x20U, 0xB8U,
        0xEAU, 0xBDU, 0xE8U, 0xBCU, 0x57U, 0xA5U, 0x5BU, 0x20U};
    const auto expected_notice = [&notice_prefix](const int gain) {
        std::vector<std::uint8_t> result{notice_prefix.begin(), notice_prefix.end()};
        const auto digits = std::to_string(gain);
        result.insert(result.end(), digits.begin(), digits.end());
        result.push_back(0U);
        return result;
    };

    const openlegend::resource::DataRoot data_root{root};
    for (const auto [before, after] :
         std::array<std::pair<std::int16_t, std::int16_t>, 2>{
             std::pair<std::int16_t, std::int16_t>{99, 100}, {32766, 0}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::iq, before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        auto result = session.begin_event(673, 0, 0, 0);
        bool saw_iq_notice = false;
        for (int step = 0; step < 256 && result.kind != SceneStepKind::stay; ++step) {
            if (result.kind == SceneStepKind::notice) {
                const auto expected = expected_notice(1);
                if (session.pending_text().size() == expected.size() &&
                    std::equal(
                        session.pending_text().begin(), session.pending_text().end(),
                        expected.begin())) {
                    saw_iq_notice = true;
                    OL_CHECK(result.style == -3);
                }
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
        OL_CHECK(result.kind == SceneStepKind::stay);
        OL_CHECK(saw_iq_notice == (after > before));
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::iq) == after);
    }

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot synthetic_root{synthetic.path()};
    struct RoleIqCase {
        std::int16_t script;
        std::int16_t before;
        std::int16_t after;
        int gain;
        std::uint64_t frame_hash;
    };
    constexpr std::array<RoleIqCase, 5> cases{{
        {39, 99, 100, 1, 0x6E7FE3EB3D0961B7ULL},
        {40, 32766, 0, -1, 0U},
        {41, -10, 0, 10, 0x4E4523EE5F7AFDA0ULL},
        {42, -32768, 100, 32868, 0x225ECF10614F7CB8ULL},
        {43, 32767, 0, -1, 0U},
    }};
    for (const auto& test : cases) {
        auto snapshot = load_baseline(root);
        auto& role = snapshot.ranger.roles[0];
        role.set_word(openlegend::model::role_word::iq, test.before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{
            synthetic_root, snapshot, random, 70};
        OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render_map(framebuffer));
        const auto random_before_notice = random.state();
        auto result = session.begin_event(test.script, 0, 0, 0);
        if (test.gain > 0) {
            OL_CHECK(result.kind == SceneStepKind::notice);
            OL_CHECK(result.style == -3);
            const auto expected = expected_notice(test.gain);
            OL_CHECK(session.pending_text().size() == expected.size());
            OL_CHECK(std::equal(
                session.pending_text().begin(), session.pending_text().end(), expected.begin()));
            OL_CHECK(session.render(framebuffer));
            OL_CHECK(fnv1a64(framebuffer.pixels()) == test.frame_hash);
            OL_CHECK(random.state() == random_before_notice);
            OL_CHECK(session.render(framebuffer));
            OL_CHECK(fnv1a64(framebuffer.pixels()) == test.frame_hash);
            OL_CHECK(random.state() == random_before_notice);
            OL_CHECK(session.resume(SceneResponse::acknowledge).kind ==
                     SceneStepKind::present);
            OL_CHECK(session.resume(SceneResponse::acknowledge).kind ==
                     SceneStepKind::stay);
        } else {
            OL_CHECK(result.kind == SceneStepKind::stay);
            OL_CHECK(session.pending_text().empty());
        }
        OL_CHECK(role.word(openlegend::model::role_word::iq) == test.after);
    }
}

void check_event_magic_slot_write(const std::filesystem::path& root) {
    using openlegend::scene::SceneStepKind;

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot data_root{synthetic.path()};
    const auto fill_slots = [](openlegend::model::RoleRecord& role) {
        for (std::size_t slot = 0U; slot < openlegend::model::role_word::magic_count; ++slot) {
            role.set_word(
                openlegend::model::role_word::magic_id_begin + slot,
                static_cast<std::int16_t>(40 + slot));
            role.set_word(openlegend::model::role_word::magic_level_begin + slot, 100);
        }
    };
    const auto run = [&data_root](
                         openlegend::model::GameSnapshot& snapshot,
                         const std::int16_t script) {
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{data_root, snapshot, random, 70};
        return session.begin_event(script, 0, 0, 0).kind;
    };

    auto explicit_snapshot = load_baseline(root);
    auto& explicit_role = explicit_snapshot.ranger.roles[0];
    fill_slots(explicit_role);
    OL_CHECK(run(explicit_snapshot, 44) == SceneStepKind::stay);
    OL_CHECK(explicit_role.word(openlegend::model::role_word::magic_id_begin + 9U) == -32768);
    OL_CHECK(explicit_role.word(openlegend::model::role_word::magic_level_begin + 9U) == 32767);
    OL_CHECK(explicit_role.word(openlegend::model::role_word::magic_id_begin) == 40);
    OL_CHECK(explicit_role.word(openlegend::model::role_word::magic_level_begin) == 100);

    auto first_empty_snapshot = load_baseline(root);
    auto& first_empty_role = first_empty_snapshot.ranger.roles[0];
    fill_slots(first_empty_role);
    first_empty_role.set_word(openlegend::model::role_word::magic_id_begin + 2U, 0);
    OL_CHECK(run(first_empty_snapshot, 45) == SceneStepKind::stay);
    OL_CHECK(first_empty_role.word(openlegend::model::role_word::magic_id_begin + 2U) == 60);
    OL_CHECK(first_empty_role.word(openlegend::model::role_word::magic_level_begin + 2U) == 100);
    OL_CHECK(first_empty_role.word(openlegend::model::role_word::magic_id_begin) == 40);

    auto full_snapshot = load_baseline(root);
    auto& full_role = full_snapshot.ranger.roles[0];
    fill_slots(full_role);
    OL_CHECK(run(full_snapshot, 46) == SceneStepKind::stay);
    OL_CHECK(full_role.word(openlegend::model::role_word::magic_id_begin) == 60);
    OL_CHECK(full_role.word(openlegend::model::role_word::magic_level_begin) == 100);
    OL_CHECK(full_role.word(openlegend::model::role_word::magic_id_begin + 1U) == 41);

    auto zero_snapshot = load_baseline(root);
    auto& zero_role = zero_snapshot.ranger.roles[0];
    fill_slots(zero_role);
    zero_role.set_word(openlegend::model::role_word::magic_id_begin + 4U, 0);
    OL_CHECK(run(zero_snapshot, 47) == SceneStepKind::stay);
    OL_CHECK(zero_role.word(openlegend::model::role_word::magic_id_begin + 4U) == 0);
    OL_CHECK(zero_role.word(openlegend::model::role_word::magic_level_begin + 4U) == -32768);
    OL_CHECK(zero_role.word(openlegend::model::role_word::magic_id_begin) == 40);

    for (const auto script : std::array<std::int16_t, 2>{48, 49}) {
        auto guarded_snapshot = load_baseline(root);
        auto& guarded_role = guarded_snapshot.ranger.roles[0];
        fill_slots(guarded_role);
        const auto before = guarded_role.bytes;
        OL_CHECK(run(guarded_snapshot, script) == SceneStepKind::stay);
        OL_CHECK(guarded_role.bytes == before);
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
    OL_CHECK(snapshot.ranger.scenes.size() == openlegend::model::kSceneMetadataCount);
    OL_CHECK(snapshot.ranger.scenes.size() == 84U);
    for (std::size_t scene = 0; scene < snapshot.ranger.scenes.size(); ++scene) {
        std::int16_t expected = 0;
        if (scene == 2U || scene == 38U) {
            expected = 2;
        } else if (scene == 75U || scene == 80U) {
            expected = 1;
        }
        OL_CHECK(snapshot.ranger.scenes[scene].word(
                     openlegend::model::scene_metadata_word::entrance_condition) == expected);
    }
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
    snapshot.ranger.header.set_team_member(3U, openlegend::model::CharacterId{2});
    snapshot.ranger.header.set_team_member(5U, openlegend::model::CharacterId{0});
    snapshot.ranger.roles[0].set_word(openlegend::model::role_word::mp, 77);
    snapshot.ranger.roles[1].set_word(openlegend::model::role_word::mp, 88);
    snapshot.ranger.roles[2].set_word(openlegend::model::role_word::mp, 66);
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
    OL_CHECK(snapshot.ranger.roles[2].word(openlegend::model::role_word::mp) == 0);
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
    OL_CHECK(result.style == -1);
    constexpr std::array<std::uint8_t, 11> expected_item_109_notice{
        0xB1U, 0x6FU, 0xA8U, 0xECU, 0xADU, 0xCAU, 0xA4U, 0xD1U, 0xBCU, 0x43U, 0x00U};
    OL_CHECK(session.pending_text().size() == expected_item_109_notice.size());
    OL_CHECK(std::equal(
        session.pending_text().begin(), session.pending_text().end(),
        expected_item_109_notice.begin()));
    OL_CHECK(snapshot.ranger.header.team_member(1U).value == 1);
    OL_CHECK(snapshot.ranger.header.inventory_count(0U) == 2);
    OL_CHECK(snapshot.ranger.header.inventory_count(1U) == 3);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == 109);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin) == 0);
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin) == 10);
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin + 1U) == 11);
    OL_CHECK(role.word(openlegend::model::role_word::practice_item) == 12);
    OL_CHECK(role.word(openlegend::model::role_word::item_experience) == 77);

    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::present);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == 109);
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin) == 10);
    result = session.resume(SceneResponse::acknowledge);
    OL_CHECK(result.kind == SceneStepKind::notice);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin) == 0);
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin) == 10);
    for (int step = 0; step < 32 &&
                       (result.kind == SceneStepKind::notice ||
                        result.kind == SceneStepKind::present ||
                        result.kind == SceneStepKind::dialogue);
         ++step) {
        result = session.resume(SceneResponse::acknowledge);
    }
    for (std::size_t slot = 0U; slot < openlegend::model::role_word::taking_item_count; ++slot) {
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin + slot) == -1);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin + slot) == 0);
    }
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::equipment_begin + 1U) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::practice_item) == -1);
    OL_CHECK(role.word(openlegend::model::role_word::item_experience) == 0);
    OL_CHECK(snapshot.ranger.items[10].word(openlegend::model::item_word::user) == 1);
    OL_CHECK(snapshot.ranger.items[11].word(openlegend::model::item_word::user) == 1);
    OL_CHECK(snapshot.ranger.items[12].word(openlegend::model::item_word::user) == 1);
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

    auto low_fame_snapshot = load_baseline(root);
    for (std::size_t slot = 0U; slot < openlegend::model::kInventoryCount; ++slot) {
        low_fame_snapshot.ranger.header.set_inventory(
            slot, openlegend::model::ItemId{-1}, 0);
    }
    for (std::int16_t item_id = 144; item_id <= 157; ++item_id) {
        low_fame_snapshot.ranger.header.set_inventory(
            static_cast<std::size_t>(item_id - 144), openlegend::model::ItemId{item_id}, 0);
    }
    low_fame_snapshot.ranger.roles[0].set_word(openlegend::model::role_word::fame, 199);
    static_cast<void>(low_fame_snapshot.set_event_value(
        70U, 11U, openlegend::model::SceneEventField::event_1, 111));
    openlegend::random::LegacyRandom low_fame_random{1U};
    openlegend::scene::SceneSession low_fame_session{
        data_root, low_fame_snapshot, low_fame_random, 70};
    OL_CHECK(low_fame_session.begin_event(36, 0, 44, 29).kind ==
             openlegend::scene::SceneStepKind::notice);
    OL_CHECK(low_fame_snapshot.event_value(
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
    auto& heavily_hurt_role = rest_snapshot.ranger.roles[3];
    heavily_hurt_role.set_word(openlegend::model::role_word::hp, 10);
    heavily_hurt_role.set_word(openlegend::model::role_word::maximum_hp, 99);
    heavily_hurt_role.set_word(openlegend::model::role_word::mp, 11);
    heavily_hurt_role.set_word(openlegend::model::role_word::maximum_mp, 89);
    heavily_hurt_role.set_word(openlegend::model::role_word::hurt, 33);
    heavily_hurt_role.set_word(openlegend::model::role_word::poison, 0);
    heavily_hurt_role.set_word(openlegend::model::role_word::physical_power, 12);
    rest_snapshot.ranger.header.set_team_member(0U, openlegend::model::CharacterId{0});
    rest_snapshot.ranger.header.set_team_member(1U, openlegend::model::CharacterId{1});
    rest_snapshot.ranger.header.set_team_member(2U, openlegend::model::CharacterId{3});
    rest_snapshot.ranger.header.set_team_member(3U, openlegend::model::CharacterId{-1});
    rest_snapshot.ranger.header.set_team_member(4U, openlegend::model::CharacterId{2});
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
    OL_CHECK(heavily_hurt_role.word(openlegend::model::role_word::hp) == 10);
    OL_CHECK(heavily_hurt_role.word(openlegend::model::role_word::mp) == 11);
    OL_CHECK(heavily_hurt_role.word(openlegend::model::role_word::hurt) == 33);
    OL_CHECK(heavily_hurt_role.word(openlegend::model::role_word::physical_power) == 12);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::hp) == 7);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::mp) == 8);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::hurt) == 10);
    OL_CHECK(after_gap_role.word(openlegend::model::role_word::physical_power) == 9);

    constexpr std::array<std::uint8_t, 21> expected_learn_notice{
        0xB5U, 0xEAU, 0xA6U, 0xCBU, 0x20U, 0xBEU, 0xC7U,
        0xB7U, 0x7CU, 0x20U, 0xA4U, 0xD1U, 0xA4U, 0x73U,
        0xA4U, 0xBBU, 0xB6U, 0xA7U, 0xB4U, 0x78U, 0x00U};
    constexpr std::array<std::uint8_t, 17> expected_speed_notice{
        0xB5U, 0xEAU, 0xA6U, 0xCBU, 0x20U, 0xBBU, 0xB4U, 0xA5U, 0x5CU,
        0xBCU, 0x57U, 0xA5U, 0x5BU, 0x20U, 0x31U, 0x30U, 0x00U};
    constexpr std::array<std::uint8_t, 19> expected_mp_notice{
        0xB5U, 0xEAU, 0xA6U, 0xCBU, 0x20U, 0xA4U, 0xBAU, 0xA4U, 0x4FU,
        0xBCU, 0x57U, 0xA5U, 0x5BU, 0x20U, 0x31U, 0x31U, 0x30U, 0x30U, 0x00U};
    constexpr std::array<std::uint8_t, 17> expected_attack_notice{
        0xB5U, 0xEAU, 0xA6U, 0xCBU, 0x20U, 0xAAU, 0x5AU, 0xA4U, 0x4FU,
        0xBCU, 0x57U, 0xA5U, 0x5BU, 0x20U, 0x31U, 0x30U, 0x00U};
    constexpr std::array<std::uint8_t, 19> unexpected_hp_notice{
        0xB5U, 0xEAU, 0xA6U, 0xCBU, 0x20U, 0xA5U, 0xCDU, 0xA9U, 0x52U,
        0xBCU, 0x57U, 0xA5U, 0x5BU, 0x20U, 0x31U, 0x30U, 0x30U, 0x30U, 0x00U};
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
    bool expect_notice_restore = false;
    bool saw_learn_notice = false;
    bool saw_speed_notice = false;
    bool saw_mp_notice = false;
    bool saw_attack_notice = false;
    bool saw_hp_notice = false;
    for (int step = 0; step < 128 && join_result.kind != openlegend::scene::SceneStepKind::stay;
         ++step) {
        if (expect_notice_restore) {
            OL_CHECK(join_result.kind == openlegend::scene::SceneStepKind::present);
        }
        if (join_result.kind == openlegend::scene::SceneStepKind::notice &&
            join_session.pending_text().size() == expected_learn_notice.size() &&
            std::equal(
                join_session.pending_text().begin(), join_session.pending_text().end(),
                expected_learn_notice.begin())) {
            saw_learn_notice = true;
        }
        if (join_result.kind == openlegend::scene::SceneStepKind::notice &&
            join_session.pending_text().size() == expected_speed_notice.size() &&
            std::equal(
                join_session.pending_text().begin(), join_session.pending_text().end(),
                expected_speed_notice.begin())) {
            OL_CHECK(join_result.style == -3);
            saw_speed_notice = true;
        }
        if (join_result.kind == openlegend::scene::SceneStepKind::notice &&
            join_session.pending_text().size() == expected_mp_notice.size() &&
            std::equal(
                join_session.pending_text().begin(), join_session.pending_text().end(),
                expected_mp_notice.begin())) {
            OL_CHECK(join_result.style == -3);
            saw_mp_notice = true;
        }
        if (join_result.kind == openlegend::scene::SceneStepKind::notice &&
            join_session.pending_text().size() == expected_attack_notice.size() &&
            std::equal(
                join_session.pending_text().begin(), join_session.pending_text().end(),
                expected_attack_notice.begin())) {
            OL_CHECK(join_result.style == -3);
            saw_attack_notice = true;
        }
        if (join_result.kind == openlegend::scene::SceneStepKind::notice &&
            join_session.pending_text().size() == unexpected_hp_notice.size() &&
            std::equal(
                join_session.pending_text().begin(), join_session.pending_text().end(),
                unexpected_hp_notice.begin())) {
            saw_hp_notice = true;
        }
        expect_notice_restore = join_result.kind == openlegend::scene::SceneStepKind::notice;
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
    OL_CHECK(!expect_notice_restore);
    OL_CHECK(saw_learn_notice);
    OL_CHECK(saw_speed_notice);
    OL_CHECK(saw_mp_notice);
    OL_CHECK(saw_attack_notice);
    OL_CHECK(!saw_hp_notice);
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
                     openlegend::model::item_word::user) == 49);
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
    for (std::int16_t item_id = 4; item_id <= 6; ++item_id) {
        OL_CHECK(leave_snapshot.ranger.items[static_cast<std::size_t>(item_id)].word(
                     openlegend::model::item_word::user) == -1);
    }
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

    const SyntheticKdefDataRoot synthetic{root};
    const openlegend::resource::DataRoot synthetic_root{synthetic.path()};
    for (const auto [sexual, talk_id] :
         std::array<std::pair<std::int16_t, std::int16_t>, 2>{
             std::pair<std::int16_t, std::int16_t>{-32768, 1123}, {32767, 1122}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::sexual, sexual);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        const auto result = session.begin_event(50, 0, 0, 0);
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
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::present);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::morality) == after);
    }

    for (const auto [script, before, after] :
         std::array<std::array<std::int16_t, 3>, 6>{
             std::array<std::int16_t, 3>{51, 99, 100},
             {52, 32767, 0},
             {53, -32768, 100},
             {54, 100, 0},
             {55, 0, 0},
             {56, -1, 0}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::morality, before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(script, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::morality) == after);
    }

    {
        auto snapshot = load_baseline(root);
        constexpr auto layer = openlegend::model::SceneLayer::decoration_height;
        constexpr std::array<std::size_t, 4> changed_indices{0U, 63U, 64U, 4095U};
        for (const auto index : changed_indices) {
            OL_CHECK(snapshot.set_scene_value(7U, layer, index, 123));
        }
        OL_CHECK(snapshot.set_scene_value(7U, layer, 65U, 124));
        OL_CHECK(snapshot.set_scene_value(70U, layer, 0U, 123));
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(57, 0, 0, 0).kind == SceneStepKind::stay);
        for (const auto index : changed_indices) {
            OL_CHECK(snapshot.scene_value(7U, layer, index).value_or(0) == -32768);
        }
        OL_CHECK(snapshot.scene_value(7U, layer, 65U).value_or(0) == 124);
        OL_CHECK(snapshot.scene_value(70U, layer, 0U).value_or(0) == 123);
    }

    for (const auto [script, scene, before] :
         std::array<std::array<std::int16_t, 3>, 2>{
             std::array<std::int16_t, 3>{58, 0, -32768}, {59, 83, 32767}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.scenes[static_cast<std::size_t>(scene)].set_word(
            openlegend::model::scene_metadata_word::entrance_condition, before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(script, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.scenes[static_cast<std::size_t>(scene)].word(
                     openlegend::model::scene_metadata_word::entrance_condition) == 0);
    }

    for (const auto script : std::array<std::int16_t, 2>{60, 61}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.scenes[0].set_word(
            openlegend::model::scene_metadata_word::entrance_condition, 111);
        snapshot.ranger.scenes[83].set_word(
            openlegend::model::scene_metadata_word::entrance_condition, 222);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(script, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.scenes[0].word(
                     openlegend::model::scene_metadata_word::entrance_condition) == 111);
        OL_CHECK(snapshot.ranger.scenes[83].word(
                     openlegend::model::scene_metadata_word::entrance_condition) == 222);
    }

    for (const auto [script, direction, frame] :
         std::array<std::array<std::int16_t, 3>, 6>{
             std::array<std::int16_t, 3>{62, 0, 5002},
             {63, 1, 5016},
             {64, 2, 5030},
             {65, 3, 5044},
             {66, 0, 5002},
             {67, 3, 5044}}) {
        auto snapshot = load_baseline(root);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(script, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(static_cast<std::int16_t>(session.direction()) == direction);
        OL_CHECK(session.player_frame() == frame);
        OL_CHECK(snapshot.ranger.header.word(
                     openlegend::model::header_word::face_towards) == direction);
    }

    const auto run_item_script = [&synthetic_root](
                                     openlegend::model::GameSnapshot& snapshot,
                                     const std::int16_t script) {
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        return session.begin_event(script, 0, 0, 0).kind;
    };
    const auto set_role_items = [](openlegend::model::RoleRecord& role,
                                   const std::array<std::int16_t, 4>& ids,
                                   const std::array<std::int16_t, 4>& counts) {
        for (std::size_t index = 0U; index < ids.size(); ++index) {
            role.set_word(openlegend::model::role_word::taking_item_begin + index, ids[index]);
            role.set_word(
                openlegend::model::role_word::taking_item_count_begin + index,
                counts[index]);
        }
    };
    for (const auto [script, before, after] :
         std::array<std::tuple<std::int16_t, std::int16_t, std::int16_t>, 2>{
             std::tuple<std::int16_t, std::int16_t, std::int16_t>{68, 32767, -32768},
             {69, -32768, 32767}}) {
        auto snapshot = load_baseline(root);
        auto& role = snapshot.ranger.roles[0];
        set_role_items(role, {78, 78, -1, -1}, {before, 5, 0, 0});
        OL_CHECK(run_item_script(snapshot, script) == SceneStepKind::stay);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == 78);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin) == after);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin + 1U) == 5);
    }
    for (const auto [script, count] :
         std::array<std::pair<std::int16_t, std::int16_t>, 2>{
             std::pair<std::int16_t, std::int16_t>{70, 0},
             {71, -7}}) {
        auto snapshot = load_baseline(root);
        auto& role = snapshot.ranger.roles[0];
        set_role_items(role, {-2, -1, -1, -1}, {11, 22, 33, 44});
        OL_CHECK(run_item_script(snapshot, script) == SceneStepKind::stay);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == -2);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin) == 11);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin + 1U) == 99);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin + 1U) == count);
    }
    {
        auto snapshot = load_baseline(root);
        auto& role = snapshot.ranger.roles[0];
        set_role_items(role, {1, 2, 3, 4}, {11, 22, 33, 44});
        OL_CHECK(run_item_script(snapshot, 73) == SceneStepKind::stay);
        for (std::size_t index = 0U; index < 4U; ++index) {
            OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin + index) ==
                     static_cast<std::int16_t>(index + 1U));
            OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin + index) ==
                     static_cast<std::int16_t>((index + 1U) * 11U));
        }
    }
    {
        auto snapshot = load_baseline(root);
        auto& role = snapshot.ranger.roles[0];
        set_role_items(role, {-1, -1, -1, -1}, {7, 0, 0, 0});
        OL_CHECK(run_item_script(snapshot, 74) == SceneStepKind::stay);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_begin) == -1);
        OL_CHECK(role.word(openlegend::model::role_word::taking_item_count_begin) == 12);
    }
    {
        auto snapshot = load_baseline(root);
        const auto before = snapshot.ranger.roles[0].bytes;
        OL_CHECK(run_item_script(snapshot, 72) == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].bytes == before);
    }

    const auto run_female_condition = [&synthetic_root](
                                          openlegend::model::GameSnapshot& snapshot) {
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        return session.begin_event(75, 0, 0, 0);
    };
    for (const auto [slot, sexual, expected_talk] :
         std::array<std::tuple<std::int16_t, std::int16_t, std::int16_t>, 4>{
             std::tuple<std::int16_t, std::int16_t, std::int16_t>{-1, 0, 1575},
             {5, 1, 1574},
             {5, 2, 1575},
             {0, 1, 1574}}) {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kTeamMemberCount; ++index) {
            snapshot.ranger.header.set_team_member(index, openlegend::model::CharacterId{-1});
        }
        if (slot >= 0) {
            snapshot.ranger.header.set_team_member(
                static_cast<std::size_t>(slot), openlegend::model::CharacterId{2});
            snapshot.ranger.roles[2].set_word(openlegend::model::role_word::sexual, sexual);
        }
        const auto result = run_female_condition(snapshot);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == expected_talk);
    }
    {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kTeamMemberCount; ++index) {
            snapshot.ranger.header.set_team_member(index, openlegend::model::CharacterId{-1});
        }
        snapshot.ranger.header.set_team_member(0U, openlegend::model::CharacterId{0});
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::sexual, 1);
        snapshot.ranger.header.set_team_member(5U, openlegend::model::CharacterId{32767});
        const auto result = run_female_condition(snapshot);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == 1574);
    }

    const auto run_inventory_condition = [&synthetic_root](
                                             openlegend::model::GameSnapshot& snapshot,
                                             const std::int16_t script) {
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        return session.begin_event(script, 0, 0, 0);
    };
    {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kInventoryCount; ++index) {
            snapshot.ranger.header.set_inventory(
                index, openlegend::model::ItemId{1}, static_cast<std::int16_t>(index));
        }
        snapshot.ranger.header.set_inventory(
            openlegend::model::kInventoryCount - 1U,
            openlegend::model::ItemId{110}, -32768);
        const auto result = run_inventory_condition(snapshot, 76);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == 1574);
    }
    {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kInventoryCount; ++index) {
            snapshot.ranger.header.set_inventory(index, openlegend::model::ItemId{1}, 0);
        }
        const auto result = run_inventory_condition(snapshot, 76);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == 1575);
    }
    {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kInventoryCount; ++index) {
            snapshot.ranger.header.set_inventory(index, openlegend::model::ItemId{1}, 0);
        }
        snapshot.ranger.header.set_inventory(100U, openlegend::model::ItemId{-1}, 32767);
        const auto result = run_inventory_condition(snapshot, 77);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        OL_CHECK(result.talk_id == 1574);
    }

    {
        auto snapshot = load_baseline(root);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(78, 0, 0, 0).kind == SceneStepKind::stay);
    }
    {
        auto snapshot = load_baseline(root);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        auto result = session.begin_event(79, 0, 0, 0);
        OL_CHECK(result.kind == SceneStepKind::present && result.wait_ticks == 2U);
        OL_CHECK(session.player_frame() == 100);
        result = session.resume(SceneResponse::acknowledge);
        OL_CHECK(result.kind == SceneStepKind::present && result.wait_ticks == 2U);
        OL_CHECK(session.player_frame() == 102);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    }
    {
        auto snapshot = load_baseline(root);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        const auto result = session.begin_event(80, 0, 0, 0);
        OL_CHECK(result.kind == SceneStepKind::present && result.wait_ticks == 2U);
        OL_CHECK(session.player_frame() == -32768);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    }
    {
        auto snapshot = load_baseline(root);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        auto result = session.begin_event(81, 0, 0, 0);
        OL_CHECK(result.kind == SceneStepKind::present && session.player_frame() == 200);
        result = session.resume(SceneResponse::acknowledge);
        OL_CHECK(result.kind == SceneStepKind::present && session.player_frame() == 202);
        OL_CHECK(session.resume(SceneResponse::acknowledge).kind == SceneStepKind::stay);
    }

    for (const auto [script, before, after, notice, frame_hash] :
         std::array<
             std::tuple<std::int16_t, std::int16_t, std::int16_t, bool, std::uint64_t>,
             3>{
             std::tuple<
                 std::int16_t, std::int16_t, std::int16_t, bool, std::uint64_t>{
                 82, 32767, 0, false, 0},
             {83, -32768, 100, true, 0x4B693C807A3F6EE2ULL},
             {84, -1, 0, true, 0x112AB8F9E491EC4DULL}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::speed, before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render(framebuffer));
        const auto result = session.begin_event(script, 0, 0, 0);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::speed) == after);
        OL_CHECK(result.kind ==
                 (notice ? SceneStepKind::notice : SceneStepKind::stay));
        if (notice) {
            OL_CHECK(result.style == -3);
            OL_CHECK(session.render(framebuffer));
            OL_CHECK(fnv1a64(framebuffer.pixels()) == frame_hash);
        }
    }
    {
        auto snapshot = load_baseline(root);
        const auto before = snapshot.ranger.roles[0].bytes;
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(85, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].bytes == before);
    }

    for (const auto [script, before_maximum, before_current, after, notice, frame_hash] :
         std::array<std::tuple<
             std::int16_t, std::int16_t, std::int16_t, std::int16_t, bool, std::uint64_t>,
             4>{
             std::tuple<
                 std::int16_t, std::int16_t, std::int16_t, std::int16_t, bool,
                 std::uint64_t>{86, 32767, -32768, -32768, false, 0},
             {87, -32768, -32768, 32767, true, 0x4624B5C6C4CA6D16ULL},
             {88, 100, 0, 100, true, 0xBBFF4E602CA8260CULL},
             {89, 100, 100, 90, false, 0}}) {
        auto snapshot = load_baseline(root);
        auto& role = snapshot.ranger.roles[0];
        role.set_word(openlegend::model::role_word::maximum_mp, before_maximum);
        role.set_word(openlegend::model::role_word::mp, before_current);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render(framebuffer));
        const auto result = session.begin_event(script, 0, 0, 0);
        OL_CHECK(role.word(openlegend::model::role_word::maximum_mp) == after);
        OL_CHECK(role.word(openlegend::model::role_word::mp) == after);
        OL_CHECK(result.kind ==
                 (notice ? SceneStepKind::notice : SceneStepKind::stay));
        if (notice) {
            OL_CHECK(result.style == -3);
            OL_CHECK(session.render(framebuffer));
            OL_CHECK(fnv1a64(framebuffer.pixels()) == frame_hash);
        }
    }
    {
        auto snapshot = load_baseline(root);
        const auto before = snapshot.ranger.roles[0].bytes;
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(90, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].bytes == before);
    }

    for (const auto [script, before, after, notice, frame_hash] :
         std::array<
             std::tuple<std::int16_t, std::int16_t, std::int16_t, bool, std::uint64_t>,
             3>{
             std::tuple<
                 std::int16_t, std::int16_t, std::int16_t, bool, std::uint64_t>{
                 91, 32767, 0, false, 0},
             {92, -32768, 100, true, 0x02F03B6E246A4973ULL},
             {93, -1, 0, true, 0x9E3C9A177D228048ULL}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::attack, before);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render(framebuffer));
        const auto result = session.begin_event(script, 0, 0, 0);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::attack) == after);
        OL_CHECK(result.kind ==
                 (notice ? SceneStepKind::notice : SceneStepKind::stay));
        if (notice) {
            OL_CHECK(result.style == -3);
            OL_CHECK(session.render(framebuffer));
            OL_CHECK(fnv1a64(framebuffer.pixels()) == frame_hash);
        }
    }
    {
        auto snapshot = load_baseline(root);
        const auto before = snapshot.ranger.roles[0].bytes;
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(94, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].bytes == before);
    }

    for (const auto [script, before_maximum, before_current, after, in_party, notice,
                     frame_hash] :
         std::array<std::tuple<
             std::int16_t, std::int16_t, std::int16_t, std::int16_t, bool, bool,
             std::uint64_t>,
             5>{
             std::tuple<
                 std::int16_t, std::int16_t, std::int16_t, std::int16_t, bool, bool,
                 std::uint64_t>{95, 32767, -32768, -32768, true, false, 0},
             {96, -32768, -32768, 32767, true, true, 0xA5AD1328BFFE69DFULL},
             {97, 100, 0, 100, true, true, 0x831AD6F0707BCDA1ULL},
             {98, 100, 100, 90, true, false, 0},
             {99, 100, 0, 101, false, false, 0}}) {
        auto snapshot = load_baseline(root);
        if (!in_party) {
            for (std::size_t slot = 0U; slot < openlegend::model::kTeamMemberCount; ++slot) {
                snapshot.ranger.header.set_team_member(
                    slot, openlegend::model::CharacterId{-1});
            }
        }
        auto& role = snapshot.ranger.roles[0];
        role.set_word(openlegend::model::role_word::maximum_hp, before_maximum);
        role.set_word(openlegend::model::role_word::hp, before_current);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(finish_scene_title(session).kind == SceneStepKind::stay);
        openlegend::render::IndexedFramebuffer framebuffer;
        OL_CHECK(session.render(framebuffer));
        const auto result = session.begin_event(script, 0, 0, 0);
        OL_CHECK(role.word(openlegend::model::role_word::maximum_hp) == after);
        OL_CHECK(role.word(openlegend::model::role_word::hp) == after);
        OL_CHECK(result.kind ==
                 (notice ? SceneStepKind::notice : SceneStepKind::stay));
        if (notice) {
            OL_CHECK(result.style == -3);
            OL_CHECK(session.render(framebuffer));
            OL_CHECK(fnv1a64(framebuffer.pixels()) == frame_hash);
        }
    }
    {
        auto snapshot = load_baseline(root);
        const auto before = snapshot.ranger.roles[0].bytes;
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(100, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].bytes == before);
    }

    for (const auto [script, value] :
         std::array<std::pair<std::int16_t, std::int16_t>, 3>{
             std::pair<std::int16_t, std::int16_t>{101, 32767}, {102, -32768}, {103, 0}}) {
        auto snapshot = load_baseline(root);
        snapshot.ranger.roles[0].set_word(openlegend::model::role_word::mp_type, 123);
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(script, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].word(openlegend::model::role_word::mp_type) == value);
    }
    {
        auto snapshot = load_baseline(root);
        const auto before = snapshot.ranger.roles[0].bytes;
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        OL_CHECK(session.begin_event(104, 0, 0, 0).kind == SceneStepKind::stay);
        OL_CHECK(snapshot.ranger.roles[0].bytes == before);
    }

    const auto run_five_item_condition = [&synthetic_root](
                                             openlegend::model::GameSnapshot& snapshot,
                                             const std::int16_t script) {
        openlegend::random::LegacyRandom random{1U};
        openlegend::scene::SceneSession session{synthetic_root, snapshot, random, 70};
        const auto result = session.begin_event(script, 0, 0, 0);
        OL_CHECK(result.kind == SceneStepKind::dialogue);
        return result.talk_id;
    };
    {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < 5U; ++index) {
            snapshot.ranger.header.set_inventory(
                index, openlegend::model::ItemId{static_cast<std::int16_t>(110 + index)}, 0);
        }
        OL_CHECK(run_five_item_condition(snapshot, 105) == 1574);
        snapshot.ranger.header.set_inventory(4U, openlegend::model::ItemId{1}, 32767);
        OL_CHECK(run_five_item_condition(snapshot, 105) == 1575);
    }
    {
        auto snapshot = load_baseline(root);
        for (std::size_t index = 0U; index < openlegend::model::kInventoryCount; ++index) {
            snapshot.ranger.header.set_inventory(index, openlegend::model::ItemId{1}, 0);
        }
        snapshot.ranger.header.set_inventory(
            openlegend::model::kInventoryCount - 1U,
            openlegend::model::ItemId{110}, -32768);
        OL_CHECK(run_five_item_condition(snapshot, 106) == 1574);
        snapshot.ranger.header.set_inventory(
            openlegend::model::kInventoryCount - 1U,
            openlegend::model::ItemId{-1}, 0);
        OL_CHECK(run_five_item_condition(snapshot, 107) == 1574);
    }

    for (const auto event_1 : std::array<std::int16_t, 2>{-1, 999}) {
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
        auto result = session.begin_event(464, 0, 44, 29);
        for (int present = 0; present < 4; ++present) {
            OL_CHECK(result.kind == SceneStepKind::present);
            result = session.resume(SceneResponse::acknowledge);
        }
        OL_CHECK(result.kind == SceneStepKind::stay);
        for (std::size_t event = 2U; event <= 5U; ++event) {
            OL_CHECK(snapshot.event_value(
                         70U, event, openlegend::model::SceneEventField::event_1).value_or(-2) ==
                     static_cast<std::int16_t>(860 + event));
        }
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
        snapshot.ranger.roles[4].set_word(openlegend::model::role_word::medicine, 77);
        snapshot.ranger.roles[4].set_word(openlegend::model::role_word::use_poison, -32768);
        snapshot.ranger.roles[4].set_word(openlegend::model::role_word::detoxification, 88);
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
        OL_CHECK(snapshot.ranger.roles[4].word(openlegend::model::role_word::medicine) == 77);
        OL_CHECK(snapshot.ranger.roles[4].word(openlegend::model::role_word::use_poison) == 99);
        OL_CHECK(snapshot.ranger.roles[4].word(
                     openlegend::model::role_word::detoxification) == 88);
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
        OL_CHECK(result.kind == SceneStepKind::present);
        result = session.resume(SceneResponse::acknowledge);
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
    constexpr std::array<std::uint8_t, 25> morality_minimum_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xAAU, 0xBAU,
        0xABU, 0x7EU, 0xBCU, 0x77U, 0xABU, 0xFCU, 0xBCU, 0xC6U,
        0xACU, 0xB0U, 0x2DU, 0x33U, 0x32U, 0x37U, 0x36U, 0x38U, 0x00U};
    constexpr std::array<std::uint8_t, 24> morality_zero_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xAAU, 0xBAU,
        0xABU, 0x7EU, 0xBCU, 0x77U, 0xABU, 0xFCU, 0xBCU, 0xC6U,
        0xACU, 0xB0U, 0x20U, 0x20U, 0x20U, 0x20U, 0x30U, 0x00U};
    constexpr std::array<std::uint8_t, 24> morality_maximum_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xAAU, 0xBAU,
        0xABU, 0x7EU, 0xBCU, 0x77U, 0xABU, 0xFCU, 0xBCU, 0xC6U,
        0xACU, 0xB0U, 0x33U, 0x32U, 0x37U, 0x36U, 0x37U, 0x00U};
    constexpr std::array<std::uint8_t, 25> fame_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xADU, 0xD3U,
        0xA4U, 0x48U, 0xC1U, 0x6EU, 0xB1U, 0xE6U, 0xABU, 0xFCU,
        0xBCU, 0xC6U, 0xACU, 0xB0U, 0x20U, 0x31U, 0x32U, 0x33U, 0x00U};
    constexpr std::array<std::uint8_t, 27> fame_minimum_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xADU, 0xD3U,
        0xA4U, 0x48U, 0xC1U, 0x6EU, 0xB1U, 0xE6U, 0xABU, 0xFCU,
        0xBCU, 0xC6U, 0xACU, 0xB0U, 0x2DU, 0x33U, 0x32U, 0x37U,
        0x36U, 0x38U, 0x00U};
    constexpr std::array<std::uint8_t, 25> fame_zero_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xADU, 0xD3U,
        0xA4U, 0x48U, 0xC1U, 0x6EU, 0xB1U, 0xE6U, 0xABU, 0xFCU,
        0xBCU, 0xC6U, 0xACU, 0xB0U, 0x20U, 0x20U, 0x20U, 0x30U, 0x00U};
    constexpr std::array<std::uint8_t, 26> fame_maximum_text{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xADU, 0xD3U,
        0xA4U, 0x48U, 0xC1U, 0x6EU, 0xB1U, 0xE6U, 0xABU, 0xFCU,
        0xBCU, 0xC6U, 0xACU, 0xB0U, 0x33U, 0x32U, 0x37U, 0x36U,
        0x37U, 0x00U};
    const openlegend::resource::DataRoot data_root{root};
    const auto check_notice = [&data_root, &root](
                                  const std::int16_t script_id,
                                  const std::size_t field,
                                  const std::int16_t value,
                                  const std::int16_t style,
                                  const std::span<const std::uint8_t> expected_text,
                                  const std::uint64_t expected_hash,
                                  const bool restores_scene) {
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
        auto tail = session.resume(SceneResponse::acknowledge);
        if (restores_scene) {
            OL_CHECK(tail.kind == SceneStepKind::present);
            tail = session.resume(SceneResponse::acknowledge);
        }
        OL_CHECK(tail.kind == SceneStepKind::stay);
    };
    check_notice(
        825, openlegend::model::role_word::morality, 7, 52, morality_text,
        0x1CC47112086C10E7ULL, true);
    check_notice(
        825, openlegend::model::role_word::morality, -32768, 52, morality_minimum_text,
        0xD6E8A2C917F2ADA7ULL, true);
    check_notice(
        825, openlegend::model::role_word::morality, 0, 52, morality_zero_text,
        0x1E6D8DA21B3BB12FULL, true);
    check_notice(
        825, openlegend::model::role_word::morality, 32767, 52, morality_maximum_text,
        0x4BF7246D16058DADULL, true);
    check_notice(
        828, openlegend::model::role_word::fame, 123, 53, fame_text,
        0x5678C57A93EC10C4ULL, true);
    check_notice(
        828, openlegend::model::role_word::fame, -32768, 53, fame_minimum_text,
        0x430D455D1EA59A8DULL, true);
    check_notice(
        828, openlegend::model::role_word::fame, 0, 53, fame_zero_text,
        0xE16CFC1B1BF9ED80ULL, true);
    check_notice(
        828, openlegend::model::role_word::fame, 32767, 53, fame_maximum_text,
        0x362EA896D68BA3C6ULL, true);
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
             openlegend::scene::SceneStepKind::present);
    OL_CHECK(session.resume(openlegend::scene::SceneResponse::acknowledge).kind ==
             openlegend::scene::SceneStepKind::stay);

    const openlegend::scene::SceneAssets assets{data_root};
    const auto script = assets.script(274U);
    OL_CHECK(script.size() == 14U);
    OL_CHECK(script[0] == 17 && script[1] == -2 && script[2] == 1);
    OL_CHECK(script[3] == 13 && script[4] == 22 && script[5] == 0);
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
    check_event_dialogue_rendering(root);
    check_new_game_entry(root);
    check_event_load_menu(root);
    check_event_state_write_helpers(root);
    check_scene_render_and_movement(root);
    check_scene_movement_guards(root);
    check_scene_movement_idle_state(root);
    check_scene_entry_state(root);
    check_scene_archive_ownership(root);
    check_scene_interaction_present(root);
    check_scene_item_and_auto_event_present(root);
    check_scene_loop_transitions(root);
    check_scene_exit_music_override(root);
    check_scene_event_animation(root);
    check_scene_weather(root);
    check_event_camera_pan(root);
    check_event_camera_pan_word_wrap(root);
    check_event_picture_animation(root);
    check_event_picture_animation_boundaries(root);
    check_event_scripted_walk(root);
    check_event_scripted_walk_boundaries(root);
    check_event_dual_picture_animation(root);
    check_event_three_statue_animation(root);
    check_event_ending_prelude_animation(root);
    check_event_role_sexual_and_audio(root);
    check_event_shop_helpers(root);
    check_event_presence_and_party_tail_conditions(root);
    check_event_role_stat_conditions(root);
    check_event_attack_condition_boundaries(root);
    check_event_inventory_condition_edge_cases(root);
    check_event_learn_magic(root);
    check_event_all_book_pictures_condition(root);
    check_event_current_picture_condition(root);
    check_event_tournament_trial(root);
    check_event_finale_party_cleanup(root);
    check_event_role_iq_clamp(root);
    check_event_magic_slot_write(root);
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
