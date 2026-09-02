#include "openlegend/ui/basic_ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>

#include "openlegend/model/new_game.hpp"

namespace openlegend::ui {
namespace {

constexpr std::array<std::array<std::uint8_t, 4>, 6> kMainLabels{{
    {0xC2U, 0xE5U, 0xC0U, 0xF8U},
    {0xB8U, 0xD1U, 0xACU, 0x72U},
    {0xAAU, 0xABU, 0xABU, 0x7EU},
    {0xAAU, 0xACU, 0xBAU, 0x41U},
    {0xC2U, 0xF7U, 0xB6U, 0xA4U},
    {0xA8U, 0x74U, 0xB2U, 0xCEU},
}};
constexpr std::array<std::array<std::uint8_t, 4>, 3> kSystemLabels{{
    {0xC5U, 0xAAU, 0xC0U, 0xC9U},
    {0xA6U, 0x73U, 0xC0U, 0xC9U},
    {0xC2U, 0xF7U, 0xB6U, 0x7DU},
}};
constexpr std::array<std::array<std::uint8_t, 2>, 3> kSlotLabels{{
    {0xA4U, 0x40U},
    {0xA4U, 0x47U},
    {0xA4U, 0x54U},
}};
constexpr std::array<std::uint8_t, 13> kNamePrompt{
    0xBDU, 0xD0U, 0xBFU, 0xE9U, 0xA4U, 0x4AU, 0xA9U,
    0x6DU, 0xA6U, 0x57U, 0x20U, 0x20U, 0x3AU};
constexpr std::array<std::uint8_t, 10> kZhuyinPrompt{
    0xA1U, 0x5DU, 0xAAU, 0x60U, 0xADU, 0xB5U, 0xA1U, 0x5EU, 0xA1U, 0x47U};
constexpr std::array<std::uint8_t, 10> kAlnumPrompt{
    0xA1U, 0x5DU, 0xADU, 0x5EU, 0xBCU, 0xC6U, 0xA1U, 0x5EU, 0xA1U, 0x47U};
constexpr std::array<std::uint8_t, 6> kNoNameCandidates{
    0xB5U, 0x4CU, 0xA6U, 0xB9U, 0xA6U, 0x72U};
constexpr std::array<std::uint8_t, 6> kCandidateBothPages{
    0xA1U, 0xD5U, 0xA1U, 0xFEU, 0xA1U, 0xD6U};
constexpr std::array<std::uint8_t, 2> kCandidatePreviousPage{0xA1U, 0xD5U};
constexpr std::array<std::uint8_t, 2> kCandidateNextPage{0xA1U, 0xD6U};
constexpr std::array<std::uint8_t, 31> kAttributeQuestion{
    0x20U, 0x20U, 0x20U, 0xB3U, 0x6FU, 0xBCU, 0xCBU, 0xAAU, 0xBAU, 0xC4U,
    0xDDU, 0xA9U, 0xCAU, 0xBAU, 0xA1U, 0xB7U, 0x4EU, 0xB6U, 0xDCU, 0xA1U,
    0x48U, 0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU, 0xA2U, 0xDCU, 0xA1U,
    0x5EU};
constexpr std::array<std::uint8_t, 22> kQuitPrompt{
    0xAFU, 0x75U, 0xADU, 0x6EU, 0xC2U, 0xF7U, 0xB6U, 0x7DU, 0xB9U, 0x43U,
    0xC0U, 0xB8U, 0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU, 0xA2U, 0xDCU,
    0xA1U, 0x5EU};
constexpr std::array<std::uint8_t, 8> kIoWaitLabel{
    0xBDU, 0xD0U, 0xB5U, 0x79U, 0xADU, 0xD4U, 0xA1U, 0x49U};
constexpr std::array<std::uint8_t, 26> kLeaveProtagonistNotice{
    0xA9U, 0xEAU, 0xBAU, 0x70U, 0xA1U, 0x49U, 0xA8U, 0x53U, 0xA6U,
    0xB3U, 0xA7U, 0x41U, 0xB9U, 0x43U, 0xC0U, 0xB8U, 0xB6U, 0x69U,
    0xA6U, 0xE6U, 0xA4U, 0xA3U, 0xA4U, 0x55U, 0xA5U, 0x68U};
constexpr std::array<std::uint8_t, 20> kEquipmentUnsuitableNotice{
    0xA6U, 0xB9U, 0xA4U, 0x48U, 0xA4U, 0xA3U, 0xBEU, 0x41U, 0xA6U, 0x58U,
    0xB0U, 0x74U, 0xB3U, 0xC6U, 0xA6U, 0xB9U, 0xAAU, 0xABU, 0xABU, 0x7EU};
constexpr std::array<std::uint8_t, 24> kPracticeAssignedNotice{
    0xA6U, 0xB9U, 0xAAU, 0xABU, 0xABU, 0x7EU, 0xB2U, 0x7BU, 0xA6U, 0x62U,
    0xA4U, 0x77U, 0xB8U, 0x67U, 0xA6U, 0xB3U, 0xA4U, 0x48U, 0xADU, 0xD7U,
    0xBDU, 0x6DU, 0xA4U, 0x46U};
constexpr std::array<std::uint8_t, 24> kPracticeReassignQuestion{
    0xACU, 0x4FU, 0xA7U, 0x5FU, 0xADU, 0x6EU, 0xB4U, 0xABU, 0xA4U, 0x48U,
    0xADU, 0xD7U, 0xBDU, 0x6DU, 0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU,
    0xA2U, 0xDCU, 0xA1U, 0x5EU};
constexpr std::array<std::uint8_t, 20> kPracticeMagicFullNotice{
    0xA4U, 0x40U, 0xA4U, 0x48U, 0xA5U, 0x75U, 0xAFU, 0xE0U, 0xADU, 0xD7U,
    0xBDU, 0x6DU, 0xA4U, 0x51U, 0xBAU, 0xD8U, 0xA5U, 0x5CU, 0xA4U, 0xD2U};
constexpr std::array<std::uint8_t, 24> kPracticeCastrationNotice{
    0xADU, 0xD7U, 0xBDU, 0x6DU, 0xA6U, 0xB9U, 0xAEU, 0xD1U, 0xA5U, 0xB2U,
    0xB6U, 0xB7U, 0xA5U, 0xFDU, 0xA6U, 0xE6U, 0xB4U, 0xA7U, 0xBCU, 0x43U,
    0xA6U, 0xDBU, 0xAEU, 0x63U};
constexpr std::array<std::uint8_t, 24> kPracticeCastrationQuestion{
    0xA7U, 0x41U, 0xACU, 0x4FU, 0xA7U, 0x5FU, 0xA4U, 0xB4U, 0xADU, 0x6EU,
    0xADU, 0xD7U, 0xBDU, 0x6DU, 0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU,
    0xA2U, 0xDCU, 0xA1U, 0x5EU};
constexpr std::array<std::uint8_t, 20> kPracticeUnsuitableNotice{
    0xA6U, 0xB9U, 0xA4U, 0x48U, 0xA4U, 0xA3U, 0xBEU, 0x41U, 0xA6U, 0x58U,
    0xADU, 0xD7U, 0xBDU, 0x6DU, 0xA6U, 0xB9U, 0xAAU, 0xABU, 0xABU, 0x7EU};

struct AttributeLine {
    std::array<std::uint8_t, 6> label;
    std::size_t word;
};

constexpr std::array<AttributeLine, 12> kAttributeLines{{
    {{{0xA4U, 0xBAU, 0xA4U, 0x4FU, 0xA1U, 0x47U}}, model::role_word::maximum_mp},
    {{{0xAAU, 0x5AU, 0xA4U, 0x4FU, 0xA1U, 0x47U}}, model::role_word::attack},
    {{{0xBBU, 0xB4U, 0xA5U, 0x5CU, 0xA1U, 0x47U}}, model::role_word::speed},
    {{{0xA8U, 0xBEU, 0xBFU, 0x6DU, 0xA1U, 0x47U}}, model::role_word::defence},
    {{{0xA5U, 0xCDU, 0xA9U, 0x52U, 0xA1U, 0x47U}}, model::role_word::maximum_hp},
    {{{0xC2U, 0xE5U, 0xC0U, 0xF8U, 0xA1U, 0x47U}}, model::role_word::medicine},
    {{{0xA8U, 0xCFU, 0xACU, 0x72U, 0xA1U, 0x47U}}, model::role_word::use_poison},
    {{{0xB8U, 0xD1U, 0xACU, 0x72U, 0xA1U, 0x47U}}, model::role_word::detoxification},
    {{{0xAEU, 0xB1U, 0xB4U, 0x78U, 0xA1U, 0x47U}}, model::role_word::fist},
    {{{0xBCU, 0x43U, 0xB3U, 0x4EU, 0xA1U, 0x47U}}, model::role_word::sword},
    {{{0xA4U, 0x4DU, 0xB3U, 0x4EU, 0xA1U, 0x47U}}, model::role_word::knife},
    {{{0xB7U, 0x74U, 0xBEU, 0xB9U, 0xA1U, 0x47U}}, model::role_word::hidden_weapon},
}};

[[nodiscard]] std::vector<std::uint8_t> terminated(std::span<const std::uint8_t> text) {
    std::vector<std::uint8_t> result(text.begin(), text.end());
    result.push_back(0U);
    return result;
}

void append_number(std::vector<std::uint8_t>& text, const std::int16_t value, const int width = 0) {
    std::array<char, 16> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    const auto count = static_cast<int>(converted.ptr - buffer.data());
    for (int index = count; index < width; ++index) {
        text.push_back(' ');
    }
    for (const auto* cursor = buffer.data(); cursor != converted.ptr; ++cursor) {
        text.push_back(static_cast<std::uint8_t>(*cursor));
    }
}

[[nodiscard]] std::optional<std::array<std::uint8_t, 2>> zhuyin_label(
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
[[nodiscard]] std::span<const std::uint8_t> fixed_text(
    const std::array<std::uint8_t, ByteCount>& bytes,
    const std::size_t begin,
    const std::size_t maximum) {
    const auto source = std::span<const std::uint8_t>{bytes}.subspan(begin, maximum);
    const auto end = std::find(source.begin(), source.end(), std::uint8_t{0U});
    return source.first(static_cast<std::size_t>(std::distance(source.begin(), end)));
}

}  // namespace

BasicUiRenderer::BasicUiRenderer(const resource::DataRoot& data_root) {
    auto ascii = data_root.read("FONT.X16");
    if (!ascii) {
        error_ = ascii.error;
        return;
    }
    auto big5 = data_root.read("FONT.C16");
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
        !framebuffer.fill_rectangle(0, 140, 320U, 60U, 0U)) {
        return false;
    }
    const auto has_candidates = !editor.candidates().empty();
    if (!draw_text(framebuffer, 48, 141, kNamePrompt) ||
        !draw_text(
            framebuffer,
            3,
            161,
            editor.mode() == NameInputMode::zhuyin
                ? std::span<const std::uint8_t>{kZhuyinPrompt}
                : std::span<const std::uint8_t>{kAlnumPrompt},
            has_candidates ? 0x1719U : 0x1715U) ||
        !draw_text(framebuffer, 158, 141, editor.display_name(), 0x0503U)) {
        return false;
    }
    if (!editor.accepted() && editor.name().size() < model::kNewGameNameMaximumBytes &&
        !framebuffer.fill_rectangle(
            158 + static_cast<int>(editor.name().size()) * 8,
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
            if (!draw_text(framebuffer, x, 180, digit, 0x1719U)) {
                return false;
            }
            x += 8;
            if ((*candidate)[0] <= 0x7FU) {
                const std::array<std::uint8_t, 1> first{(*candidate)[0]};
                if (!draw_text(framebuffer, x, 180, first, 0x1719U)) {
                    return false;
                }
                x += 8;
                if ((*candidate)[1] <= 0x7FU) {
                    const std::array<std::uint8_t, 1> second{(*candidate)[1]};
                    if (!draw_text(framebuffer, x, 180, second, 0x1719U)) {
                        return false;
                    }
                }
            } else {
                const std::array<std::uint8_t, 2> pair{
                    (*candidate)[0], (*candidate)[1]};
                if (!draw_text(framebuffer, x, 180, pair, 0x1719U) &&
                    editor.candidate_page() >= 0) {
                    return false;
                }
            }
        }
        if (editor.candidate_page() == 0) {
            return !editor.has_next_candidate_page() ||
                draw_text(framebuffer, 300, 180, kCandidateNextPage, 0x1719U);
        }
        if (editor.has_next_candidate_page()) {
            return draw_text(framebuffer, 270, 180, kCandidateBothPages, 0x1719U);
        }
        return draw_text(framebuffer, 300, 180, kCandidatePreviousPage, 0x1719U);
    }

