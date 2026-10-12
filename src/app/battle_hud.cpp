#include "battle_hud.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "modern_page_layout.hpp"
#include "openlegend/render/reference_layout.hpp"
#include "openlegend/text/game_strings.hpp"

namespace openlegend::app {
namespace {

namespace strings = text::game_strings;
using page::decimal_text;

// 战斗界面字号（320×200参考像素），约为原版16像素字的一半。
constexpr int kHudReferenceFont = 7;
// 行动面板与武功列表字号大一号。
constexpr int kMenuReferenceFont = 9;

constexpr compat::Rgba8 kShadow{0x00U, 0x00U, 0x00U, 0xFFU};
constexpr compat::Rgba8 kWhite{0xF0U, 0xF0U, 0xF0U, 0xFFU};
constexpr compat::Rgba8 kDim{0x90U, 0x90U, 0x90U, 0xFFU};
constexpr compat::Rgba8 kDisabled{0x60U, 0x60U, 0x60U, 0xFFU};
constexpr compat::Rgba8 kYellow{0xFFU, 0xE0U, 0x40U, 0xFFU};
constexpr compat::Rgba8 kPoisonColor{0xC8U, 0x88U, 0xFFU, 0xFFU};
constexpr compat::Rgba8 kHurtColor{0xFFU, 0x70U, 0x70U, 0xFFU};
constexpr compat::Rgba8 kAllyColor{0x50U, 0xA8U, 0xFFU, 0xFFU};
constexpr compat::Rgba8 kEnemyColor{0xFFU, 0x60U, 0x50U, 0xFFU};
constexpr compat::Rgba8 kBarBack{0x20U, 0x20U, 0x28U, 0xE0U};
constexpr compat::Rgba8 kHpFill{0xB8U, 0x30U, 0x30U, 0xFFU};
constexpr compat::Rgba8 kPowerFill{0x38U, 0x98U, 0x40U, 0xFFU};
constexpr compat::Rgba8 kSelectedCell{0xFFU, 0xE0U, 0x40U, 0x50U};

NODISCARD compat::Rgba8 mp_fill(const std::int64_t mp_type) noexcept {
    switch (mp_type) {
    case 0:
        return {0x80U, 0x50U, 0xD0U, 0xFFU};
    case 1:
        return {0xE0U, 0x80U, 0x30U, 0xFFU};
    case 2:
        return {0xD0U, 0xB0U, 0x40U, 0xFFU};
    default:
        return {0x40U, 0x70U, 0xD0U, 0xFFU};
    }
}

NODISCARD render::rgba::TextColors colors(const compat::Rgba8 foreground) noexcept {
    return {kShadow, foreground};
}

class Hud {
public:
    Hud(ui::ModernUiRenderer& ui_renderer, const compat::LegacyPalette& palette,
        render::RgbaFramebuffer& framebuffer, const int reference_font)
        : ui_(ui_renderer),
          palette_(palette),
          framebuffer_(framebuffer),
          font_{static_cast<std::uint16_t>(render::scale_legacy_reference_length(
              reference_font, framebuffer.logical_width(), framebuffer.logical_height()))},
          metrics_(render::rgba::font_metrics(font_)) {}

    NODISCARD int width() const noexcept { return framebuffer_.logical_width(); }

    NODISCARD int height() const noexcept { return framebuffer_.logical_height(); }

    NODISCARD int unit() const noexcept {
        return render::scale_legacy_reference_length(1, width(), height());
    }

    NODISCARD int line() const noexcept { return static_cast<int>(metrics_.line_height); }

    NODISCARD int big5_advance() const noexcept { return static_cast<int>(metrics_.big5_width); }

    NODISCARD int text_width(const std::u8string_view text) const noexcept {
        int result = 0;
        for (const auto unit : text) {
            if (unit < 0x80U) {
                result += metrics_.ascii_width;
            } else if ((unit & 0xC0U) != 0x80U) {
                result += metrics_.big5_width;
            }
        }
        return result;
    }

    NODISCARD int big5_width(const std::span<const std::uint8_t> bytes) const noexcept {
        int result = 0;
        for (std::size_t index = 0U; index < bytes.size(); ++index) {
            if (bytes[index] >= 0x80U && index + 1U < bytes.size()) {
                result += metrics_.big5_width;
                ++index;
            } else {
                result += metrics_.ascii_width;
            }
        }
        return result;
    }

