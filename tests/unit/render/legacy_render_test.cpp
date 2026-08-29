#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "openlegend/render/indexed_framebuffer.hpp"
#include "openlegend/render/legacy_effects.hpp"
#include "openlegend/render/legacy_font_renderer.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/render/world_depth_order.hpp"
#include "openlegend/render/world_projection.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/resource/packed_archive.hpp"
#include "test_support.hpp"

#ifndef OPENLEGEND_GAME_DATA_ROOT
#error OPENLEGEND_GAME_DATA_ROOT must name the read-only original data directory
#endif

namespace {

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
    IndexedFramebuffer framebuffer;
    framebuffer.clear(3U);
    OL_CHECK(framebuffer.pixels().front() == 3U);
    OL_CHECK(framebuffer.pixels().back() == 3U);
    OL_CHECK(framebuffer.fill_rectangle(10, 20, 3U, 2U, 9U));
    OL_CHECK(framebuffer.row(20)[9] == 3U);
    OL_CHECK(framebuffer.row(20)[10] == 9U);
    OL_CHECK(framebuffer.row(21)[12] == 9U);
    OL_CHECK(framebuffer.row(22)[10] == 3U);
    OL_CHECK(!framebuffer.fill_rectangle(-1, 0, 1U, 1U, 0U));
    OL_CHECK(!framebuffer.fill_rectangle(319, 199, 2U, 1U, 0U));

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
    owner_x[index(0, 2)] = 40;
    owner_y[index(0, 2)] = 50;
    sprites[index(8, 18)] = 100;
    sprites[index(9, 18)] = 200;

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
    OL_CHECK(result.entries.size() == 3U);
    if (result.entries.size() == 3U) {
        OL_CHECK(result.entries[0] == (LegacyDepthEntry{41, 50, 200}));
        OL_CHECK(result.entries[1] == (LegacyDepthEntry{40, 50, 100}));
        OL_CHECK(result.entries[2] == (LegacyDepthEntry{60, 61, 5000}));
    }

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
}

void run_synthetic_sprite_tests() {
    using namespace openlegend::render;
    using openlegend::resource::SpriteFrameView;

    const std::array<std::uint8_t, 14> bytes{
        4U, 0U, 2U, 0U, 1U, 0U, 0xFFU, 0xFFU,
        4U, 1U, 2U, 7U, 8U,
        0U};
    OL_CHECK(legacy_sprite_index(0U) == 0U);
    OL_CHECK(legacy_sprite_index(1U) == 0U);
    OL_CHECK(legacy_sprite_index(2U) == 1U);
    OL_CHECK(legacy_sprite_index(0x7FFEU) == 0x3FFFU);
    OL_CHECK(!legacy_sprite_index(0x7FFFU));

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
    OL_CHECK(draw_ascii_glyph(framebuffer, 10, 10, ascii, 12U, 250U));
    OL_CHECK(framebuffer.row(10)[10] == 250U);
    OL_CHECK(framebuffer.row(10)[11] == 250U);
    OL_CHECK(framebuffer.row(10)[12] == 12U);

    std::array<std::uint8_t, 32> big5{};
    big5[0] = 0x80U;
    big5[1] = 0x80U;
    framebuffer.clear(0U);
    OL_CHECK(draw_big5_glyph(framebuffer, 20, 20, big5, 5U, 6U));
    OL_CHECK(framebuffer.row(20)[20] == 6U);
    OL_CHECK(framebuffer.row(20)[21] == 5U);
    OL_CHECK(framebuffer.row(20)[28] == 6U);
    OL_CHECK(framebuffer.row(20)[29] == 5U);
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

void run_real_asset_golden() {
    using namespace openlegend::render;
    using namespace openlegend::resource;
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);

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
        framebuffer.set_palette(palette.palette);
        OL_CHECK(framebuffer.palette()[0].red == palette.palette[0].red);
        OL_CHECK(framebuffer.palette()[0].green == palette.palette[0].green);
        OL_CHECK(framebuffer.palette()[0].blue == palette.palette[0].blue);
        OL_CHECK(framebuffer.palette()[255].red == palette.palette[255].red);
        OL_CHECK(framebuffer.palette()[255].green == palette.palette[255].green);
        OL_CHECK(framebuffer.palette()[255].blue == palette.palette[255].blue);
    }

    const auto mmap_frame = frame_zero(mmap);
    const auto title_frame = frame_zero(title);
    const auto cloud_frame = frame_zero(cloud);
    draw_rle_sprite(framebuffer, mmap_frame, 10, 10);
    draw_rle_sprite(framebuffer, title_frame, 238, 160);
    draw_rle_sprite(framebuffer, cloud_frame, 160, 40);

    Big5GlyphCache cache{big5_font.bytes};
    const std::array<std::uint8_t, 6> text{'A', '_', 'Z', 0xA4U, 0x40U, 0U};
    OL_CHECK(draw_legacy_text(framebuffer, 20, 80, text, ascii_font.bytes, cache, 12U, 250U));
    OL_CHECK(cache.next_replacement_slot() == 1U);
    OL_CHECK(static_cast<bool>(cache.resolve(0xA440U)));
    OL_CHECK(cache.next_replacement_slot() == 1U);

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
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
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
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
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
        OL_CHECK(draw_ascii_glyph(framebuffer, 0, 0, glyph, 12U, 250U));
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
            OL_CHECK(draw_big5_glyph(framebuffer, 0, 0, *glyph, 12U, 250U));
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
    const auto root = openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT);
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
    run_real_asset_golden();
    run_real_palette_fade_golden();
    run_all_glyph_golden();
    run_all_sprite_corner_golden();
    return openlegend::test::failures == 0 ? 0 : 1;
}