    const std::array<std::int16_t, 4> composition{
        editor.initial(), editor.medial(), editor.final(), editor.tone()};
    for (std::size_t index = 0U; index < composition.size(); ++index) {
        const auto label = zhuyin_label(static_cast<int>(index + 1U), composition[index]);
        if (label.has_value() &&
            !draw_text(framebuffer, 100 + static_cast<int>(index) * 20, 161, *label)) {
            return false;
        }
    }
    return !editor.no_candidates() ||
        draw_text(framebuffer, 240, 161, kNoNameCandidates, 0x0705U);
}

bool BasicUiRenderer::render_attributes(
    const TitleMenuRenderer& title,
    const model::RoleRecord& protagonist,
    const std::span<const std::uint8_t> name,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid() || !title.render_background(framebuffer) ||
        !framebuffer.fill_rectangle(0, 135, 320U, 65U, 0U)) {
        return false;
    }
    std::vector<std::uint8_t> question(name.begin(), name.end());
    question.insert(question.end(), kAttributeQuestion.begin(), kAttributeQuestion.end());
    if (!draw_text(framebuffer, 10, 135, question, 0x0806U)) {
        return false;
    }
    for (std::size_t index = 0U; index < kAttributeLines.size(); ++index) {
        std::vector<std::uint8_t> line(
            kAttributeLines[index].label.begin(), kAttributeLines[index].label.end());
        append_number(line, protagonist.word(kAttributeLines[index].word), 2);
        const auto column = index % 4U;
        const auto row = index / 4U;
        const auto x = 10 + static_cast<int>(column) * 75;
        const auto y = 152 + static_cast<int>(row) * 16;
        const auto value = protagonist.word(kAttributeLines[index].word);
        const auto highlighted =
            (kAttributeLines[index].word == model::role_word::maximum_mp && value == 40) ||
            (kAttributeLines[index].word == model::role_word::maximum_hp && value == 50) ||
            (kAttributeLines[index].word != model::role_word::maximum_mp &&
             kAttributeLines[index].word != model::role_word::maximum_hp && value == 30);
        if ((highlighted && !framebuffer.fill_rectangle(x, y + 1, 64U, 15U, 21U)) ||
            !draw_text(
                framebuffer,
                x,
                y,
                line,
                highlighted ? 0x1F1DU : 0x1715U)) {
            return false;
        }
    }
    return true;
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
        if (!draw_text(
                framebuffer,
                24,
                25 + static_cast<int>(index) * 20,
                kMainLabels[index],
                index == menu.selection() ? 0x6663U : 0x2321U)) {
            return false;
        }
    }
    return true;
}

