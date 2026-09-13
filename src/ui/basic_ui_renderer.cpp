#include "openlegend/attributes.hpp"
#include "openlegend/ui/basic_ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>

#include "openlegend/input/name_input_method.hpp"
#include "openlegend/model/new_game.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/text/big5.hpp"
#include "openlegend/text/game_strings.hpp"

namespace openlegend::ui {
namespace {

using namespace openlegend::text::game_strings;
namespace palette_colors = render::legacy_color;
namespace text_colors = render::legacy_color::text;

constexpr std::array<std::size_t, 17> kRandomAttributeWords{
    model::role_word::maximum_mp,
    model::role_word::attack,
    model::role_word::speed,
    model::role_word::defence,
    model::role_word::maximum_hp,
    model::role_word::medicine,
    model::role_word::use_poison,
    model::role_word::detoxification,
    model::role_word::fist,
    model::role_word::sword,
    model::role_word::knife,
    model::role_word::hidden_weapon,
    model::role_word::mp_type,
    model::role_word::increased_life,
    model::role_word::iq,
    model::role_word::anti_poison,
    model::role_word::unusual,
};

NODISCARD bool random_attribute_highlighted(
    const std::size_t word, const std::int16_t value) noexcept {
    switch (word) {
    case model::role_word::mp_type:
        return value == 2;
    case model::role_word::maximum_mp:
        return value == 40;
    case model::role_word::maximum_hp:
        return value == 50;
    case model::role_word::increased_life:
        return value == 7 || value == 10;
    case model::role_word::iq:
        return value == 94 || value == 100;
    default:
        return value == 30;
    }
}

void append_number(std::u8string& text, const std::int32_t value, const int width = 0) {
    std::array<char, 16> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    const auto count = static_cast<int>(converted.ptr - buffer.data());
    for (int index = count; index < width; ++index) {
        text.push_back(u8' ');
    }
    for (const auto* cursor = buffer.data(); cursor != converted.ptr; ++cursor) {
        text.push_back(static_cast<char8_t>(*cursor));
    }
}

NODISCARD int legacy_item_metric(const model::RangerState& ranger) noexcept {
    for (std::size_t slot = 0U; slot < model::kInventoryCount; ++slot) {
        if (ranger.header.inventory_item(slot).value == -1) {
            return static_cast<int>(slot + 1U);
        }
    }
    return static_cast<int>(model::kInventoryCount);
}

NODISCARD std::u8string coordinate_item_text(
    const model::RangerState& ranger,
    const GameMenuContext context) {
    const auto player_x = ranger.header.word(
        context == GameMenuContext::scene
            ? model::header_word::sub_map_x
            : model::header_word::main_map_x);
    const auto player_y = ranger.header.word(
        context == GameMenuContext::scene
            ? model::header_word::sub_map_y
            : model::header_word::main_map_y);
    std::u8string text{kPersonOpen};
    append_number(text, player_x, 3);
    text.append(kComma);
    append_number(text, player_y, 3);
    text.append(kPersonCloseShipOpen);
    append_number(text, ranger.header.word(model::header_word::ship_x), 3);
    text.append(kComma);
    append_number(text, ranger.header.word(model::header_word::ship_y), 3);
    text.append(kClose);
    return text;
}

NODISCARD std::optional<std::array<std::uint8_t, 2>> zhuyin_label(
    const int type,
    const std::int16_t value) noexcept {
    if (type == 1 && value >= 1 && value <= 21) {
        return std::array<std::uint8_t, 2>{
            0xA3U,
            static_cast<std::uint8_t>(value <= 11 ? 0x73 + value : 0x95 + value)};
    }
    if (type == 2 && value >= 1 && value <= 3) {
        return std::array<std::uint8_t, 2>{
            0xA3U, static_cast<std::uint8_t>(0xB7 + value)};
    }
    if (type == 3 && value >= 1 && value <= 13) {
        return std::array<std::uint8_t, 2>{
            0xA3U, static_cast<std::uint8_t>(0xAA + value)};
    }
    if (type == 4 && value >= 1 && value <= 4) {
        return std::array<std::uint8_t, 2>{
            0xA3U,
            static_cast<std::uint8_t>(value == 1 ? 0xBB : 0xBB + value)};
    }
    return std::nullopt;
}

template <std::size_t ByteCount>
NODISCARD std::span<const std::uint8_t> fixed_text(
    const std::array<std::uint8_t, ByteCount>& bytes,
    const std::size_t begin,
    const std::size_t maximum) {
    const auto source = std::span<const std::uint8_t>{bytes}.subspan(begin, maximum);
    const auto end = std::find(source.begin(), source.end(), std::uint8_t{0U});
    return source.first(static_cast<std::size_t>(std::distance(source.begin(), end)));
}

}  // namespace

BasicUiRenderer::BasicUiRenderer(const resource::DataRoot& data_root)
    : item_sprites_(resource::PackedArchive::open(
          data_root.path() / "MMAP.IDX", data_root.path() / "MMAP.GRP")) {
    const auto final_item_sprite = render::legacy_item_sprite_index(
        static_cast<std::int16_t>(model::kItemCount - 1U));
    if (!item_sprites_.valid() || !final_item_sprite.has_value() ||
        item_sprites_.entry_count() <= *final_item_sprite) {
        error_ = item_sprites_.valid()
            ? "MMAP archive is missing item icon frames"
            : item_sprites_.error();
        return;
    }
    auto ascii = data_root.read("FONT3.E16");
    if (!ascii) {
        error_ = ascii.error;
        return;
    }
    auto big5 = data_root.read("FONT3.C16");
    if (!big5) {
        error_ = big5.error;
        return;
    }
    if (ascii.bytes.size() != 128U * 16U || big5.bytes.size() % 32U != 0U) {
        error_ = "legacy UI fonts have unexpected sizes";
        return;
    }
    ascii_font_ = std::move(ascii.bytes);
    big5_font_ = std::move(big5.bytes);
    big5_cache_.emplace(big5_font_);
}

bool BasicUiRenderer::render_name_entry(
    const TitleMenuRenderer& title,
    const NewGameNameEditor& editor,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid() || !title.render_background(framebuffer) ||
        !framebuffer.fill_rectangle(
            0, 140, 320U, 60U, palette_colors::menu_background)) {
        return false;
    }
    const auto modern_input =
        editor.input_method() == input::NameInputMethod::modern;
    const auto has_candidates = !modern_input && !editor.candidates().empty();
    const auto input_prompt = modern_input
        ? kModernNameInputPrompt
        : (editor.mode() == NameInputMode::zhuyin ? kZhuyinPrompt : kAlnumPrompt);
    if (!draw_text_utf8(framebuffer, 3, 141, kNamePrompt) ||
        !draw_text_utf8(
            framebuffer,
            3,
            161,
            input_prompt,
            has_candidates ? text_colors::candidate : text_colors::new_game_prompt_inactive) ||
        !draw_text_big5(
            framebuffer,
            113,
            141,
            text::Big5TextView{editor.display_name()},
            text_colors::name_value) ||
        !draw_text_utf8(
            framebuffer,
            215,
            141,
            kNameInputMethodHint,
            text_colors::new_game_prompt_inactive)) {
        return false;
    }
    if (!editor.accepted() && editor.name().size() < model::kNewGameNameMaximumBytes &&
        !framebuffer.fill_rectangle(
            113 + static_cast<int>(editor.name().size()) * 8,
            156,
            8U,
            1U,
            editor.cursor_color())) {
        return false;
    }

