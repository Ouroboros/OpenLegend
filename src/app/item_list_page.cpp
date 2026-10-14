#include "item_list_page.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "modern_page_layout.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/reference_layout.hpp"
#include "openlegend/text/game_strings.hpp"
#include "openlegend/ui/basic_ui_renderer.hpp"

namespace openlegend::app {
namespace {

using namespace openlegend::text::game_strings;
using namespace openlegend::app::page;
namespace text_colors = render::legacy_color::text;

// 物品列表与详情使用页面字号的80%，页签保持页面字号。
constexpr render::rgba::FontSize kItemListFont{10};

// 物品属性加成字段与 kItemEffectLabels 一一对应（add_hp..add_attack_with_poison）。
static_assert(kItemEffectLabels.size() ==
    model::item_word::add_attack_with_poison - model::item_word::add_hp + 1U);
constexpr std::size_t kChangeMpTypeEffectIndex =
    model::item_word::change_mp_type - model::item_word::add_hp;
constexpr std::size_t kAddMpEffectIndex = model::item_word::add_mp - model::item_word::add_hp;
constexpr std::size_t kAddAttackEffectIndex = model::item_word::add_attack - model::item_word::add_hp;
constexpr std::size_t kAddDefenceEffectIndex = model::item_word::add_defence - model::item_word::add_hp;

struct Requirement {
    std::size_t item_word;
    std::size_t label_index;
};

// 装备与秘笈的能力门槛，标签复用 kItemEffectLabels 的同名能力。
constexpr std::array<Requirement, 11U> kRequirements{{
    {model::item_word::need_mp, 5U},
    {model::item_word::need_attack, 7U},
    {model::item_word::need_speed, 8U},
    {model::item_word::need_use_poison, 11U},
    {model::item_word::need_medicine, 10U},
    {model::item_word::need_detoxification, 12U},
    {model::item_word::need_fist, 14U},
    {model::item_word::need_sword, 15U},
    {model::item_word::need_knife, 16U},
    {model::item_word::need_unusual, 17U},
    {model::item_word::need_hidden_weapon, 18U},
}};

NODISCARD std::span<const std::uint8_t> item_name(const model::ItemRecord& item) {
    return item.word(model::item_word::show_introduction) == 0
        ? fixed_text(item.bytes, model::item_word::name_byte, model::item_word::name_bytes)
        : fixed_text(item.bytes, model::item_word::secondary_name_begin * 2U,
              model::item_word::secondary_name_count * 2U);
}

NODISCARD std::span<const std::uint8_t> role_name(
    const model::RuntimeRangerState& ranger, const std::int64_t role_id,
    std::array<std::uint8_t, model::role_word::name_bytes>& storage) {
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger.roles.size()) {
        return {};
    }
    storage = ranger.roles[static_cast<std::size_t>(role_id)].legacy_name();
    return zero_terminated(storage);
}

NODISCARD std::u8string signed_text(const std::int64_t value) {
    return value > 0 ? u8"+" + decimal_text(value) : decimal_text(value);
}

// 装备名后缀：第n周目装备攻防加成为n倍，显示“+(n−1)”；第一周目与非装备为空。
NODISCARD std::u8string equipment_suffix(
    const model::ItemRecord& item, const std::int64_t playthrough) {
    return item.word(model::item_word::item_type) == 1 && playthrough > 1
        ? u8"+" + decimal_text(playthrough - 1) : std::u8string{};
}

// 按可用宽度切分 Big5 文本为多行。
NODISCARD std::vector<std::span<const std::uint8_t>> wrap_big5(
    const PageLayout& layout, const std::span<const std::uint8_t> text, const int width) {
    std::vector<std::span<const std::uint8_t>> lines;
    std::size_t begin = 0U;
    std::size_t index = 0U;
    int line_width = 0;
    while (index < text.size()) {
        const auto wide = text[index] >= 0x80U && index + 1U < text.size();
        const auto advance = static_cast<int>(
            wide ? layout.metrics().big5_width : layout.metrics().ascii_width);
        if (line_width + advance > width && index > begin) {
            lines.push_back(text.subspan(begin, index - begin));
            begin = index;
            line_width = 0;
        }
        line_width += advance;
        index += wide ? 2U : 1U;
    }
    if (index > begin) {
        lines.push_back(text.subspan(begin, std::min(index, text.size()) - begin));
    }
    return lines;
}

struct DetailEntry {
    std::u8string label;
    std::u8string value;
    std::span<const std::uint8_t> big5_value;
    // 非空时以 Big5 文字作为标签（如炼制产物名），代替 label。
    std::span<const std::uint8_t> big5_label{};
};

// 单列竖排：列头一行，其下每项“标签 值”一行，值按固定列宽对齐。
NODISCARD bool draw_detail_column(
    PageLayout& layout, const int left, const int top, const int row_step,
    const std::u8string_view heading, const std::span<const DetailEntry> values) {
    if (!layout.text(left, top, heading, text_colors::menu_normal)) {
        return false;
    }
    if (values.empty()) {
        return layout.text(left, top + row_step, kItemDetailNone, text_colors::disabled);
    }
    const auto value_left = left + 5 * static_cast<int>(layout.metrics().big5_width) +
        layout.length(4);
    for (std::size_t index = 0U; index < values.size(); ++index) {
        const auto row_top = top + static_cast<int>(index + 1U) * row_step;
        const auto& entry = values[index];
        if ((entry.big5_label.empty()
                ? !layout.text(left, row_top, entry.label, text_colors::notice)
                : !layout.big5(left, row_top, entry.big5_label, text_colors::notice)) ||
            (!entry.big5_value.empty() &&
             !layout.big5(value_left, row_top, entry.big5_value, text_colors::selected)) ||
            (!entry.value.empty() &&
             !layout.text(value_left + layout.big5_width(entry.big5_value), row_top,
                 entry.value, text_colors::selected))) {
            return false;
        }
    }
    return true;
}

NODISCARD bool draw_detail(
    const battle::BattleRenderer& data, PageLayout& layout, const ContentArea& area,
    const model::RuntimeRangerState& ranger, const std::size_t inventory_slot,
    const std::u8string_view coordinate_text, const std::int64_t hp_percent_base,
    const std::int64_t mp_percent_base, const std::int64_t playthrough, const int row_step) {
    const auto item_id = ranger.header.inventory_item(inventory_slot).value;
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger.items.size()) {
        return true;
    }
    const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
    const auto item_type = item.word(model::item_word::item_type);

