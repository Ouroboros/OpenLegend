#include "character_status_page.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "openlegend/model/hurt.hpp"
#include "openlegend/model/practice.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/reference_layout.hpp"
#include "openlegend/text/big5.hpp"
#include "openlegend/text/game_strings.hpp"

namespace openlegend::app {
namespace {

using namespace openlegend::text::game_strings;
namespace text_colors = render::legacy_color::text;

constexpr render::rgba::FontSize kStatusFont{13};
static_assert(kStatusTabLabels.size() == kCharacterStatusPageCount);

NODISCARD std::u8string decimal_text(const std::int64_t value) {
    const auto text = std::to_string(value);
    return std::u8string{text.begin(), text.end()};
}

NODISCARD std::span<const std::uint8_t> zero_terminated(const std::span<const std::uint8_t> bytes) {
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0U});
    return bytes.first(static_cast<std::size_t>(end - bytes.begin()));
}

NODISCARD std::span<const std::uint8_t> fixed_text(
    const std::span<const std::uint8_t> bytes, const std::size_t begin, const std::size_t count) {
    return zero_terminated(bytes.subspan(begin, count));
}

class StatusLayout {
public:
    StatusLayout(
        ui::ModernUiRenderer& ui_renderer,
        const compat::LegacyPalette& palette,
        render::RgbaFramebuffer& framebuffer,
        const render::ReferenceViewport viewport)
        : ui_renderer_(ui_renderer),
          palette_(palette),
          framebuffer_(framebuffer),
          viewport_(viewport),
          metrics_(ui_renderer.font_metrics(framebuffer, kStatusFont)) {}

    NODISCARD int x(const int reference) const noexcept {
        return viewport_.x + reference * viewport_.width / static_cast<int>(compat::kLegacyWidth);
    }

    NODISCARD int y(const int reference) const noexcept {
        return viewport_.y + reference * viewport_.height / static_cast<int>(compat::kLegacyHeight);
    }

    NODISCARD int length(const int reference) const noexcept {
        return render::scale_legacy_reference_length(
            reference, framebuffer_.logical_width(), framebuffer_.logical_height());
    }

    NODISCARD const render::rgba::FontMetrics& metrics() const noexcept { return metrics_; }

    NODISCARD int utf8_width(const std::u8string_view text) const noexcept {
        int width = 0;
        for (const auto unit : text) {
            if (unit < 0x80U) {
                width += metrics_.ascii_width;
            } else if ((unit & 0xC0U) != 0x80U) {
                width += metrics_.big5_width;
            }
        }
        return width;
    }

    NODISCARD int big5_width(const std::span<const std::uint8_t> bytes) const noexcept {
        int width = 0;
        for (std::size_t index = 0U; index < bytes.size(); ++index) {
            if (bytes[index] >= 0x80U && index + 1U < bytes.size()) {
                width += metrics_.big5_width;
                ++index;
            } else {
                width += metrics_.ascii_width;
            }
        }
        return width;
    }

    NODISCARD bool box(const int left, const int top, const int width, const int height) {
        return ui_renderer_.draw_box(framebuffer_, left, top,
            static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height), palette_);
    }

    NODISCARD bool text(
        const int left, const int top, const std::u8string_view value, const render::TextColors colors) {
        return ui_renderer_.draw_text_utf8(framebuffer_, left, top, value, colors, palette_, kStatusFont);
    }

    NODISCARD bool big5(
        const int left, const int top, const std::span<const std::uint8_t> value,
        const render::TextColors colors) {
        return value.empty() ||
            ui_renderer_.draw_text_big5(
                framebuffer_, left, top, text::Big5TextView{value}, colors, palette_, kStatusFont);
    }

    NODISCARD bool portrait(const resource::SpriteFrameView& frame, const int left, const int top) {
        for (std::size_t row = 0U; row < frame.rows().size(); ++row) {
            const auto row_top = top + scaled_height(static_cast<int>(row));
            const auto row_bottom = top + scaled_height(static_cast<int>(row) + 1);
            int column = 0;
            for (const auto& run : frame.rows()[row].runs) {
                column += static_cast<int>(run.skip);
                for (const auto index : run.pixels) {
                    const auto column_left = left + scaled_width(column);
                    const auto column_right = left + scaled_width(column + 1);
                    const auto color = palette_[index];
                    if (column_right > column_left && row_bottom > row_top &&
                        !framebuffer_.fill_rectangle(column_left, row_top,
                            static_cast<std::uint16_t>(column_right - column_left),
                            static_cast<std::uint16_t>(row_bottom - row_top),
                            compat::Rgba8{compat::expand_rgb6(color.red), compat::expand_rgb6(color.green),
                                compat::expand_rgb6(color.blue), compat::kOpaqueAlpha})) {
                        return false;
                    }
                    ++column;
                }
            }
        }
        return true;
    }

    NODISCARD int scaled_width(const int reference) const noexcept {
        return reference * viewport_.width / static_cast<int>(compat::kLegacyWidth);
    }

    NODISCARD int scaled_height(const int reference) const noexcept {
        return reference * viewport_.height / static_cast<int>(compat::kLegacyHeight);
    }