    if (has_candidates) {
        const auto visible = editor.visible_candidate_count();
        for (std::size_t index = 0U; index < visible; ++index) {
            const auto candidate = editor.visible_candidate(index);
            if (!candidate.has_value()) {
                return false;
            }
            auto x = 30 * static_cast<int>(index + 1U);
            const std::array<std::uint8_t, 1> digit{
                static_cast<std::uint8_t>('1' + index)};
            if (!draw_text_big5(
                    framebuffer, x, 180, text::Big5TextView{digit}, text_colors::candidate)) {
                return false;
            }
            x += 8;
            if ((*candidate)[0] <= 0x7FU) {
                const std::array<std::uint8_t, 1> first{(*candidate)[0]};
                if (!draw_text_big5(
                        framebuffer, x, 180, text::Big5TextView{first}, text_colors::candidate)) {
                    return false;
                }
                x += 8;
                if ((*candidate)[1] <= 0x7FU) {
                    const std::array<std::uint8_t, 1> second{(*candidate)[1]};
                    if (!draw_text_big5(
                            framebuffer, x, 180, text::Big5TextView{second}, text_colors::candidate)) {
                        return false;
                    }
                }
            } else {
                const std::array<std::uint8_t, 2> pair{
                    (*candidate)[0], (*candidate)[1]};
                if (!draw_text_big5(
                        framebuffer, x, 180, text::Big5TextView{pair}, text_colors::candidate) &&
                    editor.candidate_page() >= 0) {
                    return false;
                }
            }
        }
        if (editor.candidate_page() == 0) {
            return !editor.has_next_candidate_page() ||
                draw_text_utf8(framebuffer, 300, 180, kCandidateNextPage, text_colors::candidate);
        }
        if (editor.has_next_candidate_page()) {
            return draw_text_utf8(framebuffer, 270, 180, kCandidateBothPages, text_colors::candidate);
        }
        return draw_text_utf8(framebuffer, 300, 180, kCandidatePreviousPage, text_colors::candidate);
    }

