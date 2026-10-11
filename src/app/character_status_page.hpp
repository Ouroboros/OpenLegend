#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "openlegend/attributes.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/model/new_game_plus_configuration.hpp"
#include "openlegend/model/runtime_snapshot.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

namespace openlegend::app {

inline constexpr std::uint8_t kCharacterStatusPageCount = 3U;

NODISCARD bool render_character_status_page(
    const battle::BattleRenderer& data,
    ui::ModernUiRenderer& ui_renderer,
    const compat::LegacyPalette& palette,
    const model::RuntimeRangerState& ranger,
    std::span<const model::RoleState* const> roles,
    std::size_t cursor,
    std::uint8_t page,
    const model::PlaythroughLimits& limits,
    render::RgbaFramebuffer& framebuffer);

}  // namespace openlegend::app