private:
    ui::ModernUiRenderer& ui_renderer_;
    const compat::LegacyPalette& palette_;
    render::RgbaFramebuffer& framebuffer_;
    render::ReferenceViewport viewport_;
    render::rgba::FontMetrics metrics_;
};

struct ContentArea {
    int left{};
    int right{};
    int top{};
    int bottom{};
};

struct ValueRow {
    std::u8string_view label;
    std::u8string value;
    render::TextColors colors{text_colors::notice};
};

NODISCARD bool draw_value_row(
    StatusLayout& layout, int left, int value_x, int top, std::u8string_view label,
    std::u8string_view value, render::TextColors value_colors);

NODISCARD bool draw_centered_columns(
    StatusLayout& layout, const ContentArea& area,
    const std::span<const ValueRow> first_column, const std::span<const ValueRow> second_column,
    const int label_width_first, const int label_width_second) {
    const auto row_step = static_cast<int>(layout.metrics().line_height) + layout.length(2);
    const auto gap = layout.length(8);
    const auto value_width = [&layout](const std::span<const ValueRow> rows) {
        int width = 0;
        for (const auto& row : rows) {
            width = std::max(width, layout.utf8_width(row.value));
        }
        return width;
    };
    const auto first_width = label_width_first + gap + value_width(first_column);
    const auto second_width = second_column.empty()
        ? 0 : label_width_second + gap + value_width(second_column);
    const auto inter_gap = second_column.empty() ? 0 : layout.length(20);
    const auto total_width = first_width + inter_gap + second_width;
    const auto rows = std::max(first_column.size(), second_column.size());
    const auto total_height = static_cast<int>(rows) * row_step - layout.length(2);
    const auto left = area.left + std::max(0, (area.right - area.left - total_width) / 2);
    const auto top = area.top + std::max(0, (area.bottom - area.top - total_height) / 2);
    const auto draw_column = [&](const std::span<const ValueRow> rows_in_column,
                                 const int column_left, const int label_width) {
        for (std::size_t index = 0U; index < rows_in_column.size(); ++index) {
            if (!draw_value_row(layout, column_left, column_left + label_width + gap,
                    top + static_cast<int>(index) * row_step, rows_in_column[index].label,
                    rows_in_column[index].value, rows_in_column[index].colors)) {
                return false;
            }
        }
        return true;
    };
    return draw_column(first_column, left, label_width_first) &&
        (second_column.empty() ||
         draw_column(second_column, left + first_width + inter_gap, label_width_second));
}

NODISCARD bool draw_value_row(
    StatusLayout& layout, const int left, const int value_x, const int top,
    const std::u8string_view label, const std::u8string_view value,
    const render::TextColors value_colors) {
    return layout.text(left, top, label, text_colors::menu_normal) &&
        layout.text(value_x, top, value, value_colors);
}

