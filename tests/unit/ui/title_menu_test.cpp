#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/ui/basic_ui_renderer.hpp"
#include "openlegend/ui/game_menu.hpp"
#include "openlegend/ui/new_game_attributes.hpp"
#include "openlegend/ui/new_game_name_editor.hpp"
#include "openlegend/ui/title_menu.hpp"
#include "test_support.hpp"

namespace openlegend::app {

struct LegacyGameRuntimeTestAccess {
    static battle::BattleSession* battle_session(LegacyGameRuntime& runtime) noexcept {
        return runtime.battle_session_.get();
    }

    static std::array<bool, 3> scene_input_requests(
        const LegacyGameRuntime& runtime) noexcept {
        return {
            runtime.scene_interact_requested_,
            runtime.scene_ui_requested_,
            runtime.scene_idle_skip_requested_};
    }

    static scene::SceneStepKind scene_pending_kind(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.scene_session_ != nullptr
            ? runtime.scene_session_->pending().kind
            : scene::SceneStepKind::stay;
    }

    static std::int16_t scene_player_frame(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.scene_session_ != nullptr
            ? runtime.scene_session_->player_frame()
            : -1;
    }

    static scene::SceneQuestion scene_pending_question(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.scene_session_ != nullptr
            ? runtime.scene_session_->pending().question
            : scene::SceneQuestion::none;
    }

    static bool scene_question_presented(const LegacyGameRuntime& runtime) noexcept {
        return runtime.scene_question_presented_;
    }

    static bool scene_shop_presented(const LegacyGameRuntime& runtime) noexcept {
        return runtime.scene_shop_presented_;
    }

    static bool game_menu_items_presented(const LegacyGameRuntime& runtime) noexcept {
        return runtime.game_menu_items_presented_;
    }

    static bool game_menu_item_dispatch_pending(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.pending_menu_item_dispatch_.has_value();
    }

    static bool game_menu_item_stage_presented(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.game_menu_item_stage_presented_;
    }

    static std::uint16_t game_menu_item_selection(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.game_menu_.item_selection();
    }

    static std::span<const std::int16_t> game_menu_inventory_slots(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.game_menu_.inventory_slots();
    }

    static std::int16_t scene_pending_menu_index(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.scene_session_ != nullptr
            ? runtime.scene_session_->pending().menu_index
            : -1;
    }

    static bool scene_pending_death_confirm(
        const LegacyGameRuntime& runtime) noexcept {
        return runtime.scene_session_ != nullptr &&
               runtime.scene_session_->pending().death_confirm;
    }

    static bool begin_scene_walk(LegacyGameRuntime& runtime) {
        auto* snapshot = runtime.game_state_.snapshot();
        if (snapshot == nullptr) {
            return false;
        }
        // Isolate ordinary walking from the inn's exit and automatic events.
        auto& metadata = snapshot->ranger.scenes[70U];
        for (std::size_t index = 0U; index < model::scene_metadata_word::exit_count; ++index) {
            metadata.set_word(model::scene_metadata_word::exit_x_begin + index, -1);
            metadata.set_word(model::scene_metadata_word::exit_y_begin + index, -1);
        }
        metadata.set_word(model::scene_metadata_word::jump_scene, -1);
        if (!snapshot->set_scene_value(70U, model::SceneLayer::event_index, 29U * 64U + 45U, -1) ||
            !snapshot->set_scene_value(70U, model::SceneLayer::event_index, 28U * 64U + 45U, -1)) {
            return false;
        }
        return runtime.start_scene(
            70, LegacyGameView::scene,
            scene::SceneEntryOverride{44, 29, scene::SceneDirection::right, -1, -1});
    }

    static bool begin_battle_in_scene(
        LegacyGameRuntime& runtime, const std::int16_t battle_id) {
        if (!runtime.start_scene(
                70,
                LegacyGameView::scene,
                scene::SceneEntryOverride{
                    32, 15, scene::SceneDirection::right, -1, -1})) {
            return false;
        }
        runtime.clear_scene_effect();
        return runtime.start_battle(battle_id, false);
    }

    static bool begin_scene_event(
        LegacyGameRuntime& runtime,
        const std::int16_t scene_id,
        const std::int16_t script_id,
        const std::int16_t event_id,
        const std::int16_t event_x,
        const std::int16_t event_y) {
        if (!runtime.start_scene(
                scene_id,
                LegacyGameView::scene,
                scene::SceneEntryOverride{
                    32, 15, scene::SceneDirection::right, -1, -1})) {
            return false;
        }
        runtime.clear_scene_effect();
        runtime.handle_scene_result(
            runtime.scene_session_->begin_event(script_id, event_id, event_x, event_y));
        return runtime.scene_session_ != nullptr && runtime.view_ == LegacyGameView::scene;
    }
};

}  // namespace openlegend::app

namespace {

[[nodiscard]] std::uint64_t fnv1a64(const std::span<const std::uint8_t> bytes) {
    std::uint64_t result = 0xCBF29CE484222325ULL;
    for (const auto byte : bytes) {
        result ^= byte;
        result *= 0x100000001B3ULL;
    }
    return result;
}

[[nodiscard]] bool prepare_runtime_fixture(
    const std::filesystem::path& source, const std::filesystem::path& destination) {
    constexpr std::array<std::string_view, 33> files{
        "TITLE.IDX", "TITLE.GRP", "TITLE.BIG", "DEAD.BIG", "MMAP.COL", "MMAP.IDX",
        "MMAP.GRP", "CLOUD.IDX", "CLOUD.GRP", "3_shadow.msk", "4_shadow.msk", "EARTH.002",
        "SURFACE.002", "BUILDING.002", "BUILDX.002", "BUILDY.002", "FONT3.E16", "FONT3.C16",
        "CFONT", "RANGER.IDX", "RANGER.GRP", "ALLSIN.IDX", "ALLSIN.GRP", "ALLDEF.IDX",
        "ALLDEF.GRP", "TALK.IDX", "TALK.GRP", "KDEF.IDX", "KDEF.GRP", "HDGRP.IDX",
        "HDGRP.GRP", "SDX070", "SMP070"};
    std::error_code error;
    std::filesystem::remove_all(destination, error);
    error.clear();
    std::filesystem::create_directories(destination, error);
    if (error) {
        return false;
    }
    for (const auto file : files) {
        std::filesystem::copy_file(
            source / file,
            destination / file,
            std::filesystem::copy_options::overwrite_existing,
            error);
        if (error) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool prepare_statue_runtime_fixture(
    const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (!prepare_runtime_fixture(source, destination)) {
        return false;
    }
    std::error_code error;
    for (const auto file : {"SDX014", "SMP014"}) {
        std::filesystem::copy_file(
            source / file,
            destination / file,
            std::filesystem::copy_options::overwrite_existing,
            error);
        if (error) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool prepare_ending_prelude_runtime_fixture(
    const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (!prepare_runtime_fixture(source, destination)) {
        return false;
    }
    constexpr std::array<std::string_view, 5> files{
        "SDX083", "SMP083", "ENDCOL.COL", "ENDWORD.IDX", "ENDWORD.GRP"};
    std::error_code error;
    for (const auto file : files) {
        std::filesystem::copy_file(
            source / file,
            destination / file,
            std::filesystem::copy_options::overwrite_existing,
            error);
        if (error) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool prepare_shop_runtime_fixture(
    const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (!prepare_runtime_fixture(source, destination)) {
        return false;
    }
    constexpr std::array<std::string_view, 2> files{"SDX001", "SMP001"};
    std::error_code error;
    for (const auto file : files) {
        std::filesystem::copy_file(
            source / file,
            destination / file,
            std::filesystem::copy_options::overwrite_existing,
            error);
        if (error) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool prepare_battle_runtime_fixture(
    const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (!prepare_runtime_fixture(source, destination)) {
        return false;
    }
    constexpr std::array<std::string_view, 9> files{
        "WAR.STA", "WARFLD.IDX", "WARFLD.GRP", "WDX000", "WMP000",
        "WDX002", "WMP002", "EFT.IDX", "EFT.GRP"};
    std::error_code error;
    for (const auto file : files) {
        std::filesystem::copy_file(
            source / file,
            destination / file,
            std::filesystem::copy_options::overwrite_existing,
            error);
        if (error) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool install_initial_script(
    const std::filesystem::path& root,
    const std::span<const std::int16_t> initial_script) {
    std::vector<std::uint8_t> index;
    std::vector<std::uint8_t> group;
    index.reserve(openlegend::scene::kEventScriptCount * 4U);
    const auto append_i16 = [&group](const std::int16_t value) {
        const auto bits = static_cast<std::uint16_t>(value);
        group.push_back(static_cast<std::uint8_t>(bits & 0xFFU));
        group.push_back(static_cast<std::uint8_t>(bits >> 8U));
    };
    const auto append_u32 = [&index](const std::uint32_t value) {
        for (unsigned shift = 0U; shift < 32U; shift += 8U) {
            index.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    };
    for (std::size_t script = 0U; script < openlegend::scene::kEventScriptCount; ++script) {
        if (script == 691U) {
            for (const auto word : initial_script) {
                append_i16(word);
            }
        } else {
            append_i16(-1);
        }
        append_u32(static_cast<std::uint32_t>(group.size()));
    }
    const auto write = [](const std::filesystem::path& path,
                          const std::span<const std::uint8_t> bytes) {
        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        return output.good();
    };
    return write(root / "KDEF.IDX", index) && write(root / "KDEF.GRP", group);
}

[[nodiscard]] bool install_opcode24_initial_script(const std::filesystem::path& root) {
    constexpr std::array<std::int16_t, 2> script{24, -1};
    return install_initial_script(root, script);
}

[[nodiscard]] bool install_battle_initial_script(const std::filesystem::path& root) {
    constexpr std::array<std::int16_t, 6> script{6, 4, 0, 0, 9, -1};
    return install_initial_script(root, script);
}

[[nodiscard]] bool install_question_initial_script(const std::filesystem::path& root) {
    constexpr std::array<std::int16_t, 10> script{5, 0, 0, 9, 0, 0, 11, 0, 0, -1};
    return install_initial_script(root, script);
}

[[nodiscard]] bool install_picture_animation_initial_script(
    const std::filesystem::path& root) {
    constexpr std::array<std::int16_t, 12> script{
        27, -1, 5002, 5006,
        44, -1, 5002, 5006, -1, 5016, -30000,
        -1,
    };
    return install_initial_script(root, script);
}

[[nodiscard]] bool install_statue_animation_initial_script(
    const std::filesystem::path& root) {
    constexpr std::array<std::int16_t, 2> script{57, -1};
    return install_initial_script(root, script);
}

[[nodiscard]] bool install_ending_prelude_initial_script(
    const std::filesystem::path& root) {
    constexpr std::array<std::int16_t, 8> script{
        62, 0, 8054, 8128, 1, 8130, -30000, -1};
    return install_initial_script(root, script);
}

[[nodiscard]] bool install_death_menu_initial_script(const std::filesystem::path& root) {
    constexpr std::array<std::int16_t, 3> script{15, 83, -1};
    return install_initial_script(root, script);
}

void advance_rendered_frames(
    openlegend::app::LegacyGameRuntime& game, const std::size_t frame_count) {
    for (std::size_t frame = 0U; frame < frame_count; ++frame) {
        OL_CHECK(game.render());
        game.advance();
    }
}

void finish_title_startup(openlegend::app::LegacyGameRuntime& game) {
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::title);
    for (std::size_t frame = 0U; frame < 64U; ++frame) {
        OL_CHECK(game.render());
        OL_CHECK(std::ranges::all_of(
            game.framebuffer().pixels(), [](const auto pixel) { return pixel == 0U; }));
        game.advance();
    }
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::title);
    OL_CHECK(game.render());
    OL_CHECK(fnv1a64(game.framebuffer().pixels()) == 0x86690E3B3B68FE20ULL);
    OL_CHECK(std::all_of(
        game.framebuffer().palette().begin(),
        game.framebuffer().palette().end(),
        [](const auto& color) {
            return color.red == 0U && color.green == 0U && color.blue == 0U;
        }));
    game.advance();
    advance_rendered_frames(game, 65U);
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::title);
}

void finish_title_confirmation(openlegend::app::LegacyGameRuntime& game) {
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::title);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
}

void finish_new_game_scene_transition(openlegend::app::LegacyGameRuntime& game) {
    using openlegend::app::LegacyGameView;
    using openlegend::app::LegacyKeyStateReset;

    OL_CHECK(game.view() == LegacyGameView::attributes);
    OL_CHECK(game.render());
    const auto wait_pixels = fnv1a64(game.framebuffer().pixels());
    OL_CHECK(wait_pixels == 0x7333253CA7400DE6ULL);
    OL_CHECK(
        game.handle_key(0x0DU, false, false) ==
        LegacyKeyStateReset::none);
    OL_CHECK(game.view() == LegacyGameView::attributes);
    game.advance();
    for (std::size_t frame = 0U; frame < 64U; ++frame) {
        OL_CHECK(game.render());
        OL_CHECK(fnv1a64(game.framebuffer().pixels()) == wait_pixels);
        game.advance();
    }
    OL_CHECK(std::all_of(
        game.framebuffer().palette().begin(),
        game.framebuffer().palette().end(),
        [](const auto& color) {
            return color.red == 0U && color.green == 0U && color.blue == 0U;
        }));
    OL_CHECK(game.view() == LegacyGameView::scene);
}

void finish_numbered_load_transition(
    openlegend::app::LegacyGameRuntime& game,
    const openlegend::app::LegacyGameView destination) {
    advance_rendered_frames(game, 64U);
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::world);
    OL_CHECK(game.game_state().loaded());

    std::uint64_t first_black_pixels = 0U;
    for (std::size_t present = 0U; present < 2U; ++present) {
        OL_CHECK(game.view() == openlegend::app::LegacyGameView::world);
        OL_CHECK(game.render());
        OL_CHECK(std::all_of(
            game.framebuffer().palette().begin(),
            game.framebuffer().palette().end(),
            [](const auto& color) {
                return color.red == 0U && color.green == 0U && color.blue == 0U;
            }));
        const auto pixels = fnv1a64(game.framebuffer().pixels());
        if (present == 0U) {
            first_black_pixels = pixels;
        } else {
            OL_CHECK(pixels == first_black_pixels);
        }
        game.advance();
    }

    for (std::size_t frame = 0U; frame < 65U; ++frame) {
        OL_CHECK(game.view() == openlegend::app::LegacyGameView::world);
        OL_CHECK(game.render());
        game.advance();
    }
    OL_CHECK(game.view() == destination);
}

void finish_world_scene_transition(openlegend::app::LegacyGameRuntime& game) {
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
    for (std::size_t frame = 0U; frame < 64U; ++frame) {
        OL_CHECK(game.render());
        game.finish_presented_tick();
        game.advance();
    }
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::scene);
}

void finish_scene_entry(openlegend::app::LegacyGameRuntime& game) {
    std::uint64_t initial_black_pixels{};
    for (std::size_t frame = 0U; frame < 66U; ++frame) {
        OL_CHECK(game.render());
        if (frame < 2U) {
            OL_CHECK(std::all_of(
                game.framebuffer().palette().begin(),
                game.framebuffer().palette().end(),
                [](const auto& color) {
                    return color.red == 0U && color.green == 0U && color.blue == 0U;
                }));
            const auto pixels = fnv1a64(game.framebuffer().pixels());
            if (frame == 0U) {
                initial_black_pixels = pixels;
            } else {
                OL_CHECK(pixels == initial_black_pixels);
            }
        }
        game.advance();
    }
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::scene);
    game.handle_key(0x0DU, false, false);
    advance_rendered_frames(game, 1U);
}

void advance_scene_idle_ticks(
    openlegend::app::LegacyGameRuntime& game, const std::size_t tick_count) {
    for (std::size_t tick = 0U; tick < tick_count; ++tick) {
        OL_CHECK(game.view() == openlegend::app::LegacyGameView::scene);
        advance_rendered_frames(game, 2U);
    }
}

void finish_world_scene_return(openlegend::app::LegacyGameRuntime& game) {
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::world);
    OL_CHECK(game.render());
    OL_CHECK(std::all_of(
        game.framebuffer().palette().begin(),
        game.framebuffer().palette().end(),
        [](const auto& color) {
            return color.red == 0U && color.green == 0U && color.blue == 0U;
        }));
    game.finish_presented_tick();
    game.advance();
    for (std::size_t frame = 0U; frame < 65U; ++frame) {
        OL_CHECK(game.render());
        game.finish_presented_tick();
        game.advance();
    }
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::world);
}

void check_controller() {
    using namespace openlegend::ui;

    TitleMenuController menu;
    OL_CHECK(menu.screen() == TitleScreen::main);
    OL_CHECK(menu.main_selection() == 0U);
    OL_CHECK(menu.handle_key(0x1BU).command == TitleCommand::none);
    OL_CHECK(menu.main_selection() == 0U);

    static_cast<void>(menu.handle_key(0x9EU));
    OL_CHECK(menu.main_selection() == 2U);
    static_cast<void>(menu.handle_key(0x98U));
    OL_CHECK(menu.main_selection() == 0U);
    static_cast<void>(menu.handle_key(0x98U));
    OL_CHECK(menu.main_selection() == 1U);
    static_cast<void>(menu.handle_key(0x0DU));
    OL_CHECK(menu.screen() == TitleScreen::load_slots);
    OL_CHECK(menu.slot_selection() == 0U);

    static_cast<void>(menu.handle_key(0x9EU));
    OL_CHECK(menu.slot_selection() == 2U);
    auto result = menu.handle_key(0x20U);
    OL_CHECK(result.command == TitleCommand::load_slot);
    OL_CHECK(result.slot == 2U);

    static_cast<void>(menu.handle_key(0x1BU));
    OL_CHECK(menu.screen() == TitleScreen::main);
    OL_CHECK(menu.main_selection() == 1U);
    static_cast<void>(menu.handle_key(0x98U));
    result = menu.handle_key(0x96U);
    OL_CHECK(result.command == TitleCommand::exit_game);

    menu.show_main();
    static_cast<void>(menu.handle_key(0x98U));
    result = menu.handle_key(0x0DU);
    OL_CHECK(result.command == TitleCommand::start_new_game);
}

void check_game_menu_controller() {
    using namespace openlegend::ui;

    GameMenuController menu;
    OL_CHECK(menu.visible_main_items() == 6U);
    static_cast<void>(menu.handle_key(0x9EU));
    OL_CHECK(menu.selection() == 5U);
    static_cast<void>(menu.handle_key(0x0DU));
    OL_CHECK(menu.screen() == GameMenuScreen::system);
    OL_CHECK(menu.system_selection() == 0U);

    static_cast<void>(menu.handle_key(0x98U));
    OL_CHECK(menu.system_selection() == 1U);
    static_cast<void>(menu.handle_key(0x20U));
    OL_CHECK(menu.screen() == GameMenuScreen::save_slots);
    static_cast<void>(menu.handle_key(0x9EU));
    OL_CHECK(menu.slot_selection() == 2U);
    auto result = menu.handle_key(0x96U);
    OL_CHECK(result.command == GameMenuCommand::save_slot);
    OL_CHECK(result.slot == 2U);
    OL_CHECK(menu.screen() == GameMenuScreen::save_slots);
    menu.complete_slot_operation();
    OL_CHECK(menu.screen() == GameMenuScreen::system);

    static_cast<void>(menu.handle_key(0x1BU));
    OL_CHECK(menu.screen() == GameMenuScreen::main);
    result = menu.handle_key(0x1BU);
    OL_CHECK(result.command == GameMenuCommand::resume);

    menu.set_context(GameMenuContext::scene);
    OL_CHECK(menu.visible_main_items() == 4U);
    OL_CHECK(menu.selection() == 0U);
    static_cast<void>(menu.handle_key(0x9EU));
    OL_CHECK(menu.selection() == 3U);
    result = menu.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::none);
    OL_CHECK(menu.screen() == GameMenuScreen::party_select);
    OL_CHECK(menu.pending_party_command() == GameMenuCommand::status);
    static_cast<void>(menu.handle_key(0x0DU));
    OL_CHECK(menu.screen() == GameMenuScreen::status_panel);
    OL_CHECK(menu.status_page() == 0U);
    static_cast<void>(menu.handle_key(0x0DU));
    OL_CHECK(menu.screen() == GameMenuScreen::status_panel);
    OL_CHECK(menu.status_page() == 1U);
    static_cast<void>(menu.handle_key(0x0DU));
    OL_CHECK(menu.screen() == GameMenuScreen::main);

    menu.set_context(GameMenuContext::world);
    menu.show_main();
    static_cast<void>(menu.handle_key(0x9EU));
    OL_CHECK(menu.selection() == 5U);
    static_cast<void>(menu.handle_key(0x0DU));
    static_cast<void>(menu.handle_key(0x9EU));
    OL_CHECK(menu.system_selection() == 2U);
    static_cast<void>(menu.handle_key(0x0DU));
    OL_CHECK(menu.screen() == GameMenuScreen::quit_confirmation);
    result = menu.handle_key('N');
    OL_CHECK(result.command == GameMenuCommand::none);
    OL_CHECK(menu.screen() == GameMenuScreen::system);
    static_cast<void>(menu.handle_key(0x0DU));
    result = menu.handle_key('Y');
    OL_CHECK(result.command == GameMenuCommand::exit_game);

    GameMenuController medicine;
    medicine.set_party_count(2U);
    medicine.set_party_abilities({0, 20, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0});
    static_cast<void>(medicine.handle_key(0x0DU));
    OL_CHECK(medicine.screen() == GameMenuScreen::party_select);
    OL_CHECK(medicine.party_stage() == GameMenuPartyStage::source);
    OL_CHECK(medicine.party_options().size() == 1U);
    OL_CHECK(medicine.party_options()[0] == 1U);
    static_cast<void>(medicine.handle_key(0x0DU));
    OL_CHECK(medicine.party_stage() == GameMenuPartyStage::target);
    static_cast<void>(medicine.handle_key(0x98U));
    static_cast<void>(medicine.handle_key(0x1BU));
    OL_CHECK(medicine.party_stage() == GameMenuPartyStage::source);
    OL_CHECK(medicine.selected_party_slot() == 1U);
    static_cast<void>(medicine.handle_key(0x0DU));
    static_cast<void>(medicine.handle_key(0x98U));
    result = medicine.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::medicine);
    OL_CHECK(result.slot == 1U);
    OL_CHECK(result.index == 1U);
    OL_CHECK(medicine.screen() == GameMenuScreen::party_notice);
    OL_CHECK(!medicine.party_action_amount().has_value());
    medicine.complete_party_action(63);
    OL_CHECK(medicine.party_action_amount() == 63);
    static_cast<void>(medicine.handle_key('A'));
    OL_CHECK(medicine.screen() == GameMenuScreen::main);

    GameMenuController no_medicine;
    static_cast<void>(no_medicine.handle_key(0x0DU));
    OL_CHECK(no_medicine.screen() == GameMenuScreen::party_notice);
    OL_CHECK(no_medicine.pending_party_command() == GameMenuCommand::medicine);
    static_cast<void>(no_medicine.handle_key(0U));
    OL_CHECK(no_medicine.screen() == GameMenuScreen::party_notice);
    static_cast<void>(no_medicine.handle_key(0x1BU));
    OL_CHECK(no_medicine.screen() == GameMenuScreen::main);

    GameMenuController detoxification;
    detoxification.set_party_abilities({0, 0, 0, 0, 0, 0}, {10, 0, 0, 0, 0, 0});
    static_cast<void>(detoxification.handle_key(0x98U));
    static_cast<void>(detoxification.handle_key(0x0DU));
    OL_CHECK(detoxification.party_stage() == GameMenuPartyStage::source);
    static_cast<void>(detoxification.handle_key(0x0DU));
    result = detoxification.handle_key(0x96U);
    OL_CHECK(result.command == GameMenuCommand::detoxification);
    OL_CHECK(result.slot == 0U);
    OL_CHECK(result.index == 0U);

    GameMenuController items;
    items.set_inventory_count(3U);
    static_cast<void>(items.handle_key(0x98U));
    static_cast<void>(items.handle_key(0x98U));
    static_cast<void>(items.handle_key(0x0DU));
    OL_CHECK(items.item_selection() == 0U);
    static_cast<void>(items.handle_key(0x98U));
    OL_CHECK(items.item_selection() == 5U);
    static_cast<void>(items.handle_key(0x9EU));
    OL_CHECK(items.item_selection() == 0U);
    static_cast<void>(items.handle_key(0x99U));
    OL_CHECK(items.item_selection() == 15U);
    static_cast<void>(items.handle_key(0x9FU));
    OL_CHECK(items.item_selection() == 0U);
    static_cast<void>(items.handle_key(0x9AU));
    OL_CHECK(items.item_selection() == 4U);
    static_cast<void>(items.handle_key(0x9CU));
    OL_CHECK(items.item_selection() == 0U);
    static_cast<void>(items.handle_key(0x9CU));
    static_cast<void>(items.handle_key(0x9CU));
    result = items.handle_key(0x96U);
    OL_CHECK(result.command == GameMenuCommand::none);
    OL_CHECK(result.index == 0U);
    OL_CHECK(items.screen() == GameMenuScreen::items);
    OL_CHECK(items.item_selection() == 2U);
    result = items.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::items);
    OL_CHECK(result.index == 2U);
    OL_CHECK(items.screen() == GameMenuScreen::main);

