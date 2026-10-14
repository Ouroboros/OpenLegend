#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "openlegend/attributes.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/model/runtime_snapshot.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

namespace openlegend::app {

struct ItemListPageView {
    std::span<const std::int16_t> inventory_slots;
    std::size_t cursor{};
    std::size_t scroll{};
    std::size_t visible_rows{};
    // 显示的页签（kItemCategoryLabels 下标）与当前页签在其中的位置。
    std::span<const std::uint8_t> tabs;
    std::size_t active_tab{};
    // 坐标物品的说明栏替换文字，多行以换行符分隔。
    std::u8string_view coordinate_text;
    // 药品正向回血／回内力的倍率基准（原版最大生命、最大内力上限）：回复量按目标最大值的
    // 加成 / 基准 计算，正的生命值、内力值加成显示为百分比；不大于0时按原数值显示。
    std::int64_t hp_percent_base{};
    std::int64_t mp_percent_base{};
    // 当前周目；装备的攻击、防御加成按周目数显示，与战斗结算一致。
    std::int64_t playthrough{1};
};

NODISCARD bool render_item_list_page(
    const battle::BattleRenderer& data,
    ui::ModernUiRenderer& ui_renderer,
    const compat::LegacyPalette& palette,
    const model::RuntimeRangerState& ranger,
    const ItemListPageView& view,
    render::RgbaFramebuffer& framebuffer);

}  // namespace openlegend::app