NODISCARD bool draw_summary_page(
    const battle::BattleRenderer& data, StatusLayout& layout, const ContentArea& area,
    const model::RoleState& role, const model::PlaythroughLimits& limits) {
    const auto hurt = model::hurt_band(role.hurt, limits.hurt_ratio_denominator);
    if (!hurt.has_value()) {
        return false;
    }
    const auto hurt_colors = *hurt == model::HurtBand::severe ? text_colors::severe_injury
        : *hurt == model::HurtBand::moderate ? text_colors::moderate_injury : text_colors::notice;
    const auto poison_colors = role.poison == 0 ? text_colors::menu_normal
        : role.poison >= 50 ? text_colors::severe_poison : text_colors::poison;
    const auto mp_colors = role.mp_type == 0 ? text_colors::yin_mp
        : role.mp_type == 1 ? text_colors::notice
        : role.mp_type == 2 ? text_colors::selected : poison_colors;

    const auto* frame = data.portrait_frame(role.head_id);
    const auto portrait_width = frame != nullptr ? layout.scaled_width(frame->width()) : 0;
    const auto portrait_height = frame != nullptr ? layout.scaled_height(frame->height()) : 0;
    if (frame != nullptr && !layout.portrait(*frame, area.left, area.top)) {
        return false;
    }
    const auto row_step = static_cast<int>(layout.metrics().line_height) + layout.length(2);
    const auto big5 = static_cast<int>(layout.metrics().big5_width);
    const auto gap = layout.length(8);
    const auto side_left = area.left + portrait_width + layout.length(6);
    const auto side_value_x = side_left + layout.utf8_width(kLevelLabel) + gap;
    const auto name_storage = role.legacy_name();
    if (!layout.big5(side_left, area.top, zero_terminated(name_storage), text_colors::selected) ||
        !draw_value_row(layout, side_left, side_value_x, area.top + row_step, kLevelLabel,
            decimal_text(role.level), text_colors::notice)) {
        return false;
    }
    const auto pair_text = [](const std::int64_t current, const std::u8string& maximum) {
        return decimal_text(current) + std::u8string{kSlash} + maximum;
    };
    const auto next_experience = data.next_level_experience(role.level);
    const auto below = std::max(area.top + portrait_height, area.top + 2 * row_step) + layout.length(4);
    const std::array<ValueRow, 5> first_column{{
        {kLifeLabel, pair_text(role.hp, decimal_text(role.maximum_hp)), hurt_colors},
        {kMpLabel, pair_text(role.mp, decimal_text(role.maximum_mp)), mp_colors},
        {kPowerLabel, pair_text(role.physical_power, std::u8string{kHundred})},
        {kExperienceLabel, decimal_text(role.experience)},
        {kUpgradeLabel,
            next_experience ? decimal_text(*next_experience) : std::u8string{kMaximumLevel}},
    }};
    const auto hurt_text = *hurt == model::HurtBand::severe ? kHurtBandSevereSuffix
        : *hurt == model::HurtBand::moderate ? kHurtBandModerateSuffix
        : role.hurt > 0 ? kHurtBandLightSuffix : std::u8string_view{};
    const std::array<ValueRow, 4> second_column{{
        {kHurtStatusLabel, decimal_text(role.hurt) + std::u8string{hurt_text}, hurt_colors},
        {kPoisonStatusLabel, decimal_text(role.poison), poison_colors},
        {kMoralityStatusLabel, decimal_text(role.morality)},
        {kFameStatusLabel, decimal_text(role.fame)},
    }};
    return draw_centered_columns(layout,
        ContentArea{area.left, area.right, below, area.bottom},
        first_column, second_column, 2 * big5, 2 * big5);
}

NODISCARD bool draw_ability_page(
    const battle::BattleRenderer& data, StatusLayout& layout, const ContentArea& area,
    const model::RuntimeRangerState& ranger, const model::RoleState& role) {
    std::array<ValueRow, 15> rows{};
    const auto abilities = data.character_status_abilities(ranger, role);
    for (std::size_t index = 0U; index < abilities.size(); ++index) {
        rows[index] = {abilities[index].first,
            abilities[index].second ? decimal_text(*abilities[index].second)
                                    : std::u8string{kUnknownValue}};
    }
    rows[11] = {kAntiPoisonAbilityLabel, decimal_text(role.anti_poison)};
    const auto mp_colors = role.mp_type == 0 ? text_colors::yin_mp
        : role.mp_type == 1 ? text_colors::notice : text_colors::selected;
    rows[12] = role.mp_type >= 0 && role.mp_type <= 2
        ? ValueRow{kMpTypeAbilityLabel, std::u8string{kMpTypeLabels[
              static_cast<std::size_t>(role.mp_type)]}, mp_colors}
        : ValueRow{kMpTypeAbilityLabel, decimal_text(role.mp_type)};
    rows[13] = {kIncreasedLifeAbilityLabel, decimal_text(role.increased_life)};
    rows[14] = {kIqAbilityLabel, decimal_text(role.iq)};

    const auto big5 = static_cast<int>(layout.metrics().big5_width);
    return draw_centered_columns(layout, area,
        std::span<const ValueRow>{rows}.first(8U), std::span<const ValueRow>{rows}.subspan(8U),
        4 * big5, 4 * big5);
}

