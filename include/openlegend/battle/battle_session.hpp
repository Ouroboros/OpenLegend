#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_pathing.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::battle {

enum class BattleSessionPhase {
    party_selection,
    initial_present,
    initial_fade,
    round_start,
    actor_present,
    player_action,
    ai_action,
};

enum class BattleSessionInputResult {
    ignored,
    changed,
    selection_complete,
};

class BattleSession {
public:
    BattleSession(
        const resource::DataRoot& data_root,
        model::RangerState& ranger,
        random::LegacyRandom& random,
        std::int16_t battle_id,
        bool grant_experience);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] BattleSessionPhase phase() const noexcept { return phase_; }
    [[nodiscard]] std::int16_t battle_id() const noexcept { return data_.battle_id(); }
    [[nodiscard]] bool grants_experience() const noexcept { return grants_experience_; }
    [[nodiscard]] std::int16_t view_x() const noexcept { return render_state_.view_x; }
    [[nodiscard]] std::int16_t view_y() const noexcept { return render_state_.view_y; }
    [[nodiscard]] std::size_t current_actor_slot() const noexcept { return current_actor_slot_; }
    [[nodiscard]] std::size_t fade_frame_count() const noexcept {
        return fade_palettes_.size();
    }
    [[nodiscard]] std::size_t fade_frame() const noexcept { return fade_frame_; }
    [[nodiscard]] const BattleSetup& setup() const noexcept { return setup_; }
    [[nodiscard]] BattleSetup& setup() noexcept { return setup_; }
    [[nodiscard]] const BattleData& data() const noexcept { return data_; }

    [[nodiscard]] BattleSessionInputResult handle_key(std::uint8_t translated_key);
    void advance();
    [[nodiscard]] bool render(render::IndexedFramebuffer& framebuffer);
    void finish_presented_tick();

private:
    [[nodiscard]] bool begin_initial_battle();
    [[nodiscard]] bool begin_round();
    [[nodiscard]] bool render_party_selection(
        render::IndexedFramebuffer& framebuffer);
    [[nodiscard]] bool render_battlefield(
        render::IndexedFramebuffer& framebuffer);
    void capture_selection_background(
        const render::IndexedFramebuffer& framebuffer) noexcept;
    void restore_selection_background(
        render::IndexedFramebuffer& framebuffer) const noexcept;

    model::RangerState& ranger_;
    random::LegacyRandom& random_;
    BattleData data_;
    BattleSetup setup_;
    BattlePathing pathing_;
    BattleRenderer renderer_;
    BattleRenderState render_state_{};
    BattleSessionPhase phase_{BattleSessionPhase::party_selection};
    std::vector<compat::LegacyPalette> fade_palettes_;
    std::size_t fade_frame_{};
    std::size_t current_actor_slot_{};
    std::array<std::uint8_t, compat::kLegacyPixelCount> selection_background_{};
    compat::LegacyPalette selection_palette_{};
    bool selection_background_captured_{};
    bool frame_rendered_{};
    bool grants_experience_{};
    std::string error_;
};

}  // namespace openlegend::battle