bool BasicUiRenderer::render_game_menu(
    const GameMenuController& menu,
    const model::RangerState& ranger,
    render::IndexedFramebuffer& framebuffer) {
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
            if (!draw_text(
                    framebuffer,
                    74,
                    25 + static_cast<int>(index) * 20,
                    fixed_text(role.bytes, model::role_word::name_byte, model::role_word::name_bytes),
                    index == menu.party_selection() ? 0x6663U : 0x2321U)) {
                return false;
            }
        }
        return true;
    case GameMenuScreen::party_notice:
    case GameMenuScreen::status_panel:
        return false;
    case GameMenuScreen::items:
        return render_items(ranger, menu.item_selection(), framebuffer);
    case GameMenuScreen::item_confirmation:
        if (!draw_box(framebuffer, 62, 18, 203U, 50U)) {
            return false;
        }
        if (menu.item_confirmation() == GameMenuItemConfirmation::practice_reassign) {
            return draw_text(framebuffer, 67, 25, kPracticeAssignedNotice, 0x0705U) &&
                draw_text(framebuffer, 67, 45, kPracticeReassignQuestion, 0x0705U);
        }
        return draw_text(framebuffer, 67, 25, kPracticeCastrationNotice, 0x0705U) &&
            draw_text(framebuffer, 67, 45, kPracticeCastrationQuestion, 0x0705U);
    case GameMenuScreen::item_effect:
        return false;
    case GameMenuScreen::notice:
        switch (menu.notice()) {
        case GameMenuNotice::leave_protagonist:
            return draw_box(framebuffer, 40, 40, 228U, 27U) &&
                draw_text(framebuffer, 50, 45, kLeaveProtagonistNotice, 0x0705U);
        case GameMenuNotice::equipment_unsuitable:
            return draw_box(framebuffer, 70, 18, 170U, 30U) &&
                draw_text(framebuffer, 75, 25, kEquipmentUnsuitableNotice, 0x0705U);
        case GameMenuNotice::practice_magic_full:
            return draw_box(framebuffer, 70, 18, 170U, 30U) &&
                draw_text(framebuffer, 75, 25, kPracticeMagicFullNotice, 0x0705U);
        case GameMenuNotice::practice_unsuitable:
            return draw_box(framebuffer, 70, 18, 180U, 30U) &&
                draw_text(framebuffer, 80, 25, kPracticeUnsuitableNotice, 0x0705U);
        }
        return false;
    case GameMenuScreen::system:
        if (!draw_box(framebuffer, 70, 18, 74U, 72U)) {
            return false;
        }
        for (std::size_t index = 0U; index < kSystemLabels.size(); ++index) {
            if (!draw_text(
                    framebuffer,
                    74,
                    25 + static_cast<int>(index) * 20,
                    kSystemLabels[index],
                    index == menu.system_selection() ? 0x6663U : 0x2321U)) {
                return false;
            }
        }
        return true;
    case GameMenuScreen::load_slots:
    case GameMenuScreen::save_slots:
        if (!draw_box(framebuffer, 120, 18, 42U, 72U)) {
            return false;
        }
        for (std::size_t index = 0U; index < kSlotLabels.size(); ++index) {
            if (!draw_text(
                    framebuffer,
                    124,
                    25 + static_cast<int>(index) * 20,
                    kSlotLabels[index],
                    index == menu.slot_selection() ? 0x6663U : 0x2321U)) {
                return false;
            }
        }
        return true;
    case GameMenuScreen::quit_confirmation:
        if (!draw_box(framebuffer, 120, 18, 177U, 31U)) {
            return false;
        }
        return draw_text(framebuffer, 124, 25, kQuitPrompt, 0x0705U);
    }
    return false;
}

