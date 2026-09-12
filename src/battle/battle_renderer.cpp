#include "openlegend/battle/battle_renderer.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstdio>
#include <iterator>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/text/game_strings.hpp"

namespace openlegend::battle {
namespace {

using namespace openlegend::text::game_strings;
namespace palette_colors = render::legacy_color;
namespace text_colors = render::legacy_color::text;
constexpr std::array<std::uint16_t, 30> kLevelExperienceThresholds{
    0,     50,    150,   300,   500,   750,   1050,  1400,  1800,  2250,
    2750,  3850,  5050,  6350,  7750,  9250,  10850, 12550, 14350, 16750,
    18250, 21400, 24700, 28150, 31750, 35500, 39400, 43450, 47650, 52000};

[[nodiscard]] std::u8string decimal_text(
    const std::int32_t value,
    const int width = 0) {
    std::array<char, 16> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    std::u8string result;
    const auto count = static_cast<int>(converted.ptr - buffer.data());
    result.reserve(static_cast<std::size_t>(std::max(width, count)));
    for (int index = count; index < width; ++index) {
        result.push_back(u8' ');
    }
    for (const auto* cursor = buffer.data(); cursor != converted.ptr; ++cursor) {
        result.push_back(static_cast<char8_t>(*cursor));
    }
    return result;
}

[[nodiscard]] std::span<const std::uint8_t> zero_terminated_prefix(
    const std::span<const std::uint8_t> bytes) {
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0U});
    return bytes.first(static_cast<std::size_t>(std::distance(bytes.begin(), end)));
}

[[nodiscard]] std::span<const std::uint8_t> fixed_text(
    const std::span<const std::uint8_t> bytes,
    const std::size_t begin,
    const std::size_t maximum) {
    return zero_terminated_prefix(bytes.subspan(begin, maximum));
}

[[nodiscard]] std::optional<int> legacy_name_extent(
    const std::span<const std::uint8_t> name_bytes) noexcept {
    for (std::size_t offset = 1U; offset <= 6U && offset < name_bytes.size(); ++offset) {
        if (name_bytes[offset] == 0U) {
            return static_cast<int>(offset);
        }
    }
    return std::nullopt;
}

}  // namespace

BattleRenderer::BattleRenderer(
    const resource::DataRoot& data_root,
    const std::int16_t battlefield_id)
    : data_root_(data_root),
      battlefield_id_(battlefield_id),
      cloud_sprites_(resource::PackedArchive::open(
          data_root.path() / "CLOUD.IDX", data_root.path() / "CLOUD.GRP")),
      portraits_(resource::PackedArchive::open(
          data_root.path() / "HDGRP.IDX", data_root.path() / "HDGRP.GRP")),
      item_sprites_(resource::PackedArchive::open(
          data_root.path() / "MMAP.IDX", data_root.path() / "MMAP.GRP")) {
    cloud_frame_cache_.resize(cloud_sprites_.entry_count());
    portrait_frame_cache_.resize(portraits_.entry_count());
    item_frame_cache_.resize(item_sprites_.entry_count());
    if (battlefield_id_ < 0 || battlefield_id_ > 999) {
        error_ = "battlefield sprite id is outside filename range";
        return;
    }
    if (!cloud_sprites_.valid() || cloud_sprites_.entry_count() <= 5U) {
        error_ = cloud_sprites_.valid()
            ? "CLOUD archive is missing battle cursor frames"
            : cloud_sprites_.error();
        return;
    }
    if (!portraits_.valid()) {
        error_ = portraits_.error();
        return;
    }
    const auto final_item_sprite = render::legacy_item_sprite_index(
        static_cast<std::int16_t>(model::kItemCount - 1U));
    if (!item_sprites_.valid() || !final_item_sprite.has_value() ||
        item_sprites_.entry_count() <= *final_item_sprite) {
        error_ = item_sprites_.valid()
            ? "MMAP archive is missing item icon frames"
            : item_sprites_.error();
        return;
    }
    const auto palette_file = data_root.read("MMAP.COL");
    if (!palette_file) {
        error_ = palette_file.error;
        return;
    }
    const auto palette = resource::parse_vga_palette(palette_file.bytes);
    if (!palette) {
        error_ = palette.error;
        return;
    }
    palette_ = palette.palette;
    auto ascii = data_root.read("FONT3.E16");
    auto big5 = data_root.read("FONT3.C16");
    if (!ascii) {
        error_ = ascii.error;
        return;
    }
    if (!big5) {
        error_ = big5.error;
        return;
    }
    if (ascii.bytes.size() != 128U * 16U || big5.bytes.size() % 32U != 0U) {
        error_ = "battle renderer fonts have unexpected sizes";
        return;
    }
    ascii_font_ = std::move(ascii.bytes);
    big5_font_ = std::move(big5.bytes);
    big5_cache_.emplace(big5_font_);
    build_rgb4_lookup();
}