    const std::array<std::int16_t, 4> composition{
        editor.initial(), editor.medial(), editor.final(), editor.tone()};
    for (std::size_t index = 0U; index < composition.size(); ++index) {
        const auto label = zhuyin_label(static_cast<int>(index + 1U), composition[index]);
        if (label.has_value() &&
            !draw_text_big5(
                framebuffer,
                100 + static_cast<int>(index) * 20,
                161,
                text::Big5TextView{*label})) {
            return false;
        }
    }
    return !editor.no_candidates() ||
        draw_text_utf8(framebuffer, 240, 161, kNoNameCandidates, text_colors::notice);
}

bool BasicUiRenderer::render_attributes(
    const model::RoleRecord& protagonist,
    const std::span<const std::uint8_t> name,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid()) {
        return false;
    }
    framebuffer.clear(palette_colors::menu_background);
    text::GameText question;
    question.append_legacy(text::Big5TextView{name});
    question.append_utf8(kAttributeQuestion);
    if (!draw_text_mixed(framebuffer, 8, 8, question, text_colors::attribute_question)) {
        return false;
    }
    for (std::size_t index = 0U; index < kRandomAttributeWords.size(); ++index) {
        constexpr auto kOriginalColumnCount = 6U;
        constexpr auto kOriginalAttributeCount = 12U;
        const auto word = kRandomAttributeWords[index];
        const auto value = protagonist.word(word);
        const auto column = index < kOriginalAttributeCount
            ? index / kOriginalColumnCount
            : 2U;
        const auto row = index < kOriginalAttributeCount
            ? index % kOriginalColumnCount
            : index - kOriginalAttributeCount;
        constexpr std::array kColumnX{36, 112, 188};
        constexpr auto kCombinedMpTypeOffset = -13;
        const auto combined_mp_type =
            protagonist.word(model::role_word::mp_type) == 2;
        const auto x = kColumnX[column] +
            (combined_mp_type ? kCombinedMpTypeOffset : 0);
        const auto y = 32 + static_cast<int>(row) * 24;
        const auto highlighted = random_attribute_highlighted(word, value);
        std::u8string line{kRandomAttributeLabels[index]};
        if (word == model::role_word::mp_type && value >= 0 &&
            value < static_cast<std::int16_t>(kMpTypeLabels.size())) {
            line.append(kMpTypeLabels[static_cast<std::size_t>(value)]);
        } else {
            append_number(line, value, 2);
        }
        if (!draw_text_utf8(
                framebuffer,
                x,
                y,
                line,
                highlighted ? text_colors::attribute_highlight : text_colors::attribute_normal)) {
            return false;
        }
    }

    const auto sexual = protagonist.word(model::role_word::sexual);
    std::u8string sexual_line{kSexualAttributeLabel};
    if (sexual >= 0 &&
        sexual < static_cast<std::int16_t>(kSexualLabels.size())) {
        sexual_line.append(kSexualLabels[static_cast<std::size_t>(sexual)]);
    } else {
        append_number(sexual_line, sexual, 2);
    }
    constexpr auto kSexualColumnX = 188;
    constexpr auto kSexualRowY = 152;
    constexpr auto kCombinedMpTypeOffset = -13;
    const auto combined_mp_type =
        protagonist.word(model::role_word::mp_type) == 2;
    return draw_text_utf8(
        framebuffer,
        kSexualColumnX + (combined_mp_type ? kCombinedMpTypeOffset : 0),
        kSexualRowY,
        sexual_line,
        text_colors::attribute_normal);
}