    int icon_width = 0;
    if (const auto* frame = data.item_icon_frame(item_id); frame != nullptr) {
        icon_width = layout.scaled_width(frame->width());
        if (!layout.portrait(*frame, area.left, area.top)) {
            return false;
        }
    }
    const auto name_left = area.left + icon_width + layout.length(6);
    const auto name = item_name(item);
    if (!layout.big5(name_left, area.top, name, text_colors::selected)) {
        return false;
    }
    auto name_right = name_left + layout.big5_width(name);
    if (const auto suffix = equipment_suffix(item, playthrough); !suffix.empty()) {
        name_right += layout.length(4);
        if (!layout.text(name_right, area.top, suffix, text_colors::selected)) {
            return false;
        }
        name_right += layout.utf8_width(suffix);
    }
    std::array<std::uint8_t, model::role_word::name_bytes> user_storage{};
    if (const auto user = role_name(ranger, item.word(model::item_word::user), user_storage);
        !user.empty() &&
        !layout.big5(name_right + layout.length(8), area.top, user,
            text_colors::notice)) {
        return false;
    }
    // 名称下一行是数量，再往下是说明文字（图标右侧，过长换行）。
    if (!layout.text(name_left, area.top + row_step,
            std::u8string{kItemCountPrefix} +
                decimal_text(ranger.header.inventory_count(inventory_slot)),
            text_colors::notice)) {
        return false;
    }
    auto text_top = area.top + 2 * row_step;
    if (item.word(model::item_word::id) == ui::kCoordinateItemId && !coordinate_text.empty()) {
        auto remaining = coordinate_text;
        while (!remaining.empty()) {
            const auto line_end = remaining.find(u8'\n');
            if (!layout.text(name_left, text_top, remaining.substr(0U, line_end),
                    text_colors::menu_normal)) {
                return false;
            }
            text_top += row_step;
            remaining = line_end == std::u8string_view::npos
                ? std::u8string_view{} : remaining.substr(line_end + 1U);
        }
    } else {
        const auto description = fixed_text(
            item.bytes, model::item_word::introduction_byte, model::item_word::introduction_bytes);
        const auto lines = wrap_big5(layout, description, area.right - name_left);
        for (std::size_t line = 0U; line < std::min<std::size_t>(lines.size(), 2U); ++line) {
            if (!layout.big5(name_left, text_top, lines[line], text_colors::menu_normal)) {
                return false;
            }
            text_top += row_step;
        }
    }

