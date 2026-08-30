#include <array>
#include <cstdint>
#include <filesystem>
#include <string_view>

#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/ui/game_menu.hpp"
#include "openlegend/ui/new_game_attributes.hpp"
#include "openlegend/ui/new_game_name_editor.hpp"
#include "openlegend/ui/title_menu.hpp"
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

[[nodiscard]] bool prepare_runtime_fixture(
    const std::filesystem::path& source, const std::filesystem::path& destination) {
    constexpr std::array<std::string_view, 22> files{
        "TITLE.IDX", "TITLE.GRP", "TITLE.BIG", "MMAP.COL", "MMAP.IDX", "MMAP.GRP",
        "CLOUD.IDX", "CLOUD.GRP", "EARTH.002", "SURFACE.002", "BUILDING.002", "BUILDX.002",
        "BUILDY.002", "FONT.X16", "FONT.C16", "CFONT", "RANGER.IDX", "RANGER.GRP",
        "ALLSIN.IDX", "ALLSIN.GRP", "ALLDEF.IDX", "ALLDEF.GRP"};
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

void accept_minimal_new_game(openlegend::app::LegacyGameRuntime& game) {
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x20U, true, false);
    game.handle_key('A', false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key('Y', false, false);
}

void advance_rendered_frames(
    openlegend::app::LegacyGameRuntime& game, const std::size_t frame_count) {
    for (std::size_t frame = 0U; frame < frame_count; ++frame) {
        OL_CHECK(game.render());
        game.advance();
    }
}

void finish_scene_entry(openlegend::app::LegacyGameRuntime& game) {
    advance_rendered_frames(game, 65U);
    OL_CHECK(game.view() == openlegend::app::LegacyGameView::scene);
    game.handle_key(0x0DU, false, false);
    advance_rendered_frames(game, 1U);
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
    static_cast<void>(menu.handle_key(0x98U));
    static_cast<void>(menu.handle_key(0x98U));
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
    static_cast<void>(medicine.handle_key(0x0DU));
    static_cast<void>(medicine.handle_key(0x98U));
    result = medicine.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::medicine);
    OL_CHECK(result.index == 1U);

    GameMenuController detoxification;
    static_cast<void>(detoxification.handle_key(0x98U));
    static_cast<void>(detoxification.handle_key(0x0DU));
    result = detoxification.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::detoxification);

    GameMenuController items;
    items.set_inventory_count(3U);
    static_cast<void>(items.handle_key(0x98U));
    static_cast<void>(items.handle_key(0x98U));
    static_cast<void>(items.handle_key(0x0DU));
    static_cast<void>(items.handle_key(0x9EU));
    result = items.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::items);
    OL_CHECK(result.index == 2U);
    OL_CHECK(items.screen() == GameMenuScreen::main);

    GameMenuController leave_party;
    for (int index = 0; index < 4; ++index) {
        static_cast<void>(leave_party.handle_key(0x98U));
    }
    static_cast<void>(leave_party.handle_key(0x0DU));
    result = leave_party.handle_key(0x0DU);
    OL_CHECK(result.command == GameMenuCommand::leave_party);
    OL_CHECK(result.index == 0U);

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
    static_cast<void>(editor.handle_key('R', false, false));
    static_cast<void>(editor.handle_key('U', false, false));
    static_cast<void>(editor.handle_key('P', false, false));
    static_cast<void>(editor.handle_key(0x20U, false, false));
    OL_CHECK(editor.candidates().size() == 20U);
    OL_CHECK(editor.visible_candidate_count() == 8U);
    OL_CHECK(editor.candidates()[0][0] == 0xA4U);
    OL_CHECK(editor.candidates()[0][1] == 0xB5U);
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
}