bool BasicUiRenderer::render_game_menu_main(
    const GameMenuController& menu,
    render::IndexedFramebuffer& framebuffer) {
    if (!draw_box(
            framebuffer,
            20,
            18,
            42U,
            static_cast<std::uint16_t>(12U + 20U * menu.visible_main_items()))) {
        return false;
    }
    for (std::size_t index = 0U; index < menu.visible_main_items(); ++index) {
        if (!draw_text_utf8(
                framebuffer,
                24,
                25 + static_cast<int>(index) * 20,
                kMainLabels[index],
                index == menu.selection() ? text_colors::selected : text_colors::menu_normal)) {
            return false;
        }
    }
    return true;
}

bool BasicUiRenderer::render_game_menu(
    const GameMenuController& menu,
    const model::RangerState& ranger,
    render::IndexedFramebuffer& framebuffer) {
    const auto render_system_menu = [&]() {
        if (!draw_box(framebuffer, 70, 18, 42U, 72U)) {
            return false;
        }
        for (std::size_t index = 0U; index < kSystemLabels.size(); ++index) {
            if (!draw_text_utf8(
                    framebuffer,
                    74,
                    25 + static_cast<int>(index) * 20,
                    kSystemLabels[index],
                    index == menu.system_selection() ? text_colors::selected : text_colors::menu_normal)) {
                return false;
            }
        }
        return true;
    };

    switch (menu.screen()) {
    case GameMenuScreen::main:
        return render_game_menu_main(menu, framebuffer);
    case GameMenuScreen::party_select:
        if (!draw_box(framebuffer, 68, 18, 92U, 132U)) {
            return false;
        }
        for (std::uint8_t index = 0U; index < 6U; ++index) {
            const auto role_id = ranger.header.team_member(index).value;
            if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger.roles.size()) {
                break;
            }
            const auto& role = ranger.roles[static_cast<std::size_t>(role_id)];
            if (!draw_text_big5(
                    framebuffer,
                    74,
                    25 + static_cast<int>(index) * 20,
                    text::Big5TextView{fixed_text(
                        role.bytes,
                        model::role_word::name_byte,
                        model::role_word::name_bytes)},
                    index == menu.party_selection() ? text_colors::selected : text_colors::menu_normal)) {
                return false;
            }
        }
        return true;
    case GameMenuScreen::party_notice:
    case GameMenuScreen::status_panel:
        return false;
    case GameMenuScreen::items:
        return render_items(menu, ranger, framebuffer);
    case GameMenuScreen::item_confirmation:
        if (!draw_box(framebuffer, 62, 18, 203U, 50U)) {
            return false;
        }
        if (menu.item_confirmation() == GameMenuItemConfirmation::practice_reassign) {
            return draw_text_utf8(
                       framebuffer, 67, 25, kPracticeAssignedNotice, text_colors::notice) &&
                draw_text_utf8(
                       framebuffer, 67, 45, kPracticeReassignQuestion, text_colors::notice);
        }
        return draw_text_utf8(
                   framebuffer, 67, 25, kPracticeCastrationNotice, text_colors::notice) &&
            draw_text_utf8(
                   framebuffer, 67, 45, kPracticeCastrationQuestion, text_colors::notice);
    case GameMenuScreen::item_effect:
        return false;
    case GameMenuScreen::notice:
        switch (menu.notice()) {
        case GameMenuNotice::leave_protagonist:
            return draw_box(framebuffer, 40, 40, 228U, 27U) &&
                draw_text_utf8(
                    framebuffer, 50, 45, kLeaveProtagonistNotice, text_colors::notice);
        case GameMenuNotice::equipment_unsuitable:
            return draw_box(framebuffer, 70, 18, 170U, 30U) &&
                draw_text_utf8(
                    framebuffer, 75, 25, kEquipmentUnsuitableNotice, text_colors::notice);
        case GameMenuNotice::practice_magic_full:
            return draw_box(framebuffer, 70, 18, 170U, 30U) &&
                draw_text_utf8(
                    framebuffer, 75, 25, kPracticeMagicFullNotice, text_colors::notice);
        case GameMenuNotice::practice_unsuitable:
            return draw_box(framebuffer, 70, 18, 180U, 30U) &&
                draw_text_utf8(
                    framebuffer, 80, 25, kPracticeUnsuitableNotice, text_colors::notice);
        }
        return false;
    case GameMenuScreen::system:
        return render_game_menu_main(menu, framebuffer) && render_system_menu();
    case GameMenuScreen::load_slots:
    case GameMenuScreen::save_slots:
        if (!render_game_menu_main(menu, framebuffer) || !render_system_menu() ||
            !draw_box(framebuffer, 120, 18, 26U, 72U)) {
            return false;
        }
        for (std::size_t index = 0U; index < kSlotLabels.size(); ++index) {
            if (!draw_text_utf8(
                    framebuffer,
                    124,
                    25 + static_cast<int>(index) * 20,
                    kSlotLabels[index],
                    index == menu.slot_selection() ? text_colors::selected : text_colors::menu_normal)) {
                return false;
            }
        }
        return true;
    case GameMenuScreen::delete_confirmation:
        return false;
    case GameMenuScreen::quit_confirmation:
        return render_game_menu_main(menu, framebuffer) && render_system_menu() &&
            draw_box(framebuffer, 120, 18, 177U, 31U) &&
            draw_text_utf8(framebuffer, 124, 25, kQuitPrompt, text_colors::notice);
    }
    return false;
}