    // 列头位置固定：图标标准高度与名称、数量加两行说明中较高者之下，不随说明行数变化。
    const auto columns_top =
        area.top + std::max(layout.scaled_height(40), 4 * row_step) + layout.length(6);
    const auto requirement_left = area.left + (area.right - area.left) / 2;

    std::vector<DetailEntry> effects;
    for (std::size_t index = 0U; index < kItemEffectLabels.size(); ++index) {
        const auto value = item.word(model::item_word::add_hp + index);
        if (index == kChangeMpTypeEffectIndex) {
            if (value == 2) {
                effects.push_back({std::u8string{kItemEffectLabels[index]},
                    std::u8string{kMpTypeLabels[2U]}, {}});
            }
            continue;
        }
        const auto percent_base = index == 0U ? hp_percent_base
            : index == kAddMpEffectIndex ? mp_percent_base : 0;
        if (value > 0 && percent_base > 0) {
            // 药品回血、回内力按目标最大值比例生效，超出缺口不再增加，最多显示100%。
            const auto percent = std::min<std::int64_t>(
                (value * 100 + percent_base / 2) / percent_base, 100);
            effects.push_back({std::u8string{kItemEffectLabels[index]},
                signed_text(percent) + std::u8string{kPercentSuffix}, {}});
            continue;
        }
        if (value != 0) {
            // 装备的攻击、防御加成在战斗中乘以周目数，轻功不乘。
            const auto shown = item_type == 1 &&
                    (index == kAddAttackEffectIndex || index == kAddDefenceEffectIndex)
                ? value * playthrough : value;
            effects.push_back({std::u8string{kItemEffectLabels[index]}, signed_text(shown), {}});
        }
    }
    const auto magic_id = item.word(model::item_word::magic_id);
    if (item_type == 2 && magic_id > 0 &&
        static_cast<std::size_t>(magic_id) < ranger.magics.size()) {
        effects.push_back({std::u8string{kItemDetailLearnLabel}, {},
            fixed_text(ranger.magics[static_cast<std::size_t>(magic_id)].bytes,
                model::magic_word::name_byte, model::magic_word::name_bytes)});
    }
    if (!draw_detail_column(
            layout, area.left, columns_top, row_step, kItemDetailEffectLabel, effects)) {
        return false;
    }

