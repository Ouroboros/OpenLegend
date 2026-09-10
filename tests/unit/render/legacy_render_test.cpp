#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/legacy_effects.hpp"
#include "openlegend/render/legacy_font_renderer.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/render/world_depth_order.hpp"
#include "openlegend/render/world_projection.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/resource/packed_archive.hpp"
#include "test_support.hpp"

namespace {

constexpr openlegend::render::TextColors kTestTextColors{12U, 250U};
constexpr openlegend::render::TextColors kTestBig5TextColors{5U, 6U};

std::uint64_t fnv1a64(const std::span<const std::uint8_t> bytes) {
    auto hash = std::uint64_t{0xCBF29CE484222325ULL};
    for (const auto byte : bytes) {
        hash ^= byte;
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

void run_framebuffer_tests() {
    using openlegend::render::IndexedFramebuffer;
    static_assert(
        sizeof(IndexedFramebuffer) <= 1024U,
        "framebuffer pixel storage must not be allocated inside stack objects");

    IndexedFramebuffer framebuffer;
    framebuffer.clear(3U);
    OL_CHECK(framebuffer.pixels().front() == 3U);
    OL_CHECK(framebuffer.pixels().back() == 3U);
    auto copied_framebuffer = framebuffer;
    copied_framebuffer.pixels().front() = 4U;
    OL_CHECK(framebuffer.pixels().front() == 3U);
    OL_CHECK(copied_framebuffer.pixels().front() == 4U);
    OL_CHECK(framebuffer.fill_rectangle(10, 20, 3U, 2U, 9U));
    OL_CHECK(framebuffer.row(20)[9] == 3U);
    OL_CHECK(framebuffer.row(20)[10] == 9U);
    OL_CHECK(framebuffer.row(21)[12] == 9U);
    OL_CHECK(framebuffer.row(22)[10] == 3U);
    OL_CHECK(!framebuffer.fill_rectangle(-1, 0, 1U, 1U, 0U));
    OL_CHECK(!framebuffer.fill_rectangle(319, 199, 2U, 1U, 0U));

    // Independent sub_2010A vectors: 320-byte pitch, inclusive lower-right edge,
    // full-byte color, and a zero-width REP operation that changes no pixels.
    framebuffer.clear(7U);
    OL_CHECK(framebuffer.fill_rectangle(10, 20, 3U, 2U, 9U));
    OL_CHECK(framebuffer.fill_rectangle(0, 0, 320U, 1U, 21U));
    OL_CHECK(framebuffer.fill_rectangle(319, 199, 1U, 1U, 255U));
    OL_CHECK(framebuffer.fill_rectangle(50, 40, 0U, 2U, 99U));
    OL_CHECK(std::ranges::count_if(framebuffer.pixels(), [](const std::uint8_t value) {
        return value != 7U;
    }) == 327);
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0xCCA0D464AA7BCF4DULL);

    // The DOS primitive writes unchecked memory and underflows a zero height to
    // 65,536 rows. The hosted boundary is a stable no-op/rejection instead.
    const std::vector<std::uint8_t> safe_boundary_before(
        framebuffer.pixels().begin(), framebuffer.pixels().end());
    OL_CHECK(framebuffer.fill_rectangle(0, 0, 1U, 0U, 99U));
    OL_CHECK(!framebuffer.fill_rectangle(
        std::numeric_limits<int>::max(), 0, 1U, 1U, 99U));
    OL_CHECK(!framebuffer.fill_rectangle(
        0, std::numeric_limits<int>::max(), 1U, 1U, 99U));
    OL_CHECK(!framebuffer.outline_rectangle(
        std::numeric_limits<int>::max(), 0, 1U, 1U, 99U));
    OL_CHECK(std::ranges::equal(framebuffer.pixels(), safe_boundary_before));

    framebuffer.clear(7U);
    OL_CHECK(framebuffer.outline_rectangle(55, 62, 40U, 40U, 0U));
    OL_CHECK(framebuffer.row(62)[55] == 0U);
    OL_CHECK(framebuffer.row(62)[94] == 0U);
    OL_CHECK(framebuffer.row(101)[55] == 0U);
    OL_CHECK(framebuffer.row(101)[94] == 0U);
    OL_CHECK(framebuffer.row(63)[56] == 7U);
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0x63EB8C2A7F900ED9ULL);

    framebuffer.clear(7U);
    OL_CHECK(framebuffer.outline_rectangle(55, 62, 40U, 40U, 0xFFU));
    OL_CHECK(framebuffer.row(63)[56] == 7U);
    OL_CHECK(fnv1a64(framebuffer.pixels()) == 0xE154C07BA899CBA5ULL);
    OL_CHECK(!framebuffer.outline_rectangle(55, 62, 0U, 40U, 0xFFU));
    OL_CHECK(!framebuffer.outline_rectangle(300, 180, 40U, 40U, 0xFFU));

    openlegend::render::RgbaFramebuffer rgba_framebuffer;
    rgba_framebuffer.clear({10U, 20U, 30U, 0xFFU});
    OL_CHECK(rgba_framebuffer.row(0)[0] == 10U);
    OL_CHECK(rgba_framebuffer.row(0)[1] == 20U);
    OL_CHECK(rgba_framebuffer.row(0)[2] == 30U);
    OL_CHECK(rgba_framebuffer.row(0)[3] == 0xFFU);
    OL_CHECK(rgba_framebuffer.blend_pixel(0, 0, {0xFFU, 0U, 0U, 128U}));
    OL_CHECK(rgba_framebuffer.row(0)[0] == 133U);
    OL_CHECK(rgba_framebuffer.row(0)[1] == 10U);
    OL_CHECK(rgba_framebuffer.row(0)[2] == 15U);
    OL_CHECK(rgba_framebuffer.row(0)[3] == 0xFFU);
    OL_CHECK(rgba_framebuffer.blend_pixel(0, 0, {1U, 2U, 3U, 0U}));
    OL_CHECK(rgba_framebuffer.row(0)[0] == 133U);
    OL_CHECK(rgba_framebuffer.row(0)[1] == 10U);
    OL_CHECK(rgba_framebuffer.row(0)[2] == 15U);
    OL_CHECK(rgba_framebuffer.blend_pixel(0, 0, {1U, 2U, 3U, 0xFFU}));
    OL_CHECK(rgba_framebuffer.row(0)[0] == 1U);
    OL_CHECK(rgba_framebuffer.row(0)[1] == 2U);
    OL_CHECK(rgba_framebuffer.row(0)[2] == 3U);
    OL_CHECK(rgba_framebuffer.row(0)[3] == 0xFFU);
    rgba_framebuffer.clear({10U, 20U, 30U, 0xFFU});
    OL_CHECK(rgba_framebuffer.blend_rectangle(
        5, 6, 2U, 2U, {0xFFU, 0U, 0U, 128U}));
    OL_CHECK(rgba_framebuffer.row(6)[20] == 133U);
    OL_CHECK(rgba_framebuffer.row(6)[21] == 10U);
    OL_CHECK(rgba_framebuffer.row(6)[22] == 15U);
    OL_CHECK(rgba_framebuffer.row(6)[23] == 0xFFU);
    OL_CHECK(rgba_framebuffer.row(6)[16] == 10U);
    OL_CHECK(rgba_framebuffer.fill_rectangle(
        2, 3, 2U, 2U, {1U, 2U, 3U, 4U}));
    OL_CHECK(rgba_framebuffer.row(3)[8] == 1U);
    OL_CHECK(rgba_framebuffer.row(3)[9] == 2U);
    OL_CHECK(rgba_framebuffer.row(3)[10] == 3U);
    OL_CHECK(rgba_framebuffer.row(3)[11] == 4U);
    OL_CHECK(!rgba_framebuffer.blend_pixel(-1, 0, {}));
    OL_CHECK(!rgba_framebuffer.fill_rectangle(319, 199, 2U, 1U, {}));

    OL_CHECK(openlegend::render::project_isometric(0, 0, 160, 100) ==
             (openlegend::render::ScreenPoint{160, 100}));
    OL_CHECK(openlegend::render::project_isometric(1, 0, 160, 100) ==
             (openlegend::render::ScreenPoint{178, 109}));
    OL_CHECK(openlegend::render::project_isometric(0, 1, 160, 100) ==
             (openlegend::render::ScreenPoint{142, 109}));
    OL_CHECK(openlegend::render::legacy_world_tile_screen(0, 0, 11, 11) ==
             (openlegend::render::ScreenPoint{145, -81}));
    OL_CHECK(openlegend::render::legacy_world_tile_screen(11, 11, 11, 11) ==
             (openlegend::render::ScreenPoint{145, 117}));
}

void run_depth_order_tests() {
    using namespace openlegend::render;
    constexpr auto cell_count = static_cast<std::size_t>(
        legacy_world_cache_extent * legacy_world_cache_extent);
    std::array<std::int16_t, cell_count> owner_x{};
    std::array<std::int16_t, cell_count> owner_y{};
    std::array<std::int16_t, cell_count> sprites{};
    const auto index = [](const int x, const int y) {
        return static_cast<std::size_t>(y * legacy_world_cache_extent + x);
    };

    owner_x[index(0, 0)] = 40;
    owner_y[index(0, 0)] = 50;
    owner_x[index(0, 1)] = 41;
    owner_y[index(0, 1)] = 50;
    owner_x[index(0, 2)] = 42;
    owner_y[index(0, 2)] = 50;
    owner_x[index(0, 3)] = 40;
    owner_y[index(0, 3)] = 50;
    sprites[index(8, 18)] = 100;
    sprites[index(9, 18)] = 200;
    sprites[index(10, 18)] = 300;

    const LegacyWorldDepthInput input{
        owner_x,
        owner_y,
        sprites,
        11,
        11,
        32,
        32,
        LegacyDepthActor{60, 61, 11, 11, 5000},
        std::nullopt};
    const auto result = build_legacy_world_depth_list(input);
    OL_CHECK(static_cast<bool>(result));
    OL_CHECK(result.entries.size() == 4U);
    if (result.entries.size() == 4U) {
        OL_CHECK(result.entries[0] == (LegacyDepthEntry{41, 50, 200}));
        OL_CHECK(result.entries[1] == (LegacyDepthEntry{42, 50, 300}));
        OL_CHECK(result.entries[2] == (LegacyDepthEntry{40, 50, 100}));
        OL_CHECK(result.entries[3] == (LegacyDepthEntry{60, 61, 5000}));
    }

    auto with_ship = input;
    with_ship.secondary_actor = LegacyDepthActor{70, 71, 1, 0, 6000};
    const auto ship_result = build_legacy_world_depth_list(with_ship);
    OL_CHECK(static_cast<bool>(ship_result));
    OL_CHECK(std::count_if(
                 ship_result.entries.begin(),
                 ship_result.entries.end(),
                 [](const LegacyDepthEntry& entry) { return entry.sprite_id == 6000; }) == 1);

    auto same_cell = input;
    same_cell.secondary_actor = LegacyDepthActor{70, 71, 11, 11, 6000};
    const auto same_cell_result = build_legacy_world_depth_list(same_cell);
    OL_CHECK(static_cast<bool>(same_cell_result));
    OL_CHECK(std::none_of(
        same_cell_result.entries.begin(),
        same_cell_result.entries.end(),
        [](const LegacyDepthEntry& entry) { return entry.sprite_id == 6000; }));

    auto maximum_sprites = sprites;
    maximum_sprites[index(8, 18)] = 0x2064;
    auto maximum = input;
    maximum.building_sprite = maximum_sprites;
    OL_CHECK(static_cast<bool>(build_legacy_world_depth_list(maximum)));

    auto invalid_sprites = sprites;
    invalid_sprites[index(8, 18)] = -1;
    auto invalid = input;
    invalid.building_sprite = invalid_sprites;
    OL_CHECK(!build_legacy_world_depth_list(invalid));
}

void run_effect_tests() {
    using namespace openlegend::render;
    IndexedFramebuffer framebuffer;
    const std::array<std::uint16_t, 3> mask{3U, 2U, 63'995U};
    const std::array<std::uint8_t, 6> mask_bytes{3U, 0U, 2U, 0U, 0xFBU, 0xF9U};
    const auto parsed_mask = parse_legacy_shadow_mask(mask_bytes);
    OL_CHECK(parsed_mask.has_value());
    OL_CHECK(*parsed_mask == std::vector<std::uint16_t>(mask.begin(), mask.end()));
    OL_CHECK(!parse_legacy_shadow_mask(std::span{mask_bytes}.first(5U)).has_value());
    OL_CHECK(!parse_legacy_shadow_mask(std::span{mask_bytes}.first(4U)).has_value());

    framebuffer.clear(9U);
    OL_CHECK(apply_legacy_shadow_mask(framebuffer, mask, 0));
    OL_CHECK(framebuffer.pixels()[0] == 0U);
    OL_CHECK(framebuffer.pixels()[2] == 0U);
    OL_CHECK(framebuffer.pixels()[3] == 9U);
    OL_CHECK(framebuffer.pixels()[4] == 9U);
    OL_CHECK(framebuffer.pixels()[5] == 0U);
    OL_CHECK(framebuffer.pixels().back() == 0U);

    framebuffer.clear(9U);
    OL_CHECK(apply_legacy_shadow_mask(framebuffer, mask, 2));
    OL_CHECK(framebuffer.pixels()[4] == 0U);
    OL_CHECK(framebuffer.pixels()[5] == 9U);
    OL_CHECK(framebuffer.pixels()[6] == 9U);
    OL_CHECK(framebuffer.pixels()[7] == 0U);

    framebuffer.clear(9U);
    OL_CHECK(apply_legacy_shadow_mask(framebuffer, mask, -2));
    OL_CHECK(framebuffer.pixels()[0] == 0U);
    OL_CHECK(framebuffer.pixels()[1] == 9U);
    OL_CHECK(framebuffer.pixels()[2] == 9U);
    OL_CHECK(framebuffer.pixels()[3] == 0U);
    OL_CHECK(framebuffer.pixels()[63'997U] == 0U);
    OL_CHECK(framebuffer.pixels()[63'998U] == 0U);
    OL_CHECK(framebuffer.pixels()[63'999U] == 0U);

    openlegend::compat::LegacyPalette palette{};
    palette[0] = openlegend::compat::Rgb6{63U, 1U, 0U};
    const auto fade_out = legacy_fade_to_black(palette);
    const auto fade_in = legacy_fade_from_black(palette);
    OL_CHECK(fade_out.size() == 64U);
    OL_CHECK(fade_in.size() == 65U);
    OL_CHECK(fade_out[0][0].red == 62U);
    OL_CHECK(fade_out[0][0].green == 0U);
    OL_CHECK(fade_out.back()[0].red == 0U);
    OL_CHECK(fade_in.front()[0].red == 0U);
    OL_CHECK(fade_in[63][0].red == 62U);
    OL_CHECK(fade_in.back()[0].red == 63U);
    OL_CHECK(palette[0].red == 63U);
    OL_CHECK(palette[0].green == 1U);
    OL_CHECK(palette[0].blue == 0U);

    openlegend::compat::LegacyPalette byte_domain_palette{};
    for (std::size_t color = 0U; color < byte_domain_palette.size(); ++color) {
        byte_domain_palette[color] = openlegend::compat::Rgb6{
            static_cast<std::uint8_t>(color),
            static_cast<std::uint8_t>(255U - color),
            static_cast<std::uint8_t>((color * 73U + 19U) & 0xFFU),
        };
    }
    const auto byte_domain_source = byte_domain_palette;
    auto byte_domain_fade = legacy_fade_to_black(byte_domain_palette);
    OL_CHECK(byte_domain_fade.size() == 64U);
    for (std::size_t frame = 0U; frame < byte_domain_fade.size(); ++frame) {
        const auto decrement = static_cast<std::uint16_t>(frame + 1U);
        const auto expected = [decrement](const std::uint8_t value) {
            return static_cast<std::uint8_t>(
                value > decrement ? static_cast<std::uint16_t>(value) - decrement : 0U);
        };
        for (std::size_t color = 0U; color < byte_domain_palette.size(); ++color) {
            OL_CHECK(byte_domain_fade[frame][color].red ==
                     expected(byte_domain_source[color].red));
            OL_CHECK(byte_domain_fade[frame][color].green ==
                     expected(byte_domain_source[color].green));
            OL_CHECK(byte_domain_fade[frame][color].blue ==
                     expected(byte_domain_source[color].blue));
            OL_CHECK(byte_domain_palette[color].red == byte_domain_source[color].red);
            OL_CHECK(byte_domain_palette[color].green == byte_domain_source[color].green);
            OL_CHECK(byte_domain_palette[color].blue == byte_domain_source[color].blue);
        }
    }
    OL_CHECK(byte_domain_fade.back()[255U].red == 191U);
    OL_CHECK(byte_domain_fade.back()[0U].green == 191U);
    const auto second_frame_red = byte_domain_fade[1U][200U].red;
    byte_domain_fade[0U][200U].red = 0U;
    OL_CHECK(byte_domain_fade[1U][200U].red == second_frame_red);
    OL_CHECK(byte_domain_palette[200U].red == byte_domain_source[200U].red);

    auto byte_domain_fade_in = legacy_fade_from_black(byte_domain_palette);
    OL_CHECK(byte_domain_fade_in.size() == 65U);
    for (std::size_t frame = 0U; frame < byte_domain_fade_in.size(); ++frame) {
        const auto decrement = static_cast<std::uint16_t>(
            frame < 64U ? 64U - frame : 0U);
        const auto expected = [decrement](const std::uint8_t value) {
            return static_cast<std::uint8_t>(
                value > decrement ? static_cast<std::uint16_t>(value) - decrement : 0U);
        };
        for (std::size_t color = 0U; color < byte_domain_palette.size(); ++color) {
            OL_CHECK(byte_domain_fade_in[frame][color].red ==
                     expected(byte_domain_source[color].red));
            OL_CHECK(byte_domain_fade_in[frame][color].green ==
                     expected(byte_domain_source[color].green));
            OL_CHECK(byte_domain_fade_in[frame][color].blue ==
                     expected(byte_domain_source[color].blue));
            OL_CHECK(byte_domain_palette[color].red == byte_domain_source[color].red);
            OL_CHECK(byte_domain_palette[color].green == byte_domain_source[color].green);
            OL_CHECK(byte_domain_palette[color].blue == byte_domain_source[color].blue);
        }
    }
    OL_CHECK(byte_domain_fade_in.front()[255U].red == 191U);
    OL_CHECK(byte_domain_fade_in.front()[0U].green == 191U);
    OL_CHECK(byte_domain_fade_in[1U][64U].red == 1U);
    OL_CHECK(byte_domain_fade_in[63U][64U].red == 63U);
    OL_CHECK(byte_domain_fade_in.back()[64U].red == 64U);
    const auto fade_in_second_frame_red = byte_domain_fade_in[1U][200U].red;
    const auto fade_in_final_red = byte_domain_fade_in.back()[200U].red;
    byte_domain_fade_in[0U][200U].red = 0U;
    OL_CHECK(byte_domain_fade_in[1U][200U].red == fade_in_second_frame_red);
    OL_CHECK(byte_domain_fade_in.back()[200U].red == fade_in_final_red);
    OL_CHECK(byte_domain_palette[200U].red == byte_domain_source[200U].red);
}

void run_synthetic_sprite_tests() {
    using namespace openlegend::render;
    using openlegend::resource::SpriteFrameView;

    const std::array<std::uint8_t, 14> bytes{
        4U, 0U, 2U, 0U, 1U, 0U, 0xFFU, 0xFFU,
        4U, 1U, 2U, 7U, 8U,
        0U};
    OL_CHECK(!legacy_sprite_index(static_cast<std::uint32_t>(-1)));
    OL_CHECK(legacy_sprite_index(0U) == 0U);
    OL_CHECK(legacy_sprite_index(1U) == 0U);
    OL_CHECK(legacy_sprite_index(2U) == 1U);
    OL_CHECK(legacy_sprite_index(0x7FFEU) == 0x3FFFU);
    OL_CHECK(!legacy_sprite_index(0x7FFFU));
    OL_CHECK(!legacy_item_sprite_index(-1));
    OL_CHECK(legacy_item_sprite_index(0) == 3'501U);
    OL_CHECK(legacy_item_sprite_index(199) == 3'700U);

    const auto frame = SpriteFrameView::parse(bytes);
    OL_CHECK(frame.valid());

    IndexedFramebuffer framebuffer;
    framebuffer.clear(0U);
    draw_rle_sprite(framebuffer, frame, 0, 0);
    OL_CHECK(framebuffer.row(1)[0] == 7U);
    OL_CHECK(framebuffer.row(1)[1] == 8U);
    OL_CHECK(framebuffer.row(0)[0] == 0U);

    framebuffer.clear(0U);
    draw_rle_sprite(framebuffer, frame, 319, 0);
    OL_CHECK(framebuffer.row(1)[319] == 7U);
    OL_CHECK(framebuffer.row(1)[318] == 0U);
}

void run_glyph_write_tests() {
    using namespace openlegend::render;

    std::array<std::uint8_t, 16> ascii{};
    ascii[0] = 0xC0U;
    IndexedFramebuffer framebuffer;
    framebuffer.clear(0U);
    OL_CHECK(draw_ascii_glyph(framebuffer, 10, 10, ascii, kTestTextColors));
    OL_CHECK(framebuffer.row(10)[10] == 250U);
    OL_CHECK(framebuffer.row(10)[11] == 250U);
    OL_CHECK(framebuffer.row(10)[12] == 12U);

    std::array<std::uint8_t, 32> big5{};
    big5[0] = 0x80U;
    big5[1] = 0x80U;
    framebuffer.clear(0U);
    OL_CHECK(draw_big5_glyph(framebuffer, 20, 20, big5, kTestBig5TextColors));
    OL_CHECK(framebuffer.row(20)[20] == 6U);
    OL_CHECK(framebuffer.row(20)[21] == 5U);
    OL_CHECK(framebuffer.row(20)[28] == 6U);
    OL_CHECK(framebuffer.row(20)[29] == 5U);

    std::array<std::uint8_t, 158U * 32U> synthetic_big5{};
    for (std::size_t index = 0U; index < 158U; ++index) {
        synthetic_big5[index * 32U] = static_cast<std::uint8_t>(index);
    }
    Big5GlyphCache cache{synthetic_big5};
    const auto first = cache.resolve(0xA140U);
    const auto low_trail_end = cache.resolve(0xA17EU);
    const auto high_trail_begin = cache.resolve(0xA1A1U);
    const auto lead_end = cache.resolve(0xA1FEU);
    const auto next_lead = cache.resolve(0xA240U);
    OL_CHECK(first && (*first)[0] == 0U);
    OL_CHECK(low_trail_end && (*low_trail_end)[0] == 62U);
    OL_CHECK(high_trail_begin && (*high_trail_begin)[0] == 63U);
    OL_CHECK(lead_end && (*lead_end)[0] == 156U);
    OL_CHECK(next_lead && (*next_lead)[0] == 157U);
    OL_CHECK(cache.next_replacement_slot() == 5U);
    OL_CHECK(static_cast<bool>(cache.resolve(0xA140U)));
    OL_CHECK(cache.next_replacement_slot() == 5U);
    OL_CHECK(!cache.resolve(0xA17FU));
    OL_CHECK(!cache.resolve(0xA1A0U));
    OL_CHECK(!cache.resolve(0xA1FFU));
    OL_CHECK(!cache.resolve(0xA040U));
    OL_CHECK(cache.next_replacement_slot() == 5U);

    Big5GlyphCache ring_cache{synthetic_big5};
    for (std::size_t index = 0U; index < 64U; ++index) {
        const auto trail = static_cast<std::uint16_t>(
            index < 63U ? 0x40U + index : 0x62U + index);
        const auto code = static_cast<std::uint16_t>(0xA100U | trail);
        const auto glyph = ring_cache.resolve(code);
        OL_CHECK(glyph && (*glyph)[0] == static_cast<std::uint8_t>(index));
    }
    OL_CHECK(ring_cache.next_replacement_slot() == 0U);
    OL_CHECK(static_cast<bool>(ring_cache.resolve(0xA14AU)));
    OL_CHECK(ring_cache.next_replacement_slot() == 0U);
    const auto replacement = ring_cache.resolve(0xA240U);
    OL_CHECK(replacement && (*replacement)[0] == 157U);
    OL_CHECK(ring_cache.next_replacement_slot() == 1U);
    const auto reloaded = ring_cache.resolve(0xA140U);
    OL_CHECK(reloaded && (*reloaded)[0] == 0U);
    OL_CHECK(ring_cache.next_replacement_slot() == 2U);
}

openlegend::resource::SpriteFrameView frame_zero(
    const openlegend::resource::PackedArchive& archive) {
    OL_CHECK(archive.valid());
    OL_CHECK(archive.entry_count() > 0U);
    const auto frame = openlegend::resource::SpriteFrameView::parse(archive.entry(0U));
    OL_CHECK(frame.valid());
    return frame;
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

void run_text_encoding_tests() {
    using namespace openlegend::text;

    static_assert(
        openlegend::render::legacy_color::text::notice.legacy_packed() == 0x0705U);
    static_assert(
        openlegend::render::legacy_color::text::selected.legacy_packed() == 0x6663U);
    static_assert(
        openlegend::render::legacy_color::panel_outline == 0xFFU);

    const auto mapped = encode_big5(u8"個前德指望目聲道");
    const std::array<std::uint8_t, 16> expected_mapped{
        0xADU,
        0xD3U,
        0xABU,
        0x65U,
        0xBCU,
        0x77U,
        0xABU,
        0xFCU,
        0xB1U,
        0xE6U,
        0xA5U,
        0xD8U,
        0xC1U,
        0x6EU,
        0xB9U,
        0x44U,
    };
    OL_CHECK(mapped.has_value());
    OL_CHECK(mapped.has_value() && std::ranges::equal(*mapped, expected_mapped));

    std::vector<std::uint8_t> rollback{'A'};
    const auto original = rollback;
    const std::array<char8_t, 2> invalid_utf8{
        static_cast<char8_t>(0xC0U),
        static_cast<char8_t>(0xAFU),
    };
    OL_CHECK(!append_big5(
        rollback,
        std::u8string_view{invalid_utf8.data(), invalid_utf8.size()}));
    OL_CHECK(rollback == original);

    const std::array<std::uint8_t, 2> legacy_name{
        0xA4U,
        0x40U,
    };
    GameText mixed;
    mixed.append_utf8(u8"得到");
    mixed.append_legacy(Big5TextView{legacy_name});
    mixed.append_ascii("123");
    std::vector<std::uint8_t> encoded;
    OL_CHECK(encode_game_text(mixed, encoded));
    const std::array<std::uint8_t, 9> expected_mixed{
        0xB1U,
        0x6FU,
        0xA8U,
        0xECU,
        0xA4U,
        0x40U,
        '1',
        '2',
        '3',
    };
    OL_CHECK(std::ranges::equal(encoded, expected_mixed));
    OL_CHECK(mixed.legacy_width_units() == 9U);
    OL_CHECK(mixed.trailing_ascii_digit_count() == 3U);
}

void run_real_asset_golden() {
    using namespace openlegend::render;
    using namespace openlegend::resource;
    const auto root = openlegend::test::game_data_root();

    const auto mmap = PackedArchive::open(root / "MMAP.IDX", root / "MMAP.GRP");
    const auto title = PackedArchive::open(root / "TITLE.IDX", root / "TITLE.GRP");
    const auto cloud = PackedArchive::open(root / "CLOUD.IDX", root / "CLOUD.GRP");
    const auto ascii_font = read_binary_file(root / "FONT3.E16");
    const auto big5_font = read_binary_file(root / "FONT3.C16");
    const auto palette_file = read_binary_file(root / "MMAP.COL");
    OL_CHECK(mmap.valid());
    OL_CHECK(title.valid());
    OL_CHECK(cloud.valid());
    OL_CHECK(static_cast<bool>(ascii_font));
    OL_CHECK(static_cast<bool>(big5_font));
    OL_CHECK(static_cast<bool>(palette_file));
    if (!mmap.valid() || !title.valid() || !cloud.valid() || !ascii_font || !big5_font || !palette_file) {
        return;
    }

    IndexedFramebuffer framebuffer;
    framebuffer.clear(3U);
    const auto palette = parse_vga_palette(palette_file.bytes);
    OL_CHECK(static_cast<bool>(palette));
    if (palette) {
        auto source_palette = palette.palette;
        framebuffer.set_palette(source_palette);
        std::array<std::uint8_t, openlegend::compat::kLegacyPaletteSize * 3U> packed_palette{};
        for (std::size_t index = 0U; index < source_palette.size(); ++index) {
            const auto source = source_palette[index];
            const auto stored = framebuffer.palette()[index];
            OL_CHECK(stored.red == source.red);
            OL_CHECK(stored.green == source.green);
            OL_CHECK(stored.blue == source.blue);
            packed_palette[index * 3U] = stored.red;
            packed_palette[index * 3U + 1U] = stored.green;
            packed_palette[index * 3U + 2U] = stored.blue;
        }
        OL_CHECK(fnv1a64(packed_palette) == 0xB7546B614CF2C7CCULL);

        const auto committed_first = framebuffer.palette()[0U];
        source_palette[0U] = {63U, 63U, 63U};
        OL_CHECK(framebuffer.palette()[0U].red == committed_first.red);
        OL_CHECK(framebuffer.palette()[0U].green == committed_first.green);
        OL_CHECK(framebuffer.palette()[0U].blue == committed_first.blue);

        openlegend::compat::LegacyPixels all_indices{};
        for (std::size_t index = 0U; index < all_indices.size(); ++index) {
            all_indices[index] = static_cast<std::uint8_t>(index & 0xFFU);
        }
        const auto original_indices = all_indices;
        openlegend::compat::ModernRgbaPixels expanded{};
        OL_CHECK(openlegend::compat::convert_indexed_frame_to_rgba(
            {all_indices, framebuffer.palette()}, expanded));
        OL_CHECK(all_indices == original_indices);
        OL_CHECK(fnv1a64(expanded) == 0x20A030C1CEF0FC6DULL);
    }

    const auto mmap_frame = frame_zero(mmap);
    const auto title_frame = frame_zero(title);
    const auto cloud_frame = frame_zero(cloud);
    draw_rle_sprite(framebuffer, mmap_frame, 10, 10);
    draw_rle_sprite(framebuffer, title_frame, 238, 160);
    draw_rle_sprite(framebuffer, cloud_frame, 160, 40);

    Big5GlyphCache cache{big5_font.bytes};
    const std::array<std::uint8_t, 6> text{'A', '_', 'Z', 0xA4U, 0x40U, 0U};
    OL_CHECK(draw_text_big5(
        framebuffer,
        20,
        80,
        openlegend::text::Big5TextView{text},
        ascii_font.bytes,
        cache,
        kTestTextColors));
    OL_CHECK(cache.next_replacement_slot() == 1U);
    OL_CHECK(static_cast<bool>(cache.resolve(0xA440U)));
    OL_CHECK(cache.next_replacement_slot() == 1U);

    IndexedFramebuffer big5_text_framebuffer;
    IndexedFramebuffer utf8_text_framebuffer;
    IndexedFramebuffer mixed_text_framebuffer;
    const std::array<std::uint8_t, 2> legacy_one{
        0xA4U,
        0x40U,
    };
    openlegend::text::GameText mixed_one;
    mixed_one.append_utf8(u8"一");
    OL_CHECK(draw_text_big5(
        big5_text_framebuffer,
        20,
        80,
        openlegend::text::Big5TextView{legacy_one},
        ascii_font.bytes,
        cache,
        kTestTextColors));
    OL_CHECK(draw_text_utf8(
        utf8_text_framebuffer,
        20,
        80,
        u8"一",
        ascii_font.bytes,
        cache,
        kTestTextColors));
    OL_CHECK(draw_text_mixed(
        mixed_text_framebuffer,
        20,
        80,
        mixed_one,
        ascii_font.bytes,
        cache,
        kTestTextColors));
    OL_CHECK(std::ranges::equal(
        big5_text_framebuffer.pixels(), utf8_text_framebuffer.pixels()));
    OL_CHECK(std::ranges::equal(
        big5_text_framebuffer.pixels(), mixed_text_framebuffer.pixels()));

    const auto hash = fnv1a64(framebuffer.pixels());
    if (hash != 0xCF173BA0515B7807ULL) {
        std::cerr << "render golden mismatch: 0x" << std::hex << hash << '\n';
    }
    OL_CHECK(hash == 0xCF173BA0515B7807ULL);
}

template <typename Archive>
void draw_all_archive_frames(
    const Archive& archive,
    std::array<openlegend::render::IndexedFramebuffer, 4>& framebuffers,
    const std::array<openlegend::render::ScreenPoint, 4>& anchors,
    std::size_t& frame_count) {
    OL_CHECK(archive.valid());
    if (!archive.valid()) {
        return;
    }
    for (std::size_t index = 0U; index < archive.entry_count(); ++index) {
        const auto entry = archive.entry(index);
        if (entry.empty()) {
            continue;
        }
        const auto frame = openlegend::resource::SpriteFrameView::parse(entry);
        OL_CHECK(frame.valid());
        if (!frame.valid()) {
            continue;
        }
        for (std::size_t corner = 0U; corner < framebuffers.size(); ++corner) {
            openlegend::render::draw_rle_sprite(
                framebuffers[corner], frame, anchors[corner].x, anchors[corner].y);
        }
        ++frame_count;
    }
}

std::string three_digit_suffix(const int value) {
    std::ostringstream stream;
    stream << std::setw(3) << std::setfill('0') << value;
    return stream.str();
}

void run_real_palette_fade_golden() {
    using namespace openlegend::render;
    using namespace openlegend::resource;
    const auto root = openlegend::test::game_data_root();
    const auto palette_file = read_binary_file(root / "MMAP.COL");
    OL_CHECK(static_cast<bool>(palette_file));
    if (!palette_file) {
        return;
    }
    const auto palette = parse_vga_palette(palette_file.bytes);
    OL_CHECK(static_cast<bool>(palette));
    if (!palette) {
        return;
    }

    const auto fade_out = legacy_fade_to_black(palette.palette);
    const auto fade_in = legacy_fade_from_black(palette.palette);
    auto hash = std::uint64_t{0xCBF29CE484222325ULL};
    const auto hash_sequence = [&](const auto& sequence) {
        for (const auto& frame : sequence) {
            for (const auto color : frame) {
                for (const auto channel : {color.red, color.green, color.blue}) {
                    hash ^= channel;
                    hash *= 0x100000001B3ULL;
                }
            }
        }
    };
    hash_sequence(fade_out);
    hash_sequence(fade_in);
    OL_CHECK(fade_out.size() + fade_in.size() == 129U);
    if (hash != 0xA543BF4C501F4124ULL) {
        std::cerr << "palette fade golden mismatch: 0x" << std::hex << hash << '\n';
    }
    OL_CHECK(hash == 0xA543BF4C501F4124ULL);
}

void run_all_glyph_golden() {
    using namespace openlegend::render;
    using openlegend::resource::read_binary_file;
    const auto root = openlegend::test::game_data_root();
    const auto ascii_font = read_binary_file(root / "FONT3.E16");
    const auto big5_font = read_binary_file(root / "FONT3.C16");
    OL_CHECK(static_cast<bool>(ascii_font));
    OL_CHECK(static_cast<bool>(big5_font));
    if (!ascii_font || !big5_font) {
        return;
    }

    IndexedFramebuffer framebuffer;
    auto hash = std::uint64_t{0xCBF29CE484222325ULL};
    const auto hash_region = [&] {
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 17; ++x) {
                hash ^= framebuffer.row(y)[x];
                hash *= 0x100000001B3ULL;
            }
        }
    };

    for (std::size_t index = 0U; index < 128U; ++index) {
        OL_CHECK(framebuffer.fill_rectangle(0, 0, 17U, 16U, 0U));
        const auto glyph = std::span<const std::uint8_t, 16>{
            ascii_font.bytes.data() + static_cast<std::ptrdiff_t>(index * 16U), 16U};
        OL_CHECK(draw_ascii_glyph(framebuffer, 0, 0, glyph, kTestTextColors));
        hash_region();
    }

    Big5GlyphCache cache{big5_font.bytes};
    const auto glyph_count = big5_font.bytes.size() / 32U;
    for (std::size_t index = 0U; index < glyph_count; ++index) {
        const auto lead = static_cast<std::uint16_t>(0xA1U + index / 157U);
        const auto remainder = index % 157U;
        const auto trail = static_cast<std::uint16_t>(
            remainder < 63U ? 0x40U + remainder : 0x62U + remainder);
        const auto code = static_cast<std::uint16_t>((lead << 8U) | trail);
        const auto glyph = cache.resolve(code);
        OL_CHECK(static_cast<bool>(glyph));
        OL_CHECK(framebuffer.fill_rectangle(0, 0, 17U, 16U, 0U));
        if (glyph) {
            OL_CHECK(draw_big5_glyph(framebuffer, 0, 0, *glyph, kTestTextColors));
        }
        hash_region();
    }
    OL_CHECK(glyph_count == 13'973U);
    OL_CHECK(cache.next_replacement_slot() == 21U);
    if (hash != 0x6FA3DF724D833333ULL) {
        std::cerr << "all-glyph golden mismatch: 0x" << std::hex << hash << '\n';
    }
    OL_CHECK(hash == 0x6FA3DF724D833333ULL);
}

void run_all_sprite_corner_golden() {
    using namespace openlegend::render;
    using namespace openlegend::resource;
    const auto root = openlegend::test::game_data_root();
    std::array<IndexedFramebuffer, 4> framebuffers;
    for (std::size_t index = 0U; index < framebuffers.size(); ++index) {
        framebuffers[index].clear(static_cast<std::uint8_t>(index + 1U));
    }
    const std::array<ScreenPoint, 4> anchors{
        ScreenPoint{0, 0}, ScreenPoint{319, 0}, ScreenPoint{0, 199}, ScreenPoint{319, 199}};

    std::vector<std::pair<std::string, std::filesystem::path>> normal_indexes;
    for (const auto& item : std::filesystem::directory_iterator(root)) {
        if (!item.is_regular_file() || uppercase(item.path().extension().string()) != ".IDX") {
            continue;
        }
        const auto stem = uppercase(item.path().stem().string());
        if (stem.starts_with("FIGHT") || stem == "CLOUD" || stem == "EFT" || stem == "ENDWORD" ||
            stem == "FBK" || stem == "FMAP" || stem == "HDGRP" || stem == "MMAP" || stem == "TITLE") {
            normal_indexes.emplace_back(stem, item.path());
        }
    }
    std::sort(normal_indexes.begin(), normal_indexes.end());

    std::size_t frame_count = 0U;
    for (const auto& [stem, index_path] : normal_indexes) {
        const auto archive = PackedArchive::open(index_path, root / (stem + ".GRP"));
        draw_all_archive_frames(archive, framebuffers, anchors, frame_count);
    }
    for (const auto& [index_prefix, data_prefix, count] : {
             std::tuple{"SDX", "SMP", 84}, std::tuple{"WDX", "WMP", 26}}) {
        for (int index = 0; index < count; ++index) {
            const auto suffix = three_digit_suffix(index);
            const auto index_file = read_binary_file(root / (std::string{index_prefix} + suffix));
            auto data_file = read_binary_file(root / (std::string{data_prefix} + suffix));
            OL_CHECK(static_cast<bool>(index_file));
            OL_CHECK(static_cast<bool>(data_file));
            if (!index_file || !data_file) {
                continue;
            }
            const auto archive = SentinelArchive::parse(index_file.bytes, std::move(data_file.bytes));
            draw_all_archive_frames(archive, framebuffers, anchors, frame_count);
        }
    }
    OL_CHECK(frame_count == 78'014U);

    auto hash = std::uint64_t{0xCBF29CE484222325ULL};
    for (const auto& framebuffer : framebuffers) {
        for (const auto byte : framebuffer.pixels()) {
            hash ^= byte;
            hash *= 0x100000001B3ULL;
        }
    }
    if (hash != 0xFCE6BF593964E433ULL) {
        std::cerr << "all-sprite corner golden mismatch: 0x" << std::hex << hash << '\n';
    }
    OL_CHECK(hash == 0xFCE6BF593964E433ULL);
}

}  // namespace

int main() {
    run_framebuffer_tests();
    run_depth_order_tests();
    run_effect_tests();
    run_synthetic_sprite_tests();
    run_glyph_write_tests();
    run_text_encoding_tests();
    run_real_asset_golden();
    run_real_palette_fade_golden();
    run_all_glyph_golden();
    run_all_sprite_corner_golden();
    return openlegend::test::failures == 0 ? 0 : 1;
}