    GameMenuController sparse_items;
    const std::array<std::int16_t, 3U> sparse_inventory_slots{2, 5, 199};
    sparse_items.set_inventory_slots(sparse_inventory_slots);
    OL_CHECK(std::ranges::equal(
        sparse_items.inventory_slots(), sparse_inventory_slots));
    static_cast<void>(sparse_items.handle_key(0x98U));
    static_cast<void>(sparse_items.handle_key(0x98U));
    static_cast<void>(sparse_items.handle_key(0x0DU));
    static_cast<void>(sparse_items.handle_key(0x9CU));
    static_cast<void>(sparse_items.handle_key(0x9CU));
    OL_CHECK(sparse_items.item_selection() == 2U);
    result = sparse_items.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::items);
    OL_CHECK(result.index == 199U);

    items.set_party_count(2U);
    items.begin_item_target_selection(GameMenuItemTargetKind::equipment);
    OL_CHECK(items.screen() == GameMenuScreen::party_select);
    OL_CHECK(items.item_target_kind() == GameMenuItemTargetKind::equipment);
    static_cast<void>(items.handle_key(0x98U));
    result = items.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::items);
    OL_CHECK(result.slot == 1U);
    OL_CHECK(result.index == 2U);
    items.show_item_confirmation(GameMenuItemConfirmation::practice_reassign);
    OL_CHECK(items.screen() == GameMenuScreen::item_confirmation);
    static_cast<void>(items.handle_key('N'));
    OL_CHECK(items.screen() == GameMenuScreen::item_confirmation);
    items.show_notice(GameMenuNotice::practice_unsuitable);
    OL_CHECK(items.screen() == GameMenuScreen::notice);
    static_cast<void>(items.handle_key(0U));
    OL_CHECK(items.screen() == GameMenuScreen::notice);
    static_cast<void>(items.handle_key('A'));
    OL_CHECK(items.screen() == GameMenuScreen::main);
    items.show_item_effect();
    OL_CHECK(items.screen() == GameMenuScreen::item_effect);
    static_cast<void>(items.handle_key('A'));
    OL_CHECK(items.screen() == GameMenuScreen::main);

    GameMenuController leave_party;
    for (int index = 0; index < 4; ++index) {
        static_cast<void>(leave_party.handle_key(0x98U));
    }
    static_cast<void>(leave_party.handle_key(0x0DU));
    result = leave_party.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::leave_party);
    OL_CHECK(result.index == 0U);
    leave_party.show_notice(GameMenuNotice::leave_protagonist);
    static_cast<void>(leave_party.handle_key('A'));
    OL_CHECK(leave_party.screen() == GameMenuScreen::main);
    leave_party.set_party_count(2U);
    static_cast<void>(leave_party.handle_key(0x0DU));
    static_cast<void>(leave_party.handle_key(0x98U));
    result = leave_party.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::leave_party);
    OL_CHECK(result.index == 1U);

    GameMenuController load_cancel;
    static_cast<void>(load_cancel.handle_key(0x9EU));
    static_cast<void>(load_cancel.handle_key(0x0DU));
    static_cast<void>(load_cancel.handle_key(0x0DU));
    OL_CHECK(load_cancel.screen() == GameMenuScreen::load_slots);
    static_cast<void>(load_cancel.handle_key(0x1BU));
    OL_CHECK(load_cancel.screen() == GameMenuScreen::system);
}

void check_attribute_controller() {
    using namespace openlegend;

    model::RoleRecord protagonist;
    protagonist.set_word(model::role_word::level, 1);
    random::LegacyRandom random{0U};
    ui::NewGameAttributeController controller{protagonist, random};
    OL_CHECK(protagonist.word(model::role_word::maximum_mp) == 29);
    OL_CHECK(controller.handle_key('N') == ui::AttributeRollStatus::choosing);
    OL_CHECK(protagonist.word(model::role_word::maximum_mp) != 29);

    model::RoleRecord cheat_protagonist;
    cheat_protagonist.set_word(model::role_word::level, 1);
    random::LegacyRandom cheat_random{0U};
    ui::NewGameAttributeController cheat{cheat_protagonist, cheat_random};
    for (const auto key : {'B', 'A', 'B', 'E', 'R', 'U', 'T', 'H'}) {
        OL_CHECK(cheat.handle_key(static_cast<std::uint8_t>(key)) ==
                 ui::AttributeRollStatus::choosing);
    }
    OL_CHECK(cheat.cheat_active());
    OL_CHECK(cheat_protagonist.word(model::role_word::maximum_hp) == 50);
    OL_CHECK(cheat_protagonist.word(model::role_word::iq) == 100);
    OL_CHECK(cheat.handle_key('Y') == ui::AttributeRollStatus::accepted);

    random::LegacyRandom expected{0U};
    for (std::size_t index = 0U; index < 8U * 17U; ++index) {
        static_cast<void>(expected.next());
    }
    OL_CHECK(cheat_random.next() == expected.next());
}

void check_name_editor(const std::filesystem::path& data_root) {
    using namespace openlegend::ui;

    NewGameNameEditor editor{openlegend::resource::DataRoot{data_root}};
    OL_CHECK(editor.valid());
    if (!editor.valid()) {
        return;
    }
    OL_CHECK(editor.mode() == NameInputMode::zhuyin);
    OL_CHECK(editor.cursor_color() == 7U);
    editor.finish_presented_frame();
    OL_CHECK(editor.cursor_color() == 9U);
    editor.finish_presented_frame();
    OL_CHECK(editor.cursor_color() == 7U);
    static_cast<void>(editor.handle_key('R', false, false));
    static_cast<void>(editor.handle_key('U', false, false));
    static_cast<void>(editor.handle_key('P', false, false));
    static_cast<void>(editor.handle_key(0x20U, false, false));
    OL_CHECK(editor.candidates().size() == 20U);
    OL_CHECK(editor.visible_candidate_count() == 8U);
    OL_CHECK(editor.candidates()[0][0] == 0xA4U);
    OL_CHECK(editor.candidates()[0][1] == 0xB5U);
    OL_CHECK(editor.handle_key(0x0DU, false, false) == NameEditStatus::editing);
    static_cast<void>(editor.handle_key(0x08U, false, false));
    OL_CHECK(editor.candidates().size() == 20U);
    static_cast<void>(editor.handle_key(0x2CU, false, true));
    OL_CHECK(editor.candidate_page() == -1);
    OL_CHECK(editor.visible_candidate_count() == 8U);
    static_cast<void>(editor.handle_key('8', false, false));
    OL_CHECK(editor.candidate_page() == -1);
    static_cast<void>(editor.handle_key('1', false, false));
    OL_CHECK(editor.candidates().empty());
    OL_CHECK(editor.initial() == 0);
    OL_CHECK(editor.medial() == 0);
    OL_CHECK(editor.final() == 0);
    static_cast<void>(editor.handle_key('R', false, false));
    static_cast<void>(editor.handle_key('U', false, false));
    static_cast<void>(editor.handle_key('P', false, false));
    static_cast<void>(editor.handle_key(0x20U, false, false));
    static_cast<void>(editor.handle_key(0x2EU, false, true));
    OL_CHECK(editor.candidate_page() == 1);
    static_cast<void>(editor.handle_key(0x2EU, false, true));
    OL_CHECK(editor.candidate_page() == 2);
    OL_CHECK(editor.visible_candidate_count() == 4U);
    static_cast<void>(editor.handle_key(0x20U, false, false));
    OL_CHECK(editor.candidate_page() == 0);
    static_cast<void>(editor.handle_key('2', false, false));
    OL_CHECK(editor.name().size() == 2U);
    OL_CHECK(editor.name()[0] == 0xAAU);
    OL_CHECK(editor.name()[1] == 0xF7U);

    static_cast<void>(editor.handle_key(0x08U, false, false));
    OL_CHECK(editor.name().empty());
    static_cast<void>(editor.handle_key(0x20U, true, false));
    OL_CHECK(editor.mode() == NameInputMode::alphanumeric);
    static_cast<void>(editor.handle_key('A', false, false));
    static_cast<void>(editor.handle_key('B', false, false));
    static_cast<void>(editor.handle_key(0x08U, false, false));
    for (const auto key : {'C', 'D', 'E', 'F', 'G', 'H'}) {
        static_cast<void>(editor.handle_key(static_cast<std::uint8_t>(key), false, false));
    }
    OL_CHECK(editor.name().size() == 6U);
    OL_CHECK(editor.name()[0] == 'A');
    OL_CHECK(editor.name()[5] == 'G');
    OL_CHECK(editor.handle_key(0x0DU, false, false) == NameEditStatus::completed);
    OL_CHECK(editor.accepted());

    NewGameNameEditor no_match{openlegend::resource::DataRoot{data_root}};
    static_cast<void>(no_match.handle_key('1', false, false));
    static_cast<void>(no_match.handle_key('7', false, false));
    OL_CHECK(no_match.no_candidates());
    OL_CHECK(no_match.initial() == 1);
    OL_CHECK(no_match.tone() == 1);
    static_cast<void>(no_match.handle_key('A', false, false));
    OL_CHECK(!no_match.no_candidates());
    OL_CHECK(no_match.initial() == 0);
    OL_CHECK(no_match.tone() == 0);
    OL_CHECK(no_match.name().empty());
    static_cast<void>(no_match.handle_key('A', false, false));
    OL_CHECK(no_match.initial() == 3);

    NewGameNameEditor ghost{openlegend::resource::DataRoot{data_root}};
    static_cast<void>(ghost.handle_key(0x20U, true, false));
    static_cast<void>(ghost.handle_key('A', false, false));
    static_cast<void>(ghost.handle_key(0x08U, false, false));
    OL_CHECK(ghost.name().empty());
    OL_CHECK(ghost.display_name().size() == 1U);
    OL_CHECK(ghost.display_name()[0] == 'A');
    OL_CHECK(ghost.handle_key(0x0DU, false, false) == NameEditStatus::editing);
    static_cast<void>(ghost.handle_key('B', false, false));
    OL_CHECK(ghost.name().size() == 1U);
    OL_CHECK(ghost.name()[0] == 'B');
    OL_CHECK(ghost.display_name()[0] == 'B');

    NewGameNameEditor zhuyin_limit{openlegend::resource::DataRoot{data_root}};
    static_cast<void>(zhuyin_limit.handle_key(0x20U, true, false));
    for (const auto key : {'A', 'B', 'C', 'D', 'E'}) {
        static_cast<void>(
            zhuyin_limit.handle_key(static_cast<std::uint8_t>(key), false, false));
    }
    static_cast<void>(zhuyin_limit.handle_key(0x20U, true, false));
    static_cast<void>(zhuyin_limit.handle_key('R', false, false));
    OL_CHECK(zhuyin_limit.initial() == 0);
}