    // 炼制块接在效果列下方：所需炼制经验、材料、各产物消耗的材料数量。
    if (const auto craft_experience = item.word(model::item_word::need_make_item_experience);
        craft_experience > 0) {
        std::vector<DetailEntry> crafting;
        crafting.push_back({std::u8string{kItemDetailCraftExperienceLabel},
            decimal_text(craft_experience), {}});
        const auto material_id = item.word(model::item_word::need_material);
        if (material_id >= 0 && static_cast<std::size_t>(material_id) < ranger.items.size()) {
            crafting.push_back({std::u8string{kItemDetailMaterialLabel}, {},
                item_name(ranger.items[static_cast<std::size_t>(material_id)])});
        }
        for (std::size_t recipe = 0U; recipe < model::item_word::make_item_count; ++recipe) {
            const auto product = item.word(model::item_word::make_item_begin + recipe);
            if (product < 0 || static_cast<std::size_t>(product) >= ranger.items.size()) {
                continue;
            }
            crafting.push_back({{},
                std::u8string{kItemCountPrefix} +
                    decimal_text(item.word(model::item_word::make_item_count_begin + recipe)),
                {}, item_name(ranger.items[static_cast<std::size_t>(product)])});
        }
        const auto effect_rows = std::max<std::size_t>(effects.size(), 1U);
        if (!draw_detail_column(layout, area.left,
                columns_top + static_cast<int>(effect_rows + 1U) * row_step + layout.length(4),
                row_step, kItemDetailCraftLabel, crafting)) {
            return false;
        }
    }

    if (item_type != 1 && item_type != 2) {
        return true;
    }
    std::vector<DetailEntry> requirements;
    std::array<std::uint8_t, model::role_word::name_bytes> only_storage{};
    if (const auto only = role_name(
            ranger, item.word(model::item_word::only_suitable_role), only_storage);
        !only.empty()) {
        requirements.push_back({std::u8string{kItemDetailOnlyLabel}, {}, only});
    }
    if (const auto need_mp_type = item.word(model::item_word::need_mp_type);
        need_mp_type == 0 || need_mp_type == 1) {
        requirements.push_back({std::u8string{kMpTypeAbilityLabel},
            std::u8string{kMpTypeLabels[static_cast<std::size_t>(need_mp_type)]}, {}});
    }
    for (const auto& requirement : kRequirements) {
        if (const auto value = item.word(requirement.item_word); value > 0) {
            requirements.push_back({std::u8string{kItemEffectLabels[requirement.label_index]},
                decimal_text(value), {}});
        }
    }
    if (const auto need_experience = item.word(model::item_word::need_experience);
        item_type == 2 && need_experience > 0) {
        requirements.push_back({std::u8string{kPracticeExperienceLabel},
            decimal_text(need_experience), {}});
    }
    if (const auto need_iq = item.word(model::item_word::need_iq); need_iq != 0) {
        requirements.push_back({std::u8string{kIqAbilityLabel},
            decimal_text(need_iq > 0 ? need_iq : -need_iq) +
                std::u8string{need_iq > 0 ? kItemDetailAtLeastSuffix : kItemDetailAtMostSuffix},
            {}});
    }
    return draw_detail_column(layout, requirement_left, columns_top, row_step,
        kItemDetailRequirementLabel, requirements);
}

}  // namespace

