#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/packed_archive.hpp"
#include "openlegend/ui/save_list.hpp"

namespace openlegend::ui {

enum class TitleScreen {
    main,
    load_slots,
    delete_confirmation,
    please_wait,
};

enum class TitleCommand {
    none,
    start_new_game,
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
    std::uint16_t slot_selection_{};
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