void check_startup_resource_cache(const std::filesystem::path& data_root) {
    using namespace openlegend;

    app::LegacyStartupResources startup_resources{resource::DataRoot{data_root}};
    OL_CHECK(startup_resources.valid());
    OL_CHECK(startup_resources.fixed_shadow_mask().size() == 3'885U);
    OL_CHECK(startup_resources.shifted_shadow_mask().size() == 5'585U);

    const auto missing_cloud_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-startup-missing-cloud";
    OL_CHECK(prepare_runtime_fixture(data_root, missing_cloud_root));
    std::error_code error;
    std::filesystem::remove(missing_cloud_root / "CLOUD.GRP", error);
    OL_CHECK(!error);
    app::LegacyGameRuntime missing_cloud{missing_cloud_root, 0U};
    OL_CHECK(!missing_cloud.valid());
    OL_CHECK(missing_cloud.error().find("CLOUD.GRP") != std::string::npos);

    const auto missing_shadow_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b7-startup-missing-shadow";
    OL_CHECK(prepare_runtime_fixture(data_root, missing_shadow_root));
    std::filesystem::remove(missing_shadow_root / "3_shadow.msk", error);
    OL_CHECK(!error);
    app::LegacyGameRuntime missing_shadow{missing_shadow_root, 0U};
    OL_CHECK(!missing_shadow.valid());
    OL_CHECK(missing_shadow.error().find("3_shadow.msk") != std::string::npos);

    const auto missing_ranger_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-startup-missing-ranger";
    OL_CHECK(prepare_runtime_fixture(data_root, missing_ranger_root));
    std::filesystem::remove(missing_ranger_root / "RANGER.IDX", error);
    OL_CHECK(!error);
    app::LegacyGameRuntime missing_ranger{missing_ranger_root, 0U};
    OL_CHECK(!missing_ranger.valid());
    OL_CHECK(missing_ranger.error().find("RANGER.IDX") != std::string::npos);

    const auto missing_world_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-startup-missing-world";
    OL_CHECK(prepare_runtime_fixture(data_root, missing_world_root));
    std::filesystem::remove(missing_world_root / "EARTH.002", error);
    OL_CHECK(!error);
    app::LegacyGameRuntime missing_world{missing_world_root, 0U};
    OL_CHECK(!missing_world.valid());
    OL_CHECK(missing_world.error().find("EARTH.002") != std::string::npos);

    const auto cached_new_game_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-startup-cached-new-game";
    OL_CHECK(prepare_runtime_fixture(data_root, cached_new_game_root));
    app::LegacyGameRuntime cached_new_game{cached_new_game_root, 0U};
    OL_CHECK(cached_new_game.valid());
    std::filesystem::remove(cached_new_game_root / "RANGER.IDX", error);
    OL_CHECK(!error);
    std::filesystem::remove(cached_new_game_root / "RANGER.GRP", error);
    OL_CHECK(!error);
    finish_title_startup(cached_new_game);
    cached_new_game.handle_key(0x0DU, false, false);
    finish_title_confirmation(cached_new_game);
    OL_CHECK(cached_new_game.view() == app::LegacyGameView::name_entry);
    OL_CHECK(cached_new_game.game_state().loaded());

    const auto cached_slot_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-startup-cached-slot";
    OL_CHECK(prepare_runtime_fixture(data_root, cached_slot_root));
    const auto baseline = persistence::load_baseline(data_root);
    OL_CHECK(static_cast<bool>(baseline));
    if (!baseline.snapshot.has_value()) {
        return;
    }
    OL_CHECK(persistence::write_numbered_slot(
        cached_slot_root, persistence::SaveSlot::one, *baseline.snapshot));
    app::LegacyGameRuntime cached_slot{cached_slot_root, 0U};
    OL_CHECK(cached_slot.valid());
    std::filesystem::remove(cached_slot_root / "RANGER.IDX", error);
    OL_CHECK(!error);
    std::filesystem::remove(cached_slot_root / "CLOUD.IDX", error);
    OL_CHECK(!error);
    std::filesystem::remove(cached_slot_root / "CLOUD.GRP", error);
    OL_CHECK(!error);
    std::filesystem::remove(cached_slot_root / "MMAP.COL", error);
    OL_CHECK(!error);
    for (const auto layer : {
             "EARTH.002",
             "BUILDING.002",
             "SURFACE.002",
             "BUILDX.002",
             "BUILDY.002"}) {
        std::filesystem::remove(cached_slot_root / layer, error);
        OL_CHECK(!error);
    }
    finish_title_startup(cached_slot);
    cached_slot.handle_key(0x98U, false, false);
    cached_slot.handle_key(0x0DU, false, false);
    cached_slot.handle_key(0x0DU, false, false);
    finish_title_confirmation(cached_slot);
    OL_CHECK(cached_slot.render());
    cached_slot.finish_presented_tick();
    cached_slot.advance();
    finish_numbered_load_transition(cached_slot, app::LegacyGameView::world);
    OL_CHECK(cached_slot.game_state().loaded());
}

void check_game_runtime(const std::filesystem::path& data_root) {
    using namespace openlegend;

    {
        app::LegacyGameRuntime title_exit{data_root, 0U};
        OL_CHECK(title_exit.valid());
        finish_title_startup(title_exit);
        OL_CHECK(!title_exit.fade_music_on_exit());
        OL_CHECK(
            title_exit.handle_key(0x9EU, false, false) ==
            app::LegacyKeyStateReset::translated);
        OL_CHECK(
            title_exit.handle_key(0x0DU, false, false) ==
            app::LegacyKeyStateReset::confirmation_group);
        OL_CHECK(!title_exit.running());
        OL_CHECK(!title_exit.fade_music_on_exit());
        OL_CHECK(!title_exit.ending_complete());
    }

    std::optional<model::GameSnapshot> accepted_new_game;
    {
        app::LegacyGameRuntime intro_game{data_root, 0U};
        OL_CHECK(intro_game.valid());
        finish_title_startup(intro_game);
        OL_CHECK(intro_game.view() == app::LegacyGameView::title);
        OL_CHECK(intro_game.render());
        OL_CHECK(fnv1a64(intro_game.framebuffer().pixels()) == 0x86690E3B3B68FE20ULL);
        intro_game.handle_key(0x0DU, false, false);
        finish_title_confirmation(intro_game);
        OL_CHECK(intro_game.view() == app::LegacyGameView::name_entry);
        OL_CHECK(intro_game.game_state().loaded());
        OL_CHECK(intro_game.render());
        intro_game.handle_key(0x20U, true, false);
        intro_game.handle_key('A', false, false);
        intro_game.handle_key(0x0DU, false, false);
        OL_CHECK(intro_game.view() == app::LegacyGameView::name_entry);
        OL_CHECK(intro_game.render());
        intro_game.advance();
        OL_CHECK(intro_game.view() == app::LegacyGameView::name_entry);
        intro_game.finish_presented_tick();
        for (int tick = 0; tick < 29; ++tick) {
            intro_game.advance();
            OL_CHECK(intro_game.view() == app::LegacyGameView::name_entry);
        }
        intro_game.advance();
        OL_CHECK(intro_game.view() == app::LegacyGameView::attributes);
        OL_CHECK(intro_game.render());
        intro_game.handle_key('Y', false, false);
        OL_CHECK(intro_game.view() == app::LegacyGameView::attributes);
        finish_new_game_scene_transition(intro_game);
        OL_CHECK(intro_game.view() == app::LegacyGameView::scene);
        const auto* ranger = intro_game.game_state().ranger();
        OL_CHECK(ranger != nullptr);
        if (ranger != nullptr) {
            OL_CHECK(ranger->roles[0].bytes[model::role_word::name_byte] == 'A');
            OL_CHECK(ranger->roles[0].bytes[model::role_word::name_byte + 1U] == 0U);
            OL_CHECK(ranger->header.word(model::header_word::in_sub_map) == 1);
            OL_CHECK(ranger->header.word(model::header_word::sub_map_x) == 19);
            OL_CHECK(ranger->header.word(model::header_word::sub_map_y) == 20);
            OL_CHECK(ranger->header.word(model::header_word::face_towards) ==
                     static_cast<std::int16_t>(scene::SceneDirection::right));
        }
        accepted_new_game = intro_game.game_state().export_snapshot();
    }
    OL_CHECK(accepted_new_game.has_value());
    if (!accepted_new_game.has_value()) {
        return;
    }
    accepted_new_game->ranger.header.set_word(model::header_word::in_sub_map, 0);
    const auto world_fixture = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-new-game-world";
    OL_CHECK(prepare_runtime_fixture(data_root, world_fixture));
    OL_CHECK(persistence::write_numbered_slot(
        world_fixture, persistence::SaveSlot::one, *accepted_new_game));

    auto idle_counter_snapshot = *accepted_new_game;
    idle_counter_snapshot.ranger.header.set_word(model::header_word::in_sub_map, 0);
    idle_counter_snapshot.ranger.header.set_word(model::header_word::in_ship, 0);
    idle_counter_snapshot.ranger.header.set_word(model::header_word::main_map_x, 356);
    idle_counter_snapshot.ranger.header.set_word(model::header_word::main_map_y, 235);
    idle_counter_snapshot.ranger.header.set_word(
        model::header_word::face_towards,
        static_cast<std::int16_t>(world::WorldDirection::right));
    idle_counter_snapshot.ranger.roles[0U].set_word(
        model::role_word::physical_power, 50);
    auto& idle_counter_scene = idle_counter_snapshot.ranger.scenes[70U];
    idle_counter_scene.set_word(model::scene_metadata_word::main_entrance_x_1, 357);
    idle_counter_scene.set_word(model::scene_metadata_word::main_entrance_y_1, 235);
    idle_counter_scene.set_word(model::scene_metadata_word::entrance_condition, 0);
    const auto idle_counter_fixture =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-idle-counter-continuity";
    OL_CHECK(prepare_runtime_fixture(data_root, idle_counter_fixture));
    OL_CHECK(persistence::write_numbered_slot(
        idle_counter_fixture, persistence::SaveSlot::one, idle_counter_snapshot));

    auto continuation_fixture_snapshot = idle_counter_snapshot;
    continuation_fixture_snapshot.ranger.header.set_word(model::header_word::main_map_x, 357);
    for (auto& scene : continuation_fixture_snapshot.ranger.scenes) {
        scene.set_word(model::scene_metadata_word::main_entrance_x_1, -1);
        scene.set_word(model::scene_metadata_word::main_entrance_y_1, -1);
        scene.set_word(model::scene_metadata_word::main_entrance_x_2, -1);
        scene.set_word(model::scene_metadata_word::main_entrance_y_2, -1);
    }
    auto& continuation_scene = continuation_fixture_snapshot.ranger.scenes[70U];
    continuation_scene.set_word(model::scene_metadata_word::main_entrance_x_1, 356);
    continuation_scene.set_word(model::scene_metadata_word::main_entrance_y_1, 235);
    continuation_scene.set_word(model::scene_metadata_word::entrance_condition, 0);
    const auto continuation_fixture =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-world-scene-continuation";
    OL_CHECK(prepare_runtime_fixture(data_root, continuation_fixture));
    OL_CHECK(persistence::write_numbered_slot(
        continuation_fixture, persistence::SaveSlot::one, continuation_fixture_snapshot));
    {
        app::LegacyGameRuntime walk{idle_counter_fixture, 0U};
        OL_CHECK(walk.valid());
        finish_title_startup(walk);
        walk.handle_key(0x98U, false, false);
        walk.handle_key(0x0DU, false, false);
        walk.handle_key(0x0DU, false, false);
        finish_title_confirmation(walk);
        OL_CHECK(walk.render());
        walk.finish_presented_tick();
        walk.advance();
        finish_numbered_load_transition(walk, app::LegacyGameView::world);
        OL_CHECK(app::LegacyGameRuntimeTestAccess::begin_scene_walk(walk));
        finish_scene_entry(walk);
        OL_CHECK(walk.render());
        walk.finish_presented_tick(700U);
        // Z.DAT 0x28F66..0x292BB: one movement per loop, one tick wait at
        // its tail. Present acknowledgement must not occupy the next tick.
        for (std::uint32_t step = 0U; step < 13U; ++step) {
            OL_CHECK(walk.scene_loop_uses_key_states());
            const bool right = step == 0U;
            const bool up = step % 2U == 1U;
            const bool down = !right && !up;
            OL_CHECK(walk.handle_world_input(false, up, down, right));
            walk.advance(701U + step);
            const auto* ranger = walk.game_state().ranger();
            OL_CHECK(ranger != nullptr);
            if (ranger != nullptr) {
                OL_CHECK(ranger->header.word(model::header_word::sub_map_x) == 45);
                OL_CHECK(ranger->header.word(model::header_word::sub_map_y) == (up ? 28 : 29));
            }
            OL_CHECK(walk.render());
            walk.finish_presented_tick(701U + step);
            OL_CHECK(walk.scene_loop_uses_key_states());
            OL_CHECK(!walk.needs_immediate_frame(701U + step));
            OL_CHECK(!walk.uses_vga_retrace());
        }
    }
    {
        app::LegacyGameRuntime idle_counter{idle_counter_fixture, 0U};
        OL_CHECK(idle_counter.valid());
        finish_title_startup(idle_counter);
        idle_counter.handle_key(0x98U, false, false);
        idle_counter.handle_key(0x0DU, false, false);
        idle_counter.handle_key(0x0DU, false, false);
        finish_title_confirmation(idle_counter);
        OL_CHECK(idle_counter.render());
        idle_counter.finish_presented_tick();
        idle_counter.advance();
        finish_numbered_load_transition(idle_counter, app::LegacyGameView::world);
        auto* idle_ranger =
            const_cast<model::GameState&>(idle_counter.game_state()).ranger();
        OL_CHECK(idle_ranger != nullptr);
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 50);
        }
        for (int tick = 0; tick < 99; ++tick) {
            idle_counter.advance();
        }
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 50);
        }

        OL_CHECK(idle_counter.handle_world_input(false, false, false, true));
        idle_counter.advance();
        OL_CHECK(idle_counter.scene_request().value_or(-1) == 70);
        finish_world_scene_transition(idle_counter);
        finish_scene_entry(idle_counter);
        advance_scene_idle_ticks(idle_counter, 99U);
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 50);
        }
        advance_scene_idle_ticks(idle_counter, 1U);
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 51);
        }
        advance_scene_idle_ticks(idle_counter, 60U);

        auto* idle_scene_snapshot =
            const_cast<model::GameState&>(idle_counter.game_state()).snapshot();
        OL_CHECK(idle_scene_snapshot != nullptr);
        if (idle_scene_snapshot != nullptr) {
            OL_CHECK(idle_scene_snapshot->set_scene_value(
                70U, model::SceneLayer::event_index, 28U * 64U + 44U, -1));
        }
        OL_CHECK(idle_counter.handle_world_input(false, false, true, false));
        idle_counter.advance();
        advance_rendered_frames(idle_counter, 1U);
        OL_CHECK(idle_counter.handle_world_input(false, false, false, true));
        idle_counter.advance();
        advance_rendered_frames(idle_counter, 1U);
        advance_rendered_frames(idle_counter, 64U);
        OL_CHECK(idle_counter.view() == app::LegacyGameView::world);
        finish_world_scene_return(idle_counter);
        for (int tick = 0; tick < 139; ++tick) {
            idle_counter.advance();
        }
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 51);
        }
        idle_counter.advance();
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 52);
        }

        for (int tick = 0; tick < 40; ++tick) {
            idle_counter.advance();
        }
        idle_counter.handle_key(0x1BU, false, false);
        idle_counter.handle_key(0x9EU, false, false);
        idle_counter.handle_key(0x0DU, false, false);
        idle_counter.handle_key(0x0DU, false, false);
        idle_counter.handle_key(0x0DU, false, false);
        OL_CHECK(idle_counter.render());
        idle_counter.finish_presented_tick();
        idle_counter.advance();
        finish_numbered_load_transition(idle_counter, app::LegacyGameView::game_menu);
        idle_ranger = const_cast<model::GameState&>(idle_counter.game_state()).ranger();
        OL_CHECK(idle_ranger != nullptr);
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 50);
        }
        idle_counter.handle_key(0x1BU, false, false);
        idle_counter.handle_key(0x1BU, false, false);
        OL_CHECK(idle_counter.view() == app::LegacyGameView::world);
        for (int tick = 0; tick < 159; ++tick) {
            idle_counter.advance();
        }
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 50);
        }
        idle_counter.advance();
        if (idle_ranger != nullptr) {
            OL_CHECK(idle_ranger->roles[0U].word(model::role_word::physical_power) == 51);
        }
    }

    auto system_exit_snapshot = *accepted_new_game;
    system_exit_snapshot.ranger.roles[0U].set_word(
        model::role_word::physical_power, 50);
    const auto system_exit_fixture =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-main-system-exit";
    OL_CHECK(prepare_runtime_fixture(data_root, system_exit_fixture));
    OL_CHECK(persistence::write_numbered_slot(
        system_exit_fixture, persistence::SaveSlot::one, system_exit_snapshot));
    {
        app::LegacyGameRuntime system_exit{system_exit_fixture, 0U};
        finish_title_startup(system_exit);
        system_exit.handle_key(0x98U, false, false);
        system_exit.handle_key(0x0DU, false, false);
        system_exit.handle_key(0x0DU, false, false);
        finish_title_confirmation(system_exit);
        OL_CHECK(system_exit.render());
        system_exit.finish_presented_tick();
        system_exit.advance();
        finish_numbered_load_transition(system_exit, app::LegacyGameView::world);
        OL_CHECK(system_exit.game_state().ranger()->roles[0U].word(
                     model::role_word::physical_power) == 50);
        for (int tick = 0; tick < 199; ++tick) {
            system_exit.advance();
        }
        OL_CHECK(system_exit.game_state().ranger()->roles[0U].word(
                     model::role_word::physical_power) == 51);
        OL_CHECK(system_exit.handle_world_input(false, false, false, false, true));
        OL_CHECK(
            system_exit.handle_key(0x9EU, false, false) ==
            app::LegacyKeyStateReset::down_translated);
        OL_CHECK(
            system_exit.handle_key(0x0DU, false, false) ==
            app::LegacyKeyStateReset::confirmation_group);
        OL_CHECK(
            system_exit.handle_key(0x9EU, false, false) ==
            app::LegacyKeyStateReset::translated);
        OL_CHECK(
            system_exit.handle_key(0x0DU, false, false) ==
            app::LegacyKeyStateReset::confirmation_group);
        OL_CHECK(
            system_exit.handle_key('Y', false, false) ==
            app::LegacyKeyStateReset::none);
        OL_CHECK(system_exit.running());
        OL_CHECK(system_exit.view() == app::LegacyGameView::world);
        OL_CHECK(!system_exit.handle_world_input(true, false, false, false, false));
        system_exit.advance();
        OL_CHECK(system_exit.game_state().ranger()->roles[0U].word(
                     model::role_word::physical_power) == 51);
        OL_CHECK(system_exit.render());
        system_exit.finish_presented_tick();
        OL_CHECK(!system_exit.running());
        OL_CHECK(system_exit.fade_music_on_exit());
        OL_CHECK(!system_exit.ending_complete());
    }

    app::LegacyGameRuntime new_game{world_fixture, 0U};
    finish_title_startup(new_game);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    new_game.handle_key(0x0DU, false, false);
    finish_title_confirmation(new_game);
    OL_CHECK(new_game.render());
    new_game.finish_presented_tick();
    new_game.advance();
    finish_numbered_load_transition(new_game, app::LegacyGameView::world);
    const auto* ranger = new_game.game_state().ranger();
    OL_CHECK(ranger != nullptr);
    const resource::DataRoot world_data{world_fixture};
    const auto palette_file = world_data.read("mmap.col");
    OL_CHECK(static_cast<bool>(palette_file));
    const auto source_palette = resource::parse_vga_palette(palette_file.bytes);
    OL_CHECK(static_cast<bool>(source_palette));

    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == 0x0604155353F95194ULL);
    const auto first_present_palette = new_game.framebuffer().palette();
    OL_CHECK(first_present_palette[0].red == source_palette.palette[0].red);
    OL_CHECK(first_present_palette[224].red == source_palette.palette[231].red);
    OL_CHECK(first_present_palette[224].green == source_palette.palette[231].green);
    OL_CHECK(first_present_palette[224].blue == source_palette.palette[231].blue);
    OL_CHECK(first_present_palette[244].red == source_palette.palette[252].red);
    OL_CHECK(new_game.render());
    OL_CHECK(new_game.framebuffer().palette()[224].red == first_present_palette[224].red);
    new_game.finish_presented_tick();
    OL_CHECK(new_game.render());
    const auto palette_after_first_commit = new_game.framebuffer().palette();
    OL_CHECK(palette_after_first_commit[224].red == first_present_palette[224].red);
    OL_CHECK(palette_after_first_commit[224].green == first_present_palette[224].green);
    OL_CHECK(palette_after_first_commit[224].blue == first_present_palette[224].blue);
    for (int tick = 0; tick < 4; ++tick) {
        new_game.finish_presented_tick();
    }
    OL_CHECK(new_game.render());
    const auto second_present_palette = new_game.framebuffer().palette();
    OL_CHECK(second_present_palette[224].red == first_present_palette[231].red);
    OL_CHECK(second_present_palette[224].green == first_present_palette[231].green);
    OL_CHECK(second_present_palette[224].blue == first_present_palette[231].blue);
    new_game.finish_presented_tick();
    OL_CHECK(new_game.render());
    const auto palette_after_second_commit = new_game.framebuffer().palette();
    OL_CHECK(palette_after_second_commit[224].red == second_present_palette[224].red);
    OL_CHECK(palette_after_second_commit[224].green == second_present_palette[224].green);
    OL_CHECK(palette_after_second_commit[224].blue == second_present_palette[224].blue);
    new_game.handle_world_input(false, false, false, true);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::main_map_x) == 357);
    new_game.handle_world_input(false, true, false, false);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    new_game.handle_world_input(true, false, false, false);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    OL_CHECK(new_game.handle_world_input(false, false, true, false, true));
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::main_map_x) == 357);
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::main_map_y) == 235);

    auto* sparse_inventory_ranger =
        const_cast<model::GameState&>(new_game.game_state()).ranger();
    OL_CHECK(sparse_inventory_ranger != nullptr);
    const auto saved_sparse_inventory_header = sparse_inventory_ranger->header;
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        sparse_inventory_ranger->header.set_inventory(slot, model::ItemId{-1}, 0);
    }
    sparse_inventory_ranger->header.set_inventory(2U, model::ItemId{0}, 0);
    sparse_inventory_ranger->header.set_inventory(5U, model::ItemId{0}, -1);
    sparse_inventory_ranger->header.set_inventory(199U, model::ItemId{2}, -32768);
    new_game.handle_key(0x1BU, false, false);
    const std::array<std::int16_t, 3U> expected_sparse_slots{2, 5, 199};
    OL_CHECK(std::ranges::equal(
        app::LegacyGameRuntimeTestAccess::game_menu_inventory_slots(new_game),
        expected_sparse_slots));
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    sparse_inventory_ranger->header = saved_sparse_inventory_header;

    OL_CHECK(new_game.handle_world_input(false, false, false, false, true));
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    OL_CHECK(new_game.render());
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto status_selector_hash = fnv1a64(new_game.framebuffer().pixels());
    if (status_selector_hash != 0xA51079504B6D8894ULL) {
        std::cerr << "status_selector_hash=0x" << std::hex << status_selector_hash << std::dec << '\n';
    }
    OL_CHECK(status_selector_hash == 0xA51079504B6D8894ULL);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto status_page_0_hash = fnv1a64(new_game.framebuffer().pixels());
    if (status_page_0_hash != 0xE5ED619A78913195ULL) {
        std::cerr << "status_page_0_hash=0x" << std::hex << status_page_0_hash << std::dec << '\n';
    }
    OL_CHECK(status_page_0_hash == 0xE5ED619A78913195ULL);
    new_game.handle_key('A', false, false);
    OL_CHECK(new_game.render());
    const auto status_page_1_hash = fnv1a64(new_game.framebuffer().pixels());
    if (status_page_1_hash != 0xC1B9BB1B1BBEF613ULL) {
        std::cerr << "status_page_1_hash=0x" << std::hex << status_page_1_hash << std::dec << '\n';
    }
    OL_CHECK(status_page_1_hash == 0xC1B9BB1B1BBEF613ULL);
    new_game.handle_key('A', false, false);
    new_game.handle_key(0x9EU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 0U);
    new_game.handle_key(0x98U, false, false);
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 0U);
    OL_CHECK(new_game.render());
    new_game.handle_key(0x98U, false, false);
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 0U);
    const auto items_initial_hash = fnv1a64(new_game.framebuffer().pixels());
    if (items_initial_hash != 0xD1E30FA5A50162AEULL) {
        std::cerr << "items_initial_hash=0x" << std::hex << items_initial_hash << std::dec << '\n';
    }
    OL_CHECK(items_initial_hash == 0xD1E30FA5A50162AEULL);
    new_game.finish_presented_tick();
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
    new_game.handle_key(0x98U, false, false);
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 5U);
    new_game.handle_key(0x9EU, false, false);
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 5U);
    OL_CHECK(new_game.render());
    new_game.finish_presented_tick();
    new_game.handle_key(0x9EU, false, false);
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 0U);
    OL_CHECK(new_game.render());
    new_game.finish_presented_tick();
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_dispatch_pending(new_game));
    OL_CHECK(new_game.render());
    new_game.finish_presented_tick();
    OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_item_dispatch_pending(new_game));
    auto* leave_prefix_ranger =
        const_cast<model::GameState&>(new_game.game_state()).ranger();
    OL_CHECK(leave_prefix_ranger != nullptr);
    if (leave_prefix_ranger != nullptr) {
        leave_prefix_ranger->header.set_team_member(1U, model::CharacterId{0});
    }
    const auto before_leave = new_game.game_state().export_snapshot();
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    new_game.finish_presented_tick();
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    OL_CHECK(new_game.render());
    const auto after_leave = new_game.game_state().export_snapshot();
    OL_CHECK(before_leave.has_value() && after_leave.has_value());
    if (before_leave.has_value() && after_leave.has_value()) {
        OL_CHECK(*before_leave == *after_leave);
    }
    new_game.handle_key('A', false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    auto* leave_snapshot = const_cast<model::GameState&>(new_game.game_state()).snapshot();
    OL_CHECK(leave_snapshot != nullptr);
    if (leave_snapshot != nullptr) {
        leave_snapshot->ranger.header.set_team_member(1U, model::CharacterId{1});
    }
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    for (int index = 0; index < 4; ++index) {
        new_game.handle_key(0x98U, false, false);
    }
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto leave_selector_hash = fnv1a64(new_game.framebuffer().pixels());
    if (leave_selector_hash != 0x8F86566158644688ULL) {
        std::cerr << "leave_selector_hash=0x" << std::hex << leave_selector_hash << std::dec
                  << '\n';
    }
    OL_CHECK(leave_selector_hash == 0x8F86566158644688ULL);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) != leave_selector_hash);
    new_game.finish_presented_tick();
    new_game.advance();
    OL_CHECK(new_game.render());
    const auto leave_dialogue_pixels_hash = fnv1a64(new_game.framebuffer().pixels());
    new_game.handle_key('A', false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.game_state().ranger()->header.team_member(1U).value == -1);
    const auto* after_leave_script = new_game.game_state().snapshot();
    OL_CHECK(after_leave_script != nullptr);
    if (after_leave_script != nullptr) {
        OL_CHECK(after_leave_script->event_value(
                     0U, 0U, model::SceneEventField::event_1).value_or(-1) == 951);
    }
    OL_CHECK(!new_game.handle_world_input(false, false, false, false, true));
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == leave_dialogue_pixels_hash);
    new_game.finish_presented_tick();
    new_game.advance();
    advance_rendered_frames(new_game, 63U);
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) != leave_dialogue_pixels_hash);
    auto leave_redraw_is_black = true;
    for (const auto component : new_game.framebuffer().palette()) {
        if (component.red != 0U || component.green != 0U || component.blue != 0U) {
            leave_redraw_is_black = false;
        }
    }
    OL_CHECK(leave_redraw_is_black);
    new_game.finish_presented_tick();
    new_game.advance();
    advance_rendered_frames(new_game, 65U);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    auto* item_ranger = const_cast<model::GameState&>(new_game.game_state()).ranger();
    OL_CHECK(item_ranger != nullptr);
    if (item_ranger != nullptr) {
        const auto prepare_item = [&](const std::int16_t item_id, const std::int16_t item_type) {
            auto& item = item_ranger->items[static_cast<std::size_t>(item_id)];
            item.set_word(model::item_word::id, item_id);
            item.set_word(model::item_word::item_type, item_type);
            item.set_word(model::item_word::show_introduction, 1);
            item.set_word(model::item_word::user, -1);
            item.set_word(model::item_word::only_suitable_role, -1);
            item.set_word(model::item_word::need_mp_type, 2);
            for (std::size_t field = model::item_word::need_mp;
                 field <= model::item_word::need_iq;
                 ++field) {
                item.set_word(field, 0);
            }
        };
        const auto enter_items = [&]() {
            new_game.handle_key(0x1BU, false, false);
            new_game.handle_key(0x98U, false, false);
            new_game.handle_key(0x98U, false, false);
            new_game.handle_key(0x0DU, false, false);
            OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
            OL_CHECK(new_game.render());
            new_game.finish_presented_tick();
            OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
        };
        const auto finish_item_context = [&]() {
            OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_dispatch_pending(new_game));
            OL_CHECK(new_game.render());
            new_game.finish_presented_tick();
            OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_item_dispatch_pending(new_game));
        };
        const auto finish_item_stage = [&]() {
            OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_item_stage_presented(new_game));
            OL_CHECK(new_game.render());
            new_game.finish_presented_tick();
            OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_stage_presented(new_game));
        };
        prepare_item(197, 1);
        auto& equipment_item = item_ranger->items[197U];
        equipment_item.set_word(model::item_word::equipment_type, 0);
        equipment_item.set_word(model::item_word::show_introduction, 0);
        for (std::size_t index = 0U; index < model::item_word::name_bytes; ++index) {
            equipment_item.bytes[model::item_word::name_byte + index] = 0U;
            equipment_item.bytes[2U * model::item_word::secondary_name_begin + index] = 0U;
        }
        constexpr std::array<std::uint8_t, 7U> primary_name{'P', 'R', 'I', 'M', 'A', 'R', 'Y'};
        constexpr std::array<std::uint8_t, 6U> secondary_name{'S', 'E', 'C', 'O', 'N', 'D'};
        std::copy(
            primary_name.begin(),
            primary_name.end(),
            equipment_item.bytes.begin() + model::item_word::name_byte);
        std::copy(
            secondary_name.begin(),
            secondary_name.end(),
            equipment_item.bytes.begin() + 2U * model::item_word::secondary_name_begin);
        item_ranger->header.set_inventory(0U, model::ItemId{197}, 1);
        enter_items();
        new_game.handle_key(0x9CU, false, false);
        OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 1U);
        OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
        new_game.handle_key(0x9AU, false, false);
        OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 1U);
        OL_CHECK(new_game.render());
        new_game.finish_presented_tick();
        new_game.handle_key(0x9AU, false, false);
        OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_selection(new_game) == 0U);
        OL_CHECK(new_game.render());
        new_game.finish_presented_tick();
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_item_dispatch_pending(new_game));
        item_ranger->items[197U].set_word(model::item_word::show_introduction, 1);
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::equipment_begin) != 197);
        OL_CHECK(new_game.render());
        const auto equipment_target_hash = fnv1a64(new_game.framebuffer().pixels());
        if (equipment_target_hash != 0x2B9F1FD010E349DAULL) {
            std::cerr << "equipment_target_hash=0x" << std::hex << equipment_target_hash
                      << std::dec << '\n';
        }
        OL_CHECK(equipment_target_hash == 0x2B9F1FD010E349DAULL);
        new_game.finish_presented_tick();
        OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_stage_presented(new_game));
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::equipment_begin) == 197);
        OL_CHECK(item_ranger->items[197U].word(model::item_word::user) == 0);
        OL_CHECK(item_ranger->header.inventory_count(0U) == 1);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);

        prepare_item(198, 2);
        item_ranger->items[198U].set_word(model::item_word::magic_id, -1);
        item_ranger->header.set_inventory(0U, model::ItemId{198}, 1);
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        OL_CHECK(new_game.render());
        const auto practice_target_hash = fnv1a64(new_game.framebuffer().pixels());
        if (practice_target_hash != 0xF917886C548FC85CULL) {
            std::cerr << "practice_target_hash=0x" << std::hex << practice_target_hash
                      << std::dec << '\n';
        }
        OL_CHECK(practice_target_hash == 0xF917886C548FC85CULL);
        new_game.finish_presented_tick();
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::practice_item) == 198);
        OL_CHECK(item_ranger->items[198U].word(model::item_word::user) == 0);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);

        item_ranger->items[198U].set_word(model::item_word::user, 1);
        item_ranger->roles[0U].set_word(model::role_word::practice_item, -1);
        item_ranger->roles[1U].set_word(model::role_word::practice_item, 198);
        item_ranger->roles[1U].set_word(model::role_word::item_experience, 9);
        item_ranger->header.set_inventory(0U, model::ItemId{198}, 1);
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        finish_item_stage();
        new_game.handle_key('N', false, false);
        OL_CHECK(item_ranger->items[198U].word(model::item_word::user) == 1);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        new_game.handle_key('Y', false, false);
        OL_CHECK(item_ranger->items[198U].word(model::item_word::user) == 1);
        finish_item_stage();
        new_game.handle_key('Y', false, false);
        OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_item_stage_presented(new_game));
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(item_ranger->items[198U].word(model::item_word::user) == 1);
        finish_item_stage();
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(item_ranger->items[198U].word(model::item_word::user) == 0);
        OL_CHECK(item_ranger->roles[1U].word(model::role_word::practice_item) == -1);
        OL_CHECK(item_ranger->roles[1U].word(model::role_word::item_experience) == 0);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);

        prepare_item(195, 2);
        item_ranger->items[195U].set_word(model::item_word::magic_id, 77);
        item_ranger->header.set_inventory(0U, model::ItemId{195}, 1);
        for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
            item_ranger->roles[0U].set_word(
                model::role_word::magic_id_begin + slot,
                static_cast<std::int16_t>(slot + 1U));
        }
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        finish_item_stage();
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(new_game.render());
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::practice_item) != 195);
        new_game.finish_presented_tick();
        new_game.handle_key('A', false, false);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);

        prepare_item(194, 1);
        item_ranger->items[194U].set_word(model::item_word::only_suitable_role, 1);
        item_ranger->header.set_inventory(0U, model::ItemId{194}, 1);
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        finish_item_stage();
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(new_game.render());
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::equipment_begin) == 197);
        new_game.finish_presented_tick();
        new_game.handle_key('A', false, false);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);

        prepare_item(196, 2);
        item_ranger->items[196U].set_word(model::item_word::id, 93);
        item_ranger->items[196U].set_word(model::item_word::magic_id, -1);
        item_ranger->roles[0U].set_word(model::role_word::sexual, 0);
        item_ranger->header.set_inventory(0U, model::ItemId{196}, 1);
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        finish_item_stage();
        new_game.handle_key(0x0DU, false, false);
        finish_item_stage();
        new_game.handle_key('N', false, false);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::sexual) == 0);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        finish_item_stage();
        new_game.handle_key(0x0DU, false, false);
        new_game.handle_key('Y', false, false);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::sexual) == 0);
        finish_item_stage();
        new_game.handle_key('Y', false, false);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::sexual) == 2);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::practice_item) == 93);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);

        prepare_item(199, 3);
        for (std::size_t field = model::item_word::add_hp;
             field <= model::item_word::add_attack_with_poison;
             ++field) {
            item_ranger->items[199U].set_word(field, 0);
        }
        item_ranger->items[199U].set_word(model::item_word::add_physical_power, 10);
        item_ranger->roles[0U].set_word(model::role_word::physical_power, 50);
        item_ranger->header.set_inventory(0U, model::ItemId{199}, 1);
        enter_items();
        new_game.handle_key(0x0DU, false, false);
        finish_item_context();
        OL_CHECK(new_game.render());
        const auto consumable_target_hash = fnv1a64(new_game.framebuffer().pixels());
        if (consumable_target_hash != 0x939B6C51B64FD987ULL) {
            std::cerr << "consumable_target_hash=0x" << std::hex << consumable_target_hash
                      << std::dec << '\n';
        }
        OL_CHECK(consumable_target_hash == 0x939B6C51B64FD987ULL);
        new_game.finish_presented_tick();
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(item_ranger->roles[0U].word(model::role_word::physical_power) == 60);
        OL_CHECK(item_ranger->header.inventory_item(0U).value == 199);
        new_game.handle_key('A', false, false);
        OL_CHECK(item_ranger->header.inventory_item(0U).value == 199);
        OL_CHECK(new_game.render());
        new_game.finish_presented_tick();
        OL_CHECK(item_ranger->header.inventory_item(0U).value != 199);
        new_game.handle_key('A', false, false);
        new_game.handle_key(0x1BU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::world);
    }
    OL_CHECK(new_game.render());
    const auto world_palette_before_scene = new_game.framebuffer().palette();
    new_game.finish_presented_tick();

    new_game.handle_world_input(true, false, false, false);
    new_game.advance();
    OL_CHECK(new_game.scene_request().has_value());
    OL_CHECK(new_game.scene_request().value_or(-1) == 70);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    finish_world_scene_transition(new_game);
    finish_scene_entry(new_game);
    new_game.handle_world_input(false, true, false, false);
    new_game.set_scene_input_states(false, true, true);
    OL_CHECK(
        (app::LegacyGameRuntimeTestAccess::scene_input_requests(new_game) ==
         std::array<bool, 3>{false, false, false}));
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    new_game.set_scene_input_states(false, true, true);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    const auto scene_background = std::vector<std::uint8_t>{
        new_game.framebuffer().pixels().begin(), new_game.framebuffer().pixels().end()};
    OL_CHECK(new_game.render());
    const auto scene_menu_background = std::vector<std::uint8_t>{
        new_game.framebuffer().pixels().begin(), new_game.framebuffer().pixels().end()};
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    OL_CHECK(new_game.render());
    const auto scene_status_menu_background = std::vector<std::uint8_t>{
        new_game.framebuffer().pixels().begin(), new_game.framebuffer().pixels().end()};
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    bool selector_preserved_scene_background = true;
    for (int y = 0; y < render::IndexedFramebuffer::height; ++y) {
        for (int x = 0; x < render::IndexedFramebuffer::width; ++x) {
            const auto inside_title = x >= 70 && x < 194 && y >= 18 && y < 44;
            const auto inside_list = x >= 70 && x < 132 && y >= 45 && y < 75;
            const auto inside_player = x >= 120 && x < 171 && y >= 60 && y < 126;
            if (!inside_title && !inside_list && !inside_player &&
                new_game.framebuffer().row(y)[x] !=
                    scene_status_menu_background[static_cast<std::size_t>(
                        y * render::IndexedFramebuffer::width + x)]) {
                selector_preserved_scene_background = false;
            }
        }
    }
    OL_CHECK(selector_preserved_scene_background);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    bool first_page_preserved_scene_background = true;
    for (int y = 0; y < render::IndexedFramebuffer::height; ++y) {
        for (int x = 0; x < render::IndexedFramebuffer::width; ++x) {
            if ((x < 55 || x >= 265) &&
                new_game.framebuffer().row(y)[x] !=
                    scene_background[static_cast<std::size_t>(
                        y * render::IndexedFramebuffer::width + x)]) {
                first_page_preserved_scene_background = false;
            }
        }
    }
    OL_CHECK(first_page_preserved_scene_background);
    new_game.handle_key('A', false, false);
    OL_CHECK(new_game.render());
    bool second_page_preserved_scene_background = true;
    for (int y = 0; y < render::IndexedFramebuffer::height; ++y) {
        for (int x = 0; x < render::IndexedFramebuffer::width; ++x) {
            if ((x < 55 || x >= 265) &&
                new_game.framebuffer().row(y)[x] !=
                    scene_background[static_cast<std::size_t>(
                        y * render::IndexedFramebuffer::width + x)]) {
                second_page_preserved_scene_background = false;
            }
        }
    }
    OL_CHECK(second_page_preserved_scene_background);
    new_game.handle_key('A', false, false);
    auto* scene_menu_ranger = const_cast<model::GameState&>(new_game.game_state()).ranger();
    OL_CHECK(scene_menu_ranger != nullptr);
    if (scene_menu_ranger != nullptr) {
        scene_menu_ranger->roles[0U].set_word(model::role_word::medicine, 80);
    }
    new_game.handle_key(0x9EU, false, false);
    new_game.handle_key(0x9EU, false, false);
    new_game.handle_key(0x9EU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto scene_medicine_user_hash = fnv1a64(new_game.framebuffer().pixels());
    if (scene_medicine_user_hash != 0x083FB982207CB7F9ULL) {
        std::cerr << "scene_medicine_user_hash=0x" << std::hex << scene_medicine_user_hash
                  << std::dec << '\n';
    }
    OL_CHECK(scene_medicine_user_hash == 0x083FB982207CB7F9ULL);
    bool medicine_user_preserved_scene_background = true;
    for (int y = 0; y < render::IndexedFramebuffer::height; ++y) {
        for (int x = 0; x < render::IndexedFramebuffer::width; ++x) {
            const auto inside_medicine_panel = x >= 70 && x < 178 && y >= 18 && y < 95;
            const auto inside_player = x >= 120 && x < 171 && y >= 60 && y < 126;
            if (!inside_medicine_panel && !inside_player &&
                new_game.framebuffer().row(y)[x] !=
                    scene_menu_background[static_cast<std::size_t>(
                        y * render::IndexedFramebuffer::width + x)]) {
                medicine_user_preserved_scene_background = false;
            }
        }
    }
    OL_CHECK(medicine_user_preserved_scene_background);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto scene_medicine_target_hash = fnv1a64(new_game.framebuffer().pixels());
    if (scene_medicine_target_hash != 0xBF7FF3C96E9EAC2CULL) {
        std::cerr << "scene_medicine_target_hash=0x" << std::hex << scene_medicine_target_hash
                  << std::dec << '\n';
    }
    OL_CHECK(scene_medicine_target_hash == 0xBF7FF3C96E9EAC2CULL);
    bool medicine_target_preserved_scene_background = true;
    for (int y = 0; y < render::IndexedFramebuffer::height; ++y) {
        for (int x = 0; x < render::IndexedFramebuffer::width; ++x) {
            const auto inside_medicine_panel = x >= 70 && x < 188 && y >= 18 && y < 95;
            const auto inside_player = x >= 120 && x < 171 && y >= 60 && y < 126;
            if (!inside_medicine_panel && !inside_player &&
                new_game.framebuffer().row(y)[x] !=
                    scene_menu_background[static_cast<std::size_t>(
                        y * render::IndexedFramebuffer::width + x)]) {
                medicine_target_preserved_scene_background = false;
            }
        }
    }
    OL_CHECK(medicine_target_preserved_scene_background);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == scene_medicine_user_hash);
    new_game.handle_key(0x1BU, false, false);
    auto* scene_snapshot =
        const_cast<model::GameState&>(new_game.game_state()).snapshot();
    OL_CHECK(scene_snapshot != nullptr);
    if (scene_snapshot != nullptr) {
        OL_CHECK(scene_snapshot->ranger.header.word(model::header_word::sub_map_x) == 44);
        OL_CHECK(scene_snapshot->ranger.header.word(model::header_word::sub_map_y) == 29);
        OL_CHECK(scene_snapshot->ranger.header.word(model::header_word::face_towards) ==
                 static_cast<std::int16_t>(scene::SceneDirection::up));
        scene_snapshot->ranger.header.set_inventory(0U, model::ItemId{199}, 1);
        scene_snapshot->ranger.items[199].set_word(model::item_word::item_type, 0);
        OL_CHECK(scene_snapshot->set_scene_value(
            70U, model::SceneLayer::event_index, 28U * 64U + 44U, -1));
    }
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
    OL_CHECK(new_game.render());
    new_game.finish_presented_tick();
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_dispatch_pending(new_game));
    OL_CHECK(new_game.render());
    new_game.finish_presented_tick();
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    OL_CHECK(new_game.render());
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    new_game.handle_world_input(false, false, true, false);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    new_game.handle_world_input(false, false, false, true);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    advance_rendered_frames(new_game, 64U);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    finish_world_scene_return(new_game);
    OL_CHECK(!new_game.scene_request().has_value());
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::in_sub_map) == 0);
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::face_towards) ==
             static_cast<std::int16_t>(world::WorldDirection::right));
    OL_CHECK(new_game.render());
    auto world_palette_preserved = true;
    for (std::size_t index = 0U; index < world_palette_before_scene.size(); ++index) {
        const auto before = world_palette_before_scene[index];
        const auto after = new_game.framebuffer().palette()[index];
        if (before.red != after.red || before.green != after.green || before.blue != after.blue) {
            world_palette_preserved = false;
            break;
        }
    }
    OL_CHECK(world_palette_preserved);

    auto* world_item_snapshot =
        const_cast<model::GameState&>(new_game.game_state()).snapshot();
    OL_CHECK(world_item_snapshot != nullptr);
    std::optional<std::size_t> world_item_target;
    if (world_item_snapshot != nullptr) {
        const auto x = world_item_snapshot->ranger.header.word(model::header_word::sub_map_x);
        const auto y = world_item_snapshot->ranger.header.word(model::header_word::sub_map_y);
        const auto direction = world_item_snapshot->ranger.header.word(
            model::header_word::face_towards);
        constexpr std::array<std::pair<int, int>, 4> deltas{
            std::pair{0, -1}, std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}};
        OL_CHECK(direction >= 0 && static_cast<std::size_t>(direction) < deltas.size());
        const auto [delta_x, delta_y] = deltas[static_cast<std::size_t>(direction)];
        world_item_target = static_cast<std::size_t>((y + delta_y) * 64 + x + delta_x);
        OL_CHECK(world_item_snapshot->set_scene_value(
            70U, model::SceneLayer::event_index, *world_item_target, -1));
    }
    const auto select_world_event_item = [&new_game]() {
        OL_CHECK(
            new_game.handle_key(0x1BU, false, false) ==
            app::LegacyKeyStateReset::none);
        OL_CHECK(
            new_game.handle_key(0x98U, false, false) ==
            app::LegacyKeyStateReset::down_translated);
        OL_CHECK(
            new_game.handle_key(0x98U, false, false) ==
            app::LegacyKeyStateReset::down_translated);
        OL_CHECK(
            new_game.handle_key(0x0DU, false, false) ==
            app::LegacyKeyStateReset::confirmation_group);
        OL_CHECK(!app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
        OL_CHECK(new_game.render());
        new_game.finish_presented_tick();
        OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_items_presented(new_game));
        new_game.handle_key(0x0DU, false, false);
        OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
        OL_CHECK(app::LegacyGameRuntimeTestAccess::game_menu_item_dispatch_pending(new_game));
        OL_CHECK(new_game.render());
        new_game.finish_presented_tick();
    };
    select_world_event_item();
    advance_rendered_frames(new_game, 1U);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    OL_CHECK(world_item_snapshot != nullptr && world_item_target.has_value());
    if (world_item_snapshot != nullptr && world_item_target.has_value()) {
        OL_CHECK(world_item_snapshot->set_scene_value(
            70U, model::SceneLayer::event_index, *world_item_target, 199));
        OL_CHECK(world_item_snapshot->set_event_value(
            70U, 199U, model::SceneEventField::event_2, 0));
    }
    select_world_event_item();
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    if (world_item_snapshot != nullptr) {
        OL_CHECK(world_item_snapshot->set_event_value(
            70U, 199U, model::SceneEventField::event_2, 825));
    }
    select_world_event_item();
    advance_rendered_frames(new_game, 1U);
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    new_game.handle_key(0x0DU, false, false);
    advance_rendered_frames(new_game, 1U);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    auto* menu_ranger = const_cast<model::GameState&>(new_game.game_state()).ranger();
    OL_CHECK(menu_ranger != nullptr);
    if (menu_ranger != nullptr) {
        auto& role = menu_ranger->roles[0U];
        role.set_word(model::role_word::medicine, 80);
        role.set_word(model::role_word::detoxification, 80);
        role.set_word(model::role_word::physical_power, 100);
        role.set_word(model::role_word::hp, 20);
        role.set_word(model::role_word::maximum_hp, 100);
        role.set_word(model::role_word::hurt, 40);
        role.set_word(model::role_word::poison, 90);
    }
    new_game.handle_key(0x1BU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto medicine_user_hash = fnv1a64(new_game.framebuffer().pixels());
    if (medicine_user_hash != 0x7B612C5C9F89319CULL) {
        std::cerr << "medicine_user_hash=0x" << std::hex << medicine_user_hash << std::dec << '\n';
    }
    OL_CHECK(medicine_user_hash == 0x7B612C5C9F89319CULL);
    OL_CHECK(
        new_game.handle_key(0x98U, false, false) ==
        app::LegacyKeyStateReset::translated);
    OL_CHECK(
        new_game.handle_key(0x9EU, false, false) ==
        app::LegacyKeyStateReset::translated);
    OL_CHECK(
        new_game.handle_key(0x1BU, false, false) ==
        app::LegacyKeyStateReset::translated);
    OL_CHECK(
        new_game.handle_key(0x0DU, false, false) ==
        app::LegacyKeyStateReset::confirmation_group);
    OL_CHECK(
        new_game.handle_key(0x0DU, false, false) ==
        app::LegacyKeyStateReset::confirmation_group);
    OL_CHECK(new_game.render());
    const auto medicine_target_hash = fnv1a64(new_game.framebuffer().pixels());
    if (medicine_target_hash != 0x4B54FD702A1F4713ULL) {
        std::cerr << "medicine_target_hash=0x" << std::hex << medicine_target_hash << std::dec << '\n';
    }
    OL_CHECK(medicine_target_hash == 0x4B54FD702A1F4713ULL);
    OL_CHECK(
        new_game.handle_key(0x1BU, false, false) ==
        app::LegacyKeyStateReset::translated);
    OL_CHECK(
        new_game.handle_key(0x0DU, false, false) ==
        app::LegacyKeyStateReset::confirmation_group);
    OL_CHECK(
        new_game.handle_key(0x0DU, false, false) ==
        app::LegacyKeyStateReset::confirmation_group);
    OL_CHECK(new_game.render());
    const auto medicine_result_hash = fnv1a64(new_game.framebuffer().pixels());
    if (medicine_result_hash != 0xD33F69F1F38262D3ULL) {
        std::cerr << "medicine_result_hash=0x" << std::hex << medicine_result_hash << std::dec << '\n';
    }
    OL_CHECK(medicine_result_hash == 0xD33F69F1F38262D3ULL);
    if (menu_ranger != nullptr) {
        const auto& role = menu_ranger->roles[0U];
        if (role.word(model::role_word::hp) != 80) {
            std::cerr << "medicine_hp=" << role.word(model::role_word::hp) << '\n';
        }
        OL_CHECK(role.word(model::role_word::hp) == 80);
        OL_CHECK(role.word(model::role_word::hurt) == 0);
        OL_CHECK(role.word(model::role_word::physical_power) == 98);
    }
    new_game.handle_key('A', false, false);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    new_game.handle_key(0x1BU, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto detoxification_user_hash = fnv1a64(new_game.framebuffer().pixels());
    if (detoxification_user_hash != 0x7C06C4EF87D07649ULL) {
        std::cerr << "detoxification_user_hash=0x" << std::hex << detoxification_user_hash << std::dec << '\n';
    }
    OL_CHECK(detoxification_user_hash == 0x7C06C4EF87D07649ULL);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto detoxification_target_hash = fnv1a64(new_game.framebuffer().pixels());
    if (detoxification_target_hash != 0x0531265173649B75ULL) {
        std::cerr << "detoxification_target_hash=0x" << std::hex << detoxification_target_hash
                  << std::dec << '\n';
    }
    OL_CHECK(detoxification_target_hash == 0x0531265173649B75ULL);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto detoxification_result_hash = fnv1a64(new_game.framebuffer().pixels());
    if (detoxification_result_hash != 0x8B3BE18A07042605ULL) {
        std::cerr << "detoxification_result_hash=0x" << std::hex << detoxification_result_hash
                  << std::dec << '\n';
    }
    OL_CHECK(detoxification_result_hash == 0x8B3BE18A07042605ULL);
    if (menu_ranger != nullptr) {
        if (menu_ranger->roles[0U].word(model::role_word::poison) != 59) {
            std::cerr << "detoxification_poison="
                      << menu_ranger->roles[0U].word(model::role_word::poison) << '\n';
        }
        OL_CHECK(menu_ranger->roles[0U].word(model::role_word::poison) == 59);
        menu_ranger->roles[0U].set_word(model::role_word::medicine, 0);
        menu_ranger->roles[0U].set_word(model::role_word::detoxification, 0);
    }
    new_game.handle_key('A', false, false);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    new_game.handle_key(0x1BU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto no_medicine_user_hash = fnv1a64(new_game.framebuffer().pixels());
    if (no_medicine_user_hash != 0x684778248F170DEAULL) {
        std::cerr << "no_medicine_user_hash=0x" << std::hex << no_medicine_user_hash << std::dec << '\n';
    }
    OL_CHECK(no_medicine_user_hash == 0x684778248F170DEAULL);
    new_game.handle_key('A', false, false);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    new_game.handle_key(0x1BU, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    const auto no_detoxification_user_hash = fnv1a64(new_game.framebuffer().pixels());
    if (no_detoxification_user_hash != 0x83D570B0E820310EULL) {
        std::cerr << "no_detoxification_user_hash=0x" << std::hex
                  << no_detoxification_user_hash << std::dec << '\n';
    }
    OL_CHECK(no_detoxification_user_hash == 0x83D570B0E820310EULL);
    new_game.handle_key('A', false, false);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);

    // Numbered saves in the runtime data directory belong to the player and are mutable.
    app::LegacyGameRuntime load_game{continuation_fixture, 1U};
    finish_title_startup(load_game);
    load_game.handle_key(0x98U, false, false);
    load_game.handle_key(0x0DU, false, false);
    load_game.handle_key(0x0DU, false, false);
    finish_title_confirmation(load_game);
    OL_CHECK(load_game.view() == app::LegacyGameView::title);
    OL_CHECK(load_game.render());
    OL_CHECK(fnv1a64(load_game.framebuffer().pixels()) == 0x7333253CA7400DE6ULL);
    load_game.finish_presented_tick();
    load_game.advance();
    finish_numbered_load_transition(load_game, app::LegacyGameView::world);
    OL_CHECK(!load_game.take_clear_scene_exit_key_states_request());
    OL_CHECK(load_game.render());

    auto* continuation_ranger =
        const_cast<model::GameState&>(load_game.game_state()).ranger();
    OL_CHECK(continuation_ranger != nullptr);
    if (continuation_ranger != nullptr) {
        auto& role = continuation_ranger->roles[0U];
        role.set_word(model::role_word::hurt, 51);
        role.set_word(model::role_word::hp, 10);
        auto& entrance_scene = continuation_ranger->scenes[70U];
        entrance_scene.set_word(model::scene_metadata_word::entrance_condition, 1);
        for (int pair = 0; pair < 24; ++pair) {
            load_game.handle_world_input(false, false, false, true);
            load_game.advance();
            load_game.handle_world_input(true, false, false, false);
            load_game.advance();
        }
        OL_CHECK(role.word(model::role_word::hp) == 10);
        entrance_scene.set_word(model::scene_metadata_word::entrance_condition, 0);
    }
    OL_CHECK(load_game.handle_world_input(true, false, false, false));
    load_game.advance();
    OL_CHECK(load_game.view() == app::LegacyGameView::world);
    finish_world_scene_transition(load_game);
    finish_scene_entry(load_game);
    auto* continuation_snapshot =
        const_cast<model::GameState&>(load_game.game_state()).snapshot();
    OL_CHECK(continuation_snapshot != nullptr);
    if (continuation_snapshot != nullptr) {
        OL_CHECK(continuation_snapshot->set_scene_value(
            70U, model::SceneLayer::event_index, 28U * 64U + 44U, -1));
    }
    OL_CHECK(load_game.handle_world_input(false, false, true, false));
    load_game.advance();
    advance_rendered_frames(load_game, 1U);
    OL_CHECK(load_game.handle_world_input(false, false, false, true));
    load_game.advance();
    advance_rendered_frames(load_game, 1U);
    OL_CHECK(load_game.take_clear_scene_exit_key_states_request());
    OL_CHECK(!load_game.take_clear_scene_exit_key_states_request());
    advance_rendered_frames(load_game, 64U);
    OL_CHECK(load_game.view() == app::LegacyGameView::world);
    OL_CHECK(!load_game.take_clear_scene_exit_key_states_request());
    finish_world_scene_return(load_game);
    OL_CHECK(load_game.game_state().ranger()->header.word(model::header_word::main_map_x) == 357);
    OL_CHECK(load_game.game_state().ranger()->header.word(model::header_word::main_map_y) == 235);
    OL_CHECK(load_game.game_state().ranger()->header.word(model::header_word::face_towards) ==
             static_cast<std::int16_t>(world::WorldDirection::right));
    OL_CHECK(load_game.game_state().ranger()->roles[0U].word(model::role_word::hp) == 10);
    load_game.handle_world_input(false, true, false, false);
    load_game.advance();
    OL_CHECK(load_game.game_state().ranger()->roles[0U].word(model::role_word::hp) == 9);
    OL_CHECK(load_game.render());
}

void check_question_present_gate(const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-question-present-runtime";
    OL_CHECK(prepare_runtime_fixture(data_root, output_root));
    OL_CHECK(install_question_initial_script(output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);
    advance_rendered_frames(game, 66U);

    OL_CHECK(game.view() == app::LegacyGameView::scene);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::question);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_question_presented(game));

    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::question);
    OL_CHECK(game.render());
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_question_presented(game));

    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::question);
    game.finish_presented_tick();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_question_presented(game));

    game.handle_key(0x96U, false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::question);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_question(game) ==
        scene::SceneQuestion::join);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_question_presented(game));

    OL_CHECK(game.render());
    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::question);
    game.finish_presented_tick();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_question_presented(game));

    game.handle_key(0x96U, false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::present);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_question_presented(game));
    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::present);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::question);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_question(game) ==
        scene::SceneQuestion::rest);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_question_presented(game));

    OL_CHECK(game.render());
    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::question);
    game.finish_presented_tick();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_question_presented(game));
    game.handle_key(0x96U, false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::stay);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_question_presented(game));
}