bool BattleRenderer::load_battlefield_assets() {
    if (!valid()) {
        return false;
    }
    if (battlefield_assets_loaded_) {
        return true;
    }

    std::array<char, 7> index_name{};
    std::array<char, 7> group_name{};
    static_cast<void>(std::snprintf(
        index_name.data(), index_name.size(), "WDX%03d", battlefield_id_));
    static_cast<void>(std::snprintf(
        group_name.data(), group_name.size(), "WMP%03d", battlefield_id_));
    const auto battlefield_index = data_root_.read(index_name.data());
    const auto battlefield_group = data_root_.read(group_name.data());
    if (!battlefield_index) {
        error_ = battlefield_index.error;
        return false;
    }
    if (!battlefield_group) {
        error_ = battlefield_group.error;
        return false;
    }
    if (battlefield_index.bytes.size() < 8U ||
        battlefield_index.bytes.size() % 4U != 0U ||
        compat::read_u32le(
            battlefield_index.bytes,
            battlefield_index.bytes.size() - 4U) != 0U) {
        error_ = "battlefield WDX does not have the original trailing zero word";
        return false;
    }
    battlefield_offsets_.clear();
    battlefield_offsets_.reserve(battlefield_index.bytes.size() / 4U - 1U);
    std::uint32_t previous{};
    for (std::size_t offset = 0U;
         offset + 4U < battlefield_index.bytes.size();
         offset += 4U) {
        const auto value = compat::read_u32le(battlefield_index.bytes, offset);
        if (value < previous || value > battlefield_group.bytes.size()) {
            error_ = "battlefield WDX contains an invalid cumulative offset";
            return false;
        }
        battlefield_offsets_.push_back(value);
        previous = value;
    }
    battlefield_group_ = battlefield_group.bytes;
    battlefield_assets_loaded_ = true;
    return true;
}

bool BattleRenderer::load_effect_assets() {
    if (!valid()) {
        return false;
    }
    if (effect_assets_loaded_) {
        return true;
    }
    effect_sprites_ = resource::PackedArchive::open(
        data_root_.path() / "EFT.IDX", data_root_.path() / "EFT.GRP");
    if (!effect_sprites_.valid()) {
        error_ = effect_sprites_.error();
        return false;
    }
    fight_frame_cache_.clear();
    effect_assets_loaded_ = true;
    return true;
}

bool BattleRenderer::load_battle_assets() {
    return load_battlefield_assets() && load_effect_assets();
}

void BattleRenderer::build_rgb4_lookup() noexcept {
    for (int red = 0; red < 16; ++red) {
        for (int green = 0; green < 16; ++green) {
            for (int blue = 0; blue < 16; ++blue) {
                auto best_distance = 30'000;
                std::uint8_t best_index{};
                for (std::size_t index = 0U; index < palette_.size(); ++index) {
                    const auto red_delta = red * 4 + 2 - palette_[index].red;
                    const auto green_delta = green * 4 + 2 - palette_[index].green;
                    const auto blue_delta = blue * 4 + 2 - palette_[index].blue;
                    const auto distance = red_delta * red_delta + green_delta * green_delta +
                        blue_delta * blue_delta;
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = static_cast<std::uint8_t>(index);
                    }
                }
                rgb4_lookup_[static_cast<std::size_t>(red * 256 + green * 16 + blue)] =
                    best_index;
            }
        }
    }
}

bool BattleRenderer::render(
    const BattleRenderPlan& plan,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid() || !battlefield_assets_loaded_ || !effect_assets_loaded_) {
        return false;
    }
    framebuffer.set_palette(palette_);
    for (const auto& command : plan.commands) {
        switch (command.kind) {
        case BattleRenderCommandKind::legacy_sprite:
            static_cast<void>(draw_fight_sprite(
                framebuffer,
                command.sprite_id,
                command.screen_x,
                command.screen_y));
            break;
        case BattleRenderCommandKind::cursor_overlay:
            static_cast<void>(draw_cursor_overlay(
                framebuffer,
                command.overlay_variant,
                command.screen_x,
                command.screen_y,
                command.style));
            break;
        case BattleRenderCommandKind::highlighted_sprite:
            static_cast<void>(draw_tinted_fight_sprite(
                framebuffer,
                command.sprite_id,
                command.screen_x,
                command.screen_y,
                static_cast<std::uint8_t>(command.style)));
            break;
        case BattleRenderCommandKind::damage_text:
            static_cast<void>(draw_damage_text(framebuffer, command));
            break;
        }
    }
    return true;
}

bool BattleRenderer::render_status_panel(
    const BattleStatusPanelPlan& plan,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid() || !draw_box(
            framebuffer,
            plan.panel_x,
            plan.panel_y,
            static_cast<std::uint16_t>(plan.panel_width),
            static_cast<std::uint16_t>(plan.panel_height)) ||
        !draw_portrait(
            framebuffer,
            plan.portrait_id,
            plan.portrait_x,
            plan.portrait_y)) {
        return false;
    }
    if (plan.name_x.has_value() &&
        !draw_text_big5(
            framebuffer,
            *plan.name_x,
            plan.name_y,
            text::Big5TextView{zero_terminated_prefix(plan.name_bytes)},
            text_colors::notice)) {
        return false;
    }
    const auto power = decimal_text(plan.physical_power, 3);
    const auto hp = decimal_text(plan.hp, 3);
    const auto maximum_hp = decimal_text(plan.maximum_hp, 3);
    const auto mp = decimal_text(plan.mp, 3);
    const auto maximum_mp = decimal_text(plan.maximum_mp, 3);
    const auto x = [offset = plan.side_offset](const int value) {
        return value - offset;
    };
    return draw_text_utf8(framebuffer, x(225), 101, kPowerLabel, text_colors::menu_normal) &&
        draw_text_utf8(framebuffer, x(262), 101, power, text_colors::notice) &&
        draw_text_utf8(framebuffer, x(285), 101, kSlash, text_colors::selected) &&
        draw_text_utf8(framebuffer, x(292), 101, kHundred, text_colors::menu_normal) &&
        draw_text_utf8(framebuffer, x(225), 118, kLifeLabel, text_colors::menu_normal) &&
        draw_text_utf8(
            framebuffer,
            x(262),
            118,
            hp,
            plan.hurt_color) &&
        draw_text_utf8(framebuffer, x(285), 118, kSlash, text_colors::selected) &&
        draw_text_utf8(
            framebuffer,
            x(292),
            118,
            maximum_hp,
            plan.poison_color) &&
        draw_text_utf8(framebuffer, x(225), 135, kMpLabel, text_colors::menu_normal) &&
        draw_text_utf8(
            framebuffer,
            x(262),
            135,
            mp,
            plan.mp_color) &&
        draw_text_utf8(
            framebuffer,
            x(285),
            135,
            kSlash,
            plan.mp_color) &&
        draw_text_utf8(
            framebuffer,
            x(292),
            135,
            maximum_mp,
            plan.mp_color);
}

