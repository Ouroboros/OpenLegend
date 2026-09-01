#include "openlegend/battle/battle_session.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <span>
#include <string_view>
#include <utility>

#include "openlegend/diagnostics/log.hpp"
#include "openlegend/render/legacy_effects.hpp"
#include "openlegend/time/legacy_clock.hpp"

namespace openlegend::battle {
namespace {

constexpr std::uint8_t kEnter = 0x0DU;
constexpr std::uint8_t kSpace = 0x20U;
constexpr std::uint8_t kEscape = 0x1BU;
constexpr std::uint8_t kKeypadInsert = 0x96U;
constexpr std::uint8_t kDown = 0x98U;
constexpr std::uint8_t kPageDown = 0x99U;
constexpr std::uint8_t kLeft = 0x9AU;
constexpr std::uint8_t kRight = 0x9CU;
constexpr std::uint8_t kUp = 0x9EU;
constexpr std::uint8_t kPageUp = 0x9FU;
constexpr std::array<std::uint8_t, 20> kPartySelectionTitle{
    0xBDU, 0xD0U, 0xBFU, 0xEFU, 0xBEU, 0xDCU, 0xB0U, 0xD1U, 0xBBU, 0x50U,
    0xBEU, 0xD4U, 0xB0U, 0xABU, 0xA4U, 0xA7U, 0xA4U, 0x48U, 0xAAU, 0xABU};
constexpr std::array<std::uint8_t, 1> kSelectedMarker{'*'};
constexpr std::array<std::uint8_t, 4> kConfirmLabel{0xB5U, 0xB2U, 0xA7U, 0xF4U};
constexpr std::array<std::uint8_t, 12> kAttackDirectionPrompt{
    0xBFU, 0xEFU, 0xBEU, 0xDCU, 0xA7U, 0xF0U,
    0xC0U, 0xBBU, 0xA4U, 0xE8U, 0xA6U, 0x56U};
constexpr std::array<std::array<std::uint8_t, 4>, 10> kPlayerActionLabels{{
    {0xB2U, 0xBEU, 0xB0U, 0xCAU},
    {0xA7U, 0xF0U, 0xC0U, 0xBBU},
    {0xA5U, 0xCEU, 0xACU, 0x72U},
    {0xB8U, 0xD1U, 0xACU, 0x72U},
    {0xC2U, 0xE5U, 0xC0U, 0xF8U},
    {0xAAU, 0xABU, 0xABU, 0x7EU},
    {0xB5U, 0xA5U, 0xABU, 0xDDU},
    {0xAAU, 0xACU, 0xBAU, 0x41U},
    {0xA5U, 0xF0U, 0xAEU, 0xA7U},
    {0xA6U, 0xDBU, 0xB0U, 0xCAU},
}};
constexpr std::array<std::uint8_t, 8> kBattleDefeatText{
    0xBEU, 0xD4U, 0xB0U, 0xABU, 0xA5U, 0xA2U, 0xB1U, 0xD1U};
constexpr std::array<std::uint8_t, 8> kBattleVictoryText{
    0xBEU, 0xD4U, 0xB0U, 0xABU, 0xB3U, 0xD3U, 0xA7U, 0x51U};
constexpr std::array<std::uint8_t, 13> kExperienceGainedText{
    0x20U, 0xC0U, 0xF2U, 0xB1U, 0x6FU, 0xB8U, 0x67U, 0xC5U,
    0xE7U, 0xC2U, 0x49U, 0xBCU, 0xC6U};
constexpr std::array<std::uint8_t, 7> kLevelUpText{
    0x20U, 0xA4U, 0xC9U, 0xAFU, 0xC5U, 0xA4U, 0x46U};
constexpr std::array<std::uint8_t, 6> kPracticePrefix{
    0x20U, 0xADU, 0xD7U, 0xBDU, 0x6DU, 0x20U};
constexpr std::array<std::uint8_t, 6> kPracticeSuffix{
    0x20U, 0xA6U, 0xA8U, 0xA5U, 0x5CU, 0x20U};
constexpr std::array<std::uint8_t, 8> kMagicLevelPrefix{
    0x20U, 0xA4U, 0xC9U, 0xAFU, 0xC5U, 0xA4U, 0x46U, 0x20U};
constexpr std::array<std::uint8_t, 3> kMagicLevelSuffix{
    0x20U, 0xAFU, 0xC5U};
constexpr std::array<std::uint8_t, 8> kCraftedItemText{
    0x20U, 0xBBU, 0x73U, 0xB3U, 0x79U, 0xA5U, 0x58U, 0x20U};
constexpr std::array<std::uint8_t, 5> kUseItemPrefix{
    0xA8U, 0xCFU, 0xA5U, 0xCEU, 0x20U};
constexpr std::array<std::uint8_t, 4> kItemIncrease{
    0xB4U, 0xA3U, 0xA4U, 0xC9U};
constexpr std::array<std::uint8_t, 4> kItemDecrease{
    0xB4U, 0xEEU, 0xA4U, 0xD6U};
constexpr std::array<std::uint8_t, 20> kItemMpTypeChanged{
    0xA4U, 0xBAU, 0xA4U, 0x4FU, 0xAAU, 0xF9U, 0xB8U, 0xF4U, 0xA7U, 0xEFU,
    0xACU, 0xB0U, 0x20U, 0x20U, 0xB3U, 0xB1U, 0xB6U, 0xA7U, 0xA6U, 0x58U};
constexpr std::array<std::array<std::uint8_t, 20>, 23> kItemEffectLabels{{
    {0xA5U, 0xCDU, 0xA9U, 0x52U, 0xADU, 0xC8U},
    {0xA5U, 0xCDU, 0xA9U, 0x52U, 0xB3U, 0xCCU, 0xA4U, 0x6AU, 0xADU, 0xC8U},
    {0xA4U, 0xA4U, 0xACU, 0x72U, 0xB5U, 0x7BU, 0xABU, 0xD7U},
    {0xCAU, 0x5EU, 0xA4U, 0x4FU, 0xADU, 0xC8U},
    {0xA4U, 0xBAU, 0xA4U, 0x4FU, 0xAAU, 0xF9U, 0xB8U, 0xF4U},
    {0xA4U, 0xBAU, 0xA4U, 0x4FU, 0xADU, 0xC8U},
    {0xA4U, 0xBAU, 0xA4U, 0x4FU, 0xB3U, 0xCCU, 0xA4U, 0x6AU, 0xADU, 0xC8U},
    {0xAAU, 0x5AU, 0xA4U, 0x4FU, 0xADU, 0xC8U},
    {0xBBU, 0xB4U, 0xA5U, 0xADU, 0xC8U},
    {0xA8U, 0xBEU, 0xBFU, 0x6DU, 0xA4U, 0x4FU},
    {0xC2U, 0xE5U, 0xC0U, 0xF8U, 0xAFU, 0xE0U, 0xA4U, 0x4FU},
    {0xA8U, 0xCFU, 0xACU, 0x72U, 0xAFU, 0xE0U, 0xA4U, 0x4FU},
    {0xB8U, 0xD1U, 0xACU, 0x72U, 0xAFU, 0xE0U, 0xA4U, 0x4FU},
    {0xA7U, 0xDCU, 0xACU, 0x72U, 0xAFU, 0xE0U, 0xA4U, 0x4FU},
    {0xAEU, 0xB1U, 0xB4U, 0x78U, 0xA5U, 0xA4U, 0xD2U},
    {0xB1U, 0x73U, 0xBCU, 0x43U, 0xAFU, 0xE0U, 0xA4U, 0x4FU},
    {0xADU, 0x41U, 0xA4U, 0x4DU, 0xA7U, 0xDEU, 0xA5U, 0xA9U},
    {0xAFU, 0x53U, 0xAEU, 0xEDU, 0xA7U, 0x4CU, 0xBEU, 0xB9U},
    {0xB7U, 0x74U, 0xBEU, 0xB9U, 0xA7U, 0xDEU, 0xA5U, 0xA9U},
    {0xAAU, 0x5AU, 0xBEU, 0xC7U, 0xB1U, 0x60U, 0xC3U, 0xD1U},
    {0xA4U, 0x48U, 0xA9U, 0xCAU},
    {0xA7U, 0xF0U, 0xC0U, 0xBBU, 0xA6U, 0xB8U, 0xBCU, 0xC6U},
    {0xA5U, 0xA4U, 0xD2U, 0xB1U, 0x61U, 0xACU, 0x72U},
}};

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
    case BattleSessionPhase::player_action_selected: return "player_action_selected";
    case BattleSessionPhase::player_magic_selection: return "player_magic_selection";
    case BattleSessionPhase::player_attack_direction: return "player_attack_direction";
    case BattleSessionPhase::player_item_selection: return "player_item_selection";
    case BattleSessionPhase::player_item_effect_present: return "player_item_effect_present";
    case BattleSessionPhase::player_item_effect_wait: return "player_item_effect_wait";
    case BattleSessionPhase::player_status_selection: return "player_status_selection";
    case BattleSessionPhase::player_status_page_present: return "player_status_page_present";
    case BattleSessionPhase::player_status_page_wait: return "player_status_page_wait";
    case BattleSessionPhase::player_movement_select: return "player_movement_select";
    case BattleSessionPhase::player_targeting_select: return "player_targeting_select";
    case BattleSessionPhase::player_effect_prelude_present:
        return "player_effect_prelude_present";
    case BattleSessionPhase::player_effect_prelude_wait:
        return "player_effect_prelude_wait";
    case BattleSessionPhase::player_magic_frame_present:
        return "player_magic_frame_present";
    case BattleSessionPhase::player_magic_wait: return "player_magic_wait";
    case BattleSessionPhase::player_damage_frame_present:
        return "player_damage_frame_present";
    case BattleSessionPhase::player_damage_wait: return "player_damage_wait";
    case BattleSessionPhase::player_attack_commit_present:
        return "player_attack_commit_present";
    case BattleSessionPhase::player_attack_commit_wait:
        return "player_attack_commit_wait";
    case BattleSessionPhase::player_attack_level_present:
        return "player_attack_level_present";
    case BattleSessionPhase::player_attack_level_wait:
        return "player_attack_level_wait";
    case BattleSessionPhase::player_movement_step_present:
        return "player_movement_step_present";
    case BattleSessionPhase::player_movement_wait: return "player_movement_wait";
    case BattleSessionPhase::automatic_present: return "automatic_present";
    case BattleSessionPhase::ai_action: return "ai_action";
    case BattleSessionPhase::ai_prelude_present: return "ai_prelude_present";
    case BattleSessionPhase::ai_wait: return "ai_wait";
    case BattleSessionPhase::ai_item_effect_present: return "ai_item_effect_present";
    case BattleSessionPhase::ai_item_effect_wait: return "ai_item_effect_wait";
    case BattleSessionPhase::ai_item_post_effect_wait: return "ai_item_post_effect_wait";
    case BattleSessionPhase::ai_effect_prelude_present:
        return "ai_effect_prelude_present";
    case BattleSessionPhase::ai_effect_prelude_wait: return "ai_effect_prelude_wait";
    case BattleSessionPhase::ai_magic_frame_present: return "ai_magic_frame_present";
    case BattleSessionPhase::ai_magic_wait: return "ai_magic_wait";
    case BattleSessionPhase::ai_damage_frame_present: return "ai_damage_frame_present";
    case BattleSessionPhase::ai_damage_wait: return "ai_damage_wait";
    case BattleSessionPhase::ai_attack_commit_present:
        return "ai_attack_commit_present";
    case BattleSessionPhase::ai_attack_commit_wait: return "ai_attack_commit_wait";
    case BattleSessionPhase::ai_attack_level_present:
        return "ai_attack_level_present";
    case BattleSessionPhase::ai_attack_level_wait: return "ai_attack_level_wait";
    case BattleSessionPhase::ai_movement_step_present:
        return "ai_movement_step_present";
    case BattleSessionPhase::ai_movement_wait: return "ai_movement_wait";
    case BattleSessionPhase::round_wait: return "round_wait";
    case BattleSessionPhase::battle_outcome: return "battle_outcome";
    case BattleSessionPhase::battle_outcome_wait: return "battle_outcome_wait";
    case BattleSessionPhase::post_battle_message_present:
        return "post_battle_message_present";
    case BattleSessionPhase::post_battle_message_wait:
        return "post_battle_message_wait";
    case BattleSessionPhase::complete: return "complete";
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

[[nodiscard]] int centered_magic_name_x(
    const std::span<const std::uint8_t> name) noexcept {
    for (std::size_t byte = 2U; byte <= name.size(); byte += 2U) {
        if (byte == name.size() || name[byte] == 0U) {
            return 65 - static_cast<int>(byte * 4U);
        }
    }
    return 25;
}

[[nodiscard]] std::vector<std::uint8_t> decimal_text(
    const std::int32_t value,
    const int width = 0) {
    std::array<char, 16> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    const auto count = static_cast<int>(converted.ptr - buffer.data());
    std::vector<std::uint8_t> text;
    text.reserve(static_cast<std::size_t>(std::max(width, count)));
    for (int index = count; index < width; ++index) {
        text.push_back(static_cast<std::uint8_t>(' '));
    }
    for (const auto* cursor = buffer.data(); cursor != converted.ptr; ++cursor) {
        text.push_back(static_cast<std::uint8_t>(*cursor));
    }
    return text;
}

}  // namespace

BattleSession::BattleSession(
    const resource::DataRoot& data_root,
    model::RangerState& ranger,
    random::LegacyRandom& random,
    const std::int16_t battle_id,
    const bool grant_experience,
    const BattleRenderState initial_render_state)
    : ranger_(ranger),
      random_(random),
      data_(data_root, battle_id),
      setup_(data_, ranger_),
      pathing_(data_),
      renderer_(data_root, data_.battlefield_id()),
      render_state_(initial_render_state),
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

const BattleItemSelectionState* BattleSession::player_item_selection() const noexcept {
    return player_item_ ? &player_item_->selection : nullptr;
}

std::int16_t BattleSession::player_item_page() const noexcept {
    return player_item_ ? player_item_->page : 0;
}

std::int16_t BattleSession::player_item_row() const noexcept {
    return player_item_ ? player_item_->row : 0;
}

std::int16_t BattleSession::player_item_column() const noexcept {
    return player_item_ ? player_item_->column : 0;
}

std::size_t BattleSession::player_status_count() const noexcept {
    return player_status_.has_value() ? player_status_->party_count : 0U;
}

std::size_t BattleSession::player_status_cursor() const noexcept {
    return player_status_.has_value() ? player_status_->cursor : 0U;
}

std::int16_t BattleSession::player_status_role_id() const noexcept {
    return player_status_.has_value() ? player_status_->role_id : -1;
}

std::uint8_t BattleSession::player_status_page() const noexcept {
    return player_status_.has_value() ? player_status_->page : 0U;
}

BattleSessionInputResult BattleSession::handle_key(
    const std::uint8_t translated_key,
    const std::optional<std::uint32_t> bios_tick) {
    if (!valid() || translated_key == 0U) {
        return BattleSessionInputResult::ignored;
    }
    if (phase_ == BattleSessionPhase::battle_outcome_wait) {
        if (!begin_post_battle_settlement()) {
            return BattleSessionInputResult::ignored;
        }
        return BattleSessionInputResult::outcome_acknowledged;
    }
    if (phase_ == BattleSessionPhase::post_battle_message_wait) {
        if (!advance_post_battle_message()) {
            return BattleSessionInputResult::ignored;
        }
        return BattleSessionInputResult::post_battle_message_acknowledged;
    }
    if (phase_ == BattleSessionPhase::player_action) {
        return handle_player_action_key(translated_key);
    }
    if (phase_ == BattleSessionPhase::player_magic_selection) {
        return handle_player_magic_selection_key(translated_key);
    }
    if (phase_ == BattleSessionPhase::player_attack_direction) {
        return handle_player_attack_direction_key(translated_key);
    }
    if (phase_ == BattleSessionPhase::player_item_selection) {
        return handle_player_item_key(translated_key);
    }
    if (phase_ == BattleSessionPhase::player_status_selection) {
        return handle_player_status_selection_key(translated_key);
    }
    if (phase_ == BattleSessionPhase::player_status_page_wait) {
        return handle_player_status_page_key(translated_key);
    }
    if (phase_ == BattleSessionPhase::player_item_effect_wait) {
        if (!player_item_ || !player_item_->effect_result.has_value() ||
            !setup_.finish_player_item_action(current_actor_slot_)) {
            error_ = setup_.valid()
                ? "battle player item completion state is absent"
                : setup_.error();
            return BattleSessionInputResult::ignored;
        }
        diagnostics::log_info(
            "battle player item effect acknowledged id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " item=" + std::to_string(player_item_->selected_item_id) +
            " effects=" + std::to_string(player_item_->effect_result->effect_count));
        player_item_.reset();
        if (!finish_current_actor(BattlePlayerAction::item)) {
            return BattleSessionInputResult::ignored;
        }
        return BattleSessionInputResult::item_effect_acknowledged;
    }
    if (phase_ == BattleSessionPhase::ai_item_effect_wait) {
        if (bios_tick.has_value()) {
            ai_item_wait_tick_ = *bios_tick;
        }
        if (!player_item_ || !player_item_->effect_result.has_value() ||
            !begin_ai_item_post_effect_wait()) {
            return BattleSessionInputResult::ignored;
        }
        diagnostics::log_info(
            "battle AI item effect acknowledged id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " item=" + std::to_string(player_item_->selected_item_id) +
            " effects=" + std::to_string(player_item_->effect_result->effect_count) +
            " wait_tick_changes=" +
            std::to_string(ai_item_wait_tick_changes_remaining_));
        return BattleSessionInputResult::item_effect_acknowledged;
    }
    if (phase_ == BattleSessionPhase::player_movement_select) {
        return handle_player_movement_key(translated_key);
    }
    if (phase_ == BattleSessionPhase::player_targeting_select) {
        return handle_player_targeting_key(translated_key);
    }
    if (phase_ != BattleSessionPhase::party_selection) {
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

std::vector<BattleAudioCommand> BattleSession::take_audio_commands() {
    if (!player_target_effect_) {
        return {};
    }
    auto commands = std::move(player_target_effect_->audio_commands);
    player_target_effect_->audio_commands.clear();
    return commands;
}

void BattleSession::advance(const std::uint32_t bios_tick) {
    if (!valid()) {
        return;
    }
    if (phase_ == BattleSessionPhase::round_start) {
        static_cast<void>(begin_round(bios_tick));
    } else if (phase_ == BattleSessionPhase::ai_action) {
        static_cast<void>(begin_ai_action());
    } else if (phase_ == BattleSessionPhase::ai_wait) {
        static_cast<void>(advance_ai_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::ai_movement_wait) {
        static_cast<void>(advance_ai_movement_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::player_movement_wait) {
        static_cast<void>(advance_player_movement_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::player_effect_prelude_wait ||
               phase_ == BattleSessionPhase::ai_effect_prelude_wait) {
        static_cast<void>(advance_player_effect_prelude_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::ai_item_post_effect_wait) {
        static_cast<void>(advance_ai_item_post_effect_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::player_magic_wait ||
               phase_ == BattleSessionPhase::ai_magic_wait) {
        static_cast<void>(advance_player_magic_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::player_damage_wait ||
               phase_ == BattleSessionPhase::ai_damage_wait) {
        static_cast<void>(advance_player_damage_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::player_attack_commit_wait ||
               phase_ == BattleSessionPhase::ai_attack_commit_wait) {
        static_cast<void>(advance_player_attack_commit_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::player_attack_level_wait ||
               phase_ == BattleSessionPhase::ai_attack_level_wait) {
        static_cast<void>(advance_player_attack_level_wait(bios_tick));
    } else if (phase_ == BattleSessionPhase::round_wait && bios_tick != round_tick_) {
        static_cast<void>(begin_round(bios_tick));
    }
}

bool BattleSession::render(render::IndexedFramebuffer& framebuffer) {
    if (!valid()) {
        return false;
    }
    bool rendered = false;
    if (phase_ == BattleSessionPhase::party_selection) {
        rendered = render_party_selection(framebuffer);
    } else if (phase_ == BattleSessionPhase::battle_outcome ||
               phase_ == BattleSessionPhase::battle_outcome_wait) {
        rendered = render_battle_outcome(framebuffer);
    } else if (phase_ == BattleSessionPhase::post_battle_message_present ||
               phase_ == BattleSessionPhase::post_battle_message_wait) {
        rendered = render_post_battle_message(framebuffer);
    } else if (phase_ == BattleSessionPhase::player_action) {
        rendered = render_player_action_menu(framebuffer);
    } else if (phase_ == BattleSessionPhase::ai_prelude_present ||
               phase_ == BattleSessionPhase::ai_wait) {
        const auto status_panel = setup_.status_panel_plan(current_actor_slot_);
        rendered = render_battlefield(framebuffer) && status_panel.has_value() &&
            renderer_.render_status_panel(*status_panel, framebuffer);
    } else if (phase_ == BattleSessionPhase::player_magic_selection) {
        rendered = render_player_magic_selection(framebuffer);
    } else if (phase_ == BattleSessionPhase::player_item_selection) {
        rendered = render_player_item_selection(framebuffer);
    } else if (phase_ == BattleSessionPhase::player_item_effect_present ||
               phase_ == BattleSessionPhase::player_item_effect_wait ||
               phase_ == BattleSessionPhase::ai_item_effect_present ||
               phase_ == BattleSessionPhase::ai_item_effect_wait ||
               (phase_ == BattleSessionPhase::ai_item_post_effect_wait &&
                player_item_ && player_item_->effect_result.has_value() &&
                player_item_->effect_result->has_effect)) {
        rendered = render_player_item_effect(framebuffer);
    } else if (phase_ == BattleSessionPhase::player_status_selection) {
        rendered = render_player_status_selection(framebuffer);
    } else if (phase_ == BattleSessionPhase::player_status_page_present ||
               phase_ == BattleSessionPhase::player_status_page_wait) {
        rendered = render_player_status_page(framebuffer);
    } else if (phase_ == BattleSessionPhase::player_attack_direction) {
        rendered = render_player_attack_direction(framebuffer);
    } else if (phase_ == BattleSessionPhase::player_attack_level_present ||
               phase_ == BattleSessionPhase::player_attack_level_wait ||
               phase_ == BattleSessionPhase::ai_attack_level_present ||
               phase_ == BattleSessionPhase::ai_attack_level_wait) {
        rendered = render_player_attack_level(framebuffer);
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

void BattleSession::finish_presented_tick(const std::uint32_t bios_tick) {
    if (!valid() || !frame_rendered_) {
        return;
    }
    frame_rendered_ = false;
    if (phase_ == BattleSessionPhase::initial_present) {
        if (!setup_.sort_by_effective_speed() || setup_.combatant_count() <= 0) {
            error_ = setup_.valid()
                ? "battle has no combatants after initial presentation"
                : setup_.error();
            return;
        }
        current_actor_slot_ = 0U;
        const auto& actor = setup_.combatants()[0U].words;
        render_state_.view_x = static_cast<std::int16_t>(
            std::clamp(static_cast<int>(actor[combatant_word::x]) - 11, 0, 32));
        render_state_.view_y = static_cast<std::int16_t>(
            std::clamp(static_cast<int>(actor[combatant_word::y]) - 11, 0, 32));
        render_state_.primary_cursor = {
            actor[combatant_word::x], actor[combatant_word::y]};
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
    if (phase_ == BattleSessionPhase::battle_outcome) {
        phase_ = BattleSessionPhase::battle_outcome_wait;
        diagnostics::log_info(
            "battle outcome presented id=" + std::to_string(battle_id()) +
            " outcome=" + std::to_string(static_cast<int>(outcome_)));
        return;
    }
    if (phase_ == BattleSessionPhase::post_battle_message_present) {
        if (!post_battle_result_.has_value() ||
            post_battle_message_index_ >= post_battle_messages_.size()) {
            error_ = "battle post-battle message continuation is absent";
            return;
        }
        phase_ = BattleSessionPhase::post_battle_message_wait;
        diagnostics::log_info(
            "battle post-battle message presented id=" + std::to_string(battle_id()) +
            " index=" + std::to_string(post_battle_message_index_) +
            " count=" + std::to_string(post_battle_messages_.size()));
        return;
    }
    if (phase_ == BattleSessionPhase::player_status_page_present) {
        if (!player_status_.has_value() || player_status_->page > 1U) {
            error_ = "battle player status page continuation is absent";
            return;
        }
        phase_ = BattleSessionPhase::player_status_page_wait;
        diagnostics::log_info(
            "battle player status page presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " role=" + std::to_string(player_status_->role_id) +
            " page=" + std::to_string(player_status_->page));
        return;
    }
    if (phase_ == BattleSessionPhase::player_item_effect_present) {
        if (!player_item_ || !player_item_->selected_inventory_slot.has_value() ||
            !player_item_->effect_result.has_value() || player_item_->inventory_consumed ||
            !setup_.consume_inventory_item_slot(*player_item_->selected_inventory_slot)) {
            error_ = setup_.valid()
                ? "battle player item inventory continuation is invalid"
                : setup_.error();
            return;
        }
        player_item_->inventory_consumed = true;
        player_item_->effect_result->item_consumed = true;
        phase_ = BattleSessionPhase::player_item_effect_wait;
        diagnostics::log_info(
            "battle player item effect presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " item=" + std::to_string(player_item_->selected_item_id) +
            " effects=" + std::to_string(player_item_->effect_result->effect_count));
        return;
    }
    if (phase_ == BattleSessionPhase::ai_item_effect_present) {
        if (!player_item_ || !player_item_->effect_result.has_value() ||
            !player_item_->effect_result->has_effect ||
            player_item_->effect_result->item_consumed) {
            error_ = "battle AI item effect continuation is invalid";
            return;
        }
        ai_item_wait_tick_ = bios_tick;
        phase_ = BattleSessionPhase::ai_item_effect_wait;
        diagnostics::log_info(
            "battle AI item effect presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " item=" + std::to_string(player_item_->selected_item_id) +
            " effects=" + std::to_string(player_item_->effect_result->effect_count));
        return;
    }
    if (phase_ == BattleSessionPhase::player_effect_prelude_present ||
        phase_ == BattleSessionPhase::ai_effect_prelude_present) {
        const auto ai_controlled = phase_ == BattleSessionPhase::ai_effect_prelude_present;
        if (!player_target_effect_ || !player_target_effect_->effect_animation.has_value()) {
            error_ = "battle effect prelude continuation is absent";
            return;
        }
        player_target_effect_->animation_wait_tick = bios_tick;
        player_target_effect_->animation_wait_tick_changes_remaining =
            timing::legacy_delay_tick_count(
                player_target_effect_->effect_animation->prelude_wait_ticks);
        phase_ = ai_controlled ? BattleSessionPhase::ai_effect_prelude_wait
                               : BattleSessionPhase::player_effect_prelude_wait;
        diagnostics::log_debug(
            std::string{"battle "} + (ai_controlled ? "AI" : "player") +
            " effect prelude presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " wait_tick_changes=" + std::to_string(
                player_target_effect_->animation_wait_tick_changes_remaining));
        return;
    }
    if (phase_ == BattleSessionPhase::player_magic_frame_present ||
        phase_ == BattleSessionPhase::ai_magic_frame_present) {
        const auto ai_controlled = phase_ == BattleSessionPhase::ai_magic_frame_present;
        if (!player_target_effect_) {
            error_ = "battle magic frame continuation is absent";
            return;
        }
        auto& effect = *player_target_effect_;
        const auto& frames = effect.effect_animation.has_value()
            ? effect.effect_animation->frames
            : effect.magic_animation.frames;
        if (effect.magic_frame >= frames.size()) {
            error_ = "battle magic frame is outside animation plan";
            return;
        }
        effect.animation_wait_tick = bios_tick;
        effect.animation_wait_tick_changes_remaining = timing::legacy_delay_tick_count(
            frames[effect.magic_frame].wait_ticks);
        phase_ = ai_controlled ? BattleSessionPhase::ai_magic_wait
                               : BattleSessionPhase::player_magic_wait;
        diagnostics::log_debug(
            std::string{"battle "} + (ai_controlled ? "AI" : "player") +
            " magic frame presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " frame=" + std::to_string(effect.magic_frame) +
            " wait_tick_changes=" +
            std::to_string(effect.animation_wait_tick_changes_remaining));
        return;
    }
    if (phase_ == BattleSessionPhase::player_damage_frame_present ||
        phase_ == BattleSessionPhase::ai_damage_frame_present) {
        const auto ai_controlled = phase_ == BattleSessionPhase::ai_damage_frame_present;
        if (!player_target_effect_ ||
            player_target_effect_->damage_frame >=
                player_target_effect_->damage_animation.size()) {
            error_ = "battle damage frame is outside animation plan";
            return;
        }
        auto& effect = *player_target_effect_;
        effect.animation_wait_tick = bios_tick;
        effect.animation_wait_tick_changes_remaining = timing::legacy_delay_tick_count(
            effect.damage_animation[effect.damage_frame].wait_ticks);
        phase_ = ai_controlled ? BattleSessionPhase::ai_damage_wait
                               : BattleSessionPhase::player_damage_wait;
        diagnostics::log_debug(
            std::string{"battle "} + (ai_controlled ? "AI" : "player") +
            " damage frame presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " frame=" + std::to_string(effect.damage_frame) +
            " wait_tick_changes=" +
            std::to_string(effect.animation_wait_tick_changes_remaining));
        return;
    }
    if (phase_ == BattleSessionPhase::player_attack_commit_present ||
        phase_ == BattleSessionPhase::ai_attack_commit_present) {
        const auto ai_controlled = phase_ == BattleSessionPhase::ai_attack_commit_present;
        if (!player_target_effect_ || !player_attack_) {
            error_ = "battle attack commit continuation is absent";
            return;
        }
        player_target_effect_->animation_wait_tick = bios_tick;
        player_target_effect_->animation_wait_tick_changes_remaining =
            timing::legacy_delay_tick_count(17);
        phase_ = ai_controlled ? BattleSessionPhase::ai_attack_commit_wait
                               : BattleSessionPhase::player_attack_commit_wait;
        diagnostics::log_debug(
            std::string{"battle "} + (ai_controlled ? "AI" : "player") +
            " attack commit frame presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " iteration=" + std::to_string(player_attack_->iteration) +
            " wait_tick_changes=" +
            std::to_string(
                player_target_effect_->animation_wait_tick_changes_remaining));
        return;
    }
    if (phase_ == BattleSessionPhase::player_attack_level_present ||
        phase_ == BattleSessionPhase::ai_attack_level_present) {
        const auto ai_controlled = phase_ == BattleSessionPhase::ai_attack_level_present;
        if (!player_target_effect_ || !player_attack_) {
            error_ = "battle attack level continuation is absent";
            return;
        }
        player_target_effect_->animation_wait_tick = bios_tick;
        player_target_effect_->animation_wait_tick_changes_remaining =
            timing::legacy_delay_tick_count(500);
        phase_ = ai_controlled ? BattleSessionPhase::ai_attack_level_wait
                               : BattleSessionPhase::player_attack_level_wait;
        diagnostics::log_debug(
            std::string{"battle "} + (ai_controlled ? "AI" : "player") +
            " attack level frame presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " iteration=" + std::to_string(player_attack_->iteration) +
            " wait_tick_changes=" +
            std::to_string(
                player_target_effect_->animation_wait_tick_changes_remaining));
        return;
    }
    if (phase_ == BattleSessionPhase::automatic_present) {
        setup_.enable_automatic_mode();
        phase_ = BattleSessionPhase::ai_action;
        diagnostics::log_info(
            "battle automatic mode enabled id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " phase=" + std::string{phase_name(phase_)});
        return;
    }
    if (phase_ == BattleSessionPhase::player_movement_step_present) {
        player_movement_wait_tick_ = bios_tick;
        player_movement_wait_tick_changes_remaining_ = timing::legacy_delay_tick_count(40);
        phase_ = BattleSessionPhase::player_movement_wait;
        diagnostics::log_info(
            "battle player movement step presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " wait_tick_changes=" +
            std::to_string(player_movement_wait_tick_changes_remaining_));
        return;
    }
    if (phase_ == BattleSessionPhase::ai_movement_step_present) {
        ai_movement_wait_tick_ = bios_tick;
        ai_movement_wait_tick_changes_remaining_ = timing::legacy_delay_tick_count(40);
        phase_ = BattleSessionPhase::ai_movement_wait;
        diagnostics::log_info(
            "battle AI movement step presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " wait_tick_changes=" +
            std::to_string(ai_movement_wait_tick_changes_remaining_));
        return;
    }
    if (phase_ == BattleSessionPhase::ai_prelude_present) {
        ai_wait_tick_ = bios_tick;
        ai_wait_tick_changes_remaining_ = timing::legacy_delay_tick_count(
            ai_turn_prelude_.has_value() ? ai_turn_prelude_->wait_ticks : 300);
        phase_ = BattleSessionPhase::ai_wait;
        diagnostics::log_info(
            "battle AI prelude presented id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " wait_tick_changes=" +
            std::to_string(ai_wait_tick_changes_remaining_));
        return;
    }
    if (phase_ == BattleSessionPhase::actor_present) {
        auto& words = setup_.combatants()[current_actor_slot_].words;
        words[combatant_word::action_done] = 0;
        words[combatant_word::ai_action] = 0;
        const auto side = words[combatant_word::side];
        if (side == 0 && !setup_.automatic_enabled()) {
            if (!begin_player_action_menu()) {
                diagnostics::log_error(
                    "battle player action menu failed id=" + std::to_string(battle_id()) +
                    " slot=" + std::to_string(current_actor_slot_) +
                    " reason=" + error_);
                return;
            }
        } else {
            phase_ = BattleSessionPhase::ai_action;
        }
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
    if (setup_.combatant_count() <= 0) {
        error_ = "battle has no combatants after setup";
        return false;
    }
    if (!renderer_.load_battle_assets()) {
        error_ = renderer_.error();
        return false;
    }
    current_actor_slot_ = 0U;
    const auto& actor = setup_.combatants()[0U].words;
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

bool BattleSession::begin_round(const std::uint32_t bios_tick) {
    if (!setup_.sort_by_effective_speed() || !setup_.prepare_round() ||
        setup_.combatant_count() <= 0) {
        error_ = setup_.valid() ? "battle round preparation failed" : setup_.error();
        return false;
    }
    round_tick_ = bios_tick;
    render_state_.effect_frame_offset = 0;
    render_state_.effect_visible = false;
    render_state_.highlight_mode = 0;
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

bool BattleSession::begin_ai_action() {
    ai_turn_prelude_ = setup_.begin_ai_turn(current_actor_slot_);
    ai_turn_decision_.reset();
    if (!ai_turn_prelude_.has_value()) {
        error_ = "battle AI prelude is invalid";
        return false;
    }
    phase_ = BattleSessionPhase::ai_prelude_present;
    diagnostics::log_info(
        "battle AI prelude ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " allied_total=" + std::to_string(ai_turn_prelude_->allied_total) +
        " opponent_total=" + std::to_string(ai_turn_prelude_->opponent_total) +
        " allied_count=" + std::to_string(ai_turn_prelude_->allied_count) +
        " opponent_count=" + std::to_string(ai_turn_prelude_->opponent_count));
    return true;
}

bool BattleSession::advance_ai_wait(const std::uint32_t bios_tick) {
    if (bios_tick == ai_wait_tick_) {
        return true;
    }
    ai_wait_tick_ = bios_tick;
    if (ai_wait_tick_changes_remaining_ > 0) {
        --ai_wait_tick_changes_remaining_;
    }
    if (ai_wait_tick_changes_remaining_ > 0) {
        return true;
    }

    ai_turn_decision_ = setup_.choose_ai_turn_action(current_actor_slot_, random_);
    if (!ai_turn_decision_.has_value()) {
        error_ = "battle AI action selection failed";
        return false;
    }
    diagnostics::log_info(
        "battle AI action selected id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " action=" +
        std::to_string(static_cast<std::int16_t>(ai_turn_decision_->choice.action)) +
        " handler=" +
        std::to_string(static_cast<std::int16_t>(ai_turn_decision_->handler)) +
        " target=" + std::to_string(ai_turn_decision_->choice.target_slot));
    if (!dispatch_selected_ai_action()) {
        diagnostics::log_error(
            "battle AI action dispatch failed id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " action=" +
            std::to_string(static_cast<std::int16_t>(ai_turn_decision_->choice.action)) +
            " reason=" + error_);
        return false;
    }
    return true;
}

bool BattleSession::dispatch_selected_ai_action() {
    if (!ai_turn_decision_.has_value()) {
        error_ = "battle AI decision is absent";
        return false;
    }
    const auto& choice = ai_turn_decision_->choice;
    const auto target_coordinate = [this](const std::int16_t slot)
        -> std::optional<BattlePathCoord> {
        if (slot < 0 || slot >= setup_.combatant_count()) {
            return std::nullopt;
        }
        const auto& target = setup_.combatants()[static_cast<std::size_t>(slot)].words;
        return BattlePathCoord{target[combatant_word::x], target[combatant_word::y]};
    };
    const auto begin_targeted_movement = [this, &target_coordinate](
                                             const std::int16_t target_slot,
                                             const std::int16_t mode,
                                             const std::int16_t range,
                                             const AiMovementContinuation continuation) {
        const auto target = target_coordinate(target_slot);
        if (!target.has_value()) {
            error_ = "battle AI movement target is outside combatant slots";
            return false;
        }
        return begin_ai_movement_to(
            target_slot, *target, mode, range, continuation);
    };

    switch (ai_turn_decision_->handler) {
    case BattleAiHandler::rest:
        return finish_ai_handler(BattlePlayerAction::rest, true);
    case BattleAiHandler::move:
        return begin_ai_movement_to(
            choice.target_slot,
            choice.target,
            0,
            0,
            AiMovementContinuation::direct);
    case BattleAiHandler::escape: {
        const auto escape = setup_.ai_escape_plan(current_actor_slot_, true);
        if (!escape.has_value()) {
            error_ = "battle AI escape plan failed";
            return false;
        }
        if (!escape->destination.has_value()) {
            return finish_ai_handler(BattlePlayerAction::rest, true);
        }
        return begin_ai_movement_to(
            -1, *escape->destination, 0, 0, AiMovementContinuation::escape);
    }
    case BattleAiHandler::attack:
        return begin_ai_attack_action();
    case BattleAiHandler::use_poison: {
        const auto stale_target = setup_.combatants()[current_actor_slot_]
                                      .words[combatant_word::ai_poison_target];
        ai_poison_plan_ = setup_.begin_ai_poison_plan(
            current_actor_slot_, static_cast<std::size_t>(stale_target), random_);
        if (!ai_poison_plan_.has_value()) {
            error_ = setup_.valid() ? "battle AI poison plan failed" : setup_.error();
            return false;
        }
        if (ai_poison_plan_->next_step == BattleAiPoisonNextStep::move) {
            return begin_targeted_movement(
                ai_poison_plan_->target_slot,
                ai_poison_plan_->movement_mode,
                ai_poison_plan_->targeting_range,
                AiMovementContinuation::poison);
        }
        return continue_ai_poison_plan();
    }
    case BattleAiHandler::item:
        ai_item_plan_ = setup_.begin_ai_item_plan(current_actor_slot_, choice);
        if (!ai_item_plan_.has_value()) {
            error_ = setup_.valid() ? "battle AI item plan failed" : setup_.error();
            return false;
        }
        if (ai_item_plan_->next_step == BattleAiItemNextStep::move &&
            ai_item_plan_->relocation_destination.has_value()) {
            return begin_ai_movement_to(
                -1,
                *ai_item_plan_->relocation_destination,
                0,
                0,
                AiMovementContinuation::item);
        }
        return continue_ai_item_plan();
    case BattleAiHandler::request_medicine:
    case BattleAiHandler::request_detox:
        ai_request_plan_ = setup_.begin_ai_request_plan(current_actor_slot_, choice);
        if (!ai_request_plan_.has_value()) {
            error_ = "battle AI request plan failed";
            return false;
        }
        if (ai_request_plan_->next_step == BattleAiRequestNextStep::move) {
            return begin_ai_movement_to(
                ai_request_plan_->target_slot,
                ai_request_plan_->target,
                ai_request_plan_->movement_mode,
                ai_request_plan_->movement_value,
                AiMovementContinuation::request);
        }
        return continue_ai_request_plan();
    case BattleAiHandler::detox:
    case BattleAiHandler::medicine:
        ai_support_plan_ = setup_.begin_ai_support_plan(current_actor_slot_, choice);
        if (!ai_support_plan_.has_value()) {
            error_ = setup_.valid() ? "battle AI support plan failed" : setup_.error();
            return false;
        }
        if (ai_support_plan_->next_step == BattleAiSupportNextStep::move) {
            return begin_targeted_movement(
                ai_support_plan_->target_slot,
                ai_support_plan_->movement_mode,
                ai_support_plan_->movement_value,
                AiMovementContinuation::support);
        }
        return continue_ai_support_plan();
    case BattleAiHandler::throwing_weapon:
        ai_item_plan_ = setup_.begin_ai_throwing_weapon_plan(
            current_actor_slot_, choice, random_);
        if (!ai_item_plan_.has_value()) {
            error_ = setup_.valid() ? "battle AI throwing-weapon plan failed" : setup_.error();
            return false;
        }
        if (ai_item_plan_->next_step == BattleAiItemNextStep::move) {
            return begin_targeted_movement(
                ai_item_plan_->target_slot,
                ai_item_plan_->movement_mode,
                ai_item_plan_->targeting_range,
                AiMovementContinuation::throwing_weapon);
        }
        return continue_ai_item_plan();
    }
    error_ = "battle AI handler is invalid";
    return false;
}

bool BattleSession::begin_ai_attack_action() {
    ai_attack_plan_ = setup_.begin_ai_attack_plan(current_actor_slot_, random_);
    if (!ai_attack_plan_.has_value()) {
        error_ = setup_.valid() ? "battle AI attack plan failed" : setup_.error();
        return false;
    }
    if (ai_attack_plan_->next_step == BattleAiAttackNextStep::move) {
        const auto target_slot = ai_attack_plan_->target_slot;
        if (target_slot < 0 || target_slot >= setup_.combatant_count()) {
            error_ = "battle AI attack movement target is outside combatant slots";
            return false;
        }
        const auto& target_words = setup_.combatants()[
            static_cast<std::size_t>(target_slot)].words;
        return begin_ai_movement_to(
            target_slot,
            BattlePathCoord{
                target_words[combatant_word::x], target_words[combatant_word::y]},
            ai_attack_plan_->movement_mode,
            ai_attack_plan_->select_distance,
            AiMovementContinuation::attack);
    }
    if (ai_attack_plan_->next_step == BattleAiAttackNextStep::rest) {
        ai_attack_plan_.reset();
        return finish_ai_handler(BattlePlayerAction::rest, true);
    }
    if (ai_attack_plan_->next_step == BattleAiAttackNextStep::finish) {
        ai_attack_plan_.reset();
        return finish_ai_handler(BattlePlayerAction::attack, false);
    }
    return begin_ai_attack_execution();
}

bool BattleSession::continue_ai_poison_plan() {
    if (!ai_poison_plan_.has_value()) {
        error_ = "battle AI poison continuation plan is absent";
        return false;
    }
    if (ai_poison_plan_->next_step == BattleAiPoisonNextStep::poison) {
        return begin_ai_poison_execution();
    }
    if (ai_poison_plan_->next_step == BattleAiPoisonNextStep::attack_fallback) {
        diagnostics::log_info(
            "battle AI poison fallback attack id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " target=" + std::to_string(ai_poison_plan_->target_slot));
        ai_poison_plan_.reset();
        return begin_ai_attack_action();
    }
    if (ai_poison_plan_->next_step == BattleAiPoisonNextStep::rest) {
        diagnostics::log_info(
            "battle AI poison fallback rest id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " target=" + std::to_string(ai_poison_plan_->target_slot));
        ai_poison_plan_.reset();
        return finish_ai_handler(BattlePlayerAction::rest, true);
    }
    error_ = "battle AI poison continuation remained in movement state";
    return false;
}

bool BattleSession::begin_ai_poison_execution() {
    if (!ai_poison_plan_.has_value() ||
        ai_poison_plan_->next_step != BattleAiPoisonNextStep::poison ||
        ai_poison_plan_->target_slot < 0 ||
        ai_poison_plan_->target_slot >= setup_.combatant_count()) {
        error_ = "battle AI poison execution plan is invalid";
        return false;
    }
    const auto target_slot = static_cast<std::size_t>(ai_poison_plan_->target_slot);
    const auto& target_words = setup_.combatants()[target_slot].words;
    const BattlePathCoord target{
        target_words[combatant_word::x], target_words[combatant_word::y]};
    const auto result = setup_.apply_poison_target(current_actor_slot_, target);
    if (!result.has_value()) {
        error_ = setup_.valid() ? "battle AI poison state application failed" : setup_.error();
        return false;
    }

    selected_magic_slot_ = 0;
    auto animation = setup_.magic_animation_plan(
        current_actor_slot_, selected_magic_slot_, 0, 30, kBattleFightPointerBase);
    if (!animation.has_value()) {
        error_ = "battle AI poison magic animation is invalid";
        return false;
    }
    if (!renderer_.load_fight_package(animation->fight_head_id)) {
        error_ = "battle AI poison FIGHT package load failed";
        return false;
    }
    player_target_effect_ = std::make_unique<PlayerTargetEffectState>(
        PlayerTargetEffectState{
            .action = BattlePlayerAction::use_poison,
            .ai_controlled = true,
            .magic_animation = std::move(*animation),
            .effect_animation = std::nullopt,
            .effect_id = 30,
            .damage_kind = 2,
            .damage_suppress_flash = false,
            .audio_commands = {},
        });
    render_state_.path_limit = 0;
    render_state_.primary_cursor = target;
    render_state_.effect_id = kBattleEffectPointerBase;
    render_state_.effect_visible = false;
    render_state_.damage_kind = 0;
    render_state_.highlight_enabled = false;
    diagnostics::log_info(
        "battle AI poison effect ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " target=" + std::to_string(ai_poison_plan_->target_slot) +
        " target_coordinate=" + std::to_string(target.x) + "," +
        std::to_string(target.y) +
        " hits=" + std::to_string(result->hit_count) +
        " frames=" +
        std::to_string(player_target_effect_->magic_animation.frames.size()));
    return prepare_player_magic_frame();
}

bool BattleSession::continue_ai_request_plan() {
    if (!ai_request_plan_.has_value() ||
        ai_request_plan_->next_step != BattleAiRequestNextStep::automatic_attack) {
        error_ = "battle AI request continuation plan is invalid";
        return false;
    }
    diagnostics::log_info(
        "battle AI request attack ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " action=" +
        std::to_string(static_cast<std::int16_t>(ai_request_plan_->request_action)) +
        " target=" + std::to_string(ai_request_plan_->target_slot) +
        " target_coordinate=" + std::to_string(ai_request_plan_->target.x) + "," +
        std::to_string(ai_request_plan_->target.y));
    ai_request_plan_.reset();
    return begin_ai_attack_action();
}

bool BattleSession::continue_ai_support_plan() {
    if (!ai_support_plan_.has_value()) {
        error_ = "battle AI support continuation plan is absent";
        return false;
    }
    if (ai_support_plan_->next_step == BattleAiSupportNextStep::apply_support) {
        return begin_ai_support_execution();
    }
    if (ai_support_plan_->next_step == BattleAiSupportNextStep::automatic_attack) {
        diagnostics::log_info(
            "battle AI support fallback attack id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " action=" +
            std::to_string(static_cast<std::int16_t>(ai_support_plan_->support_action)) +
            " target=" + std::to_string(ai_support_plan_->target_slot));
        ai_support_plan_.reset();
        return begin_ai_attack_action();
    }
    if (ai_support_plan_->next_step == BattleAiSupportNextStep::rest) {
        diagnostics::log_info(
            "battle AI support fallback rest id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " action=" +
            std::to_string(static_cast<std::int16_t>(ai_support_plan_->support_action)) +
            " target=" + std::to_string(ai_support_plan_->target_slot));
        ai_support_plan_.reset();
        return finish_ai_handler(BattlePlayerAction::rest, true);
    }
    error_ = "battle AI support continuation remained in movement state";
    return false;
}

bool BattleSession::begin_ai_support_execution() {
    if (!ai_support_plan_.has_value() ||
        ai_support_plan_->next_step != BattleAiSupportNextStep::apply_support ||
        ai_support_plan_->target_slot < 0 ||
        ai_support_plan_->target_slot >= setup_.combatant_count()) {
        error_ = "battle AI support execution plan is invalid";
        return false;
    }
    const auto medicine =
        ai_support_plan_->support_action == BattleAiAction::medicine;
    if (!medicine && ai_support_plan_->support_action != BattleAiAction::detox) {
        error_ = "battle AI support action is invalid";
        return false;
    }
    const auto target_slot = static_cast<std::size_t>(ai_support_plan_->target_slot);
    const auto& target_words = setup_.combatants()[target_slot].words;
    const BattlePathCoord target{
        target_words[combatant_word::x], target_words[combatant_word::y]};
    const auto result = medicine
        ? setup_.apply_medicine_target(current_actor_slot_, target, random_)
        : setup_.apply_detox_target(current_actor_slot_, target, random_);
    if (!result.has_value()) {
        error_ = setup_.valid() ? "battle AI support state application failed" : setup_.error();
        return false;
    }

    const auto action = medicine ? BattlePlayerAction::medicine
                                 : BattlePlayerAction::detoxification;
    const std::int16_t effect_id = medicine ? 0 : 36;
    const std::int16_t damage_kind = medicine ? 4 : 3;
    selected_magic_slot_ = 0;
    auto animation = setup_.magic_animation_plan(
        current_actor_slot_, selected_magic_slot_, 0, effect_id, kBattleFightPointerBase);
    if (!animation.has_value()) {
        error_ = "battle AI support magic animation is invalid";
        return false;
    }
    if (!renderer_.load_fight_package(animation->fight_head_id)) {
        error_ = "battle AI support FIGHT package load failed";
        return false;
    }
    player_target_effect_ = std::make_unique<PlayerTargetEffectState>(
        PlayerTargetEffectState{
            .action = action,
            .ai_controlled = true,
            .magic_animation = std::move(*animation),
            .effect_animation = std::nullopt,
            .effect_id = effect_id,
            .damage_kind = damage_kind,
            .damage_suppress_flash = true,
            .audio_commands = {},
        });
    render_state_.path_limit = 0;
    render_state_.primary_cursor = target;
    render_state_.effect_id = kBattleEffectPointerBase;
    render_state_.effect_visible = false;
    render_state_.damage_kind = 0;
    render_state_.highlight_enabled = false;
    diagnostics::log_info(
        "battle AI support effect ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " action=" + std::to_string(static_cast<std::int16_t>(action)) +
        " target=" + std::to_string(ai_support_plan_->target_slot) +
        " target_coordinate=" + std::to_string(target.x) + "," +
        std::to_string(target.y) +
        " effect=" + std::to_string(effect_id) +
        " damage_kind=" + std::to_string(damage_kind) +
        " hits=" + std::to_string(result->hit_count) +
        " frames=" +
        std::to_string(player_target_effect_->magic_animation.frames.size()));
    return prepare_player_magic_frame();
}

bool BattleSession::continue_ai_item_plan() {
    if (!ai_item_plan_.has_value()) {
        error_ = "battle AI item continuation plan is absent";
        return false;
    }
    if (ai_item_plan_->next_step == BattleAiItemNextStep::use_item) {
        if (ai_item_plan_->use_mode == 0) {
            return begin_ai_item_execution();
        }
        if (ai_item_plan_->use_mode == 1) {
            return begin_ai_throwing_weapon_execution();
        }
        error_ = "battle AI item use mode is invalid";
        return false;
    }
    if (ai_item_plan_->next_step == BattleAiItemNextStep::attack_fallback) {
        diagnostics::log_info(
            "battle AI throwing-weapon fallback attack id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " target=" + std::to_string(ai_item_plan_->target_slot) +
            " range_checks=" + std::to_string(ai_item_plan_->range_check_count));
        ai_item_plan_.reset();
        return begin_ai_attack_action();
    }
    error_ = "battle AI item continuation remained in movement state";
    return false;
}

bool BattleSession::begin_ai_item_execution() {
    if (!ai_item_plan_.has_value() ||
        ai_item_plan_->next_step != BattleAiItemNextStep::use_item ||
        ai_item_plan_->use_mode != 0 || ai_item_plan_->target_slot < 0 ||
        ai_item_plan_->target_slot >= setup_.combatant_count()) {
        error_ = "battle AI item execution plan is invalid";
        return false;
    }
    const BattleAiChoice choice{
        .action = BattleAiAction::item,
        .target_slot = ai_item_plan_->target_slot,
        .item_source = ai_item_plan_->item_source,
        .item_slot = ai_item_plan_->item_slot,
        .action_code_written = true,
    };
    auto effect = setup_.apply_ai_item_effect(
        current_actor_slot_, choice, random_, false);
    if (!effect.has_value()) {
        error_ = setup_.valid() ? "battle AI item state application failed" : setup_.error();
        return false;
    }
    player_item_ = std::make_unique<PlayerItemState>(PlayerItemState{
        .selection = {},
        .selected_inventory_slot = std::nullopt,
        .selected_item_id = ai_item_plan_->item_id,
        .effect_result = std::move(*effect),
        .inventory_consumed = false,
    });
    render_state_.path_limit = 0;
    const auto& target = setup_.combatants()[
        static_cast<std::size_t>(ai_item_plan_->target_slot)].words;
    render_state_.primary_cursor = BattlePathCoord{
        target[combatant_word::x], target[combatant_word::y]};
    diagnostics::log_info(
        "battle AI item effect ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " target=" + std::to_string(ai_item_plan_->target_slot) +
        " item=" + std::to_string(ai_item_plan_->item_id) +
        " source=" +
        std::to_string(static_cast<std::int16_t>(ai_item_plan_->item_source)) +
        " item_slot=" + std::to_string(ai_item_plan_->item_slot) +
        " has_effect=" +
        (player_item_->effect_result->has_effect ? std::string{"true"}
                                                : std::string{"false"}) +
        " effects=" +
        std::to_string(player_item_->effect_result->effect_count) +
        " consumed=false");
    if (player_item_->effect_result->has_effect) {
        phase_ = BattleSessionPhase::ai_item_effect_present;
        return true;
    }
    ai_item_wait_tick_ = ai_wait_tick_;
    return begin_ai_item_post_effect_wait();
}

bool BattleSession::begin_ai_throwing_weapon_execution() {
    if (!ai_item_plan_.has_value() ||
        ai_item_plan_->next_step != BattleAiItemNextStep::use_item ||
        ai_item_plan_->use_mode != 1 || ai_item_plan_->target_slot < 0 ||
        ai_item_plan_->target_slot >= setup_.combatant_count()) {
        error_ = "battle AI throwing-weapon execution plan is invalid";
        return false;
    }
    const auto target_slot = static_cast<std::size_t>(ai_item_plan_->target_slot);
    const auto& target_words = setup_.combatants()[target_slot].words;
    const BattlePathCoord target{
        target_words[combatant_word::x], target_words[combatant_word::y]};
    const BattleAiChoice choice{
        .action = BattleAiAction::throwing_weapon,
        .target_slot = ai_item_plan_->target_slot,
        .target = target,
        .item_source = ai_item_plan_->item_source,
        .item_slot = ai_item_plan_->item_slot,
        .action_code_written = true,
    };
    const auto thrown = setup_.apply_ai_throwing_weapon_target(
        current_actor_slot_, target, choice, random_, false);
    if (!thrown.has_value() || thrown->hit_count != 1 ||
        !thrown->effect_id.has_value()) {
        error_ = setup_.valid()
            ? "battle AI throwing-weapon state application failed"
            : setup_.error();
        return false;
    }
    auto animation = BattleSetup::effect_animation_plan(*thrown->effect_id);
    if (!animation.has_value()) {
        error_ = "battle AI throwing-weapon effect animation is invalid";
        return false;
    }
    player_target_effect_ = std::make_unique<PlayerTargetEffectState>(
        PlayerTargetEffectState{
            .action = BattlePlayerAction::item,
            .ai_controlled = true,
            .magic_animation = {},
            .effect_animation = std::move(*animation),
            .effect_id = *thrown->effect_id,
            .damage_kind = static_cast<std::int16_t>(thrown->damage == 0 ? 0 : 1),
            .damage_suppress_flash = false,
            .audio_commands = {},
        });
    if (player_target_effect_->effect_animation->dispatch_magic_before_prelude) {
        player_target_effect_->audio_commands.push_back(BattleAudioCommand{
            BattleAudioBank::attack,
            player_target_effect_->effect_animation->magic_sample_id});
    }
    render_state_.path_limit = 0;
    render_state_.primary_cursor = target;
    render_state_.effect_id = kBattleEffectPointerBase;
    render_state_.effect_visible = false;
    render_state_.damage_kind = 0;
    render_state_.highlight_enabled = false;
    phase_ = BattleSessionPhase::ai_effect_prelude_present;
    diagnostics::log_info(
        "battle AI throwing-weapon effect ready id=" +
        std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " target=" + std::to_string(ai_item_plan_->target_slot) +
        " target_coordinate=" + std::to_string(target.x) + "," +
        std::to_string(target.y) +
        " item=" + std::to_string(ai_item_plan_->item_id) +
        " source=" +
        std::to_string(static_cast<std::int16_t>(ai_item_plan_->item_source)) +
        " item_slot=" + std::to_string(ai_item_plan_->item_slot) +
        " effect=" + std::to_string(*thrown->effect_id) +
        " damage=" + std::to_string(thrown->damage) +
        " frames=" + std::to_string(
            player_target_effect_->effect_animation->frames.size()) +
        " consumed=false");
    return true;
}

bool BattleSession::begin_ai_item_post_effect_wait() {
    if (!player_item_ || !player_item_->effect_result.has_value() ||
        !ai_item_plan_.has_value() ||
        ai_item_plan_->next_step != BattleAiItemNextStep::use_item ||
        ai_item_plan_->use_mode != 0) {
        error_ = "battle AI item post-effect continuation is absent";
        return false;
    }
    ai_item_wait_tick_changes_remaining_ =
        player_item_->effect_result->post_effect_tick_changes;
    phase_ = BattleSessionPhase::ai_item_post_effect_wait;
    if (ai_item_wait_tick_changes_remaining_ <= 0) {
        return finish_ai_item_action();
    }
    return true;
}

bool BattleSession::advance_ai_item_post_effect_wait(
    const std::uint32_t bios_tick) {
    if (bios_tick == ai_item_wait_tick_) {
        return true;
    }
    ai_item_wait_tick_ = bios_tick;
    if (ai_item_wait_tick_changes_remaining_ > 0) {
        --ai_item_wait_tick_changes_remaining_;
    }
    if (ai_item_wait_tick_changes_remaining_ > 0) {
        return true;
    }
    return finish_ai_item_action();
}

bool BattleSession::finish_ai_item_action() {
    if (!ai_item_plan_.has_value()) {
        error_ = "battle AI item completion plan is absent";
        return false;
    }
    const auto action = ai_item_plan_->use_mode == 1
        ? BattleAiAction::throwing_weapon
        : BattleAiAction::item;
    const BattleAiChoice choice{
        .action = action,
        .target_slot = ai_item_plan_->target_slot,
        .item_source = ai_item_plan_->item_source,
        .item_slot = ai_item_plan_->item_slot,
        .action_code_written = true,
    };
    if (!setup_.consume_ai_item(current_actor_slot_, choice)) {
        error_ = "battle AI item inventory completion failed";
        return false;
    }
    if (player_item_ && player_item_->effect_result.has_value()) {
        player_item_->inventory_consumed = true;
        player_item_->effect_result->item_consumed = true;
    }
    diagnostics::log_info(
        "battle AI item consumed id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " item=" + std::to_string(ai_item_plan_->item_id) +
        " use_mode=" + std::to_string(ai_item_plan_->use_mode) +
        " source=" +
        std::to_string(static_cast<std::int16_t>(ai_item_plan_->item_source)) +
        " item_slot=" + std::to_string(ai_item_plan_->item_slot));
    player_item_.reset();
    ai_item_plan_.reset();
    ai_item_wait_tick_changes_remaining_ = 0;
    return finish_ai_handler(BattlePlayerAction::item, false);
}

bool BattleSession::begin_ai_attack_execution() {
    if (!ai_attack_plan_.has_value() ||
        ai_attack_plan_->next_step != BattleAiAttackNextStep::attack ||
        ai_attack_plan_->target_slot < 0 ||
        ai_attack_plan_->target_slot >= setup_.combatant_count()) {
        error_ = "battle AI attack execution plan is invalid";
        return false;
    }
    const auto profile = setup_.attack_profile(
        current_actor_slot_, ai_attack_plan_->magic_slot);
    if (!profile.has_value() || profile->magic_id != ai_attack_plan_->magic_id ||
        profile->select_distance != ai_attack_plan_->select_distance ||
        profile->area_type != ai_attack_plan_->area_type) {
        error_ = "battle AI attack profile no longer matches its plan";
        return false;
    }

    const auto& actor_words = setup_.combatants()[current_actor_slot_].words;
    const auto& target_words = setup_.combatants()[
        static_cast<std::size_t>(ai_attack_plan_->target_slot)].words;
    const BattlePathCoord target{
        target_words[combatant_word::x], target_words[combatant_word::y]};
    const auto delta_x = static_cast<std::int32_t>(target.x) -
        actor_words[combatant_word::x];
    const auto delta_y = static_cast<std::int32_t>(target.y) -
        actor_words[combatant_word::y];
    std::int16_t direction = -1;
    if (profile->area_type == 0 || profile->area_type == 1 ||
        profile->area_type == 3) {
        if (std::abs(delta_x) < std::abs(delta_y)) {
            direction = delta_y <= 0 ? 0 : 3;
        } else {
            direction = delta_x <= 0 ? 2 : 1;
        }
        setup_.combatants()[current_actor_slot_].words[combatant_word::initial_mode] =
            direction;
    }

    selected_magic_slot_ = ai_attack_plan_->magic_slot;
    player_attack_ = std::make_unique<PlayerAttackState>(PlayerAttackState{
        .profile = *profile,
        .special_attack_bonus = ai_attack_plan_->special_attack_bonus,
        .iteration = 0,
        .ai_controlled = true,
        .target = profile->area_type == 0 || profile->area_type == 3
            ? std::optional<BattlePathCoord>{target}
            : std::nullopt,
        .direction = direction,
        .level_text = {},
    });
    setup_.clear_attack_effects();
    render_state_.path_limit = 0;
    render_state_.primary_cursor = target;
    diagnostics::log_info(
        "battle AI attack ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " magic_slot=" + std::to_string(selected_magic_slot_) +
        " target=" + std::to_string(ai_attack_plan_->target_slot) +
        " target_coordinate=" + std::to_string(target.x) + "," +
        std::to_string(target.y) +
        " area_type=" + std::to_string(profile->area_type) +
        " direction=" + std::to_string(direction) +
        " attack_count=" + std::to_string(profile->attack_count));
    return begin_player_attack_iteration(player_attack_->target);
}

bool BattleSession::begin_ai_movement_to(
    const std::int16_t target_slot,
    const BattlePathCoord target,
    const std::int16_t mode,
    const std::int16_t range,
    const AiMovementContinuation continuation) {
    auto movement = setup_.begin_ai_movement_plan(
        current_actor_slot_, target_slot, target, mode, range);
    if (!movement.has_value()) {
        error_ = setup_.valid() ? "battle AI movement plan failed" : setup_.error();
        return false;
    }
    return begin_ai_movement(std::move(*movement), continuation);
}

bool BattleSession::begin_ai_movement(
    BattleAiMovementPlan plan,
    const AiMovementContinuation continuation) {
    ai_movement_plan_ = std::make_unique<BattleAiMovementPlan>(std::move(plan));
    ai_movement_continuation_ = continuation;
    render_state_.path_limit = 0;
    diagnostics::log_info(
        "battle AI movement ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " mode=" + std::to_string(ai_movement_plan_->mode) +
        " source=" + std::to_string(ai_movement_plan_->source.x) + "," +
        std::to_string(ai_movement_plan_->source.y) +
        " destination=" + std::to_string(ai_movement_plan_->destination.x) + "," +
        std::to_string(ai_movement_plan_->destination.y) +
        " continuation=" + std::to_string(static_cast<int>(continuation)));
    if (ai_movement_plan_->complete) {
        return finish_ai_movement();
    }
    return advance_ai_movement_step();
}

bool BattleSession::advance_ai_movement_step() {
    if (!ai_movement_plan_) {
        error_ = "battle AI movement plan is absent";
        return false;
    }
    const auto step = setup_.advance_ai_movement(*ai_movement_plan_);
    if (!step.has_value()) {
        if (ai_movement_plan_->complete) {
            return finish_ai_movement();
        }
        error_ = setup_.valid() ? "battle AI movement step failed" : setup_.error();
        return false;
    }
    render_state_.view_x = step->view_x;
    render_state_.view_y = step->view_y;
    render_state_.path_limit = 0;
    render_state_.primary_cursor = step->to;
    phase_ = BattleSessionPhase::ai_movement_step_present;
    diagnostics::log_info(
        "battle AI movement step id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " mode=" + std::to_string(ai_movement_plan_->mode) +
        " from=" + std::to_string(step->from.x) + "," + std::to_string(step->from.y) +
        " to=" + std::to_string(step->to.x) + "," + std::to_string(step->to.y) +
        " round_value=" + std::to_string(step->remaining_round_value) +
        " physical_power=" + std::to_string(step->physical_power) +
        " complete=" + (step->complete ? std::string{"true"} : std::string{"false"}));
    return true;
}

bool BattleSession::advance_ai_movement_wait(const std::uint32_t bios_tick) {
    if (bios_tick == ai_movement_wait_tick_) {
        return true;
    }
    ai_movement_wait_tick_ = bios_tick;
    if (ai_movement_wait_tick_changes_remaining_ > 0) {
        --ai_movement_wait_tick_changes_remaining_;
    }
    if (ai_movement_wait_tick_changes_remaining_ > 0) {
        return true;
    }
    if (ai_movement_plan_ && !ai_movement_plan_->complete) {
        return advance_ai_movement_step();
    }
    return finish_ai_movement();
}

bool BattleSession::finish_ai_movement() {
    if (!ai_movement_continuation_.has_value()) {
        error_ = "battle AI movement continuation is absent";
        return false;
    }
    const auto continuation = *ai_movement_continuation_;
    ai_movement_plan_.reset();
    ai_movement_continuation_.reset();
    switch (continuation) {
    case AiMovementContinuation::direct:
        return finish_ai_handler(BattlePlayerAction::movement, false);
    case AiMovementContinuation::escape:
        return finish_ai_handler(BattlePlayerAction::rest, true);
    case AiMovementContinuation::attack:
        if (!ai_attack_plan_.has_value()) {
            error_ = "battle AI attack continuation plan is absent";
            return false;
        }
        ai_attack_plan_ = setup_.resume_ai_attack_after_move(
            current_actor_slot_, *ai_attack_plan_);
        if (!ai_attack_plan_.has_value()) {
            error_ = setup_.valid() ? "battle AI attack continuation failed" : setup_.error();
            return false;
        }
        if (ai_attack_plan_->next_step == BattleAiAttackNextStep::rest) {
            return finish_ai_handler(BattlePlayerAction::rest, true);
        }
        if (ai_attack_plan_->next_step == BattleAiAttackNextStep::finish) {
            return finish_ai_handler(BattlePlayerAction::attack, false);
        }
        diagnostics::log_info(
            "battle AI movement continuation ready id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " continuation=" + std::to_string(static_cast<int>(continuation)));
        return begin_ai_attack_execution();
    case AiMovementContinuation::poison:
        if (!ai_poison_plan_.has_value()) {
            error_ = "battle AI poison continuation plan is absent";
            return false;
        }
        ai_poison_plan_ = setup_.resume_ai_poison_after_move(
            current_actor_slot_, *ai_poison_plan_);
        if (!ai_poison_plan_.has_value()) {
            error_ = setup_.valid() ? "battle AI poison continuation failed" : setup_.error();
            return false;
        }
        diagnostics::log_info(
            "battle AI movement continuation ready id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " continuation=" + std::to_string(static_cast<int>(continuation)));
        return continue_ai_poison_plan();
    case AiMovementContinuation::item:
        if (!ai_item_plan_.has_value()) {
            error_ = "battle AI item continuation plan is absent";
            return false;
        }
        ai_item_plan_ = setup_.resume_ai_item_after_relocation(
            current_actor_slot_, *ai_item_plan_);
        if (!ai_item_plan_.has_value()) {
            error_ = "battle AI item continuation failed";
            return false;
        }
        diagnostics::log_info(
            "battle AI movement continuation ready id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " continuation=" + std::to_string(static_cast<int>(continuation)));
        return continue_ai_item_plan();
    case AiMovementContinuation::request:
        if (!ai_request_plan_.has_value()) {
            error_ = "battle AI request continuation plan is absent";
            return false;
        }
        ai_request_plan_ = setup_.resume_ai_request_after_move(
            current_actor_slot_, *ai_request_plan_);
        if (!ai_request_plan_.has_value()) {
            error_ = "battle AI request continuation failed";
            return false;
        }
        diagnostics::log_info(
            "battle AI movement continuation ready id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " continuation=" + std::to_string(static_cast<int>(continuation)));
        return continue_ai_request_plan();
    case AiMovementContinuation::support:
        if (!ai_support_plan_.has_value()) {
            error_ = "battle AI support continuation plan is absent";
            return false;
        }
        ai_support_plan_ = setup_.resume_ai_support_after_move(
            current_actor_slot_, *ai_support_plan_);
        if (!ai_support_plan_.has_value()) {
            error_ = setup_.valid() ? "battle AI support continuation failed" : setup_.error();
            return false;
        }
        diagnostics::log_info(
            "battle AI movement continuation ready id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " continuation=" + std::to_string(static_cast<int>(continuation)));
        return continue_ai_support_plan();
    case AiMovementContinuation::throwing_weapon:
        if (!ai_item_plan_.has_value()) {
            error_ = "battle AI throwing-weapon continuation plan is absent";
            return false;
        }
        ai_item_plan_ = setup_.resume_ai_throwing_weapon_after_move(
            current_actor_slot_, *ai_item_plan_);
        if (!ai_item_plan_.has_value()) {
            error_ = setup_.valid()
                ? "battle AI throwing-weapon continuation failed"
                : setup_.error();
            return false;
        }
        diagnostics::log_info(
            "battle AI movement continuation ready id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " continuation=" + std::to_string(static_cast<int>(continuation)));
        return continue_ai_item_plan();
    }
    error_ = "battle AI movement continuation is invalid";
    return false;
}

bool BattleSession::finish_ai_handler(
    const BattlePlayerAction action,
    const bool rest_first) {
    if (rest_first && !setup_.rest_actor(current_actor_slot_, random_).has_value()) {
        error_ = setup_.error();
        return false;
    }
    if (!setup_.finish_ai_turn(current_actor_slot_)) {
        error_ = "battle AI turn completion failed";
        return false;
    }
    return finish_current_actor(action);
}

bool BattleSession::begin_player_action_menu() {
    const auto availability = setup_.player_action_availability(current_actor_slot_);
    if (!availability.has_value() || availability->available_count <= 0) {
        error_ = "battle player action availability is invalid";
        return false;
    }
    player_action_menu_.available = availability->available;
    player_action_menu_.available_count =
        static_cast<std::size_t>(availability->available_count);
    player_action_menu_.cursor = 0U;
    player_action_menu_.selected_action = -1;
    phase_ = BattleSessionPhase::player_action;
    diagnostics::log_info(
        "battle player action menu ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " available=" + std::to_string(player_action_menu_.available_count));
    return true;
}

bool BattleSession::begin_player_attack() {
    const auto learned_count = setup_.learned_magic_count(current_actor_slot_);
    if (learned_count == 1U) {
        selected_magic_slot_ = 0;
        diagnostics::log_info(
            "battle player single magic selected id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " magic_slot=0");
        return begin_player_attack_execution();
    }

    player_magic_selection_ = setup_.begin_magic_selection(current_actor_slot_);
    if (!player_magic_selection_.has_value()) {
        error_ = "battle player magic selection is invalid";
        return false;
    }
    phase_ = BattleSessionPhase::player_magic_selection;
    diagnostics::log_info(
        "battle player magic selection ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " learned=" + std::to_string(player_magic_selection_->learned_count) +
        " available=" + std::to_string(player_magic_selection_->available_count));
    return true;
}

BattleSessionInputResult BattleSession::handle_player_magic_selection_key(
    const std::uint8_t translated_key) {
    if (!player_magic_selection_.has_value()) {
        return BattleSessionInputResult::ignored;
    }
    std::optional<BattleMagicSelectionAction> action;
    if (translated_key == kRight) {
        action = BattleMagicSelectionAction::next;
    } else if (translated_key == kLeft) {
        action = BattleMagicSelectionAction::previous;
    } else if (confirms(translated_key)) {
        action = BattleMagicSelectionAction::activate;
    } else if (translated_key == kEscape) {
        action = BattleMagicSelectionAction::cancel;
    } else {
        return BattleSessionInputResult::ignored;
    }

    const auto result = BattleSetup::apply_magic_selection(
        *player_magic_selection_, *action);
    if (result == BattleMagicSelectionResult::changed) {
        diagnostics::log_debug(
            "battle player magic cursor id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " cursor=" + std::to_string(player_magic_selection_->cursor));
        return BattleSessionInputResult::magic_changed;
    }
    if (result == BattleMagicSelectionResult::cancelled) {
        player_magic_selection_.reset();
        player_action_menu_.selected_action = -1;
        phase_ = BattleSessionPhase::player_action;
        diagnostics::log_info(
            "battle player magic selection cancelled id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_));
        return BattleSessionInputResult::magic_cancelled;
    }
    if (result != BattleMagicSelectionResult::selected ||
        !player_magic_selection_->selected_slot.has_value()) {
        return BattleSessionInputResult::ignored;
    }

    selected_magic_slot_ = *player_magic_selection_->selected_slot;
    diagnostics::log_info(
        "battle player magic selected id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " cursor=" + std::to_string(player_magic_selection_->cursor) +
        " magic_slot=" + std::to_string(selected_magic_slot_));
    player_magic_selection_.reset();
    if (!begin_player_attack_execution()) {
        return BattleSessionInputResult::ignored;
    }
    return BattleSessionInputResult::magic_selected;
}

bool BattleSession::begin_player_attack_execution() {
    const auto profile = setup_.attack_profile(current_actor_slot_, selected_magic_slot_);
    const auto special_attack_bonus =
        setup_.attack_special_bonus(current_actor_slot_, selected_magic_slot_);
    if (!profile.has_value() || !special_attack_bonus.has_value()) {
        error_ = "battle player attack profile is invalid";
        return false;
    }
    player_attack_ = std::make_unique<PlayerAttackState>(PlayerAttackState{
        .profile = *profile,
        .special_attack_bonus = *special_attack_bonus,
        .iteration = 0,
        .ai_controlled = false,
        .target = std::nullopt,
        .direction = -1,
        .level_text = {},
    });
    setup_.clear_attack_effects();
    selected_player_target_.reset();
    render_state_.path_limit = 0;
    diagnostics::log_info(
        "battle player attack ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " magic_slot=" + std::to_string(selected_magic_slot_) +
        " area_type=" + std::to_string(profile->area_type) +
        " attack_count=" + std::to_string(profile->attack_count) +
        " special_bonus=" + std::to_string(*special_attack_bonus));
    if (profile->area_type == 0 || profile->area_type == 3) {
        return begin_player_targeting(BattlePlayerAction::attack);
    }
    if (profile->area_type == 1) {
        phase_ = BattleSessionPhase::player_attack_direction;
        return true;
    }
    return begin_player_attack_iteration();
}

BattleSessionInputResult BattleSession::handle_player_attack_direction_key(
    const std::uint8_t translated_key) {
    if (!player_attack_) {
        return BattleSessionInputResult::ignored;
    }
    std::optional<std::int16_t> direction;
    if (translated_key == kDown) {
        direction = 3;
    } else if (translated_key == kRight) {
        direction = 1;
    } else if (translated_key == kLeft) {
        direction = 2;
    } else if (translated_key == kUp) {
        direction = 0;
    } else {
        return BattleSessionInputResult::ignored;
    }
    player_attack_->direction = *direction;
    setup_.combatants()[current_actor_slot_].words[combatant_word::initial_mode] =
        *direction;
    diagnostics::log_info(
        "battle player attack direction selected id=" +
        std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " direction=" + std::to_string(*direction));
    if (!begin_player_attack_iteration()) {
        return BattleSessionInputResult::ignored;
    }
    return BattleSessionInputResult::direction_selected;
}

bool BattleSession::begin_player_attack_iteration(
    const std::optional<BattlePathCoord> target) {
    if (!player_attack_ || player_attack_->iteration < 0 ||
        player_attack_->iteration >= player_attack_->profile.attack_count) {
        error_ = "battle player attack iteration is invalid";
        return false;
    }
    if (target.has_value()) {
        player_attack_->target = target;
    }

    std::optional<BattleAreaResult> result{BattleAreaResult{}};
    if (player_attack_->profile.area_type == 0 ||
        player_attack_->profile.area_type == 3) {
        if (!player_attack_->target.has_value()) {
            error_ = "battle player attack target is absent";
            return false;
        }
        result = setup_.apply_attack_area(
            current_actor_slot_,
            selected_magic_slot_,
            *player_attack_->target,
            player_attack_->special_attack_bonus,
            random_,
            &player_attack_->profile);
    } else if (player_attack_->profile.area_type == 1) {
        result = setup_.apply_line_attack_area(
            current_actor_slot_,
            selected_magic_slot_,
            player_attack_->direction,
            player_attack_->special_attack_bonus,
            random_,
            &player_attack_->profile);
    } else if (player_attack_->profile.area_type == 2) {
        result = setup_.apply_attack_area(
            current_actor_slot_,
            selected_magic_slot_,
            BattlePathCoord{},
            player_attack_->special_attack_bonus,
            random_,
            &player_attack_->profile);
    }
    if (!result.has_value()) {
        error_ = setup_.valid() ? "battle player attack area failed" : setup_.error();
        return false;
    }

    auto animation = setup_.magic_animation_plan(
        current_actor_slot_, selected_magic_slot_, kBattleFightPointerBase);
    if (!animation.has_value()) {
        error_ = "battle player attack magic animation is invalid";
        return false;
    }
    if (!renderer_.load_fight_package(animation->fight_head_id)) {
        error_ = "battle player attack FIGHT package load failed";
        return false;
    }
    const auto effect_id = animation->effect_sample_id;
    player_target_effect_ = std::make_unique<PlayerTargetEffectState>(
        PlayerTargetEffectState{
            .action = BattlePlayerAction::attack,
            .ai_controlled = player_attack_->ai_controlled,
            .magic_animation = std::move(*animation),
            .effect_animation = std::nullopt,
            .effect_id = effect_id,
            .damage_kind = static_cast<std::int16_t>(
                result->effect_kind == 3 ? 5 : 1),
            .damage_suppress_flash = false,
            .audio_commands = {},
        });
    player_cursor_selection_.reset();
    render_state_.path_limit = 0;
    render_state_.effect_id = kBattleEffectPointerBase;
    render_state_.effect_visible = false;
    render_state_.damage_kind = 0;
    render_state_.highlight_enabled = false;
    diagnostics::log_info(
        std::string{"battle "} +
        (player_attack_->ai_controlled ? "AI" : "player") +
        " attack iteration ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " iteration=" + std::to_string(player_attack_->iteration) +
        " area_type=" + std::to_string(player_attack_->profile.area_type) +
        " hits=" + std::to_string(result->hit_count) +
        " damage_kind=" + std::to_string(player_target_effect_->damage_kind) +
        " frames=" +
        std::to_string(player_target_effect_->magic_animation.frames.size()));
    if (player_target_effect_->magic_animation.frames.empty()) {
        return begin_player_damage_animation();
    }
    return prepare_player_magic_frame();
}

bool BattleSession::begin_player_movement() {
    auto selection = setup_.begin_player_movement_selection(current_actor_slot_);
    if (!selection.has_value()) {
        error_ = "battle player movement selection is invalid";
        return false;
    }
    player_cursor_selection_.emplace(std::move(*selection));
    player_movement_plan_.reset();
    render_state_.path_limit = player_cursor_selection_->path_limit;
    render_state_.primary_cursor = player_cursor_selection_->cursor;
    phase_ = BattleSessionPhase::player_movement_select;
    diagnostics::log_info(
        "battle player movement selection ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " source=" + std::to_string(player_cursor_selection_->source.x) + "," +
        std::to_string(player_cursor_selection_->source.y) +
        " path_limit=" + std::to_string(player_cursor_selection_->path_limit));
    return true;
}

bool BattleSession::begin_player_item_selection() {
    player_item_ = std::make_unique<PlayerItemState>(PlayerItemState{
        .selection = setup_.begin_item_selection(),
        .page = 0,
        .row = 0,
        .column = 0,
        .selected_inventory_slot = std::nullopt,
        .selected_item_id = -1,
        .effect_result = std::nullopt,
        .inventory_consumed = false,
    });
    phase_ = BattleSessionPhase::player_item_selection;
    diagnostics::log_info(
        "battle player item selection ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " items=" + std::to_string(player_item_->selection.count));
    return true;
}

BattleSessionInputResult BattleSession::handle_player_item_key(
    const std::uint8_t translated_key) {
    if (!player_item_) {
        return BattleSessionInputResult::ignored;
    }
    auto& item = *player_item_;
    if (translated_key == kRight) {
        item.column = item.column == 4 ? 0 : static_cast<std::int16_t>(item.column + 1);
    } else if (translated_key == kLeft) {
        item.column = item.column == 0 ? 4 : static_cast<std::int16_t>(item.column - 1);
    } else if (translated_key == kDown) {
        if (item.row < 2) {
            ++item.row;
        } else if (item.page < 37) {
            ++item.page;
        }
    } else if (translated_key == kUp) {
        if (item.row > 0) {
            --item.row;
        } else if (item.page > 0) {
            --item.page;
        }
    } else if (translated_key == kPageDown) {
        if (item.page < 35) {
            item.page = static_cast<std::int16_t>(item.page + 3);
        }
    } else if (translated_key == kPageUp) {
        if (item.page > 2) {
            item.page = static_cast<std::int16_t>(item.page - 3);
        }
    } else if (translated_key == kEscape) {
        diagnostics::log_info(
            "battle player item selection cancelled id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_));
        player_item_.reset();
        player_action_menu_.selected_action = -1;
        phase_ = BattleSessionPhase::player_action;
        return BattleSessionInputResult::item_cancelled;
    } else if (!confirms(translated_key)) {
        return BattleSessionInputResult::ignored;
    } else {
        const auto list_index = static_cast<std::int32_t>(
            5 * (item.page + item.row) + item.column);
        if (list_index < 0 || list_index >= static_cast<std::int32_t>(model::kInventoryCount)) {
            return BattleSessionInputResult::ignored;
        }
        const auto inventory_slot = item.selection.inventory_slots[
            static_cast<std::size_t>(list_index)];
        if (inventory_slot < 0 ||
            static_cast<std::size_t>(inventory_slot) >= model::kInventoryCount) {
            return BattleSessionInputResult::ignored;
        }
        const auto item_id = ranger_.header.inventory_item(
            static_cast<std::size_t>(inventory_slot)).value;
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            return BattleSessionInputResult::ignored;
        }
        const auto& record = ranger_.items[static_cast<std::size_t>(item_id)];
        if (record.word(model::item_word::show_introduction) != 1) {
            return BattleSessionInputResult::ignored;
        }
        item.selected_inventory_slot = static_cast<std::size_t>(inventory_slot);
        item.selected_item_id = item_id;
        const auto item_type = record.word(model::item_word::item_type);
        diagnostics::log_info(
            "battle player item selected id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " inventory_slot=" + std::to_string(inventory_slot) +
            " item=" + std::to_string(item_id) +
            " type=" + std::to_string(item_type));
        if (item_type == 3) {
            auto effect = setup_.apply_player_item_effect(
                current_actor_slot_, static_cast<std::size_t>(inventory_slot), random_);
            if (!effect.has_value()) {
                error_ = setup_.valid()
                    ? "battle player item state application failed"
                    : setup_.error();
                return BattleSessionInputResult::ignored;
            }
            if (!effect->has_effect) {
                diagnostics::log_info(
                    "battle player item had no visible effect id=" +
                    std::to_string(battle_id()) +
                    " slot=" + std::to_string(current_actor_slot_) +
                    " item=" + std::to_string(item_id) +
                    " consumed=false action_complete=true");
                if (!setup_.finish_player_item_action(current_actor_slot_)) {
                    error_ = "battle player zero-effect item completion failed";
                    return BattleSessionInputResult::ignored;
                }
                player_item_.reset();
                player_action_menu_.selected_action = -1;
                if (!finish_current_actor(BattlePlayerAction::item)) {
                    return BattleSessionInputResult::ignored;
                }
                return BattleSessionInputResult::item_selected;
            }
            item.effect_result = std::move(*effect);
            phase_ = BattleSessionPhase::player_item_effect_present;
            return BattleSessionInputResult::item_selected;
        }
        if (item_type == 4 && begin_player_targeting(BattlePlayerAction::item)) {
            return BattleSessionInputResult::item_selected;
        }
        error_ = "battle player item type is outside filtered records";
        return BattleSessionInputResult::ignored;
    }

    diagnostics::log_debug(
        "battle player item cursor id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " page=" + std::to_string(item.page) +
        " row=" + std::to_string(item.row) +
        " column=" + std::to_string(item.column));
    return BattleSessionInputResult::item_changed;
}

bool BattleSession::begin_player_status_selection() {
    std::size_t party_count = model::kTeamMemberCount;
    for (std::size_t slot = 1U; slot < model::kTeamMemberCount; ++slot) {
        if (ranger_.header.team_member(slot).value <= 0) {
            party_count = slot;
            break;
        }
    }
    player_status_ = PlayerStatusState{
        .party_count = party_count,
        .cursor = 0U,
        .role_id = -1,
        .page = 0U,
    };
    phase_ = BattleSessionPhase::player_status_selection;
    diagnostics::log_info(
        "battle player status selection ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " party=" + std::to_string(party_count));
    return true;
}

BattleSessionInputResult BattleSession::handle_player_status_selection_key(
    const std::uint8_t translated_key) {
    if (!player_status_.has_value() || player_status_->party_count == 0U) {
        return BattleSessionInputResult::ignored;
    }
    auto& status = *player_status_;
    if (translated_key == kDown) {
        status.cursor = status.cursor + 1U == status.party_count ? 0U : status.cursor + 1U;
    } else if (translated_key == kUp) {
        status.cursor = status.cursor == 0U ? status.party_count - 1U : status.cursor - 1U;
    } else if (translated_key == kEscape) {
        diagnostics::log_info(
            "battle player status selection cancelled id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_));
        player_status_.reset();
        player_action_menu_.selected_action = -1;
        phase_ = BattleSessionPhase::player_action;
        return BattleSessionInputResult::status_cancelled;
    } else if (!confirms(translated_key)) {
        return BattleSessionInputResult::ignored;
    } else {
        const auto role_id = ranger_.header.team_member(status.cursor).value;
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
            error_ = "battle player status role id is outside RANGER records";
            return BattleSessionInputResult::ignored;
        }
        status.role_id = role_id;
        status.page = 0U;
        phase_ = BattleSessionPhase::player_status_page_present;
        diagnostics::log_info(
            "battle player status selected id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " party_slot=" + std::to_string(status.cursor) +
            " role=" + std::to_string(role_id));
        return BattleSessionInputResult::status_selected;
    }
    diagnostics::log_debug(
        "battle player status cursor id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " cursor=" + std::to_string(status.cursor));
    return BattleSessionInputResult::status_changed;
}

BattleSessionInputResult BattleSession::handle_player_status_page_key(
    const std::uint8_t translated_key) {
    if (!player_status_.has_value() || translated_key == 0U) {
        return BattleSessionInputResult::ignored;
    }
    if (player_status_->page == 0U) {
        player_status_->page = 1U;
        phase_ = BattleSessionPhase::player_status_page_present;
        diagnostics::log_info(
            "battle player status page advanced id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " role=" + std::to_string(player_status_->role_id));
        return BattleSessionInputResult::status_page_advanced;
    }
    diagnostics::log_info(
        "battle player status closed id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " role=" + std::to_string(player_status_->role_id));
    player_status_.reset();
    player_action_menu_.selected_action = -1;
    phase_ = BattleSessionPhase::player_action;
    return BattleSessionInputResult::status_closed;
}

bool BattleSession::begin_player_targeting(const BattlePlayerAction action) {
    std::optional<std::int16_t> path_limit;
    if (action == BattlePlayerAction::attack && player_attack_) {
        path_limit = player_attack_->profile.select_distance;
    } else if (action == BattlePlayerAction::use_poison) {
        path_limit = setup_.poison_targeting_range(current_actor_slot_);
    } else if (action == BattlePlayerAction::detoxification) {
        path_limit = setup_.detox_targeting_range(current_actor_slot_);
    } else if (action == BattlePlayerAction::medicine) {
        path_limit = setup_.medicine_targeting_range(current_actor_slot_);
    } else if (action == BattlePlayerAction::item && player_item_ &&
               player_item_->selected_inventory_slot.has_value()) {
        path_limit = setup_.throwing_weapon_targeting_range(current_actor_slot_);
    }
    if (!path_limit.has_value()) {
        error_ = "battle player targeting range is invalid";
        return false;
    }
    auto selection = setup_.begin_cursor_selection(
        current_actor_slot_, *path_limit, BattleCursorSelectionMode::targeting);
    if (!selection.has_value()) {
        error_ = "battle player targeting selection is invalid";
        return false;
    }
    player_cursor_selection_.emplace(std::move(*selection));
    selected_player_target_.reset();
    render_state_.path_limit = player_cursor_selection_->path_limit;
    render_state_.primary_cursor = player_cursor_selection_->cursor;
    phase_ = BattleSessionPhase::player_targeting_select;
    diagnostics::log_info(
        "battle player targeting ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " action=" + std::to_string(static_cast<std::int16_t>(action)) +
        " source=" + std::to_string(player_cursor_selection_->source.x) + "," +
        std::to_string(player_cursor_selection_->source.y) +
        " path_limit=" + std::to_string(player_cursor_selection_->path_limit));
    return true;
}

BattleSessionInputResult BattleSession::handle_player_movement_key(
    const std::uint8_t translated_key) {
    if (!player_cursor_selection_.has_value()) {
        return BattleSessionInputResult::ignored;
    }
    std::optional<BattleCursorSelectionAction> action;
    if (translated_key == kDown) {
        action = BattleCursorSelectionAction::down;
    } else if (translated_key == kRight) {
        action = BattleCursorSelectionAction::right;
    } else if (translated_key == kLeft) {
        action = BattleCursorSelectionAction::left;
    } else if (translated_key == kUp) {
        action = BattleCursorSelectionAction::up;
    } else if (translated_key == kEscape) {
        action = BattleCursorSelectionAction::cancel;
    } else if (confirms(translated_key)) {
        action = BattleCursorSelectionAction::activate;
    } else {
        return BattleSessionInputResult::ignored;
    }

    const auto result = setup_.apply_cursor_selection(*player_cursor_selection_, *action);
    if (result == BattleCursorSelectionResult::moved) {
        render_state_.primary_cursor = player_cursor_selection_->cursor;
        diagnostics::log_debug(
            "battle player movement cursor id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " cursor=" + std::to_string(player_cursor_selection_->cursor.x) + "," +
            std::to_string(player_cursor_selection_->cursor.y));
        return BattleSessionInputResult::cursor_changed;
    }
    if (result == BattleCursorSelectionResult::cancelled) {
        render_state_.path_limit = 0;
        player_cursor_selection_.reset();
        if (!rebuild_player_menu_after_movement()) {
            return BattleSessionInputResult::ignored;
        }
        diagnostics::log_info(
            "battle player movement cancelled id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_));
        return BattleSessionInputResult::cursor_cancelled;
    }
    if (result != BattleCursorSelectionResult::selected) {
        return BattleSessionInputResult::ignored;
    }

    auto plan = setup_.finish_player_movement_selection(*player_cursor_selection_);
    if (!plan.has_value()) {
        error_ = "battle player movement path marking failed";
        return BattleSessionInputResult::ignored;
    }
    const auto destination = plan->destination;
    player_movement_plan_.emplace(std::move(*plan));
    render_state_.path_limit = 0;
    player_cursor_selection_.reset();
    diagnostics::log_info(
        "battle player movement selected id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " destination=" + std::to_string(destination.x) + "," +
        std::to_string(destination.y));
    if (player_movement_plan_->complete) {
        if (!rebuild_player_menu_after_movement()) {
            return BattleSessionInputResult::ignored;
        }
    } else if (!advance_player_movement_step()) {
        return BattleSessionInputResult::ignored;
    }
    return BattleSessionInputResult::cursor_selected;
}

BattleSessionInputResult BattleSession::handle_player_targeting_key(
    const std::uint8_t translated_key) {
    if (!player_cursor_selection_.has_value()) {
        return BattleSessionInputResult::ignored;
    }
    std::optional<BattleCursorSelectionAction> action;
    if (translated_key == kDown) {
        action = BattleCursorSelectionAction::down;
    } else if (translated_key == kRight) {
        action = BattleCursorSelectionAction::right;
    } else if (translated_key == kLeft) {
        action = BattleCursorSelectionAction::left;
    } else if (translated_key == kUp) {
        action = BattleCursorSelectionAction::up;
    } else if (translated_key == kEscape) {
        action = BattleCursorSelectionAction::cancel;
    } else if (confirms(translated_key)) {
        action = BattleCursorSelectionAction::activate;
    } else {
        return BattleSessionInputResult::ignored;
    }

    const auto result = setup_.apply_cursor_selection(*player_cursor_selection_, *action);
    if (result == BattleCursorSelectionResult::moved) {
        render_state_.primary_cursor = player_cursor_selection_->cursor;
        diagnostics::log_debug(
            "battle player targeting cursor id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " cursor=" + std::to_string(player_cursor_selection_->cursor.x) + "," +
            std::to_string(player_cursor_selection_->cursor.y));
        return BattleSessionInputResult::cursor_changed;
    }
    if (result == BattleCursorSelectionResult::cancelled) {
        render_state_.path_limit = 0;
        player_cursor_selection_.reset();
        selected_player_target_.reset();
        if (player_action_menu_.selected_action ==
            static_cast<std::int16_t>(BattlePlayerAction::attack)) {
            player_attack_.reset();
        }
        if (player_action_menu_.selected_action ==
            static_cast<std::int16_t>(BattlePlayerAction::item)) {
            player_item_.reset();
        }
        player_action_menu_.selected_action = -1;
        phase_ = BattleSessionPhase::player_action;
        diagnostics::log_info(
            "battle player targeting cancelled id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_));
        return BattleSessionInputResult::cursor_cancelled;
    }
    if (result != BattleCursorSelectionResult::selected) {
        return BattleSessionInputResult::ignored;
    }

    const auto target = player_cursor_selection_->cursor;
    selected_player_target_ = target;
    render_state_.primary_cursor = target;
    diagnostics::log_info(
        "battle player target selected id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " action=" + std::to_string(player_action_menu_.selected_action) +
        " target=" + std::to_string(target.x) + "," + std::to_string(target.y));
    const auto selected_action = static_cast<BattlePlayerAction>(
        player_action_menu_.selected_action);
    if (!begin_player_target_effect(selected_action, target)) {
        diagnostics::log_error(
            "battle player target effect failed id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " action=" + std::to_string(player_action_menu_.selected_action) +
            " reason=" + error_);
        return BattleSessionInputResult::ignored;
    }
    return BattleSessionInputResult::cursor_selected;
}

bool BattleSession::begin_player_target_effect(
    const BattlePlayerAction action,
    const BattlePathCoord target) {
    if (action == BattlePlayerAction::attack) {
        return begin_player_attack_iteration(target);
    }
    if (action == BattlePlayerAction::item) {
        if (!player_item_ || !player_item_->selected_inventory_slot.has_value()) {
            error_ = "battle player throwing-weapon selection is absent";
            return false;
        }
        const auto inventory_slot = *player_item_->selected_inventory_slot;
        const auto thrown = setup_.apply_throwing_weapon_target(
            current_actor_slot_, target, inventory_slot, random_);
        if (!thrown.has_value()) {
            error_ = setup_.valid()
                ? "battle player throwing-weapon state application failed"
                : setup_.error();
            return false;
        }
        player_cursor_selection_.reset();
        render_state_.path_limit = 0;
        if (thrown->hit_count == 0) {
            diagnostics::log_info(
                "battle player throwing-weapon target rejected id=" +
                std::to_string(battle_id()) +
                " slot=" + std::to_string(current_actor_slot_) +
                " target=" + std::to_string(target.x) + "," +
                std::to_string(target.y));
            selected_player_target_.reset();
            player_item_.reset();
            player_action_menu_.selected_action = -1;
            phase_ = BattleSessionPhase::player_action;
            return true;
        }
        if (!thrown->effect_id.has_value()) {
            error_ = "battle player throwing-weapon effect id is absent";
            return false;
        }
        auto animation = BattleSetup::effect_animation_plan(*thrown->effect_id);
        if (!animation.has_value()) {
            error_ = "battle player throwing-weapon effect animation is invalid";
            return false;
        }
        player_target_effect_ = std::make_unique<PlayerTargetEffectState>(
            PlayerTargetEffectState{
                .action = BattlePlayerAction::item,
                .magic_animation = {},
                .effect_animation = std::move(*animation),
                .effect_id = *thrown->effect_id,
                .damage_kind = static_cast<std::int16_t>(thrown->damage == 0 ? 0 : 1),
                .damage_suppress_flash = false,
                .audio_commands = {},
            });
        if (player_target_effect_->effect_animation->dispatch_magic_before_prelude) {
            player_target_effect_->audio_commands.push_back(BattleAudioCommand{
                BattleAudioBank::attack,
                player_target_effect_->effect_animation->magic_sample_id});
        }
        render_state_.effect_id = kBattleEffectPointerBase;
        render_state_.effect_visible = false;
        render_state_.damage_kind = 0;
        render_state_.highlight_enabled = false;
        phase_ = BattleSessionPhase::player_effect_prelude_present;
        diagnostics::log_info(
            "battle player throwing-weapon effect ready id=" +
            std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " inventory_slot=" + std::to_string(inventory_slot) +
            " target=" + std::to_string(target.x) + "," +
            std::to_string(target.y) +
            " effect=" + std::to_string(*thrown->effect_id) +
            " damage=" + std::to_string(thrown->damage) +
            " frames=" + std::to_string(
                player_target_effect_->effect_animation->frames.size()));
        return true;
    }

    std::optional<BattleAreaResult> result;
    std::int16_t effect_id = 0;
    std::int16_t damage_kind = 0;
    bool suppress_flash = false;
    if (action == BattlePlayerAction::use_poison) {
        result = setup_.apply_poison_target(current_actor_slot_, target);
        effect_id = 30;
        damage_kind = 2;
    } else if (action == BattlePlayerAction::detoxification) {
        result = setup_.apply_detox_target(current_actor_slot_, target, random_);
        effect_id = 36;
        damage_kind = 3;
        suppress_flash = true;
    } else if (action == BattlePlayerAction::medicine) {
        result = setup_.apply_medicine_target(current_actor_slot_, target, random_);
        effect_id = 0;
        damage_kind = 4;
        suppress_flash = true;
    } else {
        error_ = "battle player target action does not use support effect";
        return false;
    }
    if (!result.has_value()) {
        error_ = setup_.valid() ? "battle player target state application failed" : setup_.error();
        return false;
    }

    auto animation = setup_.magic_animation_plan(
        current_actor_slot_,
        selected_magic_slot_,
        0,
        effect_id,
        kBattleFightPointerBase);
    if (!animation.has_value()) {
        error_ = "battle player target magic animation is invalid";
        return false;
    }
    if (!renderer_.load_fight_package(animation->fight_head_id)) {
        error_ = "battle player target FIGHT package load failed";
        return false;
    }

    player_target_effect_ = std::make_unique<PlayerTargetEffectState>(
        PlayerTargetEffectState{
            .action = action,
            .ai_controlled = false,
            .magic_animation = std::move(*animation),
            .effect_animation = std::nullopt,
            .effect_id = effect_id,
            .damage_kind = damage_kind,
            .damage_suppress_flash = suppress_flash,
            .audio_commands = {},
        });
    player_cursor_selection_.reset();
    render_state_.path_limit = 0;
    render_state_.effect_id = kBattleEffectPointerBase;
    render_state_.effect_visible = false;
    render_state_.damage_kind = 0;
    render_state_.highlight_enabled = false;
    diagnostics::log_info(
        "battle player target effect ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " action=" + std::to_string(static_cast<std::int16_t>(action)) +
        " target=" + std::to_string(target.x) + "," + std::to_string(target.y) +
        " effect=" + std::to_string(effect_id) +
        " damage_kind=" + std::to_string(damage_kind) +
        " hits=" + std::to_string(result->hit_count) +
        " frames=" +
        std::to_string(player_target_effect_->magic_animation.frames.size()));
    return prepare_player_magic_frame();
}

bool BattleSession::advance_player_effect_prelude_wait(
    const std::uint32_t bios_tick) {
    if (!player_target_effect_ || !player_target_effect_->effect_animation.has_value()) {
        error_ = "battle player effect prelude continuation is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    if (bios_tick == effect.animation_wait_tick) {
        return true;
    }
    effect.animation_wait_tick = bios_tick;
    if (effect.animation_wait_tick_changes_remaining > 0) {
        --effect.animation_wait_tick_changes_remaining;
    }
    if (effect.animation_wait_tick_changes_remaining > 0) {
        return true;
    }
    if (effect.effect_animation->dispatch_effect_after_prelude) {
        effect.audio_commands.push_back(BattleAudioCommand{
            BattleAudioBank::effect, effect.effect_animation->effect_sample_id});
    }
    effect.magic_frame = 0U;
    return prepare_player_effect_frame();
}

bool BattleSession::prepare_player_effect_frame() {
    if (!player_target_effect_ || !player_target_effect_->effect_animation.has_value() ||
        player_target_effect_->magic_frame >=
            player_target_effect_->effect_animation->frames.size()) {
        error_ = "battle player effect animation frame is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    const auto& frame = effect.effect_animation->frames[effect.magic_frame];
    render_state_.effect_visible = frame.effect_visible;
    render_state_.effect_frame_offset = frame.effect_frame;
    const auto ai_controlled = effect.ai_controlled;
    phase_ = ai_controlled ? BattleSessionPhase::ai_magic_frame_present
                           : BattleSessionPhase::player_magic_frame_present;
    diagnostics::log_debug(
        std::string{"battle "} + (ai_controlled ? "AI" : "player") +
        " effect frame ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " frame=" + std::to_string(effect.magic_frame) +
        " effect_frame=" + std::to_string(frame.effect_frame));
    return true;
}

bool BattleSession::prepare_player_magic_frame() {
    if (!player_target_effect_ ||
        player_target_effect_->magic_frame >=
            player_target_effect_->magic_animation.frames.size()) {
        error_ = "battle player magic animation frame is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    const auto& frame = effect.magic_animation.frames[effect.magic_frame];
    if (frame.actor_sprite_updated) {
        setup_.combatants()[current_actor_slot_].words[combatant_word::sprite] =
            frame.actor_sprite;
    }
    render_state_.effect_visible = frame.effect_visible;
    render_state_.effect_frame_offset = frame.effect_frame;
    if (frame.dispatch_magic_sample) {
        effect.audio_commands.push_back(BattleAudioCommand{
            BattleAudioBank::attack, effect.magic_animation.magic_sample_id});
    }
    if (frame.dispatch_effect_sample) {
        effect.audio_commands.push_back(BattleAudioCommand{
            BattleAudioBank::effect, effect.magic_animation.effect_sample_id});
    }
    const auto ai_controlled = effect.ai_controlled;
    phase_ = ai_controlled ? BattleSessionPhase::ai_magic_frame_present
                           : BattleSessionPhase::player_magic_frame_present;
    diagnostics::log_debug(
        std::string{"battle "} + (ai_controlled ? "AI" : "player") +
        " magic frame ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " frame=" + std::to_string(effect.magic_frame) +
        " sprite=" + std::to_string(frame.actor_sprite) +
        " effect_frame=" + std::to_string(frame.effect_frame) +
        " effect_visible=" + (frame.effect_visible ? std::string{"true"} : std::string{"false"}) +
        " attack_sample=" + (frame.dispatch_magic_sample ? std::string{"true"} : std::string{"false"}) +
        " effect_sample=" + (frame.dispatch_effect_sample ? std::string{"true"} : std::string{"false"}));
    return true;
}

bool BattleSession::advance_player_magic_wait(const std::uint32_t bios_tick) {
    if (!player_target_effect_) {
        error_ = "battle player target effect continuation is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    if (bios_tick == effect.animation_wait_tick) {
        return true;
    }
    effect.animation_wait_tick = bios_tick;
    if (effect.animation_wait_tick_changes_remaining > 0) {
        --effect.animation_wait_tick_changes_remaining;
    }
    if (effect.animation_wait_tick_changes_remaining > 0) {
        return true;
    }
    ++effect.magic_frame;
    const auto frame_count = effect.effect_animation.has_value()
        ? effect.effect_animation->frames.size()
        : effect.magic_animation.frames.size();
    if (effect.magic_frame < frame_count) {
        return effect.effect_animation.has_value()
            ? prepare_player_effect_frame()
            : prepare_player_magic_frame();
    }
    render_state_.effect_visible = false;
    return begin_player_damage_animation();
}

bool BattleSession::begin_player_damage_animation() {
    if (!player_target_effect_) {
        error_ = "battle player target effect continuation is absent";
        return false;
    }
    render_state_.effect_visible = false;
    player_target_effect_->damage_animation = BattleSetup::damage_animation_frames(
        player_target_effect_->damage_suppress_flash);
    player_target_effect_->damage_frame = 0U;
    return prepare_player_damage_frame();
}

bool BattleSession::prepare_player_damage_frame() {
    if (!player_target_effect_ ||
        player_target_effect_->damage_frame >=
            player_target_effect_->damage_animation.size()) {
        error_ = "battle player damage animation frame is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    const auto& frame = effect.damage_animation[effect.damage_frame];
    render_state_.damage_kind = effect.damage_kind;
    render_state_.damage_text_offset = frame.phase;
    render_state_.highlight_enabled = frame.flash;
    render_state_.highlight_mode = effect.damage_kind == 2 ? 2 : 1;
    const auto ai_controlled = effect.ai_controlled;
    phase_ = ai_controlled ? BattleSessionPhase::ai_damage_frame_present
                           : BattleSessionPhase::player_damage_frame_present;
    diagnostics::log_debug(
        std::string{"battle "} + (ai_controlled ? "AI" : "player") +
        " damage frame ready id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " frame=" + std::to_string(effect.damage_frame) +
        " phase=" + std::to_string(frame.phase) +
        " flash=" + (frame.flash ? std::string{"true"} : std::string{"false"}));
    return true;
}

bool BattleSession::advance_player_damage_wait(const std::uint32_t bios_tick) {
    if (!player_target_effect_) {
        error_ = "battle player target effect continuation is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    if (bios_tick == effect.animation_wait_tick) {
        return true;
    }
    effect.animation_wait_tick = bios_tick;
    if (effect.animation_wait_tick_changes_remaining > 0) {
        --effect.animation_wait_tick_changes_remaining;
    }
    if (effect.animation_wait_tick_changes_remaining > 0) {
        return true;
    }
    ++effect.damage_frame;
    if (effect.damage_frame < effect.damage_animation.size()) {
        return prepare_player_damage_frame();
    }
    return finish_player_target_effect();
}

bool BattleSession::finish_player_target_effect() {
    if (!player_target_effect_) {
        error_ = "battle player target effect continuation is absent";
        return false;
    }
    const auto action = player_target_effect_->action;
    render_state_.effect_visible = false;
    render_state_.damage_kind = 0;
    render_state_.highlight_enabled = false;
    if (action == BattlePlayerAction::attack) {
        if (!player_attack_ || !setup_.refresh_combatant_sprites()) {
            error_ = "battle player attack sprite refresh failed";
            return false;
        }
        const auto ai_controlled = player_attack_->ai_controlled;
        phase_ = ai_controlled ? BattleSessionPhase::ai_attack_commit_present
                               : BattleSessionPhase::player_attack_commit_present;
        diagnostics::log_info(
            std::string{"battle "} + (ai_controlled ? "AI" : "player") +
            " attack animation complete id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " iteration=" + std::to_string(player_attack_->iteration) +
            " magic_frames=" + std::to_string(player_target_effect_->magic_frame) +
            " damage_frames=" + std::to_string(player_target_effect_->damage_frame));
        return true;
    }

    bool finished = false;
    if (action == BattlePlayerAction::use_poison) {
        finished = setup_.finish_poison_action(current_actor_slot_);
    } else if (action == BattlePlayerAction::detoxification) {
        finished = setup_.finish_detox_action(current_actor_slot_);
    } else if (action == BattlePlayerAction::medicine) {
        finished = setup_.finish_medicine_action(current_actor_slot_);
    } else if (action == BattlePlayerAction::item &&
               player_target_effect_->ai_controlled && ai_item_plan_) {
        const BattleAiChoice choice{
            .action = BattleAiAction::throwing_weapon,
            .target_slot = ai_item_plan_->target_slot,
            .item_source = ai_item_plan_->item_source,
            .item_slot = ai_item_plan_->item_slot,
            .action_code_written = true,
        };
        finished = setup_.consume_ai_item(current_actor_slot_, choice);
        if (finished) {
            diagnostics::log_info(
                "battle AI item consumed id=" + std::to_string(battle_id()) +
                " slot=" + std::to_string(current_actor_slot_) +
                " item=" + std::to_string(ai_item_plan_->item_id) +
                " use_mode=" + std::to_string(ai_item_plan_->use_mode) +
                " source=" + std::to_string(static_cast<std::int16_t>(
                    ai_item_plan_->item_source)) +
                " item_slot=" + std::to_string(ai_item_plan_->item_slot));
        }
    } else if (action == BattlePlayerAction::item && player_item_ &&
               player_item_->selected_inventory_slot.has_value()) {
        finished = setup_.finish_throwing_weapon_action(
            current_actor_slot_, *player_item_->selected_inventory_slot);
    }
    if (!finished) {
        error_ = setup_.valid() ? "battle player target action completion failed" : setup_.error();
        return false;
    }
    const auto ai_controlled = player_target_effect_->ai_controlled;
    diagnostics::log_info(
        std::string{"battle "} + (ai_controlled ? "AI" : "player") +
        " target effect complete id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " action=" + std::to_string(static_cast<std::int16_t>(action)) +
        " magic_frames=" + std::to_string(player_target_effect_->magic_frame) +
        " damage_frames=" + std::to_string(player_target_effect_->damage_frame));
    player_target_effect_.reset();
    if (action == BattlePlayerAction::item) {
        player_item_.reset();
    }
    if (ai_controlled) {
        if (action == BattlePlayerAction::use_poison) {
            ai_poison_plan_.reset();
        } else if (action == BattlePlayerAction::detoxification ||
                   action == BattlePlayerAction::medicine) {
            ai_support_plan_.reset();
        } else if (action == BattlePlayerAction::item) {
            ai_item_plan_.reset();
        }
        return finish_ai_handler(action, false);
    }
    return finish_current_actor(action);
}

bool BattleSession::advance_player_attack_commit_wait(
    const std::uint32_t bios_tick) {
    if (!player_target_effect_ || !player_attack_) {
        error_ = "battle player attack commit continuation is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    if (bios_tick == effect.animation_wait_tick) {
        return true;
    }
    effect.animation_wait_tick = bios_tick;
    if (effect.animation_wait_tick_changes_remaining > 0) {
        --effect.animation_wait_tick_changes_remaining;
    }
    if (effect.animation_wait_tick_changes_remaining > 0) {
        return true;
    }
    return commit_player_attack_iteration();
}

bool BattleSession::commit_player_attack_iteration() {
    if (!player_target_effect_ || !player_attack_) {
        error_ = "battle player attack commit continuation is absent";
        return false;
    }
    const auto level_up = setup_.commit_attack_iteration(
        current_actor_slot_,
        selected_magic_slot_,
        setup_.last_hp_cost_scale(),
        random_);
    diagnostics::log_info(
        std::string{"battle "} + (player_attack_->ai_controlled ? "AI" : "player") +
        " attack iteration committed id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " iteration=" + std::to_string(player_attack_->iteration) +
        " cost_scale=" + std::to_string(setup_.last_hp_cost_scale()) +
        " level_up=" + (level_up ? std::string{"true"} : std::string{"false"}));
    if (!level_up) {
        return finish_player_attack_iteration();
    }

    const auto role_id = setup_.combatants()[current_actor_slot_]
                             .words[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        error_ = "battle player attack level role is outside ranger records";
        return false;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    const auto magic_id = role.word(
        model::role_word::magic_id_begin + static_cast<std::size_t>(selected_magic_slot_));
    if (magic_id < 0 || static_cast<std::size_t>(magic_id) >= ranger_.magics.size()) {
        error_ = "battle player attack level magic is outside ranger records";
        return false;
    }
    const auto& magic = ranger_.magics[static_cast<std::size_t>(magic_id)];
    const auto name = std::span<const std::uint8_t>{magic.bytes}.subspan(
        model::magic_word::name_byte, model::magic_word::name_bytes);
    auto& text = player_attack_->level_text;
    const auto visible_name = terminated_name(name);
    text.assign(visible_name.begin(), visible_name.end());
    constexpr std::array<std::uint8_t, 8> kLevelPrefix{
        0x20U, 0xA4U, 0xC9U, 0xACU, 0xB0U, 0xB2U, 0xC4U, 0x20U};
    constexpr std::array<std::uint8_t, 3> kLevelSuffix{0x20U, 0xAFU, 0xC5U};
    text.insert(text.end(), kLevelPrefix.begin(), kLevelPrefix.end());
    const auto rank = static_cast<std::uint16_t>(
        role.unsigned_word(
            model::role_word::magic_level_begin +
            static_cast<std::size_t>(selected_magic_slot_)) /
            100U +
        1U);
    text.push_back(rank < 10U
                       ? static_cast<std::uint8_t>(' ')
                       : static_cast<std::uint8_t>('0' + rank / 10U));
    text.push_back(static_cast<std::uint8_t>('0' + rank % 10U));
    text.insert(text.end(), kLevelSuffix.begin(), kLevelSuffix.end());
    phase_ = player_attack_->ai_controlled
        ? BattleSessionPhase::ai_attack_level_present
        : BattleSessionPhase::player_attack_level_present;
    return true;
}

bool BattleSession::advance_player_attack_level_wait(
    const std::uint32_t bios_tick) {
    if (!player_target_effect_ || !player_attack_) {
        error_ = "battle player attack level continuation is absent";
        return false;
    }
    auto& effect = *player_target_effect_;
    if (bios_tick == effect.animation_wait_tick) {
        return true;
    }
    effect.animation_wait_tick = bios_tick;
    if (effect.animation_wait_tick_changes_remaining > 0) {
        --effect.animation_wait_tick_changes_remaining;
    }
    if (effect.animation_wait_tick_changes_remaining > 0) {
        return true;
    }
    return finish_player_attack_iteration();
}

bool BattleSession::finish_player_attack_iteration() {
    if (!player_target_effect_ || !player_attack_) {
        error_ = "battle player attack iteration continuation is absent";
        return false;
    }
    player_target_effect_.reset();
    player_attack_->level_text.clear();
    ++player_attack_->iteration;
    if (player_attack_->iteration < player_attack_->profile.attack_count) {
        return begin_player_attack_iteration();
    }
    if (!setup_.finish_attack(current_actor_slot_)) {
        error_ = "battle player attack completion failed";
        return false;
    }
    const auto ai_controlled = player_attack_->ai_controlled;
    diagnostics::log_info(
        std::string{"battle "} + (ai_controlled ? "AI" : "player") +
        " attack complete id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " iterations=" + std::to_string(player_attack_->iteration));
    player_attack_.reset();
    if (ai_controlled) {
        ai_attack_plan_.reset();
        return finish_ai_handler(BattlePlayerAction::attack, false);
    }
    return finish_current_actor(BattlePlayerAction::attack);
}

bool BattleSession::advance_player_movement_step() {
    if (!player_movement_plan_.has_value()) {
        error_ = "battle player movement plan is absent";
        return false;
    }
    const auto step = setup_.advance_player_movement(*player_movement_plan_);
    if (!step.has_value()) {
        if (player_movement_plan_->complete) {
            player_movement_plan_.reset();
            return rebuild_player_menu_after_movement();
        }
        error_ = setup_.valid() ? "battle player movement step failed" : setup_.error();
        return false;
    }
    render_state_.view_x = step->view_x;
    render_state_.view_y = step->view_y;
    render_state_.path_limit = 0;
    render_state_.primary_cursor = step->to;
    phase_ = BattleSessionPhase::player_movement_step_present;
    diagnostics::log_info(
        "battle player movement step id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " from=" + std::to_string(step->from.x) + "," +
        std::to_string(step->from.y) +
        " to=" + std::to_string(step->to.x) + "," +
        std::to_string(step->to.y) +
        " round_value=" + std::to_string(step->remaining_round_value) +
        " physical_power=" + std::to_string(step->physical_power) +
        " complete=" + (step->complete ? std::string{"true"} : std::string{"false"}));
    return true;
}

bool BattleSession::advance_player_movement_wait(const std::uint32_t bios_tick) {
    if (bios_tick == player_movement_wait_tick_) {
        return true;
    }
    player_movement_wait_tick_ = bios_tick;
    if (player_movement_wait_tick_changes_remaining_ > 0) {
        --player_movement_wait_tick_changes_remaining_;
    }
    if (player_movement_wait_tick_changes_remaining_ > 0) {
        return true;
    }
    if (player_movement_plan_.has_value() && !player_movement_plan_->complete) {
        return advance_player_movement_step();
    }
    player_movement_plan_.reset();
    return rebuild_player_menu_after_movement();
}

bool BattleSession::rebuild_player_menu_after_movement() {
    const auto availability = setup_.player_action_availability(current_actor_slot_);
    if (!availability.has_value()) {
        error_ = "battle player movement availability recheck failed";
        return false;
    }
    const auto old_movement = player_action_menu_.available[0U];
    const auto new_movement = availability->available[0U];
    if (old_movement != new_movement) {
        player_action_menu_.available[0U] = new_movement;
        if (new_movement == 0 && player_action_menu_.available_count > 0U) {
            --player_action_menu_.available_count;
        } else if (new_movement == 1) {
            ++player_action_menu_.available_count;
        }
    }
    player_action_menu_.cursor = std::min(
        player_action_menu_.cursor,
        player_action_menu_.available_count > 0U
            ? player_action_menu_.available_count - 1U
            : 0U);
    player_action_menu_.selected_action = -1;
    render_state_.path_limit = 0;
    phase_ = BattleSessionPhase::player_action;
    diagnostics::log_info(
        "battle player action menu rebuilt after movement id=" +
        std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " movement=" + std::to_string(new_movement) +
        " available=" + std::to_string(player_action_menu_.available_count));
    return player_action_menu_.available_count > 0U;
}

std::optional<std::size_t> BattleSession::action_for_ordinal(
    const std::size_t ordinal) const noexcept {
    std::size_t available_ordinal = 0U;
    for (std::size_t action = 0U; action < player_action_menu_.available.size(); ++action) {
        if (player_action_menu_.available[action] != 1) {
            continue;
        }
        if (available_ordinal == ordinal) {
            return action;
        }
        ++available_ordinal;
    }
    return std::nullopt;
}

BattleSessionInputResult BattleSession::handle_player_action_key(
    const std::uint8_t translated_key) {
    if (player_action_menu_.available_count == 0U ||
        player_action_menu_.cursor >= player_action_menu_.available_count ||
        player_action_menu_.selected_action >= 0) {
        return BattleSessionInputResult::ignored;
    }
    if (translated_key == kDown) {
        player_action_menu_.cursor =
            player_action_menu_.cursor + 1U == player_action_menu_.available_count
            ? 0U
            : player_action_menu_.cursor + 1U;
        diagnostics::log_debug(
            "battle player action cursor id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " cursor=" + std::to_string(player_action_menu_.cursor));
        return BattleSessionInputResult::action_changed;
    }
    if (translated_key == kUp) {
        player_action_menu_.cursor = player_action_menu_.cursor == 0U
            ? player_action_menu_.available_count - 1U
            : player_action_menu_.cursor - 1U;
        diagnostics::log_debug(
            "battle player action cursor id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " cursor=" + std::to_string(player_action_menu_.cursor));
        return BattleSessionInputResult::action_changed;
    }
    if (!confirms(translated_key)) {
        return BattleSessionInputResult::ignored;
    }
    const auto action = action_for_ordinal(player_action_menu_.cursor);
    if (!action.has_value()) {
        return BattleSessionInputResult::ignored;
    }
    player_action_menu_.selected_action = static_cast<std::int16_t>(*action);
    phase_ = BattleSessionPhase::player_action_selected;
    diagnostics::log_info(
        "battle player action selected id=" + std::to_string(battle_id()) +
        " slot=" + std::to_string(current_actor_slot_) +
        " cursor=" + std::to_string(player_action_menu_.cursor) +
        " action=" + std::to_string(player_action_menu_.selected_action));
    if (!dispatch_selected_player_action()) {
        diagnostics::log_error(
            "battle player action dispatch failed id=" + std::to_string(battle_id()) +
            " slot=" + std::to_string(current_actor_slot_) +
            " action=" + std::to_string(player_action_menu_.selected_action) +
            " reason=" + error_);
        return BattleSessionInputResult::ignored;
    }
    return BattleSessionInputResult::action_selected;
}

bool BattleSession::dispatch_selected_player_action() {
    const auto action = static_cast<BattlePlayerAction>(
        player_action_menu_.selected_action);
    switch (action) {
    case BattlePlayerAction::wait:
        if (!setup_.defer_turn_to_end(current_actor_slot_).has_value()) {
            error_ = setup_.error();
            return false;
        }
        return finish_current_actor(action);
    case BattlePlayerAction::rest:
        if (!setup_.rest_actor(current_actor_slot_, random_).has_value()) {
            error_ = setup_.error();
            return false;
        }
        return finish_current_actor(action);
    case BattlePlayerAction::automatic:
        phase_ = BattleSessionPhase::automatic_present;
        return true;
    case BattlePlayerAction::movement:
        return begin_player_movement();
    case BattlePlayerAction::use_poison:
    case BattlePlayerAction::detoxification:
    case BattlePlayerAction::medicine:
        return begin_player_targeting(action);
    case BattlePlayerAction::attack:
        return begin_player_attack();
    case BattlePlayerAction::item:
        return begin_player_item_selection();
    case BattlePlayerAction::status:
        return begin_player_status_selection();
    }
    error_ = "battle player action id is invalid";
    return false;
}

bool BattleSession::finish_current_actor(const BattlePlayerAction action) {
    const auto outcome = setup_.evaluate_outcome();
    if (!setup_.clear_hidden_ai_targets().has_value()) {
        error_ = setup_.error();
        return false;
    }
    if (outcome != BattleOutcome::ongoing) {
        outcome_ = outcome;
        phase_ = BattleSessionPhase::battle_outcome;
        diagnostics::log_info(
            "battle outcome reached id=" + std::to_string(battle_id()) +
            " outcome=" + std::to_string(static_cast<int>(outcome_)));
        return true;
    }
    if (action != BattlePlayerAction::wait) {
        ++current_actor_slot_;
    }
    while (current_actor_slot_ < static_cast<std::size_t>(setup_.combatant_count()) &&
           setup_.combatants()[current_actor_slot_]
                   .words[combatant_word::occupancy_hidden] != 0) {
        const auto skipped_outcome = setup_.evaluate_outcome();
        if (!setup_.clear_hidden_ai_targets().has_value()) {
            error_ = setup_.error();
            return false;
        }
        if (skipped_outcome != BattleOutcome::ongoing) {
            outcome_ = skipped_outcome;
            phase_ = BattleSessionPhase::battle_outcome;
            diagnostics::log_info(
                "battle outcome reached id=" + std::to_string(battle_id()) +
                " outcome=" + std::to_string(static_cast<int>(outcome_)));
            return true;
        }
        ++current_actor_slot_;
    }
    if (current_actor_slot_ >= static_cast<std::size_t>(setup_.combatant_count())) {
        if (!setup_.apply_round_status_damage().has_value()) {
            error_ = setup_.error();
            return false;
        }
        phase_ = BattleSessionPhase::round_wait;
        diagnostics::log_info(
            "battle round actions complete id=" + std::to_string(battle_id()) +
            " phase=" + std::string{phase_name(phase_)});
        return true;
    }
    return begin_actor_present();
}

bool BattleSession::begin_post_battle_settlement() {
    if (outcome_ == BattleOutcome::ongoing || post_battle_result_.has_value()) {
        error_ = "battle post-battle settlement state is invalid";
        return false;
    }
    auto settlement = setup_.prepare_battle_settlement(outcome_);
    if (!settlement.has_value()) {
        error_ = setup_.valid() ? "battle post-battle settlement failed" : setup_.error();
        return false;
    }
    post_battle_result_ = std::move(*settlement);
    post_battle_messages_.clear();
    post_battle_message_index_ = 0U;
    post_battle_role_index_ = 0U;
    diagnostics::log_info(
        "battle post-battle settlement ready id=" + std::to_string(battle_id()) +
        " outcome=" + std::to_string(static_cast<int>(outcome_)) +
        " roles=" + std::to_string(post_battle_result_->roles.size()) +
        " experience=" + std::to_string(post_battle_result_->total_experience) +
        " shared=" + std::to_string(post_battle_result_->shared_experience));
    return begin_post_battle_role();
}

bool BattleSession::schedule_post_battle_message(PostBattleMessage message) {
    if (!post_battle_result_.has_value() ||
        message.role_result_index >= post_battle_result_->roles.size()) {
        error_ = "battle post-battle message role is invalid";
        return false;
    }
    post_battle_result_->render_required = true;
    post_battle_result_->present_required = true;
    post_battle_result_->wait_for_input = true;
    post_battle_messages_.push_back(message);
    phase_ = BattleSessionPhase::post_battle_message_present;
    return true;
}

bool BattleSession::begin_post_battle_role() {
    if (!post_battle_result_.has_value()) {
        error_ = "battle post-battle role state is missing";
        return false;
    }
    while (post_battle_role_index_ < post_battle_result_->roles.size()) {
        const auto applied = setup_.apply_post_battle_experience(
            post_battle_result_->roles[post_battle_role_index_].combatant_slot,
            outcome_,
            grants_experience_);
        if (!applied.has_value()) {
            error_ = setup_.valid()
                ? "battle post-battle experience commit failed"
                : setup_.error();
            return false;
        }
        post_battle_result_->roles[post_battle_role_index_] = *applied;
        if (applied->experience_message_required) {
            return schedule_post_battle_message(
                {PostBattleMessageKind::experience, post_battle_role_index_});
        }
        ++post_battle_role_index_;
    }
    result_ = outcome_ == BattleOutcome::victory
        ? BattleStepResult::victory
        : BattleStepResult::defeat;
    phase_ = BattleSessionPhase::complete;
    diagnostics::log_info(
        "battle session complete id=" + std::to_string(battle_id()) +
        " result=" + std::to_string(static_cast<int>(result_)) +
        " messages=" + std::to_string(post_battle_messages_.size()));
    return true;
}

std::optional<BattleLevelUpResult> BattleSession::preview_post_battle_level(
    const std::size_t role_id) {
    if (role_id >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto saved_role = ranger_.roles[role_id];
    auto preview_random = random_;
    const auto preview = setup_.apply_battle_level_up(role_id, false, preview_random);
    ranger_.roles[role_id] = saved_role;
    return preview;
}

std::optional<BattlePracticeResult> BattleSession::preview_post_battle_practice(
    const std::size_t role_id) {
    if (role_id >= ranger_.roles.size()) {
        return std::nullopt;
    }
    const auto saved_role = ranger_.roles[role_id];
    const auto preview = setup_.apply_battle_practice(role_id, false);
    ranger_.roles[role_id] = saved_role;
    return preview;
}

bool BattleSession::continue_post_battle_level() {
    if (!post_battle_result_.has_value() ||
        post_battle_role_index_ >= post_battle_result_->roles.size()) {
        error_ = "battle post-battle level state is invalid";
        return false;
    }
    auto& role_result = post_battle_result_->roles[post_battle_role_index_];
    const auto role_id = static_cast<std::size_t>(role_result.role_id);
    if (ranger_.roles[role_id].word(model::role_word::level) < 30) {
        const auto preview = preview_post_battle_level(role_id);
        if (!preview.has_value()) {
            error_ = setup_.error();
            return false;
        }
        role_result.level_up = *preview;
        if (preview->message_required) {
            return schedule_post_battle_message(
                {PostBattleMessageKind::level_up, post_battle_role_index_});
        }
    }
    return continue_post_battle_practice();
}

bool BattleSession::continue_post_battle_practice() {
    if (!post_battle_result_.has_value() ||
        post_battle_role_index_ >= post_battle_result_->roles.size()) {
        error_ = "battle post-battle practice state is invalid";
        return false;
    }
    auto& role_result = post_battle_result_->roles[post_battle_role_index_];
    const auto role_id = static_cast<std::size_t>(role_result.role_id);
    if (ranger_.roles[role_id].word(model::role_word::practice_item) == -1) {
        return finish_post_battle_role();
    }
    const auto preview = preview_post_battle_practice(role_id);
    if (!preview.has_value()) {
        error_ = setup_.error();
        return false;
    }
    role_result.practice = *preview;
    if (preview->practice_message_required) {
        return schedule_post_battle_message(
            {PostBattleMessageKind::practice, post_battle_role_index_});
    }
    return continue_post_battle_crafting();
}

bool BattleSession::continue_post_battle_crafting() {
    if (!post_battle_result_.has_value() ||
        post_battle_role_index_ >= post_battle_result_->roles.size()) {
        error_ = "battle post-battle crafting state is invalid";
        return false;
    }
    auto& role_result = post_battle_result_->roles[post_battle_role_index_];
    const auto prepared = setup_.apply_battle_crafting(
        static_cast<std::size_t>(role_result.role_id), true, random_);
    if (!prepared.has_value()) {
        error_ = setup_.error();
        return false;
    }
    role_result.craft = *prepared;
    if (prepared->recipe_available) {
        role_result.craft.message_required = true;
        role_result.craft.present_required = true;
        role_result.craft.wait_for_input = true;
        return schedule_post_battle_message(
            {PostBattleMessageKind::craft, post_battle_role_index_});
    }
    return finish_post_battle_role();
}

bool BattleSession::finish_post_battle_role() {
    ++post_battle_role_index_;
    return begin_post_battle_role();
}

bool BattleSession::advance_post_battle_message() {
    if (!post_battle_result_.has_value() ||
        post_battle_message_index_ >= post_battle_messages_.size()) {
        error_ = "battle post-battle message state is invalid";
        return false;
    }
    const auto message = post_battle_messages_[post_battle_message_index_];
    ++post_battle_message_index_;
    auto& role_result = post_battle_result_->roles[message.role_result_index];
    const auto role_id = static_cast<std::size_t>(role_result.role_id);
    switch (message.kind) {
    case PostBattleMessageKind::experience:
        return continue_post_battle_level();
    case PostBattleMessageKind::level_up: {
        const auto level_up = setup_.apply_battle_level_up(role_id, false, random_);
        if (!level_up.has_value()) {
            error_ = setup_.error();
            return false;
        }
        role_result.level_up = *level_up;
        return continue_post_battle_practice();
    }
    case PostBattleMessageKind::practice: {
        const auto practice = setup_.apply_battle_practice(role_id, false);
        if (!practice.has_value()) {
            error_ = setup_.error();
            return false;
        }
        role_result.practice = *practice;
        if (practice->magic_message_required) {
            return schedule_post_battle_message(
                {PostBattleMessageKind::magic_level, post_battle_role_index_});
        }
        return continue_post_battle_crafting();
    }
    case PostBattleMessageKind::magic_level:
        return continue_post_battle_crafting();
    case PostBattleMessageKind::craft: {
        const auto crafted = setup_.commit_battle_crafting(role_result.craft, random_);
        if (!crafted.has_value()) {
            error_ = setup_.error();
            return false;
        }
        role_result.craft = *crafted;
        return finish_post_battle_role();
    }
    }
    error_ = "battle post-battle message kind is invalid";
    return false;
}

bool BattleSession::begin_actor_present() {
    const auto& actor = setup_.combatants()[current_actor_slot_].words;
    render_state_.view_x = static_cast<std::int16_t>(
        std::clamp(static_cast<int>(actor[combatant_word::x]) - 11, 0, 32));
    render_state_.view_y = static_cast<std::int16_t>(
        std::clamp(static_cast<int>(actor[combatant_word::y]) - 11, 0, 32));
    render_state_.path_limit = 0;
    render_state_.primary_cursor = {
        actor[combatant_word::x], actor[combatant_word::y]};
    phase_ = BattleSessionPhase::actor_present;
    diagnostics::log_info(
        "battle next actor ready id=" + std::to_string(battle_id()) +
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
    const auto path_values = player_cursor_selection_.has_value()
        ? std::span<const std::int16_t>{player_cursor_selection_->pathing.values()}
        : std::span<const std::int16_t>{};
    const auto plan = setup_.battle_render_plan(render_state_, path_values);
    return plan.has_value() && renderer_.render(*plan, framebuffer);
}

bool BattleSession::render_player_attack_direction(
    render::IndexedFramebuffer& framebuffer) {
    return player_attack_ && render_battlefield(framebuffer) &&
        renderer_.draw_box(framebuffer, 122, 40, 116U, 27U) &&
        renderer_.draw_text(
            framebuffer, 132, 45, kAttackDirectionPrompt, 0x0705U);
}

bool BattleSession::render_player_attack_level(
    render::IndexedFramebuffer& framebuffer) {
    if (!player_attack_ || player_attack_->level_text.empty() ||
        !render_battlefield(framebuffer)) {
        return false;
    }
    const auto length = static_cast<int>(player_attack_->level_text.size());
    return renderer_.draw_box(
               framebuffer,
               150 - 4 * length,
               40,
               static_cast<std::uint16_t>(8 * length + 20),
               27U) &&
        renderer_.draw_text(
            framebuffer,
            160 - 4 * length,
            45,
            player_attack_->level_text,
            0x0705U);
}

bool BattleSession::render_player_magic_selection(
    render::IndexedFramebuffer& framebuffer) {
    if (!player_magic_selection_.has_value() ||
        player_magic_selection_->learned_count <= 0 ||
        player_magic_selection_->learned_count >
            static_cast<std::int16_t>(model::role_word::magic_count) ||
        player_magic_selection_->available_count <= 0 ||
        player_magic_selection_->cursor < 0 ||
        player_magic_selection_->cursor >= player_magic_selection_->available_count ||
        !render_battlefield(framebuffer)) {
        return false;
    }
    const auto& actor = setup_.combatants()[current_actor_slot_].words;
    const auto role_id = actor[combatant_word::role_id];
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger_.roles.size()) {
        return false;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_id)];
    const auto draw_magic_name = [this, &framebuffer, &role](
                                     const std::int16_t magic_slot,
                                     const int y,
                                     const std::uint16_t colors) {
        if (magic_slot < 0 ||
            static_cast<std::size_t>(magic_slot) >= model::role_word::magic_count) {
            return false;
        }
        const auto magic_id = role.word(
            model::role_word::magic_id_begin + static_cast<std::size_t>(magic_slot));
        if (magic_id < 0 || static_cast<std::size_t>(magic_id) >= ranger_.magics.size()) {
            return false;
        }
        const auto& magic = ranger_.magics[static_cast<std::size_t>(magic_id)];
        const auto name = std::span<const std::uint8_t>{magic.bytes}.subspan(
            model::magic_word::name_byte, model::magic_word::name_bytes);
        return renderer_.draw_text(
            framebuffer,
            centered_magic_name_x(name),
            y,
            terminated_name(name),
            colors);
    };

    const auto panel_height = static_cast<std::uint16_t>(
        17 * player_magic_selection_->learned_count + 10);
    if (!renderer_.draw_box(framebuffer, 20, 10, 90U, panel_height)) {
        return false;
    }
    std::int16_t ordinal = 0;
    for (std::int16_t magic_slot = 0;
         magic_slot < player_magic_selection_->learned_count;
         ++magic_slot) {
        const auto available = std::find(
            player_magic_selection_->available_slots.begin(),
            player_magic_selection_->available_slots.begin() +
                player_magic_selection_->available_count,
            magic_slot);
        if (available == player_magic_selection_->available_slots.begin() +
                player_magic_selection_->available_count) {
            continue;
        }
        if (!draw_magic_name(magic_slot, 17 * ordinal + 15, 0x2321U)) {
            return false;
        }
        ++ordinal;
    }

    const auto selected_slot = player_magic_selection_->available_slots[
        static_cast<std::size_t>(player_magic_selection_->cursor)];
    return draw_magic_name(
        selected_slot,
        17 * player_magic_selection_->cursor + 15,
        0x6663U);
}

bool BattleSession::render_player_item_selection(
    render::IndexedFramebuffer& framebuffer) {
    if (!player_item_ || !render_battlefield(framebuffer) ||
        !renderer_.draw_box(framebuffer, 45, 2, 230U, 23U) ||
        !renderer_.draw_box(framebuffer, 45, 27, 230U, 23U) ||
        !renderer_.draw_box(framebuffer, 45, 52, 230U, 145U)) {
        return false;
    }
    const auto draw_scroll_line = [&framebuffer](
                                      const int x,
                                      const int y,
                                      const int width,
                                      const int height) {
        return framebuffer.fill_rectangle(
            x,
            y,
            static_cast<std::uint16_t>(width),
            static_cast<std::uint16_t>(height),
            99U);
    };
    if (player_item_->selection.count > 5 * (player_item_->page + 3)) {
        if (!draw_scroll_line(267, 175, 2, 1) ||
            !draw_scroll_line(266, 174, 4, 1) ||
            !draw_scroll_line(265, 173, 6, 1) ||
            !draw_scroll_line(264, 172, 8, 1) ||
            !draw_scroll_line(266, 161, 4, 11)) {
            return false;
        }
    }
    if (player_item_->selection.count > 15 && player_item_->page > 0) {
        if (!draw_scroll_line(267, 72, 2, 1) ||
            !draw_scroll_line(266, 73, 4, 1) ||
            !draw_scroll_line(265, 74, 6, 1) ||
            !draw_scroll_line(264, 75, 8, 1) ||
            !draw_scroll_line(266, 76, 4, 11)) {
            return false;
        }
    }

    for (std::int16_t row = 0; row < 3; ++row) {
        for (std::int16_t column = 0; column < 5; ++column) {
            const auto x = 55 + 42 * column;
            const auto y = 62 + 42 * row;
            if (!framebuffer.outline_rectangle(x, y, 40U, 40U, 0U)) {
                return false;
            }
            const auto list_index = static_cast<std::int32_t>(
                5 * (player_item_->page + row) + column);
            if (list_index < 0 ||
                list_index >= static_cast<std::int32_t>(model::kInventoryCount)) {
                continue;
            }
            const auto inventory_slot = player_item_->selection.inventory_slots[
                static_cast<std::size_t>(list_index)];
            if (inventory_slot < 0 ||
                static_cast<std::size_t>(inventory_slot) >= model::kInventoryCount) {
                continue;
            }
            const auto item_id = ranger_.header.inventory_item(
                static_cast<std::size_t>(inventory_slot)).value;
            if (item_id >= 0 && static_cast<std::size_t>(item_id) < ranger_.items.size() &&
                !renderer_.draw_item_icon(framebuffer, item_id, x, y)) {
                return false;
            }
        }
    }
    if (!framebuffer.outline_rectangle(
            55 + 42 * player_item_->column,
            62 + 42 * player_item_->row,
            40U,
            40U,
            255U)) {
        return false;
    }

    const auto list_index = static_cast<std::int32_t>(
        5 * (player_item_->page + player_item_->row) + player_item_->column);
    if (list_index < 0 || list_index >= static_cast<std::int32_t>(model::kInventoryCount)) {
        return true;
    }
    const auto inventory_slot = player_item_->selection.inventory_slots[
        static_cast<std::size_t>(list_index)];
    if (inventory_slot < 0 ||
        static_cast<std::size_t>(inventory_slot) >= model::kInventoryCount) {
        return true;
    }
    const auto item_id = ranger_.header.inventory_item(
        static_cast<std::size_t>(inventory_slot)).value;
    if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
        return true;
    }
    const auto& item = ranger_.items[static_cast<std::size_t>(item_id)];
    const auto name = terminated_name(std::span<const std::uint8_t>{item.bytes}.subspan(
        2U * model::item_word::secondary_name_begin,
        2U * model::item_word::secondary_name_count));
    const auto introduction = terminated_name(
        std::span<const std::uint8_t>{item.bytes}.subspan(
            model::item_word::introduction_byte,
            model::item_word::introduction_bytes));
    if (!renderer_.draw_text(
            framebuffer,
            160 - 4 * static_cast<int>(name.size()),
            5,
            name,
            0x0705U) ||
        !renderer_.draw_text(
            framebuffer,
            160 - 4 * static_cast<int>(introduction.size()),
            30,
            introduction,
            0x2321U)) {
        return false;
    }
    const auto inventory_count = ranger_.header.inventory_count(
        static_cast<std::size_t>(inventory_slot));
    if (inventory_count <= 1) {
        return true;
    }
    constexpr std::array<std::uint8_t, 1> kCountMarker{'X'};
    return renderer_.draw_text(framebuffer, 215, 5, kCountMarker, 0x2321U) &&
        renderer_.draw_text(
            framebuffer,
            235,
            5,
            decimal_text(inventory_count, 2U),
            0x6663U);
}

bool BattleSession::render_player_item_effect(
    render::IndexedFramebuffer& framebuffer) {
    if (!player_item_ || !player_item_->effect_result.has_value() ||
        player_item_->selected_item_id < 0 ||
        static_cast<std::size_t>(player_item_->selected_item_id) >= ranger_.items.size() ||
        !render_battlefield(framebuffer)) {
        return false;
    }
    const auto& effect = *player_item_->effect_result;
    if (!renderer_.draw_box(
            framebuffer,
            effect.panel_x,
            effect.panel_y,
            static_cast<std::uint16_t>(effect.panel_width),
            static_cast<std::uint16_t>(effect.panel_height))) {
        return false;
    }
    const auto& item = ranger_.items[static_cast<std::size_t>(player_item_->selected_item_id)];
    const auto name = terminated_name(std::span<const std::uint8_t>{item.bytes}.subspan(
        2U * model::item_word::secondary_name_begin,
        2U * model::item_word::secondary_name_count));
    std::vector<std::uint8_t> header{kUseItemPrefix.begin(), kUseItemPrefix.end()};
    header.insert(header.end(), name.begin(), name.end());
    if (!renderer_.draw_text(framebuffer, 75, 25, header, 0x6663U)) {
        return false;
    }
    std::int16_t visible_row = 0;
    for (std::size_t index = 0U; index < effect.deltas.size(); ++index) {
        const auto delta = effect.deltas[index];
        if (delta == 0) {
            continue;
        }
        const auto y = 45 + 18 * visible_row;
        if (!renderer_.draw_text(
                framebuffer,
                75,
                y,
                terminated_name(kItemEffectLabels[index]),
                0x0705U)) {
            return false;
        }
        if (index == 4U) {
            if (!renderer_.draw_text(framebuffer, 155, y, kItemMpTypeChanged, 0x0705U)) {
                return false;
            }
        } else {
            if (!renderer_.draw_text(
                    framebuffer,
                    155,
                    y,
                    delta > 0 ? std::span<const std::uint8_t>{kItemIncrease}
                              : std::span<const std::uint8_t>{kItemDecrease},
                    delta > 0 ? 0x0705U : 0x1014U)) {
                return false;
            }
            const auto magnitude = delta < 0
                ? -static_cast<std::int32_t>(delta)
                : static_cast<std::int32_t>(delta);
            if (!renderer_.draw_text(
                    framebuffer,
                    187,
                    y,
                    decimal_text(magnitude, 3),
                    0x0705U)) {
                return false;
            }
        }
        ++visible_row;
    }
    return true;
}

bool BattleSession::render_player_status_selection(
    render::IndexedFramebuffer& framebuffer) {
    return player_status_.has_value() && render_battlefield(framebuffer) &&
        renderer_.render_character_status_selection(
            ranger_, player_status_->cursor, framebuffer);
}

bool BattleSession::render_player_status_page(
    render::IndexedFramebuffer& framebuffer) {
    return player_status_.has_value() && player_status_->role_id >= 0 &&
        render_battlefield(framebuffer) &&
        renderer_.render_character_status(
            ranger_, player_status_->role_id, player_status_->page, framebuffer);
}

bool BattleSession::render_player_action_menu(
    render::IndexedFramebuffer& framebuffer) {
    if (player_action_menu_.available_count == 0U ||
        player_action_menu_.cursor >= player_action_menu_.available_count ||
        !render_battlefield(framebuffer)) {
        return false;
    }
    const auto panel_height = static_cast<std::uint16_t>(
        17U * player_action_menu_.available_count + 10U);
    if (!renderer_.draw_box(framebuffer, 20, 19, 42U, panel_height)) {
        return false;
    }
    std::size_t ordinal = 0U;
    for (std::size_t action = 0U; action < player_action_menu_.available.size(); ++action) {
        if (player_action_menu_.available[action] != 1) {
            continue;
        }
        if (!renderer_.draw_text(
                framebuffer,
                25,
                24 + static_cast<int>(17U * ordinal),
                kPlayerActionLabels[action],
                ordinal == player_action_menu_.cursor ? 0x6663U : 0x2321U)) {
            return false;
        }
        ++ordinal;
    }
    const auto status_panel = setup_.status_panel_plan(current_actor_slot_);
    return status_panel.has_value() &&
        renderer_.render_status_panel(*status_panel, framebuffer);
}

bool BattleSession::render_battle_outcome(
    render::IndexedFramebuffer& framebuffer) {
    if (outcome_ == BattleOutcome::ongoing || !render_battlefield(framebuffer) ||
        !renderer_.draw_box(framebuffer, 118, 30, 85U, 27U)) {
        return false;
    }
    const auto text = outcome_ == BattleOutcome::victory
        ? std::span<const std::uint8_t>{kBattleVictoryText}
        : std::span<const std::uint8_t>{kBattleDefeatText};
    return renderer_.draw_text(framebuffer, 128, 35, text, 0x0705U);
}

bool BattleSession::render_post_battle_message(
    render::IndexedFramebuffer& framebuffer) {
    if (!post_battle_result_.has_value() ||
        post_battle_message_index_ >= post_battle_messages_.size() ||
        !render_battlefield(framebuffer)) {
        return false;
    }
    const auto message = post_battle_messages_[post_battle_message_index_];
    if (message.role_result_index >= post_battle_result_->roles.size()) {
        return false;
    }
    const auto& role_result = post_battle_result_->roles[message.role_result_index];
    if (role_result.role_id < 0 ||
        static_cast<std::size_t>(role_result.role_id) >= ranger_.roles.size()) {
        return false;
    }
    const auto& role = ranger_.roles[static_cast<std::size_t>(role_result.role_id)];
    const auto role_name = terminated_name(
        std::span<const std::uint8_t>{role.bytes}.subspan(
            model::role_word::name_byte, model::role_word::name_bytes));
    std::vector<std::uint8_t> text;
    const auto append = [&text](const std::span<const std::uint8_t> bytes) {
        text.insert(text.end(), bytes.begin(), bytes.end());
    };

    switch (message.kind) {
    case PostBattleMessageKind::experience: {
        append(role_name);
        append(kExperienceGainedText);
        append(decimal_text(role_result.experience_gained, 5));
        return renderer_.draw_box(framebuffer, 60, 30, 200U, 27U) &&
            renderer_.draw_text(framebuffer, 63, 35, text, 0x0705U);
    }
    case PostBattleMessageKind::level_up:
        append(role_name);
        append(kLevelUpText);
        return renderer_.draw_box(framebuffer, 100, 30, 120U, 27U) &&
            renderer_.draw_text(framebuffer, 107, 35, text, 0x0705U);
    case PostBattleMessageKind::practice: {
        const auto item_id = role_result.practice.item_id;
        if (item_id < 0 || static_cast<std::size_t>(item_id) >= ranger_.items.size()) {
            return false;
        }
        const auto item_name = terminated_name(
            std::span<const std::uint8_t>{
                ranger_.items[static_cast<std::size_t>(item_id)].bytes}.subspan(
                    2U * model::item_word::secondary_name_begin,
                    2U * model::item_word::secondary_name_count));
        append(role_name);
        append(kPracticePrefix);
        append(item_name);
        append(kPracticeSuffix);
        const auto width_units = static_cast<int>(role_name.size() + item_name.size() + 12U);
        return renderer_.draw_box(
                   framebuffer,
                   150 - width_units * 4,
                   40,
                   static_cast<std::uint16_t>(width_units * 8 + 20),
                   27U) &&
            renderer_.draw_text(
                framebuffer, 160 - width_units * 4, 45, text, 0x0705U);
    }
    case PostBattleMessageKind::magic_level: {
        const auto magic_id = role_result.practice.magic_id;
        const auto magic_slot = role_result.practice.magic_slot;
        if (magic_id < 0 || static_cast<std::size_t>(magic_id) >= ranger_.magics.size() ||
            magic_slot < 0 ||
            static_cast<std::size_t>(magic_slot) >= model::role_word::magic_count) {
            return false;
        }
        const auto magic_name = terminated_name(
            std::span<const std::uint8_t>{
                ranger_.magics[static_cast<std::size_t>(magic_id)].bytes}.subspan(
                    model::magic_word::name_byte, model::magic_word::name_bytes));
        const auto level = static_cast<std::int32_t>(
            role.unsigned_word(
                model::role_word::magic_level_begin +
                static_cast<std::size_t>(magic_slot)) /
                100U +
            1U);
        append(magic_name);
        append(kMagicLevelPrefix);
        append(decimal_text(level, 2));
        append(kMagicLevelSuffix);
        const auto width_units = static_cast<int>(magic_name.size() + 13U);
        return renderer_.draw_box(
                   framebuffer,
                   150 - width_units * 4,
                   80,
                   static_cast<std::uint16_t>(width_units * 8 + 20),
                   27U) &&
            renderer_.draw_text(
                framebuffer, 160 - width_units * 4, 85, text, 0x0705U);
    }
    case PostBattleMessageKind::craft: {
        const auto product_id = role_result.craft.product_item_id;
        if (product_id < 0 ||
            static_cast<std::size_t>(product_id) >= ranger_.items.size()) {
            return false;
        }
        const auto product_name = terminated_name(
            std::span<const std::uint8_t>{
                ranger_.items[static_cast<std::size_t>(product_id)].bytes}.subspan(
                    2U * model::item_word::secondary_name_begin,
                    2U * model::item_word::secondary_name_count));
        append(role_name);
        append(kCraftedItemText);
        append(product_name);
        return renderer_.draw_box(framebuffer, 55, 30, 210U, 27U) &&
            renderer_.draw_text(framebuffer, 62, 35, text, 0x0705U);
    }
    }
    return false;
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