void check_picture_animation_tick_gates(const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-picture-animation-runtime";
    OL_CHECK(prepare_runtime_fixture(data_root, output_root));
    OL_CHECK(install_picture_animation_initial_script(output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);
    advance_rendered_frames(game, 66U);

    OL_CHECK(game.view() == app::LegacyGameView::scene);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::present);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5002);

    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5002);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5002);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5004);

    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5004);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5006);

    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5006);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::present);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5016);

    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5016);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5016);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5018);

    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5018);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5020);

    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5020);
    OL_CHECK(game.render());
    game.advance();
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::stay);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 5020);
}

void check_three_statue_animation_tick_gates(
    const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-statue-animation-runtime";
    OL_CHECK(prepare_statue_runtime_fixture(data_root, output_root));
    OL_CHECK(install_statue_animation_initial_script(output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);
    OL_CHECK(LegacyGameRuntimeTestAccess::begin_scene_event(
        game, 14, 691, 3, 33, 15));

    const auto check_frame = [&game](const std::size_t index) {
        const auto player_picture = index < 6U
                                        ? static_cast<std::int16_t>(7664 + index * 2U)
                                        : static_cast<std::int16_t>(
                                              std::min<std::size_t>(
                                                  7688, 7676 + (index - 6U) * 2U));
        OL_CHECK(
            LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
            scene::SceneStepKind::present);
        OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == player_picture);
        const auto* snapshot = game.game_state().snapshot();
        OL_CHECK(snapshot != nullptr);
        if (snapshot == nullptr) {
            return;
        }
        std::array<std::int16_t, 3> pictures{7690, 7748, 7806};
        if (index >= 6U) {
            const auto offset = static_cast<std::int16_t>((index - 6U) * 2U);
            pictures = {
                static_cast<std::int16_t>(7690 + offset),
                static_cast<std::int16_t>(7748 + offset),
                static_cast<std::int16_t>(7806 + offset),
            };
        }
        for (std::size_t event = 0U; event < pictures.size(); ++event) {
            for (const auto field : {
                     model::SceneEventField::current_picture,
                     model::SceneEventField::end_picture,
                     model::SceneEventField::begin_picture}) {
                OL_CHECK(snapshot->event_value(14U, event + 2U, field).value_or(-1) ==
                         pictures[event]);
            }
        }
    };

    for (std::size_t frame = 0U; frame < 35U; ++frame) {
        check_frame(frame);
        game.advance();
        check_frame(frame);
        OL_CHECK(game.render());
        game.advance();
        check_frame(frame);
        OL_CHECK(game.render());
        game.advance();
    }
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::stay);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == 7688);
    const auto* snapshot = game.game_state().snapshot();
    OL_CHECK(snapshot != nullptr);
    if (snapshot != nullptr) {
        constexpr std::array<std::int16_t, 3> final_pictures{7746, 7804, 7862};
        for (std::size_t event = 0U; event < final_pictures.size(); ++event) {
            for (const auto field : {
                     model::SceneEventField::current_picture,
                     model::SceneEventField::end_picture,
                     model::SceneEventField::begin_picture}) {
                OL_CHECK(snapshot->event_value(14U, event + 2U, field).value_or(-1) ==
                         final_pictures[event]);
            }
        }
    }
}