bool BattleRenderer::render_character_selection(
    const model::RangerState& ranger,
    const std::size_t cursor,
    const PartySelectionKind kind,
    render::IndexedFramebuffer& framebuffer,
    const std::optional<std::int16_t> item_id) {
    std::size_t party_count = model::kTeamMemberCount;
    for (std::size_t slot = 1U; slot < model::kTeamMemberCount; ++slot) {
        if (ranger.header.team_member(slot).value <= 0) {
            party_count = slot;
            break;
        }
    }
    std::u8string_view title;
    std::u8string_view subtitle;
    std::uint16_t list_width = 62U;
    switch (kind) {
    case PartySelectionKind::medicine_target:
        title = kMedicineTargetTitle;
        subtitle = kLifePointsLabel;
        list_width = 118U;
        break;
    case PartySelectionKind::detoxification_target:
        title = kDetoxificationTargetTitle;
        subtitle = kPoisonLevelLabel;
        list_width = 86U;
        break;
    case PartySelectionKind::status: title = kStatusSelectionTitle; break;
    case PartySelectionKind::equipment_target: title = kEquipmentTargetTitle; break;
    case PartySelectionKind::practice_target: title = kPracticeTargetTitle; break;
    case PartySelectionKind::item_target: title = kItemTargetTitle; break;
    case PartySelectionKind::leave_party: title = kLeavePartyTitle; break;
    }
    const auto has_details = !subtitle.empty();
    const auto has_item_title =
        kind == PartySelectionKind::equipment_target ||
        kind == PartySelectionKind::practice_target ||
        kind == PartySelectionKind::item_target;
    std::span<const std::uint8_t> item_name;
    if (has_item_title) {
        if (!item_id.has_value() || *item_id < 0 ||
            static_cast<std::size_t>(*item_id) >= ranger.items.size()) {
            return false;
        }
        item_name = fixed_text(
            std::span<const std::uint8_t>{ranger.items[static_cast<std::size_t>(*item_id)].bytes},
            2U * model::item_word::secondary_name_begin,
            2U * model::item_word::secondary_name_count);
    }
    const auto encoded_title = text::encode_big5(title);
    if (!encoded_title.has_value()) {
        return false;
    }
    const auto title_width = static_cast<std::uint16_t>(
        8U * (encoded_title->size() + item_name.size()) +
        (has_item_title ? 28U : 12U));
    const auto list_height = static_cast<std::uint16_t>(
        20U * party_count + (has_details ? 30U : 10U));
    if (!valid() || party_count == 0U || cursor >= party_count ||
        !draw_box(framebuffer, 70, 18, title_width, 26U) ||
        !draw_text_utf8(framebuffer, 75, 22, title, text_colors::notice) ||
        (has_item_title &&
         !draw_text_big5(
             framebuffer, 145, 22, text::Big5TextView{item_name}, text_colors::negative_value)) ||
        !draw_box(framebuffer, 70, 45, list_width, list_height) ||
        (has_details && !draw_text_utf8(framebuffer, 75, 52, subtitle, text_colors::notice))) {
        return false;
    }
    for (std::size_t slot = 0U; slot < party_count; ++slot) {
        const auto role_id = ranger.header.team_member(slot).value;
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger.roles.size()) {
            return false;
        }
        const auto& role = ranger.roles[static_cast<std::size_t>(role_id)];
        const auto name_storage = std::span<const std::uint8_t>{role.bytes}.subspan(
            model::role_word::name_byte, model::role_word::name_bytes);
        const auto selected = slot == cursor;
        const auto color = selected ? text_colors::selected : text_colors::menu_normal;
        const auto y = (has_details ? 72 : 52) + 20 * static_cast<int>(slot);
        const auto name_extent = legacy_name_extent(name_storage);
        if (name_extent.has_value() &&
            !draw_text_big5(
                framebuffer,
                99 - 4 * *name_extent,
                y,
                text::Big5TextView{zero_terminated_prefix(name_storage)},
                color)) {
            return false;
        }
        if (kind == PartySelectionKind::medicine_target) {
            const auto hurt = role.word(model::role_word::hurt);
            const auto hp_color = selected ? text_colors::selected
                : hurt > 66 ? text_colors::severe_injury
                : hurt > 33 ? text_colors::moderate_injury
                            : text_colors::notice;
            if (!draw_text_utf8(
                    framebuffer,
                    127,
                    y,
                    decimal_text(role.word(model::role_word::hp), 3),
                    hp_color) ||
                !draw_text_utf8(
                    framebuffer, 149, y, kSlash, selected ? text_colors::selected : text_colors::hp_separator) ||
                !draw_text_utf8(
                    framebuffer,
                    155,
                    y,
                    decimal_text(role.word(model::role_word::maximum_hp), 3),
                    color)) {
                return false;
            }
        } else if (kind == PartySelectionKind::detoxification_target &&
                   !draw_text_utf8(
                       framebuffer,
                       127,
                       y,
                       decimal_text(role.word(model::role_word::poison), 3),
                       color)) {
            return false;
        }
    }
    return true;
}