bool BasicUiRenderer::render_death_menu(
    const DeathMenuController& menu,
    render::IndexedFramebuffer& framebuffer) {
    constexpr int kPanelX = 205;
    constexpr int kPanelY = 90;
    constexpr std::uint16_t kClearWidth = 115U;
    constexpr std::uint16_t kPanelWidth = 101U;
    constexpr std::uint16_t kClearHeight = 90U;
    constexpr std::uint16_t kPanelHeight = 50U;
    if (!valid() || menu.main_selection() >= kDeathMenuItems.size() ||
        !framebuffer.fill_rectangle(
            kPanelX,
            kPanelY,
            kClearWidth,
            kClearHeight,
            palette_colors::black) ||
        !draw_box(
            framebuffer,
            kPanelX,
            kPanelY,
            kPanelWidth,
            kPanelHeight)) {
        return false;
    }
    for (std::size_t index = 0U; index < kDeathMenuItems.size(); ++index) {
        if (!draw_text_utf8(
                framebuffer,
                215,
                95 + static_cast<int>(index) * 20,
                kDeathMenuItems[index],
                index == menu.main_selection()
                    ? text_colors::selected
                    : text_colors::menu_normal)) {
            return false;
        }
    }
    return menu.screen() != DeathMenuScreen::quit_confirmation ||
        (draw_box(framebuffer, 71, 180, 177U, 20U) &&
         draw_text_utf8(
             framebuffer, 75, 182, kExitPrompt, text_colors::notice));
}

