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

namespace openlegend::battle {
namespace {

constexpr std::array<std::uint8_t, 5> kPowerLabel{0xCAU, 0x5EU, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 5> kLifeLabel{0xA5U, 0xCDU, 0xA9U, 0x52U, 0x20U};
constexpr std::array<std::uint8_t, 5> kMpLabel{0xA4U, 0xBAU, 0xA4U, 0x4FU, 0x20U};
constexpr std::array<std::uint8_t, 1> kSlash{'/'};
constexpr std::array<std::uint8_t, 3> kHundred{'1', '0', '0'};

[[nodiscard]] std::vector<std::uint8_t> decimal_text(
    const std::int16_t value,
    const int width = 0) {
    std::array<char, 16> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    std::vector<std::uint8_t> result;
    const auto count = static_cast<int>(converted.ptr - buffer.data());
    result.reserve(static_cast<std::size_t>(std::max(width, count)));
    for (int index = count; index < width; ++index) {
        result.push_back(static_cast<std::uint8_t>(' '));
    }
    for (const auto* cursor = buffer.data(); cursor != converted.ptr; ++cursor) {
        result.push_back(static_cast<std::uint8_t>(*cursor));
    }
    return result;
}

[[nodiscard]] std::span<const std::uint8_t> zero_terminated_prefix(
    const std::span<const std::uint8_t> bytes) {
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0U});
    return bytes.first(static_cast<std::size_t>(std::distance(bytes.begin(), end)));
}

}  // namespace

BattleRenderer::BattleRenderer(
    const resource::DataRoot& data_root,
    const std::int16_t battlefield_id)
    : data_root_(data_root),
      effect_sprites_(resource::PackedArchive::open(
          data_root.path() / "EFT.IDX", data_root.path() / "EFT.GRP")),
      cloud_sprites_(resource::PackedArchive::open(
          data_root.path() / "CLOUD.IDX", data_root.path() / "CLOUD.GRP")),
      portraits_(resource::PackedArchive::open(
          data_root.path() / "HDGRP.IDX", data_root.path() / "HDGRP.GRP")) {
    if (battlefield_id < 0 || battlefield_id > 999) {
        error_ = "battlefield sprite id is outside filename range";
        return;
    }
    std::array<char, 7> index_name{};
    std::array<char, 7> group_name{};
    static_cast<void>(std::snprintf(
        index_name.data(), index_name.size(), "WDX%03d", battlefield_id));
    static_cast<void>(std::snprintf(
        group_name.data(), group_name.size(), "WMP%03d", battlefield_id));
    const auto battlefield_index = data_root.read(index_name.data());
    const auto battlefield_group = data_root.read(group_name.data());
    if (!battlefield_index) {
        error_ = battlefield_index.error;
        return;
    }
    if (!battlefield_group) {
        error_ = battlefield_group.error;
        return;
    }
    if (battlefield_index.bytes.size() < 8U ||
        battlefield_index.bytes.size() % 4U != 0U ||
        compat::read_u32le(
            battlefield_index.bytes,
            battlefield_index.bytes.size() - 4U) != 0U) {
        error_ = "battlefield WDX does not have the original trailing zero word";
        return;
    }
    battlefield_offsets_.reserve(battlefield_index.bytes.size() / 4U - 1U);
    std::uint32_t previous{};
    for (std::size_t offset = 0U;
         offset + 4U < battlefield_index.bytes.size();
         offset += 4U) {
        const auto value = compat::read_u32le(battlefield_index.bytes, offset);
        if (value < previous || value > battlefield_group.bytes.size()) {
            error_ = "battlefield WDX contains an invalid cumulative offset";
            return;
        }
        battlefield_offsets_.push_back(value);
        previous = value;
    }
    battlefield_group_ = battlefield_group.bytes;
    if (!effect_sprites_.valid()) {
        error_ = effect_sprites_.error();
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
    auto ascii = data_root.read("FONT.X16");
    auto big5 = data_root.read("FONT.C16");
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
    if (!valid()) {
        return false;
    }
    framebuffer.clear(0U);
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
        !draw_text(
            framebuffer,
            *plan.name_x,
            plan.name_y,
            zero_terminated_prefix(plan.name_bytes),
            0x0705U)) {
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
    return draw_text(framebuffer, x(225), 101, kPowerLabel, 0x2321U) &&
        draw_text(framebuffer, x(262), 101, power, 0x0705U) &&
        draw_text(framebuffer, x(285), 101, kSlash, 0x6663U) &&
        draw_text(framebuffer, x(292), 101, kHundred, 0x2321U) &&
        draw_text(framebuffer, x(225), 118, kLifeLabel, 0x2321U) &&
        draw_text(
            framebuffer,
            x(262),
            118,
            hp,
            static_cast<std::uint16_t>(plan.hurt_color)) &&
        draw_text(framebuffer, x(285), 118, kSlash, 0x6663U) &&
        draw_text(
            framebuffer,
            x(292),
            118,
            maximum_hp,
            static_cast<std::uint16_t>(plan.poison_color)) &&
        draw_text(framebuffer, x(225), 135, kMpLabel, 0x2321U) &&
        draw_text(
            framebuffer,
            x(262),
            135,
            mp,
            static_cast<std::uint16_t>(plan.mp_color)) &&
        draw_text(
            framebuffer,
            x(285),
            135,
            kSlash,
            static_cast<std::uint16_t>(plan.mp_color)) &&
        draw_text(
            framebuffer,
            x(292),
            135,
            maximum_mp,
            static_cast<std::uint16_t>(plan.mp_color));
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
            left + rectangle_width, render::IndexedFramebuffer::width);
        const auto begin_y = std::max(top, 0);
        const auto end_y = std::min(
            top + rectangle_height, render::IndexedFramebuffer::height);
        for (int destination_y = begin_y; destination_y < end_y; ++destination_y) {
            for (int destination_x = begin_x; destination_x < end_x; ++destination_x) {
                auto& destination = framebuffer.row(destination_y)[destination_x];
                destination = blend_pixel(0U, destination, 4);
            }
        }
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
            0xFFU);
    };
    return fill(x + 5, y + 1, w - 10, 1) &&
        fill(x + 4, y + 2, 1, 2) && fill(x + w - 5, y + 2, 1, 2) &&
        fill(x + 2, y + 4, 2, 1) && fill(x + w - 4, y + 4, 2, 1) &&
        fill(x + 1, y + 5, 1, h - 10) && fill(x + w - 2, y + 5, 1, h - 10) &&
        fill(x + 2, y + h - 5, 2, 1) && fill(x + w - 4, y + h - 5, 2, 1) &&
        fill(x + 4, y + h - 4, 1, 2) && fill(x + w - 5, y + h - 4, 1, 2) &&
        fill(x + 5, y + h - 2, w - 10, 1);
}