bool BattleRenderer::render_character_status_selection(
    const model::RangerState& ranger,
    const std::size_t cursor,
    render::IndexedFramebuffer& framebuffer) {
    return render_character_selection(
        ranger, cursor, PartySelectionKind::status, framebuffer);
}

bool BattleRenderer::render_party_ability_selection(
    const model::RangerState& ranger,
    const std::span<const std::uint8_t> party_slots,
    const std::size_t cursor,
    const PartyAbilityKind kind,
    render::IndexedFramebuffer& framebuffer) {
    const auto title = kind == PartyAbilityKind::medicine
        ? kMedicineUserTitle
        : kDetoxificationUserTitle;
    const auto subtitle = kind == PartyAbilityKind::medicine
        ? kMedicineAbilityLabel
        : kDetoxificationAbilityLabel;
    const auto ability_word = kind == PartyAbilityKind::medicine
        ? model::role_word::medicine
        : model::role_word::detoxification;
    if (!valid() || party_slots.empty() || cursor >= party_slots.size() ||
        !draw_box(framebuffer, 70, 18, 108U, 26U) ||
        !draw_text_utf8(framebuffer, 75, 22, title, text_colors::notice) ||
        !draw_box(
            framebuffer,
            70,
            45,
            90U,
            static_cast<std::uint16_t>(20U * party_slots.size() + 30U)) ||
        !draw_text_utf8(framebuffer, 75, 52, subtitle, text_colors::notice)) {
        return false;
    }
    for (std::size_t index = 0U; index < party_slots.size(); ++index) {
        const auto party_slot = party_slots[index];
        if (party_slot >= model::kTeamMemberCount) {
            return false;
        }
        const auto role_id = ranger.header.team_member(party_slot).value;
        if (role_id < 0 || static_cast<std::size_t>(role_id) >= ranger.roles.size()) {
            return false;
        }
        const auto& role = ranger.roles[static_cast<std::size_t>(role_id)];
        const auto name_storage = std::span<const std::uint8_t>{role.bytes}.subspan(
            model::role_word::name_byte, model::role_word::name_bytes);
        const auto color = index == cursor
            ? text_colors::selected
            : text_colors::menu_normal;
        const auto y = 72 + 20 * static_cast<int>(index);
        const auto name_extent = legacy_name_extent(name_storage);
        if ((name_extent.has_value() &&
             !draw_text_big5(
                 framebuffer,
                 99 - 4 * *name_extent,
                 y,
                 text::Big5TextView{zero_terminated_prefix(name_storage)},
                 color)) ||
            !draw_text_utf8(
                framebuffer, 130, y, decimal_text(role.word(ability_word), 2), color)) {
            return false;
        }
    }
    return true;
}

bool BattleRenderer::render_party_action_notice(
    const PartyAbilityKind kind,
    const std::optional<std::int32_t> amount,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid()) {
        return false;
    }
    if (!amount.has_value()) {
        const auto message = kind == PartyAbilityKind::medicine
            ? kNoMedicineUser
            : kNoDetoxificationUser;
        return draw_box(framebuffer, 70, 18, 154U, 26U) &&
            draw_text_utf8(framebuffer, 75, 22, message, text_colors::notice);
    }
    const auto label = kind == PartyAbilityKind::medicine
        ? kMedicineResultLabel
        : kDetoxificationResultLabel;
    return draw_box(framebuffer, 112, 47, 96U, 26U) &&
        draw_text_utf8(framebuffer, 117, 51, label, text_colors::selected) &&
        draw_text_utf8(framebuffer, 181, 51, decimal_text(*amount, 3), text_colors::notice);
}