    void text(const int x, const int y, const std::u8string_view value, const compat::Rgba8 color) {
        if (!value.empty() && fits(x, y, text_width(value))) {
            static_cast<void>(ui_.draw_text_utf8_pixels(framebuffer_, x, y, value, colors(color), font_));
        }
    }

    void big5(const int x, const int y, const std::span<const std::uint8_t> value,
        const compat::Rgba8 color) {
        if (!value.empty() && fits(x, y, big5_width(value))) {
            static_cast<void>(ui_.draw_text_big5_pixels(
                framebuffer_, x, y, text::Big5TextView{value}, colors(color), font_));
        }
    }

    // 原版圆角框。
    void box(const int x, const int y, const int box_width, const int box_height) {
        static_cast<void>(ui_.draw_box(framebuffer_, x, y,
            static_cast<std::uint16_t>(std::max(box_width, 0)),
            static_cast<std::uint16_t>(std::max(box_height, 0)), palette_));
    }

    void fill(const int x, const int y, const int fill_width, const int fill_height,
        const compat::Rgba8 color) {
        if (fill_width > 0 && fill_height > 0 && x >= 0 && y >= 0 && x + fill_width <= width() &&
            y + fill_height <= height()) {
            static_cast<void>(color.alpha == 0xFFU
                ? framebuffer_.fill_rectangle(x, y, static_cast<std::uint16_t>(fill_width),
                      static_cast<std::uint16_t>(fill_height), color)
                : framebuffer_.blend_rectangle(x, y, static_cast<std::uint16_t>(fill_width),
                      static_cast<std::uint16_t>(fill_height), color));
        }
    }

    // 数值条：底色、按比例填充，文字压在条上；文字放不下时依次省去标签与上限。
    void bar(const int x, const int y, const int bar_width, const std::u8string_view label,
        const std::int64_t value, const std::optional<std::int64_t> maximum,
        const compat::Rgba8 color) {
        const auto bar_height = line() - unit();
        fill(x, y + unit(), bar_width, bar_height, kBarBack);
        if (maximum.has_value() && *maximum > 0) {
            const auto ratio = std::clamp(static_cast<double>(value) / static_cast<double>(*maximum),
                0.0, 1.0);
            fill(x, y + unit(), static_cast<int>(bar_width * ratio), bar_height, color);
        }
        const auto number = decimal_text(value);
        const auto full = maximum.has_value()
            ? number + std::u8string{strings::kSlash} + decimal_text(*maximum) : number;
        const auto inner = bar_width - 2 * unit();
        const auto with_label = std::u8string{label} + full;
        const auto& shown = text_width(with_label) <= inner ? with_label
            : text_width(full) <= inner                     ? full
                                                            : number;
        text(x + unit(), y, shown, kWhite);
    }

    void portrait(const resource::SpriteFrameView& frame, const int x, const int y,
        const int portrait_width, const int portrait_height) {
        const auto frame_width = static_cast<int>(frame.width());
        const auto frame_height = static_cast<int>(frame.height());
        if (frame_width <= 0 || frame_height <= 0 || portrait_width <= 0 || portrait_height <= 0) {
            return;
        }
        std::vector<std::int16_t> pixels(
            static_cast<std::size_t>(frame_width) * static_cast<std::size_t>(frame_height), -1);
        for (std::size_t row = 0U; row < frame.rows().size() && row < static_cast<std::size_t>(frame_height);
             ++row) {
            int column = 0;
            for (const auto& run : frame.rows()[row].runs) {
                column += static_cast<int>(run.skip);
                for (const auto index : run.pixels) {
                    if (column >= 0 && column < frame_width) {
                        pixels[row * static_cast<std::size_t>(frame_width) +
                            static_cast<std::size_t>(column)] = index;
                    }
                    ++column;
                }
            }
        }
        const auto scale = framebuffer_.scale();
        const auto left = x * scale;
        const auto top = y * scale;
        const auto physical_width = portrait_width * scale;
        const auto physical_height = portrait_height * scale;
        for (int py = 0; py < physical_height; ++py) {
            const auto target_y = top + py;
            if (target_y < 0 || target_y >= framebuffer_.pixel_height()) {
                continue;
            }
            const auto source_y = py * frame_height / physical_height;
            auto* row = framebuffer_.row(target_y);
            for (int px = 0; px < physical_width; ++px) {
                const auto target_x = left + px;
                if (target_x < 0 || target_x >= framebuffer_.pixel_width()) {
                    continue;
                }
                const auto index = pixels[static_cast<std::size_t>(source_y) *
                        static_cast<std::size_t>(frame_width) +
                    static_cast<std::size_t>(px * frame_width / physical_width)];
                if (index < 0) {
                    continue;
                }
                const auto color = palette_[static_cast<std::size_t>(index)];
                auto* pixel = row + static_cast<std::size_t>(target_x) * compat::kModernRgbaBytesPerPixel;
                pixel[0] = compat::expand_rgb6(color.red);
                pixel[1] = compat::expand_rgb6(color.green);
                pixel[2] = compat::expand_rgb6(color.blue);
                pixel[3] = compat::kOpaqueAlpha;
            }
        }
    }

private:
    NODISCARD bool fits(const int x, const int y, const int text_width) const noexcept {
        return x >= 0 && y >= 0 && x + text_width <= width() && y + line() <= height();
    }

