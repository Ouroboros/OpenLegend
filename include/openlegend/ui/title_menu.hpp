#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/packed_archive.hpp"
#include "openlegend/ui/save_list.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

namespace openlegend::ui {

enum class TitleScreen {
    main,
    new_game_options,
    inheritance_slots,
    completion_save_slots,
    load_slots,
    delete_confirmation,
    please_wait,
};

enum class TitleCommand {
    none,
    start_new_game,
    inherit_slot,
    save_completion,
    finish_ending,
    load_slot,
    delete_slot,
    exit_game,
};

struct TitleResult {
    TitleCommand command{TitleCommand::none};
    std::uint16_t slot{};
};

class TitleMenuController {
public:
    NODISCARD TitleResult handle_key(std::uint8_t translated_key) noexcept;

    void show_please_wait() noexcept { screen_ = TitleScreen::please_wait; }

    void show_main() noexcept { screen_ = TitleScreen::main; }

    void enable_new_game_plus(bool enabled) noexcept { new_game_plus_enabled_ = enabled; }

    void show_inheritance_slots() noexcept { screen_ = TitleScreen::inheritance_slots; }

    void show_completion_save_slots() noexcept { screen_ = TitleScreen::completion_save_slots; }

    NODISCARD std::uint8_t new_game_selection() const noexcept { return new_game_selection_; }

    NODISCARD constexpr TitleScreen screen() const noexcept { return screen_; }

    NODISCARD constexpr std::uint8_t main_selection() const noexcept {
        return main_selection_;
    }

    NODISCARD constexpr std::uint16_t slot_selection() const noexcept {
        return slot_selection_;
    }

private:
    TitleScreen screen_{TitleScreen::main};
    std::uint8_t main_selection_{};
    std::uint8_t new_game_selection_{};
    std::uint16_t slot_selection_{};
    bool new_game_plus_enabled_{};
};

class TitleMenuRenderer {
public:
    explicit TitleMenuRenderer(const resource::DataRoot& data_root);

    TitleMenuRenderer(
        const resource::DataRoot& data_root,
        const compat::LegacyPalette& startup_palette);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD bool render_background(render::IndexedFramebuffer& framebuffer) const;

    NODISCARD bool render_new_game_wait(
        render::IndexedFramebuffer& framebuffer) const;

    NODISCARD bool render(
        const TitleMenuController& controller, render::IndexedFramebuffer& framebuffer) const;

    NODISCARD bool render_new_game_options(const TitleMenuController& controller,
        const compat::LegacyPalette& palette, ModernUiRenderer& ui_renderer,
        render::RgbaFramebuffer& framebuffer) const;

private:
    TitleMenuRenderer(
        const resource::DataRoot& data_root,
        const compat::LegacyPalette* startup_palette);

    NODISCARD bool draw_legacy_id(
        render::IndexedFramebuffer& framebuffer,
        std::uint32_t legacy_id,
        int x,
        int y) const;

    resource::PackedArchive frames_;
    std::vector<std::uint8_t> background_;
    compat::LegacyPalette palette_{};
    std::string error_;
};

}  // namespace openlegend::ui