bool BattleRenderer::render_character_status(
    const model::RangerState& ranger,
    const std::int16_t role_id,
    const std::uint8_t page,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid() || role_id < 0 || static_cast<std::size_t>(role_id) >= ranger.roles.size() ||
        page > 1U) {
        return false;
    }
    const auto& role = ranger.roles[static_cast<std::size_t>(role_id)];
    const auto name_storage = std::span<const std::uint8_t>{role.bytes}.subspan(
        model::role_word::name_byte, model::role_word::name_bytes);
    if (!draw_box(framebuffer, 55, 0, 210U, 200U) ||
        !draw_portrait(
            framebuffer,
            role.word(model::role_word::head_id),
            78,
            68)) {
        return false;
    }
    const auto name_extent = legacy_name_extent(name_storage);
    if (name_extent.has_value() &&
        !draw_text_big5(
            framebuffer,
            104 - 4 * *name_extent,
            70,
            text::Big5TextView{zero_terminated_prefix(name_storage)},
            text_colors::selected)) {
        return false;
    }

    const auto draw_number = [this, &framebuffer](
                                 const int x,
                                 const int y,
                                 const std::int32_t value,
                                 const int width,
                                 const render::TextColors color = text_colors::notice) {
        return draw_text_utf8(framebuffer, x, y, decimal_text(value, width), color);
    };
    if (page == 0U) {
        const auto hurt = role.word(model::role_word::hurt);
        const auto hurt_color = hurt > 66 ? text_colors::severe_injury
            : hurt > 33 ? text_colors::moderate_injury
                        : text_colors::notice;
        const auto poison = role.word(model::role_word::poison);
        const auto poison_color = poison == 0 ? text_colors::menu_normal
            : poison >= 50 ? text_colors::severe_poison
                           : text_colors::poison;
        auto mp_color = poison_color;
        switch (role.word(model::role_word::mp_type)) {
        case 0: mp_color = text_colors::yin_mp; break;
        case 1: mp_color = text_colors::notice; break;
        case 2: mp_color = text_colors::selected; break;
        default: break;
        }
        if (!draw_text_utf8(framebuffer, 60, 90, kLevelLabel, text_colors::menu_normal) ||
            !draw_number(100, 90, role.word(model::role_word::level), 3) ||
            !draw_text_utf8(framebuffer, 60, 107, kLifeLabel, text_colors::menu_normal) ||
            !draw_number(97, 107, role.word(model::role_word::hp), 3, hurt_color) ||
            !draw_text_utf8(framebuffer, 120, 107, kSlash, text_colors::selected) ||
            !draw_number(
                127,
                107,
                role.word(model::role_word::maximum_hp),
                3,
                poison_color) ||
            !draw_text_utf8(framebuffer, 60, 124, kMpLabel, text_colors::menu_normal) ||
            !draw_number(97, 124, role.word(model::role_word::mp), 3, mp_color) ||
            !draw_text_utf8(framebuffer, 120, 124, kSlash, mp_color) ||
            !draw_number(127, 124, role.word(model::role_word::maximum_mp), 3, mp_color) ||
            !draw_text_utf8(framebuffer, 60, 141, kPowerLabel, text_colors::menu_normal) ||
            !draw_number(97, 141, role.word(model::role_word::physical_power), 3) ||
            !draw_text_utf8(framebuffer, 120, 141, kSlash, text_colors::selected) ||
            !draw_text_utf8(framebuffer, 127, 141, kHundred, text_colors::menu_normal) ||
            !draw_text_utf8(framebuffer, 60, 158, kExperienceLabel, text_colors::menu_normal) ||
            !draw_number(97, 158, role.unsigned_word(model::role_word::experience), 6) ||
            !draw_text_utf8(framebuffer, 60, 175, kUpgradeLabel, text_colors::menu_normal)) {
            return false;
        }
        const auto level = role.word(model::role_word::level);
        if (level >= 30) {
            if (!draw_text_utf8(framebuffer, 97, 175, kMaximumLevel, text_colors::notice)) {
                return false;
            }
        } else if (level < 0 ||
                   !draw_number(
                       97,
                       175,
                       kLevelExperienceThresholds[static_cast<std::size_t>(level)],
                       6)) {
            return false;
        }

        const auto effective_with_equipment = [&](
                                                const std::size_t role_field,
                                                const std::size_t item_field) {
            auto value = static_cast<std::int32_t>(role.word(role_field));
            for (std::size_t slot = 0U; slot < model::role_word::equipment_count; ++slot) {
                const auto item_id = role.word(model::role_word::equipment_begin + slot);
                if (item_id >= 0 && static_cast<std::size_t>(item_id) < ranger.items.size()) {
                    value += ranger.items[static_cast<std::size_t>(item_id)].word(item_field);
                }
            }
            return value;
        };
        struct RightField {
            std::u8string_view label;
            std::int32_t value;
        };
        const std::array<RightField, 11> fields{{
            {kAttackLabel, effective_with_equipment(
                               model::role_word::attack, model::item_word::add_attack)},
            {kDefenceLabel, effective_with_equipment(
                                model::role_word::defence, model::item_word::add_defence)},
            {kSpeedLabel, effective_with_equipment(
                              model::role_word::speed, model::item_word::add_speed)},
            {kMedicineLabel, role.word(model::role_word::medicine)},
            {kUsePoisonLabel, role.word(model::role_word::use_poison)},
            {kDetoxLabel, role.word(model::role_word::detoxification)},
            {kFistLabel, role.word(model::role_word::fist)},
            {kSwordLabel, role.word(model::role_word::sword)},
            {kKnifeLabel, role.word(model::role_word::knife)},
            {kUnusualLabel, role.word(model::role_word::unusual)},
            {kHiddenLabel, role.word(model::role_word::hidden_weapon)},
        }};
        for (std::size_t index = 0U; index < fields.size(); ++index) {
            const auto y = 5 + 17 * static_cast<int>(index);
            if (!draw_text_utf8(framebuffer, 160, y, fields[index].label, text_colors::selected) ||
                !draw_number(230, y, fields[index].value, 3)) {
                return false;
            }
        }
        return true;
    }

    if (!draw_text_utf8(framebuffer, 60, 90, kEquipmentLabel, text_colors::menu_normal)) {
        return false;
    }
    for (std::size_t slot = 0U; slot < model::role_word::equipment_count; ++slot) {
        const auto item_id = role.word(model::role_word::equipment_begin + slot);
        if (item_id >= 0 && static_cast<std::size_t>(item_id) < ranger.items.size()) {
            const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
            if (!draw_text_big5(
                    framebuffer,
                    60,
                    107 + 17 * static_cast<int>(slot),
                    text::Big5TextView{fixed_text(
                        item.bytes,
                        model::item_word::secondary_name_begin * 2U,
                        model::item_word::secondary_name_count * 2U)},
                    text_colors::notice)) {
                return false;
            }
        }
    }
    if (!draw_text_utf8(framebuffer, 60, 141, kPracticeLabel, text_colors::menu_normal)) {
        return false;
    }
    const auto practice_item_id = role.word(model::role_word::practice_item);
    if (practice_item_id >= 0 &&
        static_cast<std::size_t>(practice_item_id) < ranger.items.size()) {
        const auto& item = ranger.items[static_cast<std::size_t>(practice_item_id)];
        if (!draw_text_big5(
                framebuffer,
                60,
                158,
                text::Big5TextView{fixed_text(
                    item.bytes,
                    model::item_word::secondary_name_begin * 2U,
                    model::item_word::secondary_name_count * 2U)},
                text_colors::notice) ||
            !draw_number(
                60,
                175,
                role.unsigned_word(model::role_word::item_experience),
                5) ||
            !draw_text_utf8(framebuffer, 100, 175, kSlash, text_colors::selected)) {
            return false;
        }
        const auto experience_factor =
            7 - role.word(model::role_word::iq) / 15;
        const auto practice_magic = item.word(model::item_word::magic_id);
        std::int32_t required_experience{};
        bool maximum_magic_level{};
        if (practice_magic == -1) {
            required_experience = 2 * experience_factor *
                static_cast<std::int32_t>(item.word(model::item_word::need_experience));
        } else {
            std::uint16_t level_index{};
            for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
                if (role.word(model::role_word::magic_id_begin + slot) == practice_magic) {
                    level_index = static_cast<std::uint16_t>(
                        role.unsigned_word(model::role_word::magic_level_begin + slot) / 100U);
                    break;
                }
            }
            if (level_index >= 9U) {
                maximum_magic_level = true;
            } else {
                required_experience = experience_factor *
                    static_cast<std::int32_t>(level_index + 1U) *
                    static_cast<std::int32_t>(item.word(model::item_word::need_experience));
            }
        }
        if (maximum_magic_level) {
            if (!draw_text_utf8(framebuffer, 107, 175, kMaximumPractice, text_colors::menu_normal)) {
                return false;
            }
        } else if (!draw_number(108, 175, required_experience, 5, text_colors::menu_normal)) {
            return false;
        }
    }
    if (!draw_text_utf8(framebuffer, 160, 5, kMagicLabel, text_colors::menu_normal)) {
        return false;
    }
    for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
        const auto magic_id = role.word(model::role_word::magic_id_begin + slot);
        if (magic_id <= 0 || static_cast<std::size_t>(magic_id) >= ranger.magics.size()) {
            continue;
        }
        const auto y = 22 + 17 * static_cast<int>(slot);
        if (!draw_text_big5(
                framebuffer,
                160,
                y,
                text::Big5TextView{fixed_text(
                    ranger.magics[static_cast<std::size_t>(magic_id)].bytes,
                    model::magic_word::name_byte,
                    model::magic_word::name_bytes)},
                text_colors::notice) ||
            !draw_number(
                242,
                y,
                static_cast<std::int32_t>(
                    role.unsigned_word(model::role_word::magic_level_begin + slot) / 100U + 1U),
                2,
                text_colors::selected)) {
            return false;
        }
    }
    return true;
}

