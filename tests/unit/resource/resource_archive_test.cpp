#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "openlegend/resource/binary_file.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/resource/packed_archive.hpp"
#include "test_support.hpp"

#ifndef OPENLEGEND_GAME_DATA_ROOT
#error OPENLEGEND_GAME_DATA_ROOT must name the read-only original data directory
#endif

namespace {

void append_u32le(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 16U));
    bytes.push_back(static_cast<std::uint8_t>(value >> 24U));
}

std::string uppercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

void run_synthetic_archive_tests() {
    using namespace openlegend::resource;

    std::vector<std::uint8_t> index;
    append_u32le(index, 3U);
    append_u32le(index, 5U);
    auto archive = PackedArchive::parse(index, {1U, 2U, 3U, 4U, 5U});
    OL_CHECK(archive.valid());
    OL_CHECK(archive.entry_count() == 2U);
    OL_CHECK(archive.entry(0U).size() == 3U);
    OL_CHECK(archive.entry(1U)[1U] == 5U);

    index.clear();
    append_u32le(index, 2U);
    append_u32le(index, 5U);
    append_u32le(index, 0U);
    auto sentinel = SentinelArchive::parse(index, {1U, 2U, 3U, 4U, 5U, 6U, 7U});
    OL_CHECK(sentinel.valid());
    OL_CHECK(sentinel.entry_count() == 3U);
    OL_CHECK(sentinel.entry(2U).size() == 2U);

    index.clear();
    append_u32le(index, 4U);
    append_u32le(index, 3U);
    archive = PackedArchive::parse(index, {1U, 2U, 3U, 4U});
    OL_CHECK(!archive.valid());
}

void run_synthetic_sprite_tests() {
    using namespace openlegend::resource;

    const std::array<std::uint8_t, 14> bytes{
        4U, 0U, 2U, 0U, 1U, 0U, 0xFFU, 0xFFU,
        4U, 1U, 2U, 7U, 8U,
        0U};
    const auto frame = SpriteFrameView::parse(bytes);
    OL_CHECK(frame.valid());
    OL_CHECK(frame.width() == 4U);
    OL_CHECK(frame.height() == 2U);
    OL_CHECK(frame.x_offset() == 1);
    OL_CHECK(frame.y_offset() == -1);
    OL_CHECK(frame.rows().size() == 2U);
    OL_CHECK(frame.rows()[0].runs.size() == 1U);
    OL_CHECK(frame.rows()[0].runs[0].pixels[1U] == 8U);
}

void validate_sprite_archive(const openlegend::resource::PackedArchive& archive, std::size_t& frames) {
    for (std::size_t index = 0U; index < archive.entry_count(); ++index) {
        const auto entry = archive.entry(index);
        if (entry.empty()) {
            continue;
        }
        const auto frame = openlegend::resource::SpriteFrameView::parse(entry);
        if (!frame.valid()) {
            std::cerr << "sprite frame " << index << " failed: " << frame.error() << '\n';
        }
        OL_CHECK(frame.valid());
        ++frames;
    }
}

void validate_sentinel_sprite_pair(
    const std::filesystem::path& index_path,
    const std::filesystem::path& group_path,
    std::size_t& frames) {
    using namespace openlegend::resource;
    const auto index = read_binary_file(index_path);
    auto group = read_binary_file(group_path);
    OL_CHECK(static_cast<bool>(index));
    OL_CHECK(static_cast<bool>(group));
    if (!index || !group) {
        return;
    }
    const auto archive = SentinelArchive::parse(index.bytes, std::move(group.bytes));
    if (!archive.valid()) {
        std::cerr << index_path.filename().string() << ": " << archive.error() << '\n';
    }
    OL_CHECK(archive.valid());
    for (std::size_t entry_index = 0U; entry_index < archive.entry_count(); ++entry_index) {
        const auto entry = archive.entry(entry_index);
        if (entry.empty()) {
            continue;
        }
        const auto frame = SpriteFrameView::parse(entry);
        if (!frame.valid()) {
            std::cerr << index_path.filename().string() << " frame " << entry_index << ": "
                      << frame.error() << '\n';
        }
        OL_CHECK(frame.valid());
        ++frames;
    }
}

