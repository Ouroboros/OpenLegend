#include "openlegend/battle/battle_session.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <span>
#include <string_view>

#include "openlegend/diagnostics/log.hpp"
#include "openlegend/render/legacy_effects.hpp"

namespace openlegend::battle {
namespace {

constexpr std::uint8_t kEnter = 0x0DU;
constexpr std::uint8_t kSpace = 0x20U;
constexpr std::uint8_t kKeypadInsert = 0x96U;
constexpr std::uint8_t kDown = 0x98U;
constexpr std::uint8_t kUp = 0x9EU;
constexpr std::array<std::uint8_t, 20> kPartySelectionTitle{
    0xBDU, 0xD0U, 0xBFU, 0xEFU, 0xBEU, 0xDCU, 0xB0U, 0xD1U, 0xBBU, 0x50U,
    0xBEU, 0xD4U, 0xB0U, 0xABU, 0xA4U, 0xA7U, 0xA4U, 0x48U, 0xAAU, 0xABU};
constexpr std::array<std::uint8_t, 1> kSelectedMarker{'*'};
constexpr std::array<std::uint8_t, 4> kConfirmLabel{0xB5U, 0xB2U, 0xA7U, 0xF4U};

[[nodiscard]] constexpr bool confirms(const std::uint8_t key) noexcept {
    return key == kEnter || key == kSpace || key == kKeypadInsert;
}

[[nodiscard]] constexpr std::string_view phase_name(
    const BattleSessionPhase phase) noexcept {
    switch (phase) {
    case BattleSessionPhase::party_selection: return "party_selection";
    case BattleSessionPhase::initial_present: return "initial_present";
    case BattleSessionPhase::initial_fade: return "initial_fade";
    case BattleSessionPhase::round_start: return "round_start";
    case BattleSessionPhase::actor_present: return "actor_present";
    case BattleSessionPhase::player_action: return "player_action";
    case BattleSessionPhase::ai_action: return "ai_action";
    }
    return "unknown";
}

[[nodiscard]] std::optional<int> centered_name_x(
    const std::span<const std::uint8_t> name) noexcept {
    for (std::size_t byte = 1U; byte <= 6U && byte < name.size(); ++byte) {
        if (name[byte] == 0U) {
            return 99 - static_cast<int>(byte * 4U);
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::span<const std::uint8_t> terminated_name(
    const std::span<const std::uint8_t> name) noexcept {
    const auto end = std::find(name.begin(), name.end(), std::uint8_t{0U});
    return name.first(static_cast<std::size_t>(std::distance(name.begin(), end)));
}

}  // namespace

BattleSession::BattleSession(
    const resource::DataRoot& data_root,
    model::RangerState& ranger,
    random::LegacyRandom& random,
    const std::int16_t battle_id,
    const bool grant_experience)
    : ranger_(ranger),
      random_(random),
      data_(data_root, battle_id),
      setup_(data_, ranger_),
      pathing_(data_),
      renderer_(data_root, data_.battlefield_id()),
      grants_experience_(grant_experience) {
    if (!data_.valid()) {
        error_ = data_.error();
    } else if (!setup_.valid()) {
        error_ = setup_.error();
    } else if (!renderer_.valid()) {
        error_ = renderer_.error();
    }
    if (!error_.empty()) {
        diagnostics::log_error(
            "battle session initialization failed id=" + std::to_string(battle_id) +
            " reason=" + error_);
        return;
    }
    fade_palettes_ = render::legacy_fade_from_black(renderer_.palette());
    diagnostics::log_info(
        "battle session initialized id=" + std::to_string(battle_id) +
        " battlefield=" + std::to_string(data_.battlefield_id()) +
        " grant_experience=" + (grants_experience_ ? std::string{"true"} : std::string{"false"}) +
        " party_selection=" +
        (setup_.waiting_for_party_selection() ? std::string{"true"} : std::string{"false"}) +
        " combatants=" + std::to_string(setup_.combatant_count()));
    if (!setup_.waiting_for_party_selection()) {
        static_cast<void>(begin_initial_battle());
    }
}

BattleSessionInputResult BattleSession::handle_key(const std::uint8_t translated_key) {
    if (!valid() || phase_ != BattleSessionPhase::party_selection ||
        translated_key == 0U) {
        return BattleSessionInputResult::ignored;
    }
    PartySelectionResult result = PartySelectionResult::waiting;
    if (translated_key == kDown) {
        result = setup_.apply(PartySelectionAction::next);
    } else if (translated_key == kUp) {
        result = setup_.apply(PartySelectionAction::previous);
    } else if (confirms(translated_key)) {
        result = setup_.apply(PartySelectionAction::activate);
    } else {
        return BattleSessionInputResult::ignored;
    }
    diagnostics::log_debug(
        "battle party selection id=" + std::to_string(battle_id()) +
        " key=" + std::to_string(translated_key) +
        " cursor=" + std::to_string(setup_.cursor()) +
        " selected=" + std::to_string(std::count_if(
            setup_.selection_states().begin(),
            setup_.selection_states().end(),
            [](const std::int16_t state) { return state != 0; })) +
        " result=" + std::to_string(static_cast<int>(result)));
    if (result == PartySelectionResult::complete) {
        if (!begin_initial_battle()) {
            diagnostics::log_error(
                "battle party selection completion failed id=" +
                std::to_string(battle_id()) + " reason=" + error_);
            return BattleSessionInputResult::ignored;
        }
        diagnostics::log_info(
            "battle party selection complete id=" + std::to_string(battle_id()) +
            " combatants=" + std::to_string(setup_.combatant_count()));
        return BattleSessionInputResult::selection_complete;
    }
    return result == PartySelectionResult::changed
        ? BattleSessionInputResult::changed
        : BattleSessionInputResult::ignored;
}

void BattleSession::advance() {
    if (!valid() || phase_ != BattleSessionPhase::round_start) {
        return;
    }
    static_cast<void>(begin_round());
}

bool BattleSession::render(render::IndexedFramebuffer& framebuffer) {
    if (!valid()) {
        return false;
    }
    bool rendered = false;
    if (phase_ == BattleSessionPhase::party_selection) {
        rendered = render_party_selection(framebuffer);
    } else {
        rendered = render_battlefield(framebuffer);
        if (rendered && phase_ == BattleSessionPhase::initial_fade &&
            fade_frame_ < fade_palettes_.size()) {
            framebuffer.set_palette(fade_palettes_[fade_frame_]);
        }
    }
    frame_rendered_ = rendered;
    if (!rendered) {
        diagnostics::log_error(
            "battle frame render failed id=" + std::to_string(battle_id()) +
            " phase=" + std::string{phase_name(phase_)});
    }
    return rendered;
}

void BattleSession::finish_presented_tick() {
    if (!valid() || !frame_rendered_) {
        return;
    }
    frame_rendered_ = false;
    if (phase_ == BattleSessionPhase::initial_present) {
        if (fade_palettes_.empty()) {
            phase_ = BattleSessionPhase::round_start;
            diagnostics::log_info(
                "battle initial frame presented id=" + std::to_string(battle_id()) +
                " fade_frames=0");
        } else {
            fade_frame_ = 0U;
            phase_ = BattleSessionPhase::initial_fade;
            diagnostics::log_info(
                "battle initial frame presented id=" + std::to_string(battle_id()) +
                " fade_frames=" + std::to_string(fade_palettes_.size()));
        }
        return;
    }
    if (phase_ == BattleSessionPhase::initial_fade) {
        if (fade_frame_ + 1U < fade_palettes_.size()) {
            ++fade_frame_;
        } else {
            phase_ = BattleSessionPhase::round_start;
            diagnostics::log_info(
                "battle initial fade complete id=" + std::to_string(battle_id()));
        }
        return;
    }
    if (phase_ == BattleSessionPhase::actor_present) {
        const auto side = setup_.combatants()[current_actor_slot_]
                              .words[combatant_word::side];
        phase_ = side == 0 && !setup_.automatic_enabled()
            ? BattleSessionPhase::player_action
            : BattleSessionPhase::ai_action;
        diagnostics::log_info(
            "battle actor dispatch id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " role=" + std::to_string(
                setup_.combatants()[current_actor_slot_].words[combatant_word::role_id]) +
            " side=" + std::to_string(side) +
            " automatic=" +
            (setup_.automatic_enabled() ? std::string{"true"} : std::string{"false"}) +
            " phase=" + std::string{phase_name(phase_)});
    }
}

bool BattleSession::begin_initial_battle() {
    if (!setup_.sort_by_effective_speed() || setup_.combatant_count() <= 0) {
        error_ = setup_.valid() ? "battle has no combatants after setup" : setup_.error();
        return false;
    }
    current_actor_slot_ = 0U;
    const auto& actor = setup_.combatants()[0U].words;
    render_state_.view_x = static_cast<std::int16_t>(
        std::clamp(static_cast<int>(actor[combatant_word::x]) - 11, 0, 32));
    render_state_.view_y = static_cast<std::int16_t>(
        std::clamp(static_cast<int>(actor[combatant_word::y]) - 11, 0, 32));
    render_state_.path_limit = 0;
    render_state_.primary_cursor = {
        actor[combatant_word::x], actor[combatant_word::y]};
    phase_ = BattleSessionPhase::initial_present;
    diagnostics::log_info(
        "battle initial view ready id=" + std::to_string(battle_id()) +
        " slot=0 role=" + std::to_string(actor[combatant_word::role_id]) +
        " view=" + std::to_string(render_state_.view_x) + "," +
        std::to_string(render_state_.view_y));
    selection_background_captured_ = false;
    frame_rendered_ = false;
    return true;
}

bool BattleSession::begin_round() {
    if (!setup_.sort_by_effective_speed() || !setup_.prepare_round() ||
        setup_.combatant_count() <= 0) {
        error_ = setup_.valid() ? "battle round preparation failed" : setup_.error();
        return false;
    }
    current_actor_slot_ = 0U;
    const auto& actor = setup_.combatants()[current_actor_slot_]
                            .words;
    render_state_.view_x = static_cast<std::int16_t>(
        std::clamp(static_cast<int>(actor[combatant_word::x]) - 11, 0, 32));
    render_state_.view_y = static_cast<std::int16_t>(
        std::clamp(static_cast<int>(actor[combatant_word::y]) - 11, 0, 32));
    phase_ = BattleSessionPhase::actor_present;
    diagnostics::log_info(
        "battle round actor ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " role=" + std::to_string(actor[combatant_word::role_id]) +
        " side=" + std::to_string(actor[combatant_word::side]) +
        " round_value=" + std::to_string(actor[combatant_word::round_value]) +
        " view=" + std::to_string(render_state_.view_x) + "," +
        std::to_string(render_state_.view_y));
    return true;
}

bool BattleSession::render_party_selection(
    render::IndexedFramebuffer& framebuffer) {
    if (!selection_background_captured_) {
        capture_selection_background(framebuffer);
    } else {
        restore_selection_background(framebuffer);
    }
    const auto count = setup_.party_prefix_length();
    if (!renderer_.draw_box(framebuffer, 64, 17, 180U, 30U) ||
        !renderer_.draw_text(framebuffer, 69, 25, kPartySelectionTitle, 0x0705U) ||
        !renderer_.draw_box(
            framebuffer,
            64,
            48,
            66U,
            static_cast<std::uint16_t>(20U * count + 30U))) {
        return false;
    }
    const auto states = setup_.selection_states();
    for (std::size_t index = 0U; index < count; ++index) {
        const auto role_id = ranger_.header.team_member(index).value;
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            return false;
        }
        const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
        const auto name = std::span<const std::uint8_t>{role.bytes}.subspan(
            model::role_word::name_byte, model::role_word::name_bytes);
        const auto name_x = centered_name_x(name);
        const auto y = 55 + static_cast<int>(20U * index);
        if (name_x.has_value() &&
            !renderer_.draw_text(
                framebuffer,
                *name_x,
                y,
                terminated_name(name),
                setup_.cursor() == index ? 0x6663U : 0x2321U)) {
            return false;
        }
        if (states[index] != 0 &&
            !renderer_.draw_text(framebuffer, 67, y, kSelectedMarker, 0x0705U)) {
            return false;
        }
    }
    return renderer_.draw_text(
        framebuffer,
        83,
        55 + static_cast<int>(20U * count),
        kConfirmLabel,
        setup_.cursor() == count ? 0x6663U : 0x2321U);
}

bool BattleSession::render_battlefield(
    render::IndexedFramebuffer& framebuffer) {
    const auto plan = setup_.battle_render_plan(render_state_, {});
    return plan.has_value() && renderer_.render(*plan, framebuffer);
}

void BattleSession::capture_selection_background(
    const render::IndexedFramebuffer& framebuffer) noexcept {
    std::copy(
        framebuffer.pixels().begin(),
        framebuffer.pixels().end(),
        selection_background_.begin());
    selection_palette_ = framebuffer.palette();
    selection_background_captured_ = true;
}

void BattleSession::restore_selection_background(
    render::IndexedFramebuffer& framebuffer) const noexcept {
    std::copy(
        selection_background_.begin(),
        selection_background_.end(),
        framebuffer.pixels().begin());
    framebuffer.set_palette(selection_palette_);
}

}  // namespace openlegend::battle