void check_game_runtime(const std::filesystem::path& data_root) {
    using namespace openlegend;

    app::LegacyGameRuntime new_game{data_root, 0U};
    OL_CHECK(new_game.valid());
    OL_CHECK(new_game.view() == app::LegacyGameView::title);
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == 0x86690E3B3B68FE20ULL);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::name_entry);
    OL_CHECK(new_game.game_state().loaded());
    OL_CHECK(new_game.render());
    new_game.handle_key(0x20U, true, false);
    new_game.handle_key('A', false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::attributes);
    OL_CHECK(new_game.render());
    new_game.handle_key('Y', false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    const auto* ranger = new_game.game_state().ranger();
    OL_CHECK(ranger != nullptr);
    if (ranger != nullptr) {
        OL_CHECK(ranger->roles[0].bytes[model::role_word::name_byte] == 'A');
        OL_CHECK(ranger->roles[0].bytes[model::role_word::name_byte + 1U] == 0U);
    }
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == 0x6F6CF22B7C8CB4B8ULL);
    const auto palette_before = new_game.framebuffer().palette();
    new_game.finish_presented_tick();
    OL_CHECK(new_game.render());
    const auto palette_after_first = new_game.framebuffer().palette();
    OL_CHECK(palette_after_first[224].red == palette_before[231].red);
    OL_CHECK(palette_after_first[224].green == palette_before[231].green);
    OL_CHECK(palette_after_first[224].blue == palette_before[231].blue);
    for (int tick = 0; tick < 4; ++tick) {
        new_game.finish_presented_tick();
    }
    OL_CHECK(new_game.render());
    const auto palette_after_five = new_game.framebuffer().palette();
    OL_CHECK(palette_after_five[224].red == palette_after_first[224].red);
    OL_CHECK(palette_after_five[224].green == palette_after_first[224].green);
    OL_CHECK(palette_after_five[224].blue == palette_after_first[224].blue);
    new_game.finish_presented_tick();
    OL_CHECK(new_game.render());
    const auto palette_after_six = new_game.framebuffer().palette();
    OL_CHECK(palette_after_six[224].red == palette_after_first[231].red);
    OL_CHECK(palette_after_six[224].green == palette_after_first[231].green);
    OL_CHECK(palette_after_six[224].blue == palette_after_first[231].blue);
    new_game.handle_world_input(false, false, false, true);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::main_map_x) == 358);
    new_game.handle_world_input(false, true, false, false);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    new_game.handle_world_input(true, false, false, false);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    OL_CHECK(!new_game.handle_world_input(false, false, true, false, true));
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::world);
    OL_CHECK(new_game.render());
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::main_map_x) == 357);
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::main_map_y) == 235);
    OL_CHECK(new_game.handle_world_input(false, false, false, false, true));
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    OL_CHECK(new_game.render());
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == 0x51F12E62670DCB3EULL);
    new_game.handle_key('A', false, false);
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == 0x1EE8CF32156BD6FAULL);
    new_game.handle_key('A', false, false);
    new_game.handle_key(0x9EU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.render());
    OL_CHECK(fnv1a64(new_game.framebuffer().pixels()) == 0x1F2C81326BE42838ULL);
    new_game.handle_key(0x1BU, false, false);
    OL_CHECK(new_game.render());
    const auto before_leave = new_game.game_state().export_snapshot();
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::error);
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

    new_game.handle_world_input(true, false, false, false);
    new_game.advance();
    OL_CHECK(new_game.scene_request().has_value());
    OL_CHECK(new_game.scene_request().value_or(-1) == 70);
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    finish_scene_entry(new_game);
    new_game.handle_key(0x1BU, false, false);
    new_game.handle_world_input(false, true, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::scene);
    advance_rendered_frames(new_game, 1U);
    new_game.advance();
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    OL_CHECK(new_game.render());
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x98U, false, false);
    new_game.handle_key(0x0DU, false, false);
    new_game.handle_key(0x0DU, false, false);
    OL_CHECK(new_game.view() == app::LegacyGameView::game_menu);
    OL_CHECK(new_game.render());
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
    new_game.handle_key(0x0DU, false, false);
    new_game.handle_key(0x0DU, false, false);
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
    OL_CHECK(!new_game.scene_request().has_value());
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::in_sub_map) == 0);
    OL_CHECK(new_game.game_state().ranger()->header.word(model::header_word::face_towards) ==
             static_cast<std::int16_t>(world::WorldDirection::right));
    OL_CHECK(new_game.render());

    app::LegacyGameRuntime load_game{data_root, 1U};
    load_game.handle_key(0x98U, false, false);
    load_game.handle_key(0x0DU, false, false);
    load_game.handle_key(0x0DU, false, false);
    OL_CHECK(load_game.view() == app::LegacyGameView::title);
    OL_CHECK(load_game.render());
    OL_CHECK(fnv1a64(load_game.framebuffer().pixels()) == 0x7333253CA7400DE6ULL);
    load_game.advance();
    OL_CHECK(load_game.view() == app::LegacyGameView::world);
    OL_CHECK(load_game.game_state().loaded());
    OL_CHECK(load_game.render());
}

void check_runtime_persistence(const std::filesystem::path& data_root) {
    using namespace openlegend;

    const auto output_root = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "b5-runtime";
    OL_CHECK(prepare_runtime_fixture(data_root, output_root));
    app::LegacyGameRuntime game{output_root, 0U};
    OL_CHECK(game.valid());
    accept_minimal_new_game(game);
    OL_CHECK(game.view() == app::LegacyGameView::world);

    game.handle_key(0x1BU, false, false);
    game.handle_key(0x9EU, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x98U, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x0DU, false, false);
    const auto before_save = game.game_state().export_snapshot();
    OL_CHECK(before_save.has_value());
    game.advance();
    OL_CHECK(game.view() == app::LegacyGameView::game_menu);
    const auto saved =
        persistence::load_numbered_slot(output_root, persistence::SaveSlot::one);
    OL_CHECK(static_cast<bool>(saved));
    OL_CHECK(saved.snapshot.has_value());
    if (before_save.has_value() && saved.snapshot.has_value()) {
        OL_CHECK(*before_save == *saved.snapshot);
    }

    game.handle_key(0x9EU, false, false);
    game.handle_key(0x0DU, false, false);
    game.handle_key(0x0DU, false, false);
    std::error_code error;
    std::filesystem::remove(output_root / "R1.GRP", error);
    OL_CHECK(!error);
    const auto before_failed_load = game.game_state().export_snapshot();
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
    std::filesystem::remove_all(output_root, error);
    OL_CHECK(!error);
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
}

}  // namespace

int main() {
    const auto data_root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
    check_controller();
    check_game_menu_controller();
    check_attribute_controller();
    check_name_editor(data_root);
    check_game_runtime(data_root);
    check_runtime_persistence(data_root);
    check_renderer(data_root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
