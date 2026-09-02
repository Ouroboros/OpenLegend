#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/packed_archive.hpp"

namespace openlegend::ui {

enum class TitleScreen {
    main,
    load_slots,
    please_wait,
};

enum class TitleCommand {
    none,
    start_new_game,
    load_slot,
    exit_game,
};

struct TitleResult {
    TitleCommand command{TitleCommand::none};
    std::uint8_t slot{};
};

class TitleMenuController {
public:
    [[nodiscard]] TitleResult handle_key(std::uint8_t translated_key) noexcept;
    void show_please_wait() noexcept { screen_ = TitleScreen::please_wait; }
    void show_main() noexcept { screen_ = TitleScreen::main; }

    [[nodiscard]] constexpr TitleScreen screen() const noexcept { return screen_; }
    [[nodiscard]] constexpr std::uint8_t main_selection() const noexcept {
        return main_selection_;
    }
    [[nodiscard]] constexpr std::uint8_t slot_selection() const noexcept {
        return slot_selection_;
    }

private:
    TitleScreen screen_{TitleScreen::main};
    std::uint8_t main_selection_{};
    std::uint8_t slot_selection_{};
};

class TitleMenuRenderer {
public:
    explicit TitleMenuRenderer(const resource::DataRoot& data_root);
    TitleMenuRenderer(
        const resource::DataRoot& data_root,
        const compat::LegacyPalette& startup_palette);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] bool render_background(render::IndexedFramebuffer& framebuffer) const;
    [[nodiscard]] bool render_new_game_wait(
        render::IndexedFramebuffer& framebuffer) const;
    [[nodiscard]] bool render(
        const TitleMenuController& controller, render::IndexedFramebuffer& framebuffer) const;

private:
    TitleMenuRenderer(
        const resource::DataRoot& data_root,
        const compat::LegacyPalette* startup_palette);

    [[nodiscard]] bool draw_legacy_id(
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