bool BasicUiRenderer::render_error(
    const std::span<const std::uint8_t> legacy_message,
    render::IndexedFramebuffer& framebuffer) {
    if (!draw_box(framebuffer, 20, 150, 280U, 31U)) {
        return false;
    }
    return draw_text_big5(
        framebuffer, 24, 157, text::Big5TextView{legacy_message}, text_colors::notice);
}

bool BasicUiRenderer::draw_text_utf8(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::u8string_view text,
    const render::TextColors colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::draw_text_utf8(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        colors);
}

bool BasicUiRenderer::draw_text_mixed(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::GameText& text,
    const render::TextColors colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::draw_text_mixed(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        colors);
}

bool BasicUiRenderer::draw_text_big5(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::Big5TextView text,
    const render::TextColors colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::draw_text_big5(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        colors);
}

bool BasicUiRenderer::draw_box(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height) {
    if (width <= 10U || height <= 10U || x < 0 || y < 0 ||
        x + static_cast<int>(width) > framebuffer.coordinate_width() ||
        y + static_cast<int>(height) > framebuffer.coordinate_height()) {
        return false;
    }
    update_panel_palette(framebuffer.palette());
    const auto blend = [this, &framebuffer](
                           const int left,
                           const int top,
                           const int rectangle_width,
                           const int rectangle_height) {
        return framebuffer.transform_rectangle(
            left,
            top,
            rectangle_width,
            rectangle_height,
            [this](std::uint8_t& destination) {
                destination = blend_panel_pixel(destination);
            });
    };
    const auto w = static_cast<int>(width);
    const auto h = static_cast<int>(height);
    if (!blend(x + 5, y, w - 10, 1) ||
        !blend(x + 4, y + 1, w - 8, 1) ||
        !blend(x + 3, y + 2, w - 6, 1) ||
        !blend(x + 2, y + 3, w - 4, 1) ||
        !blend(x + 1, y + 4, w - 2, 1) ||
        !blend(x, y + 5, w, h - 10) ||
        !blend(x + 1, y + h - 5, w - 2, 1) ||
        !blend(x + 2, y + h - 4, w - 4, 1) ||
        !blend(x + 3, y + h - 3, w - 6, 1) ||
        !blend(x + 4, y + h - 2, w - 8, 1) ||
        !blend(x + 5, y + h - 1, w - 10, 1)) {
        return false;
    }

    const auto fill = [&framebuffer](
                          const int left,
                          const int top,
                          const int rectangle_width,
                          const int rectangle_height) {
        return framebuffer.fill_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width),
            static_cast<std::uint16_t>(rectangle_height),
            palette_colors::panel_outline);
    };
    return fill(x + 5, y + 1, w - 10, 1) &&
        fill(x + 4, y + 2, 1, 2) && fill(x + w - 5, y + 2, 1, 2) &&
        fill(x + 2, y + 4, 2, 1) && fill(x + w - 4, y + 4, 2, 1) &&
        fill(x + 1, y + 5, 1, h - 10) && fill(x + w - 2, y + 5, 1, h - 10) &&
        fill(x + 2, y + h - 5, 2, 1) && fill(x + w - 4, y + h - 5, 2, 1) &&
        fill(x + 4, y + h - 4, 1, 2) && fill(x + w - 5, y + h - 4, 1, 2) &&
        fill(x + 5, y + h - 2, w - 10, 1);
}