void run_real_asset_tests() {
    using namespace openlegend::resource;
    const std::filesystem::path root{OPENLEGEND_GAME_DATA_ROOT};
    OL_CHECK(std::filesystem::is_directory(root));

    std::map<std::string, std::filesystem::path> indexes;
    std::map<std::string, std::filesystem::path> groups;
    for (const auto& item : std::filesystem::directory_iterator(root)) {
        if (!item.is_regular_file()) {
            continue;
        }
        const auto extension = uppercase(item.path().extension().string());
        const auto stem = uppercase(item.path().stem().string());
        if (extension == ".IDX") {
            indexes.emplace(stem, item.path());
        } else if (extension == ".GRP") {
            groups.emplace(stem, item.path());
        }
    }

    const std::set<std::string> sprite_archives{
        "CLOUD", "EFT", "ENDWORD", "FBK", "FMAP", "HDGRP", "MMAP", "TITLE"};
    std::size_t pair_count = 0U;
    std::size_t sprite_frames = 0U;
    for (const auto& [stem, index_path] : indexes) {
        const auto group = groups.find(stem);
        if (group == groups.end()) {
            continue;
        }
        const auto archive = PackedArchive::open(index_path, group->second);
        if (!archive.valid()) {
            std::cerr << stem << ": " << archive.error() << '\n';
        }
        OL_CHECK(archive.valid());
        if (!archive.valid()) {
            continue;
        }
        ++pair_count;
        if (stem.starts_with("FIGHT") || sprite_archives.contains(stem)) {
            validate_sprite_archive(archive, sprite_frames);
        }
        if (stem == "MMAP") {
            OL_CHECK(archive.entry_count() == 3731U);
        }
    }
    OL_CHECK(pair_count == 118U);
    OL_CHECK(sprite_frames == 12'927U);

    std::size_t sentinel_frames = 0U;
    for (int index = 0; index < 84; ++index) {
        const auto suffix = (index < 10 ? "00" : "0") + std::to_string(index);
        validate_sentinel_sprite_pair(root / ("SDX" + suffix), root / ("SMP" + suffix), sentinel_frames);
    }
    for (int index = 0; index < 26; ++index) {
        const auto suffix = (index < 10 ? "00" : "0") + std::to_string(index);
        validate_sentinel_sprite_pair(root / ("WDX" + suffix), root / ("WMP" + suffix), sentinel_frames);
    }
    OL_CHECK(sentinel_frames == 65'087U);

    const auto palette_file = read_binary_file(root / "MMAP.COL");
    OL_CHECK(static_cast<bool>(palette_file));
    if (palette_file) {
        OL_CHECK(static_cast<bool>(parse_vga_palette(palette_file.bytes)));
    }

    const auto ascii_font = read_binary_file(root / "FONT3.E16");
    const auto big5_font = read_binary_file(root / "FONT3.C16");
    OL_CHECK(static_cast<bool>(ascii_font));
    OL_CHECK(static_cast<bool>(big5_font));
    if (ascii_font && big5_font) {
        const GlyphArchiveView ascii_glyphs{ascii_font.bytes, 16U};
        const GlyphArchiveView big5_glyphs{big5_font.bytes, 32U};
        OL_CHECK(ascii_glyphs.glyph_count() == 128U);
        OL_CHECK(big5_glyphs.glyph_count() == 13'973U);
    }

    for (const auto* name : {"EARTH.002", "SURFACE.002", "BUILDING.002", "BUILDX.002", "BUILDY.002"}) {
        const auto layer = read_binary_file(root / name);
        OL_CHECK(static_cast<bool>(layer));
        if (layer) {
            const Int16FileView view{layer.bytes};
            OL_CHECK(view.valid());
            OL_CHECK(view.size() == 480U * 480U);
        }
    }

    std::cout << "validated " << pair_count << " IDX/GRP pairs, " << sprite_frames
              << " packed sprite frames, and " << sentinel_frames << " sentinel sprite frames\n";
}

}  // namespace

int main() {
    run_synthetic_archive_tests();
    run_synthetic_sprite_tests();
    run_real_asset_tests();
    return openlegend::test::failures == 0 ? 0 : 1;
}