NODISCARD bool draw_equipment_page(
    const battle::BattleRenderer& data, StatusLayout& layout, const ContentArea& area,
    const model::RuntimeRangerState& ranger, const model::RoleState& role) {
    const auto row_step = static_cast<int>(layout.metrics().line_height) + layout.length(2);
    const auto indent = area.left + layout.length(12);
    const auto item_name = [&](const std::int16_t item_id) {
        return fixed_text(ranger.items[static_cast<std::size_t>(item_id)].bytes,
            model::item_word::secondary_name_begin * 2U, model::item_word::secondary_name_count * 2U);
    };
    if (!layout.text(area.left, area.top, kEquipmentLabel, text_colors::menu_normal)) {
        return false;
    }
    auto row = area.top + row_step;
    for (const auto equipment : role.equipment) {
        if (equipment.value >= 0 && static_cast<std::size_t>(equipment.value) < ranger.items.size() &&
            !layout.big5(indent, row, item_name(equipment.value), text_colors::notice)) {
            return false;
        }
        row += row_step;
    }
    row += layout.length(4);
    if (!layout.text(area.left, row, kPracticeLabel, text_colors::menu_normal)) {
        return false;
    }
    row += row_step;
    const auto practice_id = role.practice_item.value;
    if (practice_id < 0 || static_cast<std::size_t>(practice_id) >= ranger.items.size()) {
        return true;
    }
    const auto item_index = static_cast<std::size_t>(practice_id);
    const auto cost = model::manual_experience_requirement(
        role, ranger.items[item_index], item_index, data.practice_rules());
    if (!cost.has_value()) {
        return false;
    }
    const auto value_x = area.left + 4 * static_cast<int>(layout.metrics().big5_width) +
        layout.length(8);
    return layout.big5(indent, row, item_name(practice_id), text_colors::notice) &&
        draw_value_row(layout, area.left, value_x, row + row_step, kPracticeExperienceLabel,
            decimal_text(role.item_experience), text_colors::notice) &&
        draw_value_row(layout, area.left, value_x, row + 2 * row_step, kPracticeRequiredLabel,
            decimal_text(cost->experience), text_colors::notice);
}