void BasicUiRenderer::update_panel_palette(const compat::LegacyPalette& palette) noexcept {
    const auto same_palette = panel_palette_ready_ && std::equal(
        panel_palette_.begin(),
        panel_palette_.end(),
        palette.begin(),
        [](const compat::Rgb6 left, const compat::Rgb6 right) {
            return left.red == right.red && left.green == right.green && left.blue == right.blue;
        });
    if (same_palette) {
        return;
    }
    panel_palette_ = palette;
    panel_palette_ready_ = true;
    for (int red = 0; red < 16; ++red) {
        for (int green = 0; green < 16; ++green) {
            for (int blue = 0; blue < 16; ++blue) {
                auto best_distance = 30'000;
                std::uint8_t best_index{};
                for (std::size_t index = 0U; index < panel_palette_.size(); ++index) {
                    const auto red_delta = red * 4 + 2 - panel_palette_[index].red;
                    const auto green_delta = green * 4 + 2 - panel_palette_[index].green;
                    const auto blue_delta = blue * 4 + 2 - panel_palette_[index].blue;
                    const auto distance = red_delta * red_delta + green_delta * green_delta +
                        blue_delta * blue_delta;
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = static_cast<std::uint8_t>(index);
                    }
                }
                panel_rgb4_lookup_[
                    static_cast<std::size_t>(red * 256 + green * 16 + blue)] = best_index;
            }
        }
    }
}

std::uint8_t BasicUiRenderer::blend_panel_pixel(
    const std::uint8_t destination) const noexcept {
    const auto blend_component = [](const std::uint8_t source, const std::uint8_t target) {
        return static_cast<std::uint8_t>(3 * source / 32 + 5 * target / 32);
    };
    const auto source = panel_palette_[palette_colors::black];
    const auto target = panel_palette_[destination];
    const auto red = blend_component(source.red, target.red);
    const auto green = blend_component(source.green, target.green);
    const auto blue = blend_component(source.blue, target.blue);
    return panel_rgb4_lookup_[static_cast<std::size_t>(red) * 256U +
                              static_cast<std::size_t>(green) * 16U + blue];
}