void check_ending_prelude_animation_tick_gates(
    const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-ending-prelude-runtime";
    OL_CHECK(prepare_ending_prelude_runtime_fixture(data_root, output_root));
    OL_CHECK(install_ending_prelude_initial_script(output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);
    OL_CHECK(LegacyGameRuntimeTestAccess::begin_scene_event(
        game, 83, 691, 1, 23, 41));

    const auto check_frame = [&game](const std::size_t index) {
        OL_CHECK(
            LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
            scene::SceneStepKind::present);
        OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == -86);
        const auto* snapshot = game.game_state().snapshot();
        OL_CHECK(snapshot != nullptr);
        if (snapshot == nullptr) {
            return;
        }
        const std::array<std::int16_t, 2> pictures{
            static_cast<std::int16_t>(8054 + index * 2U),
            static_cast<std::int16_t>(8130 + index * 2U),
        };
        for (std::size_t event = 0U; event < pictures.size(); ++event) {
            for (const auto field : {
                     model::SceneEventField::current_picture,
                     model::SceneEventField::end_picture,
                     model::SceneEventField::begin_picture}) {
                OL_CHECK(snapshot->event_value(83U, event, field).value_or(-1) ==
                         pictures[event]);
            }
        }
    };

    for (std::size_t frame = 0U; frame < 38U; ++frame) {
        check_frame(frame);
        game.advance();
        check_frame(frame);
        OL_CHECK(game.render());
        game.advance();
        check_frame(frame);
        OL_CHECK(game.render());
        game.advance();
    }
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::fade_to_black);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_player_frame(game) == -86);
}