    ui::ModernUiRenderer& ui_;
    const compat::LegacyPalette& palette_;
    render::RgbaFramebuffer& framebuffer_;
    render::rgba::FontSize font_;
    render::rgba::FontMetrics metrics_;
};

NODISCARD std::span<const std::uint8_t> name_of(const battle::BattleHudCombatant& combatant) {
    return page::zero_terminated(combatant.name);
}

NODISCARD compat::Rgba8 name_color(const battle::BattleHudCombatant& combatant) noexcept {
    if (combatant.hidden) {
        return kDim;
    }
    return combatant.poison > 0 ? kPoisonColor : kWhite;
}

NODISCARD std::u8string level_text(const std::int64_t level) {
    return u8"Lv" + decimal_text(level);
}

// 中毒、内伤标签，如“毒12 傷8”；都为0时为空。
NODISCARD std::vector<std::pair<std::u8string, compat::Rgba8>> condition_tags(
    const battle::BattleHudCombatant& combatant) {
    std::vector<std::pair<std::u8string, compat::Rgba8>> tags;
    if (combatant.poison > 0) {
        tags.emplace_back(std::u8string{strings::kHudPoisonTag} + decimal_text(combatant.poison),
            kPoisonColor);
    }
    if (combatant.hurt > 0) {
        tags.emplace_back(std::u8string{strings::kHudHurtTag} + decimal_text(combatant.hurt),
            kHurtColor);
    }
    return tags;
}

// 卡片内容与卡片圆角框之间留出不小于圆角半径（5个参考像素）的边距。
constexpr int kCardInset = 6;

NODISCARD int info_bar_height(const Hud& hud) noexcept {
    return 4 * hud.line() + (2 * kCardInset + 6) * hud.unit();
}

// 战场中央可用区域（信息条以上）的垂直中点，行动与武功菜单居中于此。
NODISCARD int battlefield_center_y(const Hud& hud) noexcept {
    return (hud.height() - info_bar_height(hud) - hud.unit()) / 2;
}

// 底部信息条：我方全队卡片。
void render_info_bar(Hud& hud, const battle::BattleHudView& view) {
    const auto unit = hud.unit();
    const auto padding = 3 * unit;
    const auto line = hud.line();
    const auto bar_height = info_bar_height(hud);
    const auto top = hud.height() - bar_height - unit;
    hud.box(unit, top, hud.width() - 2 * unit, bar_height);

    // 我方卡片：按队伍顺序排列，当前行动者加框高亮，阵亡者灰显。
    std::vector<const battle::BattleHudCombatant*> party;
    for (const auto& combatant : view.combatants) {
        if (combatant.side == 0) {
            party.push_back(&combatant);
        }
    }
    std::ranges::stable_sort(party, [](const auto* left, const auto* right) {
        const auto key = [](const auto* combatant) {
            return combatant->team_index < 0 ? 1'000 + static_cast<int>(combatant->slot)
                                             : combatant->team_index;
        };
        return key(left) < key(right);
    });
    if (party.empty()) {
        return;
    }
    const auto cards_left = 2 * unit + padding;
    const auto cards_right = hud.width() - 2 * unit - padding;
    const auto card_width = std::min((cards_right - cards_left) / static_cast<int>(party.size()),
        12 * hud.big5_advance());
    const auto current_slot = view.current.has_value()
        ? std::optional<std::size_t>{view.combatants[*view.current].slot} : std::nullopt;
    for (std::size_t index = 0U; index < party.size(); ++index) {
        const auto& combatant = *party[index];
        const auto x = cards_left + static_cast<int>(index) * card_width;
        const auto inset = kCardInset * unit;
        const auto box_width = card_width - 2 * unit;
        const auto inner = box_width - 2 * inset;
        const auto box_top = top + 3 * unit;
        const auto y = box_top + inset;
        if (current_slot == combatant.slot) {
            hud.box(x, box_top, box_width, bar_height - 6 * unit);
        }
        const auto content_x = x + inset;
        hud.big5(content_x, y, name_of(combatant), name_color(combatant));
        const auto level = level_text(combatant.level);
        hud.text(content_x + inner - hud.text_width(level), y, level, kDim);
        hud.bar(content_x, y + line, inner, strings::kHudLifeShort, combatant.hp,
            combatant.maximum_hp, kHpFill);
        hud.bar(content_x, y + 2 * line, inner, strings::kHudMpShort, combatant.mp,
            combatant.maximum_mp, mp_fill(combatant.mp_type));
        // 体力条占一半宽度，右侧显示中毒、内伤。
        const auto power_width = inner / 2;
        hud.bar(content_x, y + 3 * line, power_width, strings::kHudPowerShort,
            combatant.physical_power, 100, kPowerFill);
        auto tag_x = content_x + power_width + unit;
        for (const auto& [tag, color] : condition_tags(combatant)) {
            hud.text(tag_x, y + 3 * line, tag, color);
            tag_x += hud.text_width(tag) + unit;
        }
    }
}

// 行动面板：轮到我方行动时在战场中央弹出，上方头像与姓名，下方行动选项按行排列；
// 左右键在本行内移动，上下键在同一列换行。
void render_action_panel(Hud& hud, const battle::BattleRenderer& data,
    const battle::BattleHudView& view, const int center_y) {
    if (view.action_labels.empty() || !view.current.has_value()) {
        return;
    }
    const auto& actor = view.combatants[*view.current];
    const auto unit = hud.unit();
    const auto padding = 3 * unit;
    const auto line = hud.line();
    constexpr auto kColumns = battle::kBattleActionMenuColumns;
    const auto labels = view.action_labels.size();
    const auto columns = static_cast<int>(std::min(labels, kColumns));
    const auto rows = static_cast<int>((labels + kColumns - 1U) / kColumns);
    // 每格前标快捷数字：1～9，第10项为0。
    const auto digit_width = hud.text_width(u8"0") + unit;
    const auto cell_width = digit_width + 2 * hud.big5_advance() + 2 * padding;
    const auto header_height = 2 * line;
    const auto* frame = data.portrait_frame(actor.portrait_id);
    const auto portrait_width = frame != nullptr && frame->height() > 0
        ? header_height * static_cast<int>(frame->width()) / static_cast<int>(frame->height())
        : 0;
    const auto name = name_of(actor);
    const auto header_width = portrait_width + padding + hud.big5_width(name);
    const auto width = 2 * padding + std::max(header_width, columns * cell_width);
    const auto height = 2 * padding + header_height + 2 * unit + rows * line;
    const auto left = (hud.width() - width) / 2;
    const auto top = std::max(unit, center_y - height / 2);
    hud.box(left, top, width, height);
    auto x = left + padding;
    if (frame != nullptr) {
        hud.portrait(*frame, x, top + padding, portrait_width, header_height);
        x += portrait_width + padding;
    }
    hud.big5(x, top + padding + (header_height - line) / 2, name, kYellow);
    const auto grid_top = top + padding + header_height + 2 * unit;
    hud.fill(left + padding, grid_top - unit, width - 2 * padding, unit, kDim);
    const auto grid_left = left + (width - columns * cell_width) / 2;
    for (std::size_t index = 0U; index < labels; ++index) {
        const auto cell_x = grid_left + static_cast<int>(index % kColumns) * cell_width;
        const auto cell_y = grid_top + static_cast<int>(index / kColumns) * line;
        const bool selected = index == view.action_cursor;
        if (selected) {
            hud.fill(cell_x, cell_y, cell_width, line, kSelectedCell);
        }
        const char8_t digit = index == 9U ? u8'0' : static_cast<char8_t>(u8'1' + index);
        hud.text(cell_x + padding, cell_y, std::u8string_view{&digit, 1U}, kDim);
        const bool available = index < view.action_available.size() && view.action_available[index];
        hud.text(cell_x + padding + digit_width, cell_y, view.action_labels[index],
            !available ? kDisabled : selected ? kYellow : kWhite);
    }
}

// 武功菜单：战场中央，列出快捷数字、武功、等级、威力、耗内力。
void render_magic_menu(Hud& hud, const battle::BattleHudView& view, const int center_y) {
    if (view.magics.empty()) {
        return;
    }
    const auto unit = hud.unit();
    // 比其他面板略宽的边距，字号与行动面板一致。
    const auto padding = 5 * unit;
    const auto line = hud.line();
    std::vector<std::array<std::u8string, 3>> numbers;
    int name_width = hud.text_width(strings::kHudMagicHeader);
    std::array<int, 3> widths{hud.text_width(strings::kHudLevelHeader),
        hud.text_width(strings::kHudPowerHeader), hud.text_width(strings::kHudCostHeader)};
    for (const auto& magic : view.magics) {
        name_width = std::max(name_width, hud.big5_width(magic.name));
        numbers.push_back({decimal_text(magic.level),
            magic.power.has_value() ? decimal_text(*magic.power) : std::u8string{u8"-"},
            magic.mp_cost.has_value() ? decimal_text(*magic.mp_cost) : std::u8string{u8"-"}});
        for (std::size_t column = 0U; column < 3U; ++column) {
            widths[column] = std::max(widths[column], hud.text_width(numbers.back()[column]));
        }
    }
    const auto gap = 2 * hud.text_width(u8" ");
    const auto digit_width = hud.text_width(u8"0") + unit;
    const auto width =
        2 * padding + digit_width + name_width + widths[0] + widths[1] + widths[2] + 3 * gap;
    const auto height = 2 * padding + static_cast<int>(view.magics.size() + 1U) * line;
    const auto left = (hud.width() - width) / 2;
    const auto top = std::max(unit, center_y - height / 2);
    hud.box(left, top, width, height);
    std::array<int, 4> column_x{left + padding + digit_width, 0, 0, 0};
    column_x[1] = column_x[0] + name_width + gap;
    column_x[2] = column_x[1] + widths[0] + gap;
    column_x[3] = column_x[2] + widths[1] + gap;
    // 数字列右对齐。
    const auto right_aligned = [&](const std::size_t column, const int y,
                                   const std::u8string_view value, const compat::Rgba8 color) {
        hud.text(column_x[column + 1U] + widths[column] - hud.text_width(value), y, value, color);
    };
    auto y = top + padding;
    hud.text(column_x[0], y, strings::kHudMagicHeader, kDim);
    right_aligned(0U, y, strings::kHudLevelHeader, kDim);
    right_aligned(1U, y, strings::kHudPowerHeader, kDim);
    right_aligned(2U, y, strings::kHudCostHeader, kDim);
    for (std::size_t index = 0U; index < view.magics.size(); ++index) {
        y += line;
        const bool selected = index == view.magic_cursor;
        if (selected) {
            hud.fill(left + padding / 2, y, width - padding, line, kSelectedCell);
        }
        const auto color = selected ? kYellow : kWhite;
        if (index < 10U) {
            const char8_t digit = index == 9U ? u8'0' : static_cast<char8_t>(u8'1' + index);
            hud.text(left + padding, y, std::u8string_view{&digit, 1U}, kDim);
        }
        hud.big5(column_x[0], y, view.magics[index].name, color);
        for (std::size_t column = 0U; column < 3U; ++column) {
            right_aligned(column, y, numbers[index][column], color);
        }
    }
}

// 行动顺序条：画面顶部，本回合尚未行动的参战者头像，当前行动者在最前并加黄框。
int render_turn_order(Hud& hud, const battle::BattleRenderer& data, const battle::BattleHudView& view) {
    if (!view.current.has_value()) {
        return 0;
    }
    const auto unit = hud.unit();
    // 边距大于圆角半径，头像不压到框角。
    const auto padding = kCardInset * unit;
    const auto portrait_height = std::max(12, hud.height() / 10);
    const auto label_width = hud.text_width(strings::kHudTurnOrderLabel);
    std::vector<std::pair<const battle::BattleHudCombatant*, int>> entries;
    auto total = 2 * padding + label_width + padding;
    for (auto index = *view.current; index < view.combatants.size(); ++index) {
        const auto& combatant = view.combatants[index];
        if (combatant.hidden) {
            continue;
        }
        const auto* frame = data.portrait_frame(combatant.portrait_id);
        const auto portrait_width = frame != nullptr && frame->height() > 0
            ? portrait_height * static_cast<int>(frame->width()) / static_cast<int>(frame->height())
            : portrait_height * 4 / 5;
        if (total + portrait_width + 2 * unit > hud.width() - 2 * unit) {
            break;
        }
        entries.emplace_back(&combatant, portrait_width);
        total += portrait_width + 2 * unit + unit;
    }
    if (entries.empty()) {
        return 0;
    }
    const auto height = portrait_height + 2 * padding;
    const auto left = (hud.width() - total) / 2;
    hud.box(left, unit, total, height);
    hud.text(left + padding, unit + (height - hud.line()) / 2, strings::kHudTurnOrderLabel, kDim);
    auto x = left + padding + label_width + padding;
    const auto y = unit + padding;
    for (std::size_t index = 0U; index < entries.size(); ++index) {
        const auto& [combatant, portrait_width] = entries[index];
        const auto border = index == 0U ? kYellow : combatant->side == 0 ? kAllyColor : kEnemyColor;
        hud.fill(x, y - unit, portrait_width + 2 * unit, portrait_height + 2 * unit, border);
        hud.fill(x + unit, y, portrait_width, portrait_height, kBarBack);
        if (const auto* frame = data.portrait_frame(combatant->portrait_id)) {
            hud.portrait(*frame, x + unit, y, portrait_width, portrait_height);
        }
        x += portrait_width + 3 * unit;
    }
    return unit + height;
}

// 光标所指角色的信息卡：画面右上角，头像、姓名等级、生命内力体力条与中毒内伤。
void render_inspected(Hud& hud, const battle::BattleRenderer& data,
    const battle::BattleHudView& view, const int top) {
    if (!view.inspected.has_value()) {
        return;
    }
    const auto& combatant = view.combatants[*view.inspected];
    const auto unit = hud.unit();
    const auto padding = 3 * unit;
    const auto line = hud.line();
    const auto portrait_height = 4 * line;
    const auto* frame = data.portrait_frame(combatant.portrait_id);
    const auto portrait_width = frame != nullptr && frame->height() > 0
        ? portrait_height * static_cast<int>(frame->width()) / static_cast<int>(frame->height())
        : 0;
    const auto info_width = 10 * hud.big5_advance();
    const auto width = 2 * padding + portrait_width + padding + info_width;
    const auto height = 2 * padding + 5 * line;
    const auto left = hud.width() - unit - width;
    hud.box(left, top, width, height);
    const auto y = top + padding;
    if (frame != nullptr) {
        hud.portrait(*frame, left + padding, y, portrait_width, portrait_height);
    }
    const auto x = left + padding + portrait_width + padding;
    hud.fill(x, y + line / 4, unit * 2, line / 2,
        combatant.side == 0 ? kAllyColor : kEnemyColor);
    hud.big5(x + 3 * unit, y, name_of(combatant), name_color(combatant));
    const auto level = level_text(combatant.level);
    hud.text(x + info_width - hud.text_width(level), y, level, kDim);
    hud.bar(x, y + line, info_width, strings::kHudLifeShort, combatant.hp, combatant.maximum_hp,
        kHpFill);
    hud.bar(x, y + 2 * line, info_width, strings::kHudMpShort, combatant.mp, combatant.maximum_mp,
        mp_fill(combatant.mp_type));
    hud.bar(x, y + 3 * line, info_width, strings::kHudPowerShort, combatant.physical_power, 100,
        kPowerFill);
    auto tag_x = left + padding;
    for (const auto& [tag, color] : condition_tags(combatant)) {
        hud.text(tag_x, y + 4 * line, tag, color);
        tag_x += hud.text_width(tag) + 2 * unit;
    }
}

}  // namespace

bool render_battle_hud(
    const battle::BattleRenderer& data,
    ui::ModernUiRenderer& ui_renderer,
    const compat::LegacyPalette& palette,
    const battle::BattleHudView& view,
    render::RgbaFramebuffer& framebuffer) {
    if (!view.battlefield) {
        return true;
    }
    Hud hud{ui_renderer, palette, framebuffer, kHudReferenceFont};
    const auto order_bottom = render_turn_order(hud, data, view);
    render_inspected(hud, data, view, order_bottom + hud.unit());
    render_info_bar(hud, view);
    Hud menu_hud{ui_renderer, palette, framebuffer, kMenuReferenceFont};
    render_action_panel(menu_hud, data, view, battlefield_center_y(hud));
    render_magic_menu(menu_hud, view, battlefield_center_y(hud));
    return true;
}

}  // namespace openlegend::app