bool BasicUiRenderer::render_items(
    const GameMenuController& menu,
    const model::RangerState& ranger,
    render::IndexedFramebuffer& framebuffer) {
    if (!draw_box(framebuffer, 45, 2, 230U, 23U) ||
        !draw_box(framebuffer, 45, 27, 230U, 23U) ||
        !draw_box(framebuffer, 45, 52, 230U, 145U)) {
        return false;
    }
    const auto draw_scroll_line = [&framebuffer](
                                      const int x,
                                      const int y,
                                      const int width,
                                      const int height) {
        return framebuffer.fill_rectangle(
            x,
            y,
            static_cast<std::uint16_t>(width),
            static_cast<std::uint16_t>(height),
            palette_colors::scroll_indicator);
    };
    const auto item_metric = legacy_item_metric(ranger);
    const auto page = static_cast<int>(menu.item_page());
    if (item_metric > 5 * (page + 3)) {
        if (!draw_scroll_line(267, 175, 2, 1) ||
            !draw_scroll_line(266, 174, 4, 1) ||
            !draw_scroll_line(265, 173, 6, 1) ||
            !draw_scroll_line(264, 172, 8, 1) ||
            !draw_scroll_line(266, 161, 4, 11)) {
            return false;
        }
    }
    if (item_metric > 15 && page > 0) {
        if (!draw_scroll_line(267, 72, 2, 1) ||
            !draw_scroll_line(266, 73, 4, 1) ||
            !draw_scroll_line(265, 74, 6, 1) ||
            !draw_scroll_line(264, 75, 8, 1) ||
            !draw_scroll_line(266, 76, 4, 11)) {
            return false;
        }
    }

    const auto inventory_slots = menu.inventory_slots();
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 5; ++column) {
            const auto x = 55 + 42 * column;
            const auto y = 62 + 42 * row;
            if (!framebuffer.outline_rectangle(
                    x, y, 40U, 40U, palette_colors::inventory_outline)) {
                return false;
            }
            const auto list_index = static_cast<std::size_t>(5 * (page + row) + column);
            if (list_index >= inventory_slots.size() || inventory_slots[list_index] < 0 ||
                static_cast<std::size_t>(inventory_slots[list_index]) >= model::kInventoryCount) {
                continue;
            }
            const auto inventory_slot = static_cast<std::size_t>(inventory_slots[list_index]);
            const auto item_id = ranger.header.inventory_item(inventory_slot).value;
            if (item_id >= 0 && static_cast<std::size_t>(item_id) < ranger.items.size() &&
                !draw_item_icon(framebuffer, item_id, x, y)) {
                return false;
            }
        }
    }

    if (!framebuffer.outline_rectangle(
            55 + 42 * static_cast<int>(menu.item_column()),
            62 + 42 * static_cast<int>(menu.item_row()),
            40U,
            40U,
            palette_colors::inventory_selection_outline)) {
        return false;
    }
    const auto selection = static_cast<std::size_t>(menu.item_selection());
    if (selection >= inventory_slots.size() || inventory_slots[selection] < 0 ||
        static_cast<std::size_t>(inventory_slots[selection]) >= model::kInventoryCount) {
        return true;
    }
    const auto inventory_slot = static_cast<std::size_t>(inventory_slots[selection]);
    const auto item_id = ranger.header.inventory_item(inventory_slot).value;
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger.items.size()) {
        return true;
    }
    const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
    const auto name = item.word(model::item_word::show_introduction) == 0
        ? fixed_text(item.bytes, model::item_word::name_byte, model::item_word::name_bytes)
        : fixed_text(
              item.bytes,
              2U * model::item_word::secondary_name_begin,
              2U * model::item_word::secondary_name_count);
    const auto item_type = item.word(model::item_word::item_type);
    const auto user = item.word(model::item_word::user);
    const auto name_center =
        (item_type == 1 || item_type == 2) && user > 0 ? 140 : 160;
    if (!draw_text_big5(
            framebuffer,
            name_center - 4 * static_cast<int>(name.size()),
            5,
            text::Big5TextView{name},
            text_colors::notice)) {
        return false;
    }

    if (item.word(model::item_word::id) == 0x00B6) {
        const auto text = coordinate_item_text(ranger, menu.context());
        if (!draw_text_utf8(framebuffer, 48, 30, text, text_colors::menu_normal)) {
            return false;
        }
    } else {
        const auto introduction = fixed_text(
            item.bytes,
            model::item_word::introduction_byte,
            model::item_word::introduction_bytes);
        if (!draw_text_big5(
                framebuffer,
                160 - 4 * static_cast<int>(introduction.size()),
                30,
                text::Big5TextView{introduction},
                text_colors::menu_normal)) {
            return false;
        }
    }

    if (user >= 0) {
        if (static_cast<std::size_t>(user) >= ranger.roles.size()) {
            return false;
        }
        const auto role_name = fixed_text(
            ranger.roles[static_cast<std::size_t>(user)].bytes,
            model::role_word::name_byte,
            model::role_word::name_bytes);
        text::GameText user_text;
        user_text.append_utf8(kOpenParenthesis);
        user_text.append_legacy(text::Big5TextView{role_name});
        user_text.append_utf8(kCloseParenthesis);
        if (!draw_text_mixed(framebuffer, 205, 5, user_text, text_colors::menu_normal)) {
            return false;
        }
    }

    const auto count = ranger.header.inventory_count(inventory_slot);
    if (count > 1) {
        std::u8string count_text;
        append_number(count_text, count, 2);
        if (!draw_text_utf8(framebuffer, 215, 5, kCountMarker, text_colors::menu_normal) ||
            !draw_text_utf8(framebuffer, 235, 5, count_text, text_colors::selected)) {
            return false;
        }
    }
    return true;
}

bool BasicUiRenderer::draw_item_icon(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t item_id,
    const int x,
    const int y) const {
    const auto sprite_index = render::legacy_item_sprite_index(item_id);
    if (!sprite_index.has_value() || *sprite_index >= item_sprites_.entry_count()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(item_sprites_.entry(*sprite_index));
    if (!frame.valid()) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, frame, x, y);
    return true;
}

}  // namespace openlegend::ui