NODISCARD bool draw_magic_page(
    StatusLayout& layout, const ContentArea& area, const model::RuntimeRangerState& ranger,
    const model::RoleState& role) {
    struct MagicRow {
        std::span<const std::uint8_t> name;
        std::u8string level;
        std::u8string proficiency;
    };
    std::array<MagicRow, model::role_word::magic_count> rows{};
    std::size_t count = 0U;
    for (std::size_t slot = 0U; slot < role.magic_ids.size(); ++slot) {
        const auto magic_id = role.magic_ids[slot].value;
        if (magic_id <= 0 || static_cast<std::size_t>(magic_id) >= ranger.magics.size()) {
            continue;
        }
        rows[count++] = {
            fixed_text(ranger.magics[static_cast<std::size_t>(magic_id)].bytes,
                model::magic_word::name_byte, model::magic_word::name_bytes),
            decimal_text(role.magic_levels[slot] / 100 + 1),
            decimal_text(role.magic_levels[slot])};
    }
    if (count == 0U) {
        return true;
    }
    constexpr auto kNameHeader = kMagicNameHeader;
    constexpr auto kLevelHeader = kMagicLevelHeader;
    constexpr auto kProficiencyHeader = kMagicProficiencyHeader;
    const auto row_step = static_cast<int>(layout.metrics().line_height) + layout.length(2);
    const auto gap = layout.length(12);
    auto name_width = layout.utf8_width(kNameHeader);
    auto level_width = layout.utf8_width(kLevelHeader);
    auto proficiency_width = layout.utf8_width(kProficiencyHeader);
    for (std::size_t index = 0U; index < count; ++index) {
        name_width = std::max(name_width, layout.big5_width(rows[index].name));
        level_width = std::max(level_width, layout.utf8_width(rows[index].level));
        proficiency_width = std::max(proficiency_width, layout.utf8_width(rows[index].proficiency));
    }
    const auto total_width = name_width + gap + level_width + gap + proficiency_width;
    const auto left = area.left + std::max(0, (area.right - area.left - total_width) / 2);
    const auto top = area.top;
    const auto level_x = left + name_width + gap;
    const auto proficiency_x = level_x + level_width + gap;
    if (!layout.text(left, top, kNameHeader, text_colors::menu_normal) ||
        !layout.text(level_x, top, kLevelHeader, text_colors::menu_normal) ||
        !layout.text(proficiency_x, top, kProficiencyHeader, text_colors::menu_normal)) {
        return false;
    }
    for (std::size_t index = 0U; index < count; ++index) {
        const auto row = top + static_cast<int>(index + 1U) * row_step;
        if (!layout.big5(left, row, rows[index].name, text_colors::notice) ||
            !layout.text(level_x, row, rows[index].level, text_colors::selected) ||
            !layout.text(proficiency_x, row, rows[index].proficiency, text_colors::notice)) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool render_character_status_page(
    const battle::BattleRenderer& data,
    ui::ModernUiRenderer& ui_renderer,
    const compat::LegacyPalette& palette,
    const model::RuntimeRangerState& ranger,
    const std::span<const std::uint8_t> party_slots,
    const std::size_t cursor,
    const std::uint8_t page,
    const model::PlaythroughLimits& limits,
    render::RgbaFramebuffer& framebuffer) {
    const auto viewport = render::fit_legacy_reference_viewport(
        framebuffer.logical_width(), framebuffer.logical_height());
    if (!viewport.valid() || party_slots.empty() || cursor >= party_slots.size() ||
        page >= kCharacterStatusPageCount) {
        return false;
    }
    StatusLayout layout{ui_renderer, palette, framebuffer, viewport};
    const auto line = static_cast<int>(layout.metrics().line_height);
    const auto padding = layout.length(6);

    std::array<std::int16_t, model::kTeamMemberCount> role_ids{};
    int name_width = 3 * static_cast<int>(layout.metrics().big5_width);
    for (std::size_t index = 0U; index < party_slots.size(); ++index) {
        if (party_slots[index] >= model::kTeamMemberCount) {
            return false;
        }
        const auto role_id = ranger.header.team_member(party_slots[index]).value;
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger.roles.size()) {
            return false;
        }
        role_ids[index] = role_id;
        const auto name = ranger.roles[static_cast<std::size_t>(role_id)].legacy_name();
        name_width = std::max(name_width, layout.big5_width(zero_terminated(name)));
    }

    const auto list_left = layout.x(2);
    const auto top = layout.y(2);
    const auto list_step = line + layout.length(4);
    const auto list_width = name_width + 2 * padding;
    const auto list_height = static_cast<int>(party_slots.size()) * list_step - layout.length(4) + 2 * padding;
    if (!layout.box(list_left, top, list_width, list_height)) {
        return false;
    }
    for (std::size_t index = 0U; index < party_slots.size(); ++index) {
        const auto name = ranger.roles[static_cast<std::size_t>(role_ids[index])].legacy_name();
        if (!layout.big5(list_left + padding, top + padding + static_cast<int>(index) * list_step,
                zero_terminated(name), index == cursor ? text_colors::selected : text_colors::menu_normal)) {
            return false;
        }
    }

    const auto panel_left = list_left + list_width + layout.length(4);
    const auto panel_right = layout.x(318);
    const auto panel_bottom = layout.y(198);
    if (!layout.box(panel_left, top, panel_right - panel_left, panel_bottom - top)) {
        return false;
    }
    const auto tab_inset = layout.length(5);
    const auto tab_width = 2 * static_cast<int>(layout.metrics().big5_width) + 2 * tab_inset;
    const auto tab_height = line + 2 * layout.length(3);
    const auto tab_top = top + padding;
    for (std::size_t index = 0U; index < kStatusTabLabels.size(); ++index) {
        const auto tab_left = panel_left + padding + static_cast<int>(index) * (tab_width + layout.length(3));
        const auto selected = index == page;
        if ((selected && !layout.box(tab_left, tab_top, tab_width, tab_height)) ||
            !layout.text(tab_left + tab_inset, tab_top + layout.length(3), kStatusTabLabels[index],
                selected ? text_colors::selected : text_colors::menu_normal)) {
            return false;
        }
    }

    const ContentArea area{panel_left + padding, panel_right - padding,
        tab_top + tab_height + padding, panel_bottom - padding};
    const auto& role = ranger.roles[static_cast<std::size_t>(role_ids[cursor])];
    switch (page) {
    case 0U: return draw_summary_page(data, layout, area, role, limits);
    case 1U: return draw_ability_page(data, layout, area, ranger, role);
    case 2U: return draw_equipment_page(data, layout, area, ranger, role);
    default: return draw_magic_page(layout, area, ranger, role);
    }
}

}  // namespace openlegend::app
