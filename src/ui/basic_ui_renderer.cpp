#include "openlegend/ui/basic_ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>

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
constexpr std::array<std::uint8_t, 5> kLevelLabel{0xB5U, 0xA5U, 0xAFU, 0xC5U, 0x20U};
constexpr std::array<std::uint8_t, 5> kLifeLabel{0xA5U, 0xCDU, 0xA9U, 0x52U, 0x20U};
constexpr std::array<std::uint8_t, 5> kMpLabel{0xA4U, 0xBAU, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 5> kPowerLabel{0xCAU, 0x5EU, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 5> kExperienceLabel{0xB8U, 0x67U, 0xC5U, 0xE7U, 0x20U};
constexpr std::array<std::uint8_t, 5> kUpgradeLabel{0xA4U, 0xC9U, 0xAFU, 0xC5U, 0x20U};
constexpr std::array<std::uint8_t, 7> kAttackLabel{0xA7U, 0xF0U, 0xC0U, 0xBBU, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 7> kDefenceLabel{0xA8U, 0xBEU, 0xBFU, 0x6DU, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 5> kSpeedLabel{0xBBU, 0xB4U, 0xA5U, 0x5CU, 0x20U};
constexpr std::array<std::uint8_t, 9> kMedicineLabel{0xC2U, 0xE5U, 0xC0U, 0xF8U, 0xAFU, 0xE0U, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 9> kUsePoisonLabel{0xA5U, 0xCEU, 0xACU, 0x72U, 0xAFU, 0xE0U, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 9> kDetoxLabel{0xB8U, 0xD1U, 0xACU, 0x72U, 0xAFU, 0xE0U, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 9> kFistLabel{0xAEU, 0xB1U, 0xB4U, 0x78U, 0xA5U, 0x5CU, 0xA4U, 0xD2U, 0x20U};
constexpr std::array<std::uint8_t, 9> kSwordLabel{0xB1U, 0x73U, 0xBCU, 0x43U, 0xAFU, 0xE0U, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 9> kKnifeLabel{0xADU, 0x41U, 0xA4U, 0x4DU, 0xA7U, 0xDEU, 0xA5U, 0xA9U, 0x20U};
constexpr std::array<std::uint8_t, 9> kUnusualLabel{0xAFU, 0x53U, 0xAEU, 0xEDU, 0xA7U, 0x4CU, 0xBEU, 0xB9U, 0x20U};
constexpr std::array<std::uint8_t, 9> kHiddenLabel{0xB7U, 0x74U, 0xBEU, 0xB9U, 0xA7U, 0xDEU, 0xA5U, 0xA9U, 0x20U};
constexpr std::array<std::uint8_t, 9> kEquipmentLabel{0xB8U, 0xCBU, 0xB3U, 0xC6U, 0xAAU, 0xABU, 0xABU, 0x7EU, 0x20U};
constexpr std::array<std::uint8_t, 9> kPracticeLabel{0xADU, 0xD7U, 0xBDU, 0x6DU, 0xAAU, 0xABU, 0xABU, 0x7EU, 0x20U};
constexpr std::array<std::uint8_t, 9> kMagicLabel{0xA9U, 0xD2U, 0xB7U, 0x7CU, 0xA5U, 0x5CU, 0xA4U, 0xD2U, 0x20U};
constexpr std::array<std::uint8_t, 13> kNamePrompt{
    0xBDU, 0xD0U, 0xBFU, 0xE9U, 0xA4U, 0x4AU, 0xA9U,
    0x6DU, 0xA6U, 0x57U, 0x20U, 0x20U, 0x3AU};
constexpr std::array<std::uint8_t, 10> kZhuyinPrompt{
    0xA1U, 0x5DU, 0xAAU, 0x60U, 0xADU, 0xB5U, 0xA1U, 0x5EU, 0xA1U, 0x47U};
constexpr std::array<std::uint8_t, 10> kAlnumPrompt{
    0xA1U, 0x5DU, 0xADU, 0x5EU, 0xBCU, 0xC6U, 0xA1U, 0x5EU, 0xA1U, 0x47U};
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
    if (!draw_text(framebuffer, 48, 141, kNamePrompt) ||
        !draw_text(
            framebuffer,
            3,
            161,
            editor.mode() == NameInputMode::zhuyin
                ? std::span<const std::uint8_t>{kZhuyinPrompt}
                : std::span<const std::uint8_t>{kAlnumPrompt}) ||
        !draw_text(framebuffer, 158, 141, editor.name(), 0x0503U)) {
        return false;
    }

    const auto visible = editor.visible_candidate_count();
    const auto begin = editor.candidate_page() * 8U;
    for (std::size_t index = 0U; index < visible; ++index) {
        std::array<std::uint8_t, 4> label{
            static_cast<std::uint8_t>('1' + index),
            editor.candidates()[begin + index][0],
            editor.candidates()[begin + index][1],
            0U};
        if (!draw_text(framebuffer, 4 + static_cast<int>(index) * 38, 181, label)) {
            return false;
        }
    }
    return true;
}

bool BasicUiRenderer::render_attributes(
    const TitleMenuRenderer& title,
    const model::RoleRecord& protagonist,
    const std::span<const std::uint8_t> name,
    const bool cheat_active,
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
        if (!draw_text(
                framebuffer,
                10 + static_cast<int>(column) * 75,
                152 + static_cast<int>(row) * 16,
                line,
                cheat_active ? 0x1F1DU : 0x1715U)) {
            return false;
        }
    }
    return true;
}

bool BasicUiRenderer::render_world_status(
    const model::RangerState& ranger, render::IndexedFramebuffer& framebuffer) {
    framebuffer.clear(0U);
    return render_status_panel(ranger, 0U, 0U, framebuffer);
}

bool BasicUiRenderer::render_game_menu(
    const GameMenuController& menu,
    const model::RangerState& ranger,
    render::IndexedFramebuffer& framebuffer) {
    switch (menu.screen()) {
    case GameMenuScreen::main:
        if (!draw_box(framebuffer, 20, 18, 42U, static_cast<std::uint16_t>(12U + 20U * menu.visible_main_items()))) {
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
    case GameMenuScreen::status_panel:
        return render_status_panel(
            ranger, menu.party_selection(), menu.status_page(), framebuffer);
    case GameMenuScreen::items:
        return render_items(ranger, menu.item_selection(), framebuffer);
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
    if (width < 2U || height < 2U ||
        !framebuffer.outline_rectangle(x, y, width, height, 0xFFU)) {
        return false;
    }
    return framebuffer.fill_rectangle(
        x + 1,
        y + 1,
        static_cast<std::uint16_t>(width - 2U),
        static_cast<std::uint16_t>(height - 2U),
        0U);
}

bool BasicUiRenderer::render_status_panel(
    const model::RangerState& ranger,
    const std::uint8_t party_index,
    const std::uint8_t page,
    render::IndexedFramebuffer& framebuffer) {
    const auto role_id = ranger.header.team_member(party_index).value;
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger.roles.size()) {
        return false;
    }
    const auto& role = ranger.roles[static_cast<std::size_t>(role_id)];
    framebuffer.clear(0U);
    if (!draw_text(
            framebuffer,
            80,
            70,
            fixed_text(role.bytes, model::role_word::name_byte, model::role_word::name_bytes),
            0x6663U) ||
        !draw_text(
            framebuffer,
            180,
            70,
            fixed_text(
                role.bytes, model::role_word::nickname_byte, model::role_word::nickname_bytes),
            0x6663U)) {
        return false;
    }

    const auto draw_number = [this, &framebuffer](
                                 const int x, const int y, const std::int16_t value,
                                 const std::uint16_t color = 0x0705U) {
        std::vector<std::uint8_t> number;
        append_number(number, value);
        return draw_text(framebuffer, x, y, number, color);
    };
    const auto draw_pair = [this, &framebuffer, &draw_number](
                               const int y,
                               const std::span<const std::uint8_t> label,
                               const std::int16_t current,
                               const std::optional<std::int16_t> maximum = std::nullopt) {
        if (!draw_text(framebuffer, 60, y, label, 0x2321U) ||
            !draw_number(100, y, current)) {
            return false;
        }
        if (!maximum.has_value()) {
            return true;
        }
        constexpr std::array<std::uint8_t, 1> slash{'/'};
        return draw_text(framebuffer, 120, y, slash, 0x6663U) &&
               draw_number(127, y, *maximum);
    };

    if (page == 0U) {
        if (!draw_pair(90, kLevelLabel, role.word(model::role_word::level)) ||
            !draw_pair(
                107,
                kLifeLabel,
                role.word(model::role_word::hp),
                role.word(model::role_word::maximum_hp)) ||
            !draw_pair(
                124,
                kMpLabel,
                role.word(model::role_word::mp),
                role.word(model::role_word::maximum_mp)) ||
            !draw_pair(
                141,
                kPowerLabel,
                role.word(model::role_word::physical_power),
                std::int16_t{100}) ||
            !draw_pair(158, kExperienceLabel, role.word(model::role_word::experience)) ||
            !draw_text(framebuffer, 60, 175, kUpgradeLabel, 0x2321U)) {
            return false;
        }
        struct StatusField {
            std::span<const std::uint8_t> label;
            std::size_t word;
        };
        const std::array<StatusField, 11> fields{{
            {kAttackLabel, model::role_word::attack},
            {kDefenceLabel, model::role_word::defence},
            {kSpeedLabel, model::role_word::speed},
            {kMedicineLabel, model::role_word::medicine},
            {kUsePoisonLabel, model::role_word::use_poison},
            {kDetoxLabel, model::role_word::detoxification},
            {kFistLabel, model::role_word::fist},
            {kSwordLabel, model::role_word::sword},
            {kKnifeLabel, model::role_word::knife},
            {kUnusualLabel, model::role_word::unusual},
            {kHiddenLabel, model::role_word::hidden_weapon},
        }};
        for (std::size_t index = 0U; index < fields.size(); ++index) {
            const auto y = 5 + static_cast<int>(index) * 17;
            if (!draw_text(framebuffer, 160, y, fields[index].label, 0x6663U) ||
                !draw_number(230, y, role.word(fields[index].word))) {
                return false;
            }
        }
        return true;
    }

    if (!draw_text(framebuffer, 60, 90, kEquipmentLabel, 0x2321U)) {
        return false;
    }
    for (std::size_t index = 0U; index < model::role_word::equipment_count; ++index) {
        const auto item_id = role.word(model::role_word::equipment_begin + index);
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger.items.size()) {
            continue;
        }
        const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
        if (!draw_text(
                framebuffer,
                60,
                107 + static_cast<int>(index) * 17,
                fixed_text(item.bytes, model::item_word::name_byte, model::item_word::name_bytes),
                0x0705U)) {
            return false;
        }
    }
    if (!draw_text(framebuffer, 60, 141, kPracticeLabel, 0x2321U)) {
        return false;
    }
    const auto practice_id = role.word(model::role_word::practice_item);
    if (practice_id >= 0 && static_cast<std::size_t>(practice_id) < ranger.items.size()) {
        const auto& item = ranger.items[static_cast<std::size_t>(practice_id)];
        if (!draw_text(
                framebuffer,
                60,
                158,
                fixed_text(item.bytes, model::item_word::name_byte, model::item_word::name_bytes),
                0x0705U) ||
            !draw_number(60, 175, role.word(model::role_word::item_experience))) {
            return false;
        }
    }
    if (!draw_text(framebuffer, 160, 5, kMagicLabel, 0x6663U)) {
        return false;
    }
    for (std::size_t index = 0U; index < model::role_word::magic_count; ++index) {
        const auto magic_id = role.word(model::role_word::magic_id_begin + index);
        if (magic_id <= 0 || static_cast<std::size_t>(magic_id) >= ranger.magics.size()) {
            continue;
        }
        const auto& magic = ranger.magics[static_cast<std::size_t>(magic_id)];
        const auto y = 22 + static_cast<int>(index) * 17;
        if (!draw_text(
                framebuffer,
                160,
                y,
                fixed_text(
                    magic.bytes, model::magic_word::name_byte, model::magic_word::name_bytes),
                0x0705U) ||
            !draw_number(
                242,
                y,
                static_cast<std::int16_t>(
                    role.word(model::role_word::magic_level_begin + index) / 100 + 1),
                0x6663U)) {
            return false;
        }
    }
    return true;
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