bool BattleRenderer::render_item_effect(
    const model::RangerState& ranger,
    const std::int16_t item_id,
    const BattleItemEffectResult& effect,
    render::IndexedFramebuffer& framebuffer) {
    if (!valid() || item_id < 0 || static_cast<std::size_t>(item_id) >= ranger.items.size() ||
        !draw_box(
            framebuffer,
            effect.panel_x,
            effect.panel_y,
            static_cast<std::uint16_t>(effect.panel_width),
            static_cast<std::uint16_t>(effect.panel_height))) {
        return false;
    }
    const auto& item = ranger.items[static_cast<std::size_t>(item_id)];
    const auto name = fixed_text(
        item.bytes,
        2U * model::item_word::secondary_name_begin,
        2U * model::item_word::secondary_name_count);
    text::GameText header;
    header.append_utf8(kUseItemPrefix);
    header.append_legacy(text::Big5TextView{name});
    if (!draw_text_mixed(framebuffer, 75, 25, header, text_colors::selected)) {
        return false;
    }
    std::int16_t visible_row = 0;
    for (std::size_t index = 0U; index < effect.deltas.size(); ++index) {
        const auto delta = effect.deltas[index];
        if (delta == 0) {
            continue;
        }
        const auto y = 45 + 18 * visible_row;
        if (!draw_text_utf8(
                framebuffer,
                75,
                y,
                kItemEffectLabels[index],
                text_colors::notice)) {
            return false;
        }
        if (index == 4U) {
            if (!draw_text_utf8(framebuffer, 155, y, kItemMpTypeChanged, text_colors::notice)) {
                return false;
            }
        } else {
            if (!draw_text_utf8(
                    framebuffer,
                    155,
                    y,
                    delta > 0 ? kItemIncrease : kItemDecrease,
                    delta > 0 ? text_colors::notice : text_colors::negative_value)) {
                return false;
            }
            const auto magnitude = delta < 0
                ? -static_cast<std::int32_t>(delta)
                : static_cast<std::int32_t>(delta);
            if (!draw_text_utf8(
                    framebuffer, 187, y, decimal_text(magnitude, 3), text_colors::notice)) {
                return false;
            }
        }
        ++visible_row;
    }
    return true;
}