bool render_item_list_page(
    const battle::BattleRenderer& data,
    ui::ModernUiRenderer& ui_renderer,
    const compat::LegacyPalette& palette,
    const model::RuntimeRangerState& ranger,
    const ItemListPageView& view,
    render::RgbaFramebuffer& framebuffer) {
    const auto viewport = render::fit_legacy_reference_viewport(
        framebuffer.logical_width(), framebuffer.logical_height());
    if (!viewport.valid() || view.visible_rows == 0U || view.active_tab >= view.tabs.size()) {
        return false;
    }
    PageLayout layout{ui_renderer, palette, framebuffer, viewport, kModernPageFont};
    const auto line = static_cast<int>(layout.metrics().line_height);
    const auto padding = layout.length(6);

    const auto panel_left = layout.x(2);
    const auto panel_top = layout.y(2);
    const auto panel_right = layout.x(318);
    const auto panel_bottom = layout.y(198);
    if (!layout.box(panel_left, panel_top, panel_right - panel_left, panel_bottom - panel_top)) {
        return false;
    }

    const auto tab_inset = layout.length(5);
    const auto tab_width = 2 * static_cast<int>(layout.metrics().big5_width) + 2 * tab_inset;
    const auto tab_height = line + 2 * layout.length(3);
    const auto tab_top = panel_top + padding;
    for (std::size_t index = 0U; index < view.tabs.size(); ++index) {
        if (view.tabs[index] >= kItemCategoryLabels.size()) {
            return false;
        }
        const auto tab_left = panel_left + padding +
            static_cast<int>(index) * (tab_width + layout.length(3));
        const auto selected = index == view.active_tab;
        if ((selected && !layout.box(tab_left, tab_top, tab_width, tab_height)) ||
            !layout.centered_text(tab_left, tab_top, tab_width, tab_height,
                kItemCategoryLabels[view.tabs[index]],
                selected ? text_colors::selected : text_colors::menu_normal)) {
            return false;
        }
    }

    const auto content_top = tab_top + tab_height + padding;
    const auto content_bottom = panel_bottom - padding;
    const auto list_left = panel_left + padding;
    PageLayout list_layout{ui_renderer, palette, framebuffer, viewport, kItemListFont};
    // 列表只显示物品名，宽度按小字号容纳6字计算，其余宽度留给详情。
    const auto list_right = list_left + 2 * layout.length(4) +
        6 * static_cast<int>(list_layout.metrics().big5_width);
    const auto list_line = static_cast<int>(list_layout.metrics().line_height);
    const auto rows = static_cast<int>(view.visible_rows);
    const auto row_step = std::min(
        list_line + list_layout.length(3), (content_bottom - content_top) / (rows + 1));

    if (view.inventory_slots.empty()) {
        return list_layout.text(
            list_left + layout.length(4), content_top, kItemListEmpty, text_colors::disabled);
    }

    for (std::size_t row = 0U; row < view.visible_rows; ++row) {
        const auto index = view.scroll + row;
        if (index >= view.inventory_slots.size()) {
            break;
        }
        const auto slot = view.inventory_slots[index];
        if (slot < 0 || static_cast<std::size_t>(slot) >= model::kInventoryCount) {
            continue;
        }
        const auto item_id = ranger.header.inventory_item(static_cast<std::size_t>(slot)).value;
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger.items.size()) {
            continue;
        }
        const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
        const auto top = content_top + static_cast<int>(row) * row_step;
        const auto colors =
            index == view.cursor ? text_colors::selected : text_colors::menu_normal;
        const auto name = item_name(item);
        if (!list_layout.big5(list_left + layout.length(4), top, name, colors)) {
            return false;
        }
        if (const auto suffix = equipment_suffix(item, view.playthrough);
            !suffix.empty() &&
            !list_layout.text(list_left + layout.length(8) + list_layout.big5_width(name), top,
                suffix, colors)) {
            return false;
        }
    }
    const auto position = decimal_text(static_cast<std::int64_t>(view.cursor + 1U)) + u8"/" +
        decimal_text(static_cast<std::int64_t>(view.inventory_slots.size()));
    if (!list_layout.text(list_right - layout.length(4) - list_layout.utf8_width(position),
            content_top + rows * row_step, position, text_colors::disabled)) {
        return false;
    }

    const auto detail_left = list_right + padding;
    if (!layout.box(detail_left, content_top - layout.length(2),
            panel_right - padding - detail_left, content_bottom - content_top + layout.length(2))) {
        return false;
    }
    if (view.cursor >= view.inventory_slots.size() || view.inventory_slots[view.cursor] < 0) {
        return true;
    }
    return draw_detail(data, list_layout,
        ContentArea{detail_left + padding, panel_right - 2 * padding,
            content_top + padding, content_bottom - padding},
        ranger, static_cast<std::size_t>(view.inventory_slots[view.cursor]),
        view.coordinate_text, view.hp_percent_base, view.mp_percent_base, view.playthrough,
        list_line + layout.length(2));
}

}  // namespace openlegend::app