bool BattleRenderer::draw_text(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::span<const std::uint8_t> text,
    const std::uint16_t packed_colors) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    std::vector<std::uint8_t> terminated(text.begin(), text.end());
    terminated.push_back(0U);
    return render::draw_legacy_text(
        framebuffer,
        x,
        y,
        terminated,
        ascii_font_,
        *big5_cache_,
        static_cast<std::uint8_t>(packed_colors & 0xFFU),
        static_cast<std::uint8_t>(packed_colors >> 8U));
}

bool BattleRenderer::draw_portrait(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t portrait_id,
    const int x,
    const int y) const {
    if (portrait_id < 0 || static_cast<std::size_t>(portrait_id) >= portraits_.entry_count()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(
        portraits_.entry(static_cast<std::size_t>(portrait_id)));
    if (!frame.valid()) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, frame, x, y);
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
    return true;
}

std::span<const std::uint8_t> BattleRenderer::fight_entry(
    const std::int32_t legacy_id) const {
    constexpr std::size_t kEffectPointerBase = 6'500U;
    constexpr std::size_t kFightPointerBase = 8'000U;
    if (legacy_id < 0 || legacy_id > 0x7FFE || legacy_id % 2 != 0) {
        return {};
    }
    const auto pointer_index = static_cast<std::size_t>(legacy_id / 2);
    if (pointer_index >= kFightPointerBase && fight_sprites_.has_value()) {
        const auto local_index = pointer_index - kFightPointerBase;
        if (local_index < fight_sprites_->entry_count()) {
            return fight_sprites_->entry(local_index);
        }
    }
    if (pointer_index >= kEffectPointerBase) {
        const auto local_index = pointer_index - kEffectPointerBase;
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

bool BattleRenderer::draw_fight_sprite(
    render::IndexedFramebuffer& framebuffer,
    const std::int32_t legacy_id,
    const int anchor_x,
    const int anchor_y) const {
    const auto bytes = fight_entry(legacy_id);
    if (bytes.empty()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(bytes);
    if (!frame.valid()) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, frame, anchor_x, anchor_y);
    return true;
}

bool BattleRenderer::draw_tinted_fight_sprite(
    render::IndexedFramebuffer& framebuffer,
    const std::int32_t legacy_id,
    const int anchor_x,
    const int anchor_y,
    const std::uint8_t color) const {
    const auto bytes = fight_entry(legacy_id);
    if (bytes.empty()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(bytes);
    if (!frame.valid()) {
        return false;
    }
    const auto left = anchor_x - static_cast<int>(frame.x_offset());
    const auto top = anchor_y - static_cast<int>(frame.y_offset());
    for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
        const auto y = top + static_cast<int>(row_index);
        auto x = left;
        for (const auto& run : frame.rows()[row_index].runs) {
            x += static_cast<int>(run.skip);
            for ([[maybe_unused]] const auto pixel : run.pixels) {
                if (y >= 0 && y < render::IndexedFramebuffer::height &&
                    x >= 0 && x < render::IndexedFramebuffer::width) {
                    framebuffer.row(y)[x] = color;
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
    const auto frame = resource::SpriteFrameView::parse(
        cloud_sprites_.entry(static_cast<std::size_t>(variant) + 4U));
    if (!frame.valid()) {
        return false;
    }
    const auto left = anchor_x - static_cast<int>(frame.x_offset());
    const auto top = anchor_y - static_cast<int>(frame.y_offset());
    for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
        const auto y = top + static_cast<int>(row_index);
        auto x = left;
        for (const auto& run : frame.rows()[row_index].runs) {
            x += static_cast<int>(run.skip);
            for (const auto source : run.pixels) {
                if (y >= 0 && y < render::IndexedFramebuffer::height &&
                    x >= 0 && x < render::IndexedFramebuffer::width) {
                    auto& destination = framebuffer.row(y)[x];
                    destination = blend_pixel(source, destination, source_weight);
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
    std::vector<std::uint8_t> text;
    text.push_back(command.overlay_variant < 0 ? '-' : '+');
    const auto number = decimal_text(command.value);
    text.insert(text.end(), number.begin(), number.end());
    return draw_text(
        framebuffer,
        command.screen_x,
        command.screen_y,
        text,
        std::bit_cast<std::uint16_t>(command.style));
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