bool BattleRenderer::draw_box(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height) const noexcept {
    if (width <= 10U || height <= 10U) {
        return false;
    }
    const auto blend = [this, &framebuffer](
                           const int left,
                           const int top,
                           const int rectangle_width,
                           const int rectangle_height) {
        const auto begin_x = std::max(left, 0);
        const auto end_x = std::min(
            left + rectangle_width, framebuffer.coordinate_width());
        const auto begin_y = std::max(top, 0);
        const auto end_y = std::min(
            top + rectangle_height, framebuffer.coordinate_height());
        if (begin_x >= end_x || begin_y >= end_y) {
            return;
        }
        static_cast<void>(framebuffer.transform_rectangle(
            begin_x,
            begin_y,
            end_x - begin_x,
            end_y - begin_y,
            [this](std::uint8_t& destination) {
                destination = blend_pixel(
                    palette_colors::black, destination, 4);
            }));
    };
    const auto w = static_cast<int>(width);
    const auto h = static_cast<int>(height);
    blend(x + 5, y, w - 10, 1);
    blend(x + 4, y + 1, w - 8, 1);
    blend(x + 3, y + 2, w - 6, 1);
    blend(x + 2, y + 3, w - 4, 1);
    blend(x + 1, y + 4, w - 2, 1);
    blend(x, y + 5, w, h - 10);
    blend(x + 1, y + h - 5, w - 2, 1);
    blend(x + 2, y + h - 4, w - 4, 1);
    blend(x + 3, y + h - 3, w - 6, 1);
    blend(x + 4, y + h - 2, w - 8, 1);
    blend(x + 5, y + h - 1, w - 10, 1);

    const auto fill = [&framebuffer](
                          const int left,
                          const int top,
                          const int rectangle_width,
                          const int rectangle_height) {
        return framebuffer.fill_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width),
            static_cast<std::uint16_t>(rectangle_height),
            palette_colors::panel_outline);
    };
    return fill(x + 5, y + 1, w - 10, 1) &&
        fill(x + 4, y + 2, 1, 2) && fill(x + w - 5, y + 2, 1, 2) &&
        fill(x + 2, y + 4, 2, 1) && fill(x + w - 4, y + 4, 2, 1) &&
        fill(x + 1, y + 5, 1, h - 10) && fill(x + w - 2, y + 5, 1, h - 10) &&
        fill(x + 2, y + h - 5, 2, 1) && fill(x + w - 4, y + h - 5, 2, 1) &&
        fill(x + 4, y + h - 4, 1, 2) && fill(x + w - 5, y + h - 4, 1, 2) &&
        fill(x + 5, y + h - 2, w - 10, 1);
}

bool BattleRenderer::draw_text_utf8(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::u8string_view text,
    const render::TextColors colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::draw_text_utf8(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        colors);
}

bool BattleRenderer::draw_text_mixed(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::GameText& text,
    const render::TextColors colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::draw_text_mixed(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        colors);
}

bool BattleRenderer::draw_text_big5(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::Big5TextView text,
    const render::TextColors colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::draw_text_big5(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        colors);
}

bool BattleRenderer::draw_portrait(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t portrait_id,
    const int x,
    const int y) const {
    if (portrait_id < 0 || static_cast<std::size_t>(portrait_id) >= portraits_.entry_count()) {
        return false;
    }
    const auto* frame = archive_frame(
        portraits_, portrait_frame_cache_, static_cast<std::size_t>(portrait_id));
    if (frame == nullptr) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, *frame, x, y);
    return true;
}

bool BattleRenderer::draw_item_icon(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t item_id,
    const int x,
    const int y) const {
    const auto sprite_index = render::legacy_item_sprite_index(item_id);
    if (!sprite_index.has_value() || *sprite_index >= item_sprites_.entry_count()) {
        return false;
    }
    const auto* frame = archive_frame(item_sprites_, item_frame_cache_, *sprite_index);
    if (frame == nullptr) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, *frame, x, y);
    return true;
}

bool BattleRenderer::load_fight_package(const std::int16_t fight_head_id) {
    if (fight_head_id < 0 || fight_head_id > 999) {
        return false;
    }
    std::array<char, 13> index_name{};
    std::array<char, 13> group_name{};
    static_cast<void>(std::snprintf(
        index_name.data(), index_name.size(), "FIGHT%03d.IDX", fight_head_id));
    static_cast<void>(std::snprintf(
        group_name.data(), group_name.size(), "FIGHT%03d.GRP", fight_head_id));
    auto archive = resource::PackedArchive::open(
        data_root_.path() / index_name.data(),
        data_root_.path() / group_name.data());
    if (!archive.valid()) {
        return false;
    }
    fight_sprites_ = std::move(archive);
    fight_frame_cache_.clear();
    return true;
}

std::span<const std::uint8_t> BattleRenderer::fight_entry(
    const std::int32_t legacy_id) const {
    if (legacy_id < 0 || legacy_id > 0x7FFE) {
        return {};
    }
    const auto pointer_index = static_cast<std::size_t>(legacy_id / 2);
    if (pointer_index >= static_cast<std::size_t>(kBattleFightPointerBase) &&
        fight_sprites_.has_value()) {
        const auto local_index = pointer_index - static_cast<std::size_t>(kBattleFightPointerBase);
        if (local_index < fight_sprites_->entry_count()) {
            return fight_sprites_->entry(local_index);
        }
    }
    if (pointer_index >= static_cast<std::size_t>(kBattleEffectPointerBase)) {
        const auto local_index = pointer_index - static_cast<std::size_t>(kBattleEffectPointerBase);
        if (local_index < effect_sprites_.entry_count()) {
            return effect_sprites_.entry(local_index);
        }
    }
    if (pointer_index <= battlefield_offsets_.size()) {
        const auto begin = pointer_index == 0U
            ? 0U
            : static_cast<std::size_t>(battlefield_offsets_[pointer_index - 1U]);
        const auto end = pointer_index < battlefield_offsets_.size()
            ? static_cast<std::size_t>(battlefield_offsets_[pointer_index])
            : battlefield_group_.size();
        if (end >= begin && end <= battlefield_group_.size()) {
            return std::span<const std::uint8_t>{battlefield_group_}.subspan(
                begin, end - begin);
        }
    }
    return {};
}

