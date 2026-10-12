#pragma once

#include "openlegend/attributes.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/battle/battle_session.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

namespace openlegend::app {

// 战斗界面（现代UI层）：底部我方信息条与行动菜单、武功菜单、行动顺序条、
// 光标所指角色信息卡；框体沿用原版圆角框。
NODISCARD bool render_battle_hud(
    const battle::BattleRenderer& data,
    ui::ModernUiRenderer& ui_renderer,
    const compat::LegacyPalette& palette,
    const battle::BattleHudView& view,
    render::RgbaFramebuffer& framebuffer);

}  // namespace openlegend::app