bool BasicUiRenderer::render_io_wait(render::IndexedFramebuffer& framebuffer) {
    if (!draw_box(framebuffer, 154, 18, 68U, 31U)) {
        return false;
    }
    return draw_text(framebuffer, 158, 25, kIoWaitLabel, 0x0705U);
}

bool BasicUiRenderer::render_error(
    const std::span<const std::uint8_t> legacy_message,
    render::IndexedFramebuffer& framebuffer) {
    if (!draw_box(framebuffer, 20, 150, 280U, 31U)) {
        return false;
    }
    return draw_text(framebuffer, 24, 157, legacy_message, 0x0705U);
}

bool BasicUiRenderer::draw_text(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t> text,
    const std::uint16_t packed_colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    const auto value = terminated(text);
    return render::draw_legacy_text(
        framebuffer,
        x,
        y,
        value,
        ascii_font_,
        *big5_cache_,
        static_cast<std::uint8_t>(packed_colors & 0xFFU),
        static_cast<std::uint8_t>(packed_colors >> 8U));
}

bool BasicUiRenderer::draw_box(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height) {
    if (width <= 10U || height <= 10U || x < 0 || y < 0 ||
        x + static_cast<int>(width) > render::IndexedFramebuffer::width ||
        y + static_cast<int>(height) > render::IndexedFramebuffer::height) {
        return false;
    }
    update_panel_palette(framebuffer.palette());
    const auto blend = [this, &framebuffer](
                           const int left,
                           const int top,
                           const int rectangle_width,
                           const int rectangle_height) {
        for (int destination_y = top; destination_y < top + rectangle_height; ++destination_y) {
            for (int destination_x = left; destination_x < left + rectangle_width; ++destination_x) {
                auto& destination = framebuffer.row(destination_y)[destination_x];
                destination = blend_panel_pixel(destination);
            }
        }
    };
    const auto w = static_cast<int>(width);
    const auto h = static_cast<int>(height);
    blend(x + 5, y, w - 10, 1);
    blend(x + 4, y + 1, w - 8, 1);
    blend(x + 3, y + 2, w - 6, 1);
    blend(x + 2, y + 3, w - 4, 1);
    blend(x + 1, y + 4, w - 2, 1);
    blend(x, y + 5, w, h - 10);
    blend(x + 1, y + h - 5, w - 2, 1);
    blend(x + 2, y + h - 4, w - 4, 1);
    blend(x + 3, y + h - 3, w - 6, 1);
    blend(x + 4, y + h - 2, w - 8, 1);
    blend(x + 5, y + h - 1, w - 10, 1);

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
            0xFFU);
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
    const auto source = panel_palette_[0U];
    const auto target = panel_palette_[destination];
    const auto red = blend_component(source.red, target.red);
    const auto green = blend_component(source.green, target.green);
    const auto blue = blend_component(source.blue, target.blue);
    return panel_rgb4_lookup_[static_cast<std::size_t>(red) * 256U +
                              static_cast<std::size_t>(green) * 16U + blue];
}

bool BasicUiRenderer::render_items(
    const model::RangerState& ranger,
    const std::uint16_t selection,
    render::IndexedFramebuffer& framebuffer) {
    framebuffer.clear(0U);
    const auto page_begin = static_cast<std::size_t>(selection / 8U) * 8U;
    for (std::size_t row = 0U; row < 8U; ++row) {
        const auto slot = page_begin + row;
        if (slot >= model::kInventoryCount) {
            break;
        }
        const auto item_id = ranger.header.inventory_item(slot).value;
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger.items.size()) {
            break;
        }
        const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
        std::vector<std::uint8_t> line;
        const auto name = fixed_text(item.bytes, model::item_word::name_byte, model::item_word::name_bytes);
        line.insert(line.end(), name.begin(), name.end());
        line.insert(line.end(), {' ', 'x', ' '});
        append_number(line, ranger.header.inventory_count(slot));
        if (!draw_text(
                framebuffer,
                20,
                20 + static_cast<int>(row) * 20,
                line,
                slot == selection ? 0x6663U : 0x2321U)) {
            return false;
        }
    }
    return true;
}

}  // namespace openlegend::ui
