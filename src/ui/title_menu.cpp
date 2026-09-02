#include "openlegend/ui/title_menu.hpp"

#include <algorithm>

#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"

namespace openlegend::ui {
namespace {

constexpr std::uint8_t kEnter = 0x0DU;
constexpr std::uint8_t kEscape = 0x1BU;
constexpr std::uint8_t kSpace = 0x20U;
constexpr std::uint8_t kKeypadInsert = 0x96U;
constexpr std::uint8_t kDown = 0x98U;
constexpr std::uint8_t kUp = 0x9EU;

[[nodiscard]] constexpr bool confirms(const std::uint8_t key) noexcept {
    return key == kEnter || key == kSpace || key == kKeypadInsert;
}

void move_down(std::uint8_t& selection) noexcept {
    selection = selection == 2U ? 0U : static_cast<std::uint8_t>(selection + 1U);
}

void move_up(std::uint8_t& selection) noexcept {
    selection = selection == 0U ? 2U : static_cast<std::uint8_t>(selection - 1U);
}

}  // namespace

TitleResult TitleMenuController::handle_key(const std::uint8_t translated_key) noexcept {
    if (screen_ == TitleScreen::please_wait) {
        return {};
    }

    auto& selection = screen_ == TitleScreen::main ? main_selection_ : slot_selection_;
    if (translated_key == kDown) {
        move_down(selection);
        return {};
    }
    if (translated_key == kUp) {
        move_up(selection);
        return {};
    }

    if (screen_ == TitleScreen::load_slots && translated_key == kEscape) {
        screen_ = TitleScreen::main;
        main_selection_ = 1U;
        return {};
    }
    if (!confirms(translated_key)) {
        return {};
    }

    if (screen_ == TitleScreen::load_slots) {
        return {TitleCommand::load_slot, slot_selection_};
    }
    if (main_selection_ == 0U) {
        return {TitleCommand::start_new_game, 0U};
    }
    if (main_selection_ == 1U) {
        screen_ = TitleScreen::load_slots;
        slot_selection_ = 0U;
        return {};
    }
    return {TitleCommand::exit_game, 0U};
}

TitleMenuRenderer::TitleMenuRenderer(const resource::DataRoot& data_root)
    : TitleMenuRenderer(data_root, nullptr) {}

TitleMenuRenderer::TitleMenuRenderer(
    const resource::DataRoot& data_root,
    const compat::LegacyPalette& startup_palette)
    : TitleMenuRenderer(data_root, &startup_palette) {}

TitleMenuRenderer::TitleMenuRenderer(
    const resource::DataRoot& data_root,
    const compat::LegacyPalette* startup_palette)
    : frames_(resource::PackedArchive::open(
          data_root.path() / "title.idx", data_root.path() / "title.grp")) {
    if (!frames_.valid()) {
        error_ = frames_.error();
        return;
    }
    if (frames_.entry_count() != 9U) {
        error_ = "title archive does not contain nine frames";
        return;
    }

    auto background = data_root.read("title.big");
    if (!background) {
        error_ = background.error;
        return;
    }
    if (background.bytes.size() != compat::kLegacyPixelCount) {
        error_ = "title.big is not a complete 320x200 indexed framebuffer";
        return;
    }
    background_ = std::move(background.bytes);

    if (startup_palette != nullptr) {
        palette_ = *startup_palette;
        return;
    }
    const auto palette_file = data_root.read("mmap.col");
    if (!palette_file) {
        error_ = palette_file.error;
        return;
    }
    const auto palette = resource::parse_vga_palette(palette_file.bytes);
    if (!palette) {
        error_ = palette.error;
        return;
    }
    palette_ = palette.palette;
}

bool TitleMenuRenderer::render_background(render::IndexedFramebuffer& framebuffer) const {
    if (!valid()) {
        return false;
    }
    std::ranges::copy(background_, framebuffer.pixels().begin());
    framebuffer.set_palette(palette_);
    return true;
}

bool TitleMenuRenderer::render_new_game_wait(
    render::IndexedFramebuffer& framebuffer) const {
    return render_background(framebuffer) &&
        framebuffer.fill_rectangle(0, 135, 320U, 65U, 0U) &&
        draw_legacy_id(framebuffer, 16U, 120, 160);
}

bool TitleMenuRenderer::render(
    const TitleMenuController& controller, render::IndexedFramebuffer& framebuffer) const {
    if (!render_background(framebuffer)) {
        return false;
    }

    switch (controller.screen()) {
    case TitleScreen::main: {
        if (!draw_legacy_id(framebuffer, 0U, 117, 137)) {
            return false;
        }
        const auto selection = static_cast<std::uint32_t>(controller.main_selection());
        return draw_legacy_id(
            framebuffer,
            2U + selection * 2U,
            117,
            137 + static_cast<int>(selection) * 20);
    }
    case TitleScreen::load_slots: {
        if (!draw_legacy_id(framebuffer, 8U, 117, 137)) {
            return false;
        }
        const auto slot = static_cast<std::uint32_t>(controller.slot_selection());
        return draw_legacy_id(
            framebuffer, 10U + slot * 2U, 117, 137 + static_cast<int>(slot) * 20);
    }
    case TitleScreen::please_wait:
        if (!framebuffer.fill_rectangle(115, 135, 135U, 65U, 0U)) {
            return false;
        }
        return draw_legacy_id(framebuffer, 16U, 120, 160);
    }
    return false;
}

bool TitleMenuRenderer::draw_legacy_id(
    render::IndexedFramebuffer& framebuffer,
    const std::uint32_t legacy_id,
    const int x,
    const int y) const {
    const auto index = render::legacy_sprite_index(legacy_id);
    if (!index.has_value() || *index >= frames_.entry_count()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(frames_.entry(*index));
    if (!frame.valid()) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, frame, x, y);
    return true;
}

}  // namespace openlegend::ui
