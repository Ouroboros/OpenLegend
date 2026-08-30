#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/ui/basic_ui_renderer.hpp"
#include "openlegend/ui/game_menu.hpp"
#include "openlegend/ui/new_game_attributes.hpp"
#include "openlegend/ui/new_game_name_editor.hpp"
#include "openlegend/ui/title_menu.hpp"

namespace openlegend::app {

enum class LegacyGameView {
    title,
    name_entry,
    attributes,
    world,
    game_menu,
    error,
    exited,
};

class LegacyGameRuntime {
public:
    LegacyGameRuntime(std::filesystem::path data_root, std::uint32_t random_seed);

    void advance();
    void handle_key(std::uint8_t translated_key, bool control_down, bool shift_down);
    [[nodiscard]] bool render();

    [[nodiscard]] bool valid() const noexcept { return startup_error_.empty(); }
    [[nodiscard]] bool running() const noexcept { return view_ != LegacyGameView::exited; }
    [[nodiscard]] LegacyGameView view() const noexcept { return view_; }
    [[nodiscard]] const std::string& error() const noexcept { return startup_error_; }
    [[nodiscard]] render::IndexedFramebuffer& framebuffer() noexcept { return framebuffer_; }
    [[nodiscard]] const model::GameState& game_state() const noexcept { return game_state_; }

private:
    enum class PendingIo {
        none,
        load,
        save,
    };

    void begin_new_game();
    void perform_pending_io();
    void update_menu_counts();
    void show_error(std::string message, LegacyGameView return_view);
    void show_legacy_error(
        std::span<const std::uint8_t> message, LegacyGameView return_view);
    void handle_title_result(ui::TitleResult result);
    void handle_game_menu_result(ui::GameMenuResult result);

    std::filesystem::path data_root_path_;
    resource::DataRoot data_root_;
    random::LegacyRandom random_;
    model::GameState game_state_;
    render::IndexedFramebuffer framebuffer_;
    ui::TitleMenuController title_menu_;
    ui::TitleMenuRenderer title_renderer_;
    ui::BasicUiRenderer basic_renderer_;
    ui::GameMenuController game_menu_;
    std::optional<ui::NewGameNameEditor> name_editor_;
    std::unique_ptr<ui::NewGameAttributeController> attribute_controller_;
    LegacyGameView view_{LegacyGameView::title};
    LegacyGameView error_return_view_{LegacyGameView::title};
    PendingIo pending_io_{PendingIo::none};
    std::uint8_t pending_slot_{};
    std::vector<std::uint8_t> visible_error_;
    std::string startup_error_;
};

}  // namespace openlegend::app