void check_shop_input_present_gate(const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-shop-input-runtime";
    OL_CHECK(prepare_shop_runtime_fixture(data_root, output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);

    const auto open_shop = [&game]() {
        OL_CHECK(LegacyGameRuntimeTestAccess::begin_scene_event(
            game, 1, 938, 17, 0, 0));
        OL_CHECK(
            LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
            scene::SceneStepKind::dialogue);
        OL_CHECK(
            game.handle_key(0x0DU, false, false) == app::LegacyKeyStateReset::none);
        OL_CHECK(
            LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
            scene::SceneStepKind::shop);
        OL_CHECK(!LegacyGameRuntimeTestAccess::scene_shop_presented(game));
    };
    const auto present_shop = [&game]() {
        OL_CHECK(game.render());
        OL_CHECK(!LegacyGameRuntimeTestAccess::scene_shop_presented(game));
        game.finish_presented_tick();
        OL_CHECK(LegacyGameRuntimeTestAccess::scene_shop_presented(game));
    };

    open_shop();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    OL_CHECK(
        game.handle_key(0x98U, false, false) == app::LegacyKeyStateReset::none);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    present_shop();
    OL_CHECK(game.handle_key('1', false, false) == app::LegacyKeyStateReset::none);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_shop_presented(game));
    OL_CHECK(
        game.handle_key(0x9EU, false, false) == app::LegacyKeyStateReset::none);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    present_shop();
    OL_CHECK(
        game.handle_key(0x98U, false, false) == app::LegacyKeyStateReset::translated);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 1);
    present_shop();
    OL_CHECK(
        game.handle_key(0x9EU, false, false) == app::LegacyKeyStateReset::translated);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    present_shop();
    OL_CHECK(
        game.handle_key(0x9EU, false, false) == app::LegacyKeyStateReset::translated);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 4);
    present_shop();
    OL_CHECK(
        game.handle_key(0x98U, false, false) == app::LegacyKeyStateReset::translated);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    present_shop();
    OL_CHECK(
        game.handle_key(0x1BU, false, false) == app::LegacyKeyStateReset::translated);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::stay);

    auto* ranger = const_cast<model::GameState&>(game.game_state()).ranger();
    OL_CHECK(ranger != nullptr);
    if (ranger == nullptr) {
        return;
    }
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        if (ranger->header.inventory_item(slot).value == 174) {
            ranger->header.set_inventory(slot, model::ItemId{174}, 0);
        }
    }
    for (const auto key : std::array<std::uint8_t, 3>{0x0DU, 0x20U, 0x96U}) {
        open_shop();
        present_shop();
        OL_CHECK(
            game.handle_key(key, false, false) ==
            app::LegacyKeyStateReset::confirmation_group);
        OL_CHECK(
            LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
            scene::SceneStepKind::present);
    }

    auto& shop = ranger->shops[0U];
    for (std::size_t slot = 0U; slot < model::shop_word::item_count; ++slot) {
        shop.set_word(model::shop_word::total_begin + slot, 0);
    }
    open_shop();
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == -1);
    present_shop();
    OL_CHECK(
        game.handle_key(0x98U, false, false) == app::LegacyKeyStateReset::translated);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == -1);
    present_shop();
    OL_CHECK(
        game.handle_key(0x9EU, false, false) == app::LegacyKeyStateReset::translated);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == -1);
    present_shop();
    OL_CHECK(
        game.handle_key(0x1BU, false, false) == app::LegacyKeyStateReset::translated);
}

void check_death_menu_present_gate(const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-death-menu-present-runtime";
    OL_CHECK(prepare_runtime_fixture(data_root, output_root));
    OL_CHECK(install_death_menu_initial_script(output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);
    advance_rendered_frames(game, 66U);

    for (int frame = 0;
         frame < 128 &&
         LegacyGameRuntimeTestAccess::scene_pending_kind(game) !=
             scene::SceneStepKind::death_menu;
         ++frame) {
        OL_CHECK(game.render());
        game.finish_presented_tick();
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::scene);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::death_menu);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);

    game.handle_key(0x98U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    OL_CHECK(game.render());
    game.handle_key(0x98U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    game.finish_presented_tick();
    game.handle_key('A', false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);
    game.handle_key(0x98U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 0);

    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.handle_key(0x98U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 1);
    game.handle_key(0x98U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 1);

    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.handle_key(0x98U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 2);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.handle_key(0x98U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_menu_index(game) == 3);
    game.handle_key(0x20U, false, false);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_pending_death_confirm(game));

    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.handle_key(0x20U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_death_confirm(game));
    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::death_menu);

    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.handle_key('y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::death_menu);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_pending_death_confirm(game));
    game.handle_key(0x96U, false, false);
    OL_CHECK(!LegacyGameRuntimeTestAccess::scene_pending_death_confirm(game));

    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.handle_key(0x96U, false, false);
    OL_CHECK(LegacyGameRuntimeTestAccess::scene_pending_death_confirm(game));
    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::death_menu);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.handle_key('Y', false, false);
    OL_CHECK(
        LegacyGameRuntimeTestAccess::scene_pending_kind(game) ==
        scene::SceneStepKind::quit);
}