const resource::SpriteFrameView* BattleRenderer::archive_frame(
    const resource::PackedArchive& archive,
    std::vector<std::optional<resource::SpriteFrameView>>& cache,
    const std::size_t index) const {
    if (index >= archive.entry_count() || index >= cache.size()) {
        return nullptr;
    }
    if (!cache[index].has_value()) {
        cache[index].emplace(resource::SpriteFrameView::parse(archive.entry(index)));
    }
    return cache[index]->valid() ? &*cache[index] : nullptr;
}

const resource::SpriteFrameView* BattleRenderer::fight_frame(
    const std::int32_t legacy_id) const {
    const auto found = fight_frame_cache_.find(legacy_id);
    if (found != fight_frame_cache_.end()) {
        return found->second.valid() ? &found->second : nullptr;
    }
    const auto [entry, inserted] = fight_frame_cache_.emplace(
        legacy_id, resource::SpriteFrameView::parse(fight_entry(legacy_id)));
    static_cast<void>(inserted);
    return entry->second.valid() ? &entry->second : nullptr;
}

bool BattleRenderer::draw_fight_sprite(
    render::IndexedFramebuffer& framebuffer,
    const std::int32_t legacy_id,
    const int anchor_x,
    const int anchor_y) const {
    const auto* frame = fight_frame(legacy_id);
    if (frame == nullptr) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, *frame, anchor_x, anchor_y);
    return true;
}

bool BattleRenderer::draw_tinted_fight_sprite(
    render::IndexedFramebuffer& framebuffer,
    const std::int32_t legacy_id,
    const int anchor_x,
    const int anchor_y,
    const render::PaletteIndex color) const {
    const auto* frame = fight_frame(legacy_id);
    if (frame == nullptr) {
        return false;
    }
    const auto left = anchor_x - static_cast<int>(frame->x_offset());
    const auto top = anchor_y - static_cast<int>(frame->y_offset());
    for (std::size_t row_index = 0U; row_index < frame->rows().size(); ++row_index) {
        const auto y = top + static_cast<int>(row_index);
        auto x = left;
        for (const auto& run : frame->rows()[row_index].runs) {
            x += static_cast<int>(run.skip);
            for ([[maybe_unused]] const auto pixel : run.pixels) {
                if (y >= 0 && y < framebuffer.coordinate_height() &&
                    x >= 0 && x < framebuffer.coordinate_width()) {
                    framebuffer.draw_pixel(x, y, color);
                }
                ++x;
            }
        }
    }
    return true;
}

bool BattleRenderer::draw_cursor_overlay(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t variant,
    const int anchor_x,
    const int anchor_y,
    const std::int16_t source_weight) const {
    if (variant < 0 || variant > 1 || source_weight < 0 || source_weight > 8) {
        return false;
    }
    const auto* frame = archive_frame(
        cloud_sprites_, cloud_frame_cache_, static_cast<std::size_t>(variant) + 4U);
    if (frame == nullptr) {
        return false;
    }
    const auto left = anchor_x - static_cast<int>(frame->x_offset());
    const auto top = anchor_y - static_cast<int>(frame->y_offset());
    for (std::size_t row_index = 0U; row_index < frame->rows().size(); ++row_index) {
        const auto y = top + static_cast<int>(row_index);
        auto x = left;
        for (const auto& run : frame->rows()[row_index].runs) {
            x += static_cast<int>(run.skip);
            for (const auto source : run.pixels) {
                if (y >= 0 && y < framebuffer.coordinate_height() &&
                    x >= 0 && x < framebuffer.coordinate_width()) {
                    static_cast<void>(framebuffer.transform_rectangle(
                        x,
                        y,
                        1,
                        1,
                        [this, source, source_weight](std::uint8_t& destination) {
                            destination = blend_pixel(
                                source, destination, source_weight);
                        }));
                }
                ++x;
            }
        }
    }
    return true;
}

bool BattleRenderer::draw_damage_text(
    render::IndexedFramebuffer& framebuffer,
    const BattleRenderCommand& command) {
    std::u8string text;
    text.push_back(command.overlay_variant < 0 ? u8'-' : u8'+');
    text.append(decimal_text(command.value, 3));
    return draw_text_utf8(
        framebuffer,
        command.screen_x,
        command.screen_y,
        text,
        render::TextColors::from_legacy_packed(
            std::bit_cast<std::uint16_t>(command.style)));
}

std::uint8_t BattleRenderer::blend_pixel(
    const std::uint8_t source,
    const std::uint8_t destination,
    const std::int16_t source_weight) const noexcept {
    const auto destination_weight = 8 - source_weight;
    const auto weighted = [source_weight, destination_weight](
                              const std::uint8_t source_component,
                              const std::uint8_t destination_component) {
        return static_cast<std::uint8_t>(
            source_weight * source_component / 32 +
            destination_weight * destination_component / 32);
    };
    const auto red = weighted(palette_[source].red, palette_[destination].red);
    const auto green = weighted(palette_[source].green, palette_[destination].green);
    const auto blue = weighted(palette_[source].blue, palette_[destination].blue);
    return rgb4_lookup_[static_cast<std::size_t>(red) * 256U +
                        static_cast<std::size_t>(green) * 16U + blue];
}

}  // namespace openlegend::battle