void check_battle_party_selection_input_timing(
    const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;
    using battle::BattleSessionPhase;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-battle-party-input";
    OL_CHECK(prepare_battle_runtime_fixture(data_root, output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    const auto baseline = persistence::load_baseline(output_root);
    OL_CHECK(static_cast<bool>(baseline));
    if (!baseline) {
        return;
    }
    auto& game_state = const_cast<model::GameState&>(game.game_state());
    OL_CHECK(game_state.import_snapshot(*baseline.snapshot));
    auto* ranger = game_state.ranger();
    OL_CHECK(ranger != nullptr);
    if (ranger == nullptr) {
        return;
    }
    ranger->header.set_team_member(0U, model::CharacterId{0});
    for (std::size_t slot = 1U; slot < model::kTeamMemberCount; ++slot) {
        ranger->header.set_team_member(slot, model::CharacterId{-1});
    }
    OL_CHECK(LegacyGameRuntimeTestAccess::begin_battle_in_scene(game, 0));
    auto* session = LegacyGameRuntimeTestAccess::battle_session(game);
    OL_CHECK(session != nullptr);
    if (session == nullptr) {
        return;
    }
    OL_CHECK(session->phase() == BattleSessionPhase::party_selection);
    OL_CHECK(session->setup().party_prefix_length() == 1U);
    OL_CHECK(session->setup().cursor() == 0U);
    OL_CHECK(game.battle_menu_uses_key_states());

    game.set_battle_menu_direction_states(true, true);
    game.finish_presented_tick();
    OL_CHECK(session->setup().cursor() == 0U);
    OL_CHECK(game.take_clear_battle_menu_direction_request() == 0U);

    OL_CHECK(session->render(game.framebuffer(), true));
    game.finish_presented_tick();
    OL_CHECK(session->setup().cursor() == 1U);
    OL_CHECK(game.take_clear_battle_menu_direction_request() == 0x98U);
    game.finish_presented_tick();
    OL_CHECK(session->setup().cursor() == 1U);

    game.set_battle_menu_direction_states(false, true);
    OL_CHECK(session->render(game.framebuffer(), true));
    game.finish_presented_tick();
    OL_CHECK(session->setup().cursor() == 0U);
    OL_CHECK(game.take_clear_battle_menu_direction_request() == 0x9EU);
    game.set_battle_menu_direction_states(false, false);

    game.set_battle_confirmation_state(true);
    game.finish_presented_tick();
    OL_CHECK(!game.take_clear_battle_confirmation_states_request());
    OL_CHECK(session->render(game.framebuffer(), true));
    game.finish_presented_tick();
    OL_CHECK(game.take_clear_battle_confirmation_states_request());
    OL_CHECK(session->phase() == BattleSessionPhase::party_selection);
    OL_CHECK(session->setup().selection_states()[0U] == 2);
    game.set_battle_confirmation_state(false);

    game.set_battle_menu_direction_states(true, false);
    OL_CHECK(session->render(game.framebuffer(), true));
    game.finish_presented_tick();
    OL_CHECK(session->setup().cursor() == 1U);
    OL_CHECK(game.take_clear_battle_menu_direction_request() == 0x98U);
    game.set_battle_menu_direction_states(false, false);
    game.set_battle_confirmation_state(true);
    OL_CHECK(session->render(game.framebuffer(), true));
    game.finish_presented_tick();
    OL_CHECK(game.take_clear_battle_confirmation_states_request());
    OL_CHECK(session->phase() == BattleSessionPhase::initial_fade_to_black);
}

void check_battle_runtime_transitions(const std::filesystem::path& data_root) {
    using namespace openlegend;
    using app::LegacyGameRuntimeTestAccess;
    using battle::BattlePlayerAction;
    using battle::BattleSessionPhase;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b7-battle-entry-runtime";
    OL_CHECK(prepare_battle_runtime_fixture(data_root, output_root));
    OL_CHECK(install_battle_initial_script(output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);

    auto* ranger = const_cast<model::GameState&>(game.game_state()).ranger();
    OL_CHECK(ranger != nullptr);
    if (ranger == nullptr) {
        return;
    }
    ranger->roles[1U].set_word(model::role_word::hp, 100);
    ranger->roles[1U].set_word(model::role_word::maximum_hp, 100);
    ranger->roles[1U].set_word(model::role_word::speed, 100);
    ranger->roles[3U].set_word(model::role_word::hp, 0);
    ranger->roles[3U].set_word(model::role_word::maximum_hp, 100);
    ranger->roles[3U].set_word(model::role_word::speed, 0);
    const auto scene_music = std::max<std::int16_t>(
        ranger->scenes[70U].word(model::scene_metadata_word::entrance_music), 0);
    static_cast<void>(game.take_scene_audio_commands());

    std::uint64_t frozen_scene_hash = 0U;
    for (std::size_t frame = 0U; frame < 66U; ++frame) {
        OL_CHECK(game.view() == app::LegacyGameView::scene);
        OL_CHECK(game.render());
        if (frame == 65U) {
            frozen_scene_hash = fnv1a64(game.framebuffer().pixels());
            OL_CHECK(std::ranges::any_of(
                game.framebuffer().palette(),
                [](const auto& color) {
                    return color.red != 0U || color.green != 0U || color.blue != 0U;
                }));
        }
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::battle);
    OL_CHECK(game.battle_request().value_or(-1) == 4);
    auto* session = LegacyGameRuntimeTestAccess::battle_session(game);
    OL_CHECK(session != nullptr);
    OL_CHECK(session != nullptr && !session->grants_experience());
    OL_CHECK(session != nullptr &&
             session->phase() == BattleSessionPhase::initial_fade_to_black);
    OL_CHECK(game.handle_key(0x0DU, false, false) == app::LegacyKeyStateReset::none);

    for (std::size_t frame = 0U; frame < 64U; ++frame) {
        OL_CHECK(game.view() == app::LegacyGameView::battle);
        OL_CHECK(game.render());
        OL_CHECK(fnv1a64(game.framebuffer().pixels()) == frozen_scene_hash);
        if (frame == 63U) {
            OL_CHECK(std::ranges::all_of(
                game.framebuffer().palette(),
                [](const auto& color) {
                    return color.red == 0U && color.green == 0U && color.blue == 0U;
                }));
        }
        game.advance(100U);
    }
    session = LegacyGameRuntimeTestAccess::battle_session(game);
    OL_CHECK(session != nullptr);
    OL_CHECK(session != nullptr && session->phase() == BattleSessionPhase::initial_present);
    OL_CHECK(game.take_scene_audio_commands().empty());

    OL_CHECK(game.render());
    OL_CHECK(std::ranges::all_of(
        game.framebuffer().palette(),
        [](const auto& color) {
            return color.red == 0U && color.green == 0U && color.blue == 0U;
        }));
    game.finish_presented_tick(100U);
    const auto battle_music = game.take_scene_audio_commands();
    OL_CHECK((battle_music == std::vector<scene::SceneAudioCommand>{
        {scene::SceneAudioCommand::Kind::music, 7, false}}));
    OL_CHECK(session != nullptr && session->phase() == BattleSessionPhase::initial_fade);
    if (session == nullptr) {
        return;
    }
    OL_CHECK(session->fade_frame_count() == 66U);
    for (std::size_t frame = 0U; frame < session->fade_frame_count(); ++frame) {
        OL_CHECK(game.render());
        if (frame <= 1U) {
            OL_CHECK(std::ranges::all_of(
                game.framebuffer().palette(),
                [](const auto& color) {
                    return color.red == 0U && color.green == 0U && color.blue == 0U;
                }));
        }
        game.finish_presented_tick(100U);
    }
    OL_CHECK(session->phase() == BattleSessionPhase::round_start);

    session->setup().enable_automatic_mode();
    game.set_battle_confirmation_state(true);
    OL_CHECK(!game.take_clear_battle_confirmation_states_request());
    game.advance(100U);
    OL_CHECK(!session->setup().automatic_enabled());
    OL_CHECK(game.take_clear_battle_confirmation_states_request());
    OL_CHECK(!game.take_clear_battle_confirmation_states_request());
    game.set_battle_confirmation_state(false);
    OL_CHECK(session->phase() == BattleSessionPhase::actor_present);
    OL_CHECK(game.needs_immediate_frame(100U));
    OL_CHECK(!game.battle_menu_uses_key_states());
    OL_CHECK(game.render());
    game.finish_presented_tick(100U);
    OL_CHECK(session->phase() == BattleSessionPhase::player_action_initial_present);
    OL_CHECK(game.needs_immediate_frame(100U));
    OL_CHECK(game.battle_menu_uses_key_states());
    OL_CHECK(game.render());
    game.finish_presented_tick(100U);
    OL_CHECK(session->phase() == BattleSessionPhase::player_action);
    OL_CHECK(!game.needs_immediate_frame(100U));
    OL_CHECK(game.battle_menu_uses_key_states());
    OL_CHECK(game.render());
    game.finish_presented_tick(100U);
    OL_CHECK(session->phase() == BattleSessionPhase::player_action);
    std::size_t wait_ordinal = 0U;
    for (std::size_t action = 0U;
         action < static_cast<std::size_t>(BattlePlayerAction::wait);
         ++action) {
        if (session->player_action_menu().available[action] == 1) {
            ++wait_ordinal;
        }
    }
    while (session->player_action_menu().cursor != wait_ordinal) {
        game.set_battle_menu_direction_states(true, false);
        OL_CHECK(game.render());
        game.finish_presented_tick(100U);
        OL_CHECK(game.take_clear_battle_menu_direction_request() == 0x98U);
        game.set_battle_menu_direction_states(false, false);
    }
    game.set_battle_confirmation_state(true);
    OL_CHECK(game.render());
    game.finish_presented_tick(100U);
    OL_CHECK(game.take_clear_battle_confirmation_states_request());
    game.set_battle_confirmation_state(false);
    OL_CHECK(session->phase() == BattleSessionPhase::player_action_return_present);
    OL_CHECK(game.render());
    game.finish_presented_tick(100U);
    OL_CHECK(session->phase() == BattleSessionPhase::battle_outcome);
    OL_CHECK(game.render());
    game.finish_presented_tick(100U);
    OL_CHECK(session->phase() == BattleSessionPhase::battle_outcome_wait);
    game.handle_key(0x0DU, false, false, 100U);

    for (std::size_t guard = 0U; session != nullptr && !session->finished() && guard < 64U;
         ++guard) {
        if (session->phase() == BattleSessionPhase::post_battle_message_present) {
            OL_CHECK(game.render());
            game.finish_presented_tick(100U);
        } else if (session->phase() == BattleSessionPhase::post_battle_message_wait) {
            game.handle_key(0x0DU, false, false, 100U);
        } else if (session->phase() == BattleSessionPhase::round_wait) {
            const auto last_frame = fnv1a64(game.framebuffer().pixels());
            game.advance(100U);
            OL_CHECK(session->phase() == BattleSessionPhase::round_wait);
            OL_CHECK(game.render());
            OL_CHECK(fnv1a64(game.framebuffer().pixels()) == last_frame);
            game.advance(101U);
        } else {
            OL_CHECK(false);
            break;
        }
        session = LegacyGameRuntimeTestAccess::battle_session(game);
    }
    session = LegacyGameRuntimeTestAccess::battle_session(game);
    OL_CHECK(session != nullptr && session->finished());
    OL_CHECK(game.view() == app::LegacyGameView::battle);
    OL_CHECK(game.take_scene_audio_commands().empty());
    const auto frozen_battle_hash = fnv1a64(game.framebuffer().pixels());

    for (std::size_t frame = 0U; frame < 64U; ++frame) {
        OL_CHECK(game.render());
        OL_CHECK(fnv1a64(game.framebuffer().pixels()) == frozen_battle_hash);
        OL_CHECK(game.battle_request().value_or(-1) == 4);
        if (frame == 63U) {
            OL_CHECK(std::ranges::all_of(
                game.framebuffer().palette(),
                [](const auto& color) {
                    return color.red == 0U && color.green == 0U && color.blue == 0U;
                }));
        }
        game.advance(100U);
    }
    OL_CHECK(game.view() == app::LegacyGameView::scene);
    OL_CHECK(!game.battle_request().has_value());
    OL_CHECK(LegacyGameRuntimeTestAccess::battle_session(game) == nullptr);
    OL_CHECK((game.take_scene_audio_commands() == std::vector<scene::SceneAudioCommand>{
        {scene::SceneAudioCommand::Kind::music, scene_music, false}}));
}

void check_scene_load_runtime(const std::filesystem::path& data_root) {
    using namespace openlegend;

    const auto output_root =
        test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b9-opcode24-runtime";
    OL_CHECK(prepare_runtime_fixture(data_root, output_root));
    const auto baseline = persistence::load_baseline(data_root);
    OL_CHECK(static_cast<bool>(baseline));
    if (!baseline.snapshot.has_value()) {
        return;
    }
    auto loaded_snapshot = *baseline.snapshot;
    loaded_snapshot.ranger.roles[0U].set_word(model::role_word::morality, 77);
    OL_CHECK(persistence::write_numbered_slot(
        output_root, persistence::SaveSlot::one, loaded_snapshot));
    OL_CHECK(install_opcode24_initial_script(output_root));

    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    for (int tick = 0; tick < 30; ++tick) {
        game.advance();
    }
    OL_CHECK(game.view() == app::LegacyGameView::attributes);
    game.handle_key('Y', false, false);
    finish_new_game_scene_transition(game);

    advance_rendered_frames(game, 66U);
    advance_rendered_frames(game, 65U);
    OL_CHECK(game.view() == app::LegacyGameView::scene);
    OL_CHECK(game.render());
    const auto menu_hash = fnv1a64(game.framebuffer().pixels());
    OL_CHECK(menu_hash == 0x65C8DA776EAC540FULL);
    OL_CHECK(game.game_state().ranger()->roles[0U].word(model::role_word::morality) != 77);

    game.handle_key(0x0DU, false, false);
    advance_rendered_frames(game, 64U);
    OL_CHECK(game.view() == app::LegacyGameView::scene);
    OL_CHECK(game.take_clear_scene_exit_key_states_request());
    OL_CHECK(game.render());
    OL_CHECK(fnv1a64(game.framebuffer().pixels()) == menu_hash);
    advance_rendered_frames(game, 64U);
    OL_CHECK(game.view() == app::LegacyGameView::world);
    OL_CHECK(game.game_state().ranger()->roles[0U].word(model::role_word::morality) != 77);

    finish_world_scene_return(game);
    OL_CHECK(game.render());
    OL_CHECK(fnv1a64(game.framebuffer().pixels()) == 0xDD14FCC6528CAB25ULL);
    OL_CHECK(game.game_state().ranger()->roles[0U].word(model::role_word::morality) != 77);
    finish_numbered_load_transition(game, app::LegacyGameView::world);
    OL_CHECK(game.game_state().ranger()->roles[0U].word(model::role_word::morality) == 77);
}

void check_runtime_persistence(const std::filesystem::path& data_root) {
    using namespace openlegend;

    const auto output_root = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-runtime";
    const auto save_root = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-runtime-save";
    OL_CHECK(prepare_runtime_fixture(data_root, output_root));
    std::error_code error;
    std::filesystem::remove_all(save_root, error);
    error.clear();
    std::filesystem::create_directories(save_root, error);
    OL_CHECK(!error);
    const auto baseline = persistence::load_baseline(data_root);
    OL_CHECK(static_cast<bool>(baseline));
    if (!baseline.snapshot.has_value()) {
        return;
    }
    OL_CHECK(persistence::write_numbered_slot(
        save_root, persistence::SaveSlot::one, *baseline.snapshot));
    app::LegacyGameRuntime game{output_root, save_root, 0U};
    OL_CHECK(game.valid());
    finish_title_startup(game);
    game.handle_key(0x98U, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x0DU, false, false);
    finish_title_confirmation(game);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
    finish_numbered_load_transition(game, app::LegacyGameView::world);
    game.handle_world_input(false, false, false, true);
    game.advance();
    OL_CHECK(game.view() == app::LegacyGameView::world);
    OL_CHECK(game.game_state().ranger()->header.word(model::header_word::main_map_x) == 357);
    auto* mutable_ranger = const_cast<model::GameState&>(game.game_state()).ranger();
    OL_CHECK(mutable_ranger != nullptr);
    if (mutable_ranger != nullptr) {
        mutable_ranger->roles[0].set_word(
            model::role_word::morality,
            static_cast<std::int16_t>(mutable_ranger->roles[0].word(model::role_word::morality) + 1));
    }

    game.handle_key(0x1BU, false, false);
    game.handle_key(0x9EU, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x98U, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x0DU, false, false);
    const auto before_save = game.game_state().export_snapshot();
    OL_CHECK(before_save.has_value());
    if (before_save.has_value()) {
        OL_CHECK(before_save->ranger.header.word(model::header_word::main_map_x) == 357);
    }
    game.advance();
    const auto before_wait_present =
        persistence::load_numbered_slot(save_root, persistence::SaveSlot::one);
    OL_CHECK(static_cast<bool>(before_wait_present));
    if (before_save.has_value() && before_wait_present.snapshot.has_value()) {
        OL_CHECK(*before_save != *before_wait_present.snapshot);
    }
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
    OL_CHECK(game.view() == app::LegacyGameView::game_menu);
    const auto saved =
        persistence::load_numbered_slot(save_root, persistence::SaveSlot::one);
    OL_CHECK(static_cast<bool>(saved));
    OL_CHECK(saved.snapshot.has_value());
    OL_CHECK(!std::filesystem::exists(output_root / "R1.GRP"));
    OL_CHECK(!std::filesystem::exists(output_root / "S1.GRP"));
    OL_CHECK(!std::filesystem::exists(output_root / "D1.GRP"));
    const auto after_save = game.game_state().export_snapshot();
    OL_CHECK(after_save.has_value());
    if (before_save.has_value() && saved.snapshot.has_value() && after_save.has_value()) {
        OL_CHECK(*before_save != *saved.snapshot);
        OL_CHECK(*after_save == *saved.snapshot);
        OL_CHECK(saved.snapshot->ranger.header.word(model::header_word::main_map_x) == 358);
        OL_CHECK(saved.snapshot->ranger.header.word(model::header_word::main_map_y) == 235);
        OL_CHECK(saved.snapshot->ranger.header.word(model::header_word::face_towards) ==
                 static_cast<std::int16_t>(world::WorldDirection::right));
    }

    if (mutable_ranger != nullptr) {
        mutable_ranger->roles[0].set_word(
            model::role_word::morality,
            static_cast<std::int16_t>(mutable_ranger->roles[0].word(model::role_word::morality) + 1));
    }
    game.handle_key(0x9EU, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x0DU, false, false);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
    finish_numbered_load_transition(game, app::LegacyGameView::game_menu);
    const auto loaded_behind_menu = game.game_state().export_snapshot();
    OL_CHECK(loaded_behind_menu.has_value() && saved.snapshot.has_value());
    if (loaded_behind_menu.has_value() && saved.snapshot.has_value()) {
        OL_CHECK(*loaded_behind_menu == *saved.snapshot);
    }

    game.handle_key(0x0DU, false, false);
    game.handle_key(0x0DU, false, false);
    std::filesystem::remove(save_root / "R1.GRP", error);
    OL_CHECK(!error);
    const auto before_failed_load = game.game_state().export_snapshot();
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
    OL_CHECK(game.view() == app::LegacyGameView::error);
    const auto after_failed_load = game.game_state().export_snapshot();
    OL_CHECK(before_failed_load.has_value() && after_failed_load.has_value());
    if (before_failed_load.has_value() && after_failed_load.has_value()) {
        OL_CHECK(*before_failed_load == *after_failed_load);
    }

    game.handle_key(0x0DU, false, false);
    game.handle_key(0x98U, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x98U, false, false);
    game.handle_key(0x0DU, false, false);
    std::filesystem::remove_all(save_root, error);
    OL_CHECK(!error);
    OL_CHECK(game.render());
    game.finish_presented_tick();
    game.advance();
    OL_CHECK(game.view() == app::LegacyGameView::error);
}

void check_renderer(const std::filesystem::path& data_root) {
    using namespace openlegend;

    ui::TitleMenuController menu;
    ui::TitleMenuRenderer renderer{resource::DataRoot{data_root}};
    OL_CHECK(renderer.valid());
    if (!renderer.valid()) {
        return;
    }

    render::IndexedFramebuffer framebuffer;
    constexpr std::uint64_t main_hashes[]{
        0x86690E3B3B68FE20ULL,
        0x01A9DF9F3A147C7CULL,
        0x7E4C70629A4C00D4ULL,
    };
    for (std::size_t selection = 0U; selection < 3U; ++selection) {
        OL_CHECK(renderer.render(menu, framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == main_hashes[selection]);
        static_cast<void>(menu.handle_key(0x98U));
    }

    static_cast<void>(menu.handle_key(0x98U));
    static_cast<void>(menu.handle_key(0x0DU));
    OL_CHECK(menu.screen() == ui::TitleScreen::load_slots);
    constexpr std::uint64_t load_hashes[]{
        0xC6475EB2B76D4457ULL,
        0x7D49DB972E535393ULL,
        0xB76C74065386AB63ULL,
    };
    for (std::size_t slot = 0U; slot < 3U; ++slot) {
        OL_CHECK(renderer.render(menu, framebuffer));
        OL_CHECK(fnv1a64(framebuffer.pixels()) == load_hashes[slot]);
        static_cast<void>(menu.handle_key(0x98U));
    }

    menu.show_please_wait();
    OL_CHECK(renderer.render(menu, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x7333253CA7400DE6ULL);
    OL_CHECK(renderer.render_new_game_wait(framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x7333253CA7400DE6ULL);

    ui::BasicUiRenderer basic_renderer{resource::DataRoot{data_root}};
    OL_CHECK(basic_renderer.valid());

    compat::LegacyPalette tie_palette{};
    tie_palette.fill({63U, 63U, 63U});
    tie_palette[1U] = {6U, 6U, 6U};
    tie_palette[2U] = {6U, 6U, 6U};
    tie_palette[3U] = {0U, 0U, 0U};
    framebuffer.clear(3U);
    framebuffer.set_palette(tie_palette);
    OL_CHECK(basic_renderer.render_error({}, framebuffer));
    OL_CHECK(framebuffer.row(165)[160] == 1U);

    ui::NewGameNameEditor initial_name{resource::DataRoot{data_root}};
    OL_CHECK(basic_renderer.render_name_entry(renderer, initial_name, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x5e228f916fbca51aULL);
    static_cast<void>(initial_name.handle_key('R', false, false));
    static_cast<void>(initial_name.handle_key('U', false, false));
    static_cast<void>(initial_name.handle_key('P', false, false));
    OL_CHECK(basic_renderer.render_name_entry(renderer, initial_name, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x5e7ca6575563ee33ULL);
    static_cast<void>(initial_name.handle_key(0x20U, false, false));
    OL_CHECK(basic_renderer.render_name_entry(renderer, initial_name, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0xf6be4e7079ef77d7ULL);
    static_cast<void>(initial_name.handle_key(0x2CU, false, true));
    OL_CHECK(basic_renderer.render_name_entry(renderer, initial_name, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0xe91f546c3328c8e3ULL);

    ui::NewGameNameEditor no_name_candidates{resource::DataRoot{data_root}};
    static_cast<void>(no_name_candidates.handle_key('1', false, false));
    static_cast<void>(no_name_candidates.handle_key('7', false, false));
    OL_CHECK(basic_renderer.render_name_entry(renderer, no_name_candidates, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x34681c6631f4bfc0ULL);

    ui::NewGameNameEditor alphanumeric_name{resource::DataRoot{data_root}};
    static_cast<void>(alphanumeric_name.handle_key(0x20U, true, false));
    static_cast<void>(alphanumeric_name.handle_key('A', false, false));
    OL_CHECK(basic_renderer.render_name_entry(renderer, alphanumeric_name, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x28e4cf89b0202debULL);
    static_cast<void>(alphanumeric_name.handle_key(0x08U, false, false));
    OL_CHECK(basic_renderer.render_name_entry(renderer, alphanumeric_name, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x25ebc96179fd2703ULL);

    ui::NewGameNameEditor accepted_name{resource::DataRoot{data_root}};
    static_cast<void>(accepted_name.handle_key(0x20U, true, false));
    static_cast<void>(accepted_name.handle_key('A', false, false));
    OL_CHECK(accepted_name.handle_key(0x0DU, false, false) == ui::NameEditStatus::completed);
    OL_CHECK(basic_renderer.render_name_entry(renderer, accepted_name, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x668ae394d0fa7e13ULL);

    const auto fill_menu_oracle_background = [&framebuffer]() {
        auto pixels = framebuffer.pixels();
        for (std::size_t index = 0U; index < pixels.size(); ++index) {
            pixels[index] = static_cast<std::uint8_t>(
                ((index % 320U) * 13U + (index / 320U) * 7U) & 0xFFU);
        }
    };
    ui::GameMenuController world_game_menu;
    fill_menu_oracle_background();
    OL_CHECK(basic_renderer.render_game_menu_main(world_game_menu, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x4821B1C9F78F507DULL);
    ui::GameMenuController scene_game_menu;
    scene_game_menu.set_context(ui::GameMenuContext::scene);
    static_cast<void>(scene_game_menu.handle_key(0x9EU));
    fill_menu_oracle_background();
    OL_CHECK(basic_renderer.render_game_menu_main(scene_game_menu, framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x2D951419C79FA4F8ULL);

    app::LegacyStartupResources item_draw_resources{resource::DataRoot{data_root}};
    OL_CHECK(item_draw_resources.valid());
    framebuffer.set_palette(item_draw_resources.palette());

    const auto rectangle_matches = [](
                                       const std::span<const std::uint8_t> expected,
                                       const std::span<const std::uint8_t> actual,
                                       const int x,
                                       const int y,
                                       const int width,
                                       const int height) {
        for (int row = y; row < y + height; ++row) {
            for (int column = x; column < x + width; ++column) {
                const auto index = static_cast<std::size_t>(row * 320 + column);
                if (expected[index] != actual[index]) {
                    return false;
                }
            }
        }
        return true;
    };
    ui::GameMenuController layered_menu;
    static_cast<void>(layered_menu.handle_key(0x9EU));
    static_cast<void>(layered_menu.handle_key(0x0DU));
    static_cast<void>(layered_menu.handle_key(0x98U));
    OL_CHECK(layered_menu.screen() == ui::GameMenuScreen::system);
    OL_CHECK(layered_menu.system_selection() == 1U);
    fill_menu_oracle_background();
    OL_CHECK(basic_renderer.render_game_menu_main(layered_menu, framebuffer));
    const std::vector<std::uint8_t> main_menu_layer{
        framebuffer.pixels().begin(), framebuffer.pixels().end()};
    fill_menu_oracle_background();
    OL_CHECK(basic_renderer.render_game_menu(
        layered_menu, item_draw_resources.ranger(), framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x3F70C56F2E0DEB41ULL);
    OL_CHECK(rectangle_matches(main_menu_layer, framebuffer.pixels(), 20, 18, 42, 132));
    const std::vector<std::uint8_t> system_menu_layer{
        framebuffer.pixels().begin(), framebuffer.pixels().end()};
    static_cast<void>(layered_menu.handle_key(0x0DU));
    OL_CHECK(layered_menu.screen() == ui::GameMenuScreen::save_slots);
    fill_menu_oracle_background();
    OL_CHECK(basic_renderer.render_game_menu(
        layered_menu, item_draw_resources.ranger(), framebuffer));
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x169E23F2C8C26A14ULL);
    OL_CHECK(rectangle_matches(system_menu_layer, framebuffer.pixels(), 20, 18, 42, 132));
    OL_CHECK(rectangle_matches(system_menu_layer, framebuffer.pixels(), 70, 18, 50, 72));

    const auto clear_item_draw_inventory = [](model::RangerState& ranger) {
        for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
            ranger.header.set_inventory(slot, model::ItemId{-1}, 0);
        }
    };
    const auto set_fixed_string = [](
                                      std::span<std::uint8_t> destination,
                                      const std::span<const std::uint8_t> value) {
        OL_CHECK(value.size() < destination.size());
        std::ranges::fill(destination, 0U);
        std::ranges::copy(value, destination.begin());
    };
    const auto render_item_draw_vector = [&](
                                             const model::RangerState& ranger,
                                             const std::span<const std::int16_t> slots,
                                             const std::uint64_t expected,
                                             const std::string_view label) {
        ui::GameMenuController menu;
        menu.set_inventory_slots(slots);
        menu.show_items();
        fill_menu_oracle_background();
        OL_CHECK(basic_renderer.render_game_menu(menu, ranger, framebuffer));
        const auto actual = fnv1a64(framebuffer.pixels());
        if (actual != expected) {
            std::cerr << label << "=0x" << std::hex << actual << std::dec << '\n';
        }
        OL_CHECK(actual == expected);
    };

    auto fifteen_item_ranger = item_draw_resources.ranger();
    clear_item_draw_inventory(fifteen_item_ranger);
    constexpr std::array<std::int16_t, 4U> kDrawItemIds{0, 2, 10, 21};
    std::array<std::int16_t, 15U> fifteen_item_slots{};
    for (std::size_t slot = 0U; slot < fifteen_item_slots.size(); ++slot) {
        fifteen_item_ranger.header.set_inventory(
            slot, model::ItemId{kDrawItemIds[slot % kDrawItemIds.size()]}, 1);
        fifteen_item_slots[slot] = static_cast<std::int16_t>(slot);
    }
    render_item_draw_vector(
        fifteen_item_ranger,
        fifteen_item_slots,
        0x0084912FCC7C682BULL,
        "item_draw_fifteen_hash");

    auto coordinate_item_ranger = item_draw_resources.ranger();
    clear_item_draw_inventory(coordinate_item_ranger);
    coordinate_item_ranger.header.set_inventory(0U, model::ItemId{182}, 2);
    coordinate_item_ranger.header.set_word(model::header_word::main_map_x, -7);
    coordinate_item_ranger.header.set_word(model::header_word::main_map_y, 23);
    coordinate_item_ranger.header.set_word(model::header_word::ship_x, -4);
    coordinate_item_ranger.header.set_word(model::header_word::ship_y, 99);
    constexpr std::array<std::int16_t, 1U> kFirstInventorySlot{0};
    render_item_draw_vector(
        coordinate_item_ranger,
        kFirstInventorySlot,
        0xFED9235C929DFA92ULL,
        "item_draw_coordinate_hash");

    auto alternate_item_ranger = item_draw_resources.ranger();
    clear_item_draw_inventory(alternate_item_ranger);
    alternate_item_ranger.header.set_inventory(0U, model::ItemId{0}, 1);
    auto& alternate_item = alternate_item_ranger.items[0U];
    constexpr std::array<std::uint8_t, 1U> kPrimaryName{'A'};
    constexpr std::array<std::uint8_t, 2U> kAlternateName{'B', 'C'};
    constexpr std::array<std::uint8_t, 4U> kItemDescription{'D', 'E', 'S', 'C'};
    constexpr std::array<std::uint8_t, 1U> kRoleR{'R'};
    set_fixed_string(
        std::span<std::uint8_t>{alternate_item.bytes}.subspan(
            model::item_word::name_byte, model::item_word::name_bytes),
        kPrimaryName);
    set_fixed_string(
        std::span<std::uint8_t>{alternate_item.bytes}.subspan(
            2U * model::item_word::secondary_name_begin,
            2U * model::item_word::secondary_name_count),
        kAlternateName);
    set_fixed_string(
        std::span<std::uint8_t>{alternate_item.bytes}.subspan(
            model::item_word::introduction_byte,
            model::item_word::introduction_bytes),
        kItemDescription);
    alternate_item.set_word(model::item_word::id, 0);
    alternate_item.set_word(model::item_word::user, 1);
    alternate_item.set_word(model::item_word::show_introduction, 1);
    alternate_item.set_word(model::item_word::item_type, 1);
    set_fixed_string(
        std::span<std::uint8_t>{alternate_item_ranger.roles[1U].bytes}.subspan(
            model::role_word::name_byte, model::role_word::name_bytes),
        kRoleR);
    render_item_draw_vector(
        alternate_item_ranger,
        kFirstInventorySlot,
        0x0344AEAED6E99975ULL,
        "item_draw_alternate_hash");

    auto strict_user_item_ranger = alternate_item_ranger;
    auto& strict_user_item = strict_user_item_ranger.items[0U];
    strict_user_item.set_word(model::item_word::user, 0);
    strict_user_item.set_word(model::item_word::show_introduction, 0);
    constexpr std::array<std::uint8_t, 1U> kRoleQ{'Q'};
    set_fixed_string(
        std::span<std::uint8_t>{strict_user_item_ranger.roles[0U].bytes}.subspan(
            model::role_word::name_byte, model::role_word::name_bytes),
        kRoleQ);
    render_item_draw_vector(
        strict_user_item_ranger,
        kFirstInventorySlot,
        0xBD9633F5CFDC8430ULL,
        "item_draw_strict_user_hash");

    auto empty_item_ranger = item_draw_resources.ranger();
    clear_item_draw_inventory(empty_item_ranger);
    constexpr std::array<std::int16_t, 0U> kNoInventorySlots{};
    render_item_draw_vector(
        empty_item_ranger,
        kNoInventorySlots,
        0x464ACC170A231D46ULL,
        "item_draw_empty_hash");

    model::RoleRecord protagonist;
    constexpr std::array<std::uint8_t, 1> name{'A'};
    OL_CHECK(basic_renderer.render_attributes(renderer, protagonist, name, framebuffer));
    const std::vector<std::uint8_t> normal_attributes{
        framebuffer.pixels().begin(), framebuffer.pixels().end()};
    const auto check_highlight_cell = [&normal_attributes](
                                          const std::span<const std::uint8_t> highlighted,
                                          const int expected_x) {
        std::size_t changed = 0U;
        bool confined = highlighted.size() == normal_attributes.size();
        for (std::size_t index = 0U; confined && index < highlighted.size(); ++index) {
            if (highlighted[index] == normal_attributes[index]) {
                continue;
            }
            ++changed;
            const auto x = static_cast<int>(index % 320U);
            const auto y = static_cast<int>(index / 320U);
            confined = x >= expected_x && x < expected_x + 65 && y >= 152 && y < 168;
        }
        OL_CHECK(confined);
        OL_CHECK(changed > 500U);
    };

    protagonist.set_word(model::role_word::maximum_mp, 40);
    OL_CHECK(basic_renderer.render_attributes(renderer, protagonist, name, framebuffer));
    check_highlight_cell(framebuffer.pixels(), 10);
    protagonist.set_word(model::role_word::maximum_mp, 0);
    protagonist.set_word(model::role_word::attack, 30);
    OL_CHECK(basic_renderer.render_attributes(renderer, protagonist, name, framebuffer));
    check_highlight_cell(framebuffer.pixels(), 85);
}

using UiCheck = void (*)(const std::filesystem::path&);

void run_controller_check(const std::filesystem::path&) {
    check_controller();
}

void run_game_menu_controller_check(const std::filesystem::path&) {
    check_game_menu_controller();
}

void run_attribute_controller_check(const std::filesystem::path&) {
    check_attribute_controller();
}

[[gnu::noinline]] void run_ui_check(
    const UiCheck check,
    const std::filesystem::path& data_root) {
    check(data_root);
}

}  // namespace

int main(const int argc, char* argv[]) {
    const auto shard = openlegend::test::test_shard(argc, argv);
    const auto data_root = openlegend::test::game_data_root();
    const std::array<UiCheck, 17> checks{
        run_controller_check,
        run_game_menu_controller_check,
        run_attribute_controller_check,
        check_name_editor,
        check_startup_resource_cache,
        check_game_runtime,
        check_question_present_gate,
        check_picture_animation_tick_gates,
        check_three_statue_animation_tick_gates,
        check_ending_prelude_animation_tick_gates,
        check_shop_input_present_gate,
        check_death_menu_present_gate,
        check_battle_party_selection_input_timing,
        check_battle_runtime_transitions,
        check_scene_load_runtime,
        check_runtime_persistence,
        check_renderer,
    };
    for (std::size_t index = 0U; index < checks.size(); ++index) {
        if (shard.includes(index)) {
            run_ui_check(checks[index], data_root);
        }
    }
    return openlegend::test::failures == 0 ? 0 : 1;
}
