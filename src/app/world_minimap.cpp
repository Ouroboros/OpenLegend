#include "world_minimap.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <zstd.h>

#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/model/game_snapshot.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/resource/packed_archive.hpp"
#include "openlegend/text/big5.hpp"
#include "openlegend/text/game_strings.hpp"

namespace openlegend::app {
namespace {

// 底图按大地图原尺寸绘制后每4×4像素取平均保存；格(x,y)锚点位于原尺寸
// ((x-y+479)×18, (x+y)×9)，再加上贴图伸出锚点外的边距。
constexpr int kDownsample = 4;
constexpr int kTileHalfWidth = 18;
constexpr int kTileHalfHeight = 9;
constexpr int kDiagonalCount = world::kWorldExtent * 2 - 1;
constexpr int kStripRows = 64 * kDownsample;
constexpr compat::Rgba8 kBackground{0U, 0U, 0U, 0xFFU};
constexpr compat::Rgba8 kPersonColor{0xF0U, 0x30U, 0x30U, 0xFFU};
constexpr compat::Rgba8 kShipColor{0x30U, 0xE0U, 0xFFU, 0xFFU};
constexpr compat::Rgba8 kEntranceColor{0xFFU, 0xE8U, 0x60U, 0xFFU};
constexpr compat::Rgba8 kOutlineColor{0x10U, 0x10U, 0x10U, 0xFFU};
// 文字：地名白色、标题黄色，均带黑色阴影，不随游戏调色板变化。
constexpr compat::Rgba8 kTextShadow{0x00U, 0x00U, 0x00U, 0xFFU};
constexpr render::rgba::TextColors kLabelText{kTextShadow, {0xFFU, 0xFFU, 0xFFU, 0xFFU}};
constexpr render::rgba::TextColors kTitleText{kTextShadow, {0xFFU, 0xE0U, 0x40U, 0xFFU}};
constexpr render::rgba::TextColors kHintText{kTextShadow, {0xE8U, 0xE8U, 0xE8U, 0xFFU}};
constexpr render::rgba::TextColors kSelectedLabelText{kTextShadow, {0xFFU, 0xE0U, 0x40U, 0xFFU}};
constexpr compat::Rgba8 kSelectedColor{0xFFU, 0xE0U, 0x40U, 0xFFU};
constexpr render::rgba::FontSize kTitleFont{4};
constexpr std::uint16_t kLabelReferenceFont = 3U;
// 缩放以千分比表示：1000为整图，每次放大或缩小5%，最高8倍。
constexpr int kZoomFit = 1000;
constexpr int kMaximumZoom = 8000;
constexpr int kZoomStepPercent = 105;
// 首次打开的缩放：从整图连按20次放大。
constexpr int kInitialZoomSteps = 20;
NODISCARD constexpr int initial_zoom() noexcept {
    int zoom = kZoomFit;
    for (int step = 0; step < kInitialZoomSteps; ++step) {
        zoom = std::min(std::max(zoom * kZoomStepPercent / 100, zoom + 1), kMaximumZoom);
    }
    return zoom;
}
// 缓存文件头：魔数、资源哈希、底图宽高与锚点边距，其后为RGB像素的zstd帧。
constexpr std::array<std::uint8_t, 8> kCacheMagic{'O', 'L', 'W', 'M', 'A', 'P', '0', '1'};
constexpr std::size_t kCacheHeaderBytes = 32U;
constexpr int kMaximumImageExtent = 16384;
constexpr std::array<std::string_view, 6> kSourceFiles{
    "EARTH.002", "SURFACE.002", "BUILDING.002", "MMAP.IDX", "MMAP.GRP", "MMAP.COL"};

// 底图依赖的资源内容哈希（FNV-1a 64），资源变化即缓存失效。
NODISCARD std::optional<std::uint64_t> source_key(const resource::DataRoot& data_root) {
    std::uint64_t hash = 0xCBF29CE484222325ULL;
    const auto mix = [&hash](const std::uint8_t byte) {
        hash ^= byte;
        hash *= 0x100000001B3ULL;
    };
    for (const auto name : kSourceFiles) {
        const auto file = data_root.read(std::filesystem::path{name});
        if (!file) {
            return std::nullopt;
        }
        for (const auto character : name) {
            mix(static_cast<std::uint8_t>(character));
        }
        for (const auto byte : file.bytes) {
            mix(byte);
        }
    }
    return hash;
}

void write_u64(std::vector<std::uint8_t>& bytes, const std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void write_u32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

NODISCARD std::uint64_t read_u64(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    std::uint64_t value = 0U;
    for (std::size_t index = 0U; index < 8U; ++index) {
        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (8U * index);
    }
    return value;
}

NODISCARD std::uint32_t read_u32(const std::vector<std::uint8_t>& bytes, const std::size_t offset) {
    std::uint32_t value = 0U;
    for (std::size_t index = 0U; index < 4U; ++index) {
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (8U * index);
    }
    return value;
}

NODISCARD std::span<const std::uint8_t> scene_name(const model::SceneMetadataRecord& scene) {
    const auto field = std::span<const std::uint8_t>{scene.bytes}.subspan(
        model::scene_metadata_word::name_byte, model::scene_metadata_word::name_bytes);
    const auto end = std::find(field.begin(), field.end(), std::uint8_t{0U});
    return field.first(static_cast<std::size_t>(end - field.begin()));
}

void put_physical_pixel(
    render::RgbaFramebuffer& framebuffer, const int x, const int y, const compat::Rgba8 color) {
    if (x < 0 || y < 0 || x >= framebuffer.pixel_width() || y >= framebuffer.pixel_height()) {
        return;
    }
    auto* pixel = framebuffer.row(y) + static_cast<std::size_t>(x) * compat::kModernRgbaBytesPerPixel;
    pixel[0] = color.red;
    pixel[1] = color.green;
    pixel[2] = color.blue;
    pixel[3] = color.alpha;
}

// 以(x,y)为中心画带深色描边的实心方块，size为物理像素边长。
void draw_marker(
    render::RgbaFramebuffer& framebuffer, const int x, const int y, const int size,
    const compat::Rgba8 color) {
    const auto half = size / 2;
    for (int dy = -half - 1; dy <= half + 1; ++dy) {
        for (int dx = -half - 1; dx <= half + 1; ++dx) {
            const bool border = dx < -half || dx > half || dy < -half || dy > half;
            put_physical_pixel(framebuffer, x + dx, y + dy, border ? kOutlineColor : color);
        }
    }
}

// 把贴图画进原尺寸条带（strip_top起kStripRows行），超出条带或画布的部分裁掉。
void draw_strip_sprite(
    std::vector<std::uint8_t>& strip, const int canvas_width, const int strip_top,
    const resource::SpriteFrameView& frame, const int anchor_x, const int anchor_y) {
    const auto left = anchor_x - static_cast<int>(frame.x_offset());
    const auto top = anchor_y - static_cast<int>(frame.y_offset());
    const auto& rows = frame.rows();
    for (std::size_t row_index = 0U; row_index < rows.size(); ++row_index) {
        const auto strip_y = top + static_cast<int>(row_index) - strip_top;
        if (strip_y < 0 || strip_y >= kStripRows) {
            continue;
        }
        auto* destination = strip.data() +
            static_cast<std::size_t>(strip_y) * static_cast<std::size_t>(canvas_width);
        auto x = left;
        for (const auto& run : rows[row_index].runs) {
            x += static_cast<int>(run.skip);
            for (std::size_t index = 0U; index < run.pixels.size(); ++index) {
                const auto target_x = x + static_cast<int>(index);
                if (target_x >= 0 && target_x < canvas_width) {
                    destination[target_x] = run.pixels[index];
                }
            }
            x += static_cast<int>(run.pixels.size());
        }
    }
}

}  // namespace

WorldMinimap::WorldMinimap(const resource::DataRoot& data_root,
    const world::WorldMapData& map, const std::filesystem::path& cache_path)
    : zoom_(initial_zoom()) {
    const auto key = source_key(data_root);
    if (key.has_value() && load_cache(cache_path, *key)) {
        diagnostics::log_info("world minimap loaded from cache path=" + cache_path.string());
        return;
    }
    build(data_root, map);
    if (valid() && key.has_value()) {
        save_cache(cache_path, *key);
    }
}

bool WorldMinimap::load_cache(const std::filesystem::path& path, const std::uint64_t key) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return false;
    }
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (bytes.size() <= kCacheHeaderBytes ||
        !std::equal(kCacheMagic.begin(), kCacheMagic.end(), bytes.begin()) ||
        read_u64(bytes, 8U) != key) {
        return false;
    }
    const auto width = static_cast<int>(read_u32(bytes, 16U));
    const auto height = static_cast<int>(read_u32(bytes, 20U));
    const auto anchor_left = static_cast<int>(read_u32(bytes, 24U));
    const auto anchor_top = static_cast<int>(read_u32(bytes, 28U));
    if (width <= 0 || height <= 0 || width > kMaximumImageExtent ||
        height > kMaximumImageExtent) {
        return false;
    }
    const auto pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const auto* frame = bytes.data() + kCacheHeaderBytes;
    const auto frame_size = bytes.size() - kCacheHeaderBytes;
    if (ZSTD_getFrameContentSize(frame, frame_size) != pixel_count * 3U) {
        return false;
    }
    std::vector<std::uint8_t> rgb(pixel_count * 3U);
    const auto size = ZSTD_decompress(rgb.data(), rgb.size(), frame, frame_size);
    if (ZSTD_isError(size) || size != rgb.size()) {
        return false;
    }
    image_.resize(pixel_count);
    for (std::size_t index = 0U; index < pixel_count; ++index) {
        image_[index] = {rgb[index * 3U], rgb[index * 3U + 1U], rgb[index * 3U + 2U], 0xFFU};
    }
    image_width_ = width;
    image_height_ = height;
    anchor_left_ = anchor_left;
    anchor_top_ = anchor_top;
    center_x_ = image_width_ / 2;
    center_y_ = image_height_ / 2;
    return true;
}

void WorldMinimap::save_cache(const std::filesystem::path& path, const std::uint64_t key) const {
    std::vector<std::uint8_t> rgb;
    rgb.reserve(image_.size() * 3U);
    for (const auto& color : image_) {
        rgb.push_back(color.red);
        rgb.push_back(color.green);
        rgb.push_back(color.blue);
    }
    std::vector<std::uint8_t> bytes(kCacheMagic.begin(), kCacheMagic.end());
    write_u64(bytes, key);
    write_u32(bytes, static_cast<std::uint32_t>(image_width_));
    write_u32(bytes, static_cast<std::uint32_t>(image_height_));
    write_u32(bytes, static_cast<std::uint32_t>(anchor_left_));
    write_u32(bytes, static_cast<std::uint32_t>(anchor_top_));
    const auto bound = ZSTD_compressBound(rgb.size());
    bytes.resize(kCacheHeaderBytes + bound);
    const auto size = ZSTD_compress(
        bytes.data() + kCacheHeaderBytes, bound, rgb.data(), rgb.size(), 3);
    if (ZSTD_isError(size)) {
        diagnostics::log_error("world minimap cache compression failed");
        return;
    }
    bytes.resize(kCacheHeaderBytes + size);
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream output{temporary, std::ios::binary | std::ios::trunc};
        output.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            diagnostics::log_error("world minimap cache write failed path=" + temporary.string());
            return;
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(path, error);
        std::filesystem::rename(temporary, path, error);
    }
    if (error) {
        diagnostics::log_error("world minimap cache rename failed path=" + path.string());
        std::filesystem::remove(temporary, error);
        return;
    }
    diagnostics::log_info("world minimap cache saved path=" + path.string() +
        " bytes=" + std::to_string(bytes.size()));
}

void WorldMinimap::build(const resource::DataRoot& data_root, const world::WorldMapData& map) {
    if (!map.valid()) {
        error_ = "world minimap requires valid world map data";
        return;
    }
    const auto started = std::chrono::steady_clock::now();
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
    const auto sprites = resource::PackedArchive::open(
        data_root.path() / "MMAP.IDX", data_root.path() / "MMAP.GRP");
    if (!sprites.valid()) {
        error_ = sprites.error();
        return;
    }

    // 解析用到的贴图并统计伸出锚点的最大范围，用于确定画布边距与条带覆盖范围。
    std::vector<std::optional<resource::SpriteFrameView>> frames(sprites.entry_count());
    int extent_left = kTileHalfWidth;
    int extent_right = kTileHalfWidth;
    int extent_up = kTileHalfHeight;
    int extent_down = kTileHalfHeight;
    const auto frame_of = [&](const std::int16_t legacy_id) -> const resource::SpriteFrameView* {
        if (legacy_id <= 0) {
            return nullptr;
        }
        const auto index = render::legacy_sprite_index(static_cast<std::uint32_t>(legacy_id));
        if (!index.has_value() || *index >= frames.size()) {
            return nullptr;
        }
        auto& frame = frames[*index];
        if (!frame.has_value()) {
            frame.emplace(resource::SpriteFrameView::parse(sprites.entry(*index)));
            if (frame->valid()) {
                extent_left = std::max(extent_left, static_cast<int>(frame->x_offset()));
                extent_right = std::max(extent_right,
                    static_cast<int>(frame->width()) - static_cast<int>(frame->x_offset()));
                extent_up = std::max(extent_up, static_cast<int>(frame->y_offset()));
                extent_down = std::max(extent_down,
                    static_cast<int>(frame->height()) - static_cast<int>(frame->y_offset()));
            }
        }
        return frame->valid() ? &*frame : nullptr;
    };
    constexpr std::array<world::WorldLayer, 3> kLayers{
        world::WorldLayer::earth, world::WorldLayer::surface, world::WorldLayer::building};
    for (const auto layer : kLayers) {
        for (const auto value : map.layer(layer)) {
            static_cast<void>(frame_of(value));
        }
    }

    anchor_left_ = extent_left;
    anchor_top_ = extent_up;
    const auto canvas_width = (kDiagonalCount - 1) * kTileHalfWidth + extent_left + extent_right;
    const auto canvas_height = (kDiagonalCount - 1) * kTileHalfHeight + extent_up + extent_down;
    image_width_ = (canvas_width + kDownsample - 1) / kDownsample;
    image_height_ = (canvas_height + kDownsample - 1) / kDownsample;
    image_.assign(static_cast<std::size_t>(image_width_) * static_cast<std::size_t>(image_height_),
        kBackground);

    std::array<std::array<std::uint16_t, 3>, compat::kLegacyPaletteSize> colors{};
    for (std::size_t index = 0U; index < colors.size(); ++index) {
        colors[index] = {compat::expand_rgb6(palette.palette[index].red),
            compat::expand_rgb6(palette.palette[index].green),
            compat::expand_rgb6(palette.palette[index].blue)};
    }

    // 按条带绘制原尺寸画面：先地面、再表面、最后建筑，各层按对角线由远及近，
    // 每条带画完即按4×4取平均写入底图，避免持有整张原尺寸画布。
    const auto padded_width = image_width_ * kDownsample;
    const auto padded_width_bytes = static_cast<std::size_t>(padded_width);
    std::vector<std::uint8_t> strip(padded_width_bytes * static_cast<std::size_t>(kStripRows));
    for (int strip_top = 0; strip_top < image_height_ * kDownsample; strip_top += kStripRows) {
        std::fill(strip.begin(), strip.end(), std::uint8_t{0U});
        const auto first_diagonal = std::max(0,
            (strip_top - extent_down - anchor_top_) / kTileHalfHeight - 1);
        const auto last_diagonal = std::min(kDiagonalCount - 1,
            (strip_top + kStripRows + extent_up - anchor_top_) / kTileHalfHeight + 1);
        for (const auto layer : kLayers) {
            for (int diagonal = first_diagonal; diagonal <= last_diagonal; ++diagonal) {
                const auto first_x = std::max(0, diagonal - (world::kWorldExtent - 1));
                const auto last_x = std::min(world::kWorldExtent - 1, diagonal);
                for (int x = first_x; x <= last_x; ++x) {
                    const auto y = diagonal - x;
                    const auto* frame = frame_of(map.at(layer, x, y));
                    if (frame == nullptr) {
                        continue;
                    }
                    draw_strip_sprite(strip, padded_width, strip_top, *frame,
                        (x - y + world::kWorldExtent - 1) * kTileHalfWidth + anchor_left_,
                        diagonal * kTileHalfHeight + anchor_top_);
                }
            }
        }
        for (int row = 0; row < kStripRows / kDownsample; ++row) {
            const auto image_row = strip_top / kDownsample + row;
            if (image_row >= image_height_) {
                break;
            }
            for (int column = 0; column < image_width_; ++column) {
                std::array<std::uint32_t, 3> totals{};
                for (int dy = 0; dy < kDownsample; ++dy) {
                    const auto* source = strip.data() +
                        static_cast<std::size_t>(row * kDownsample + dy) * padded_width_bytes +
                        static_cast<std::size_t>(column * kDownsample);
                    for (int dx = 0; dx < kDownsample; ++dx) {
                        const auto& color = colors[source[dx]];
                        totals[0] += color[0];
                        totals[1] += color[1];
                        totals[2] += color[2];
                    }
                }
                constexpr auto kSamples = static_cast<std::uint32_t>(kDownsample * kDownsample);
                image_[static_cast<std::size_t>(image_row) * static_cast<std::size_t>(image_width_) +
                    static_cast<std::size_t>(column)] = {
                    static_cast<std::uint8_t>(totals[0] / kSamples),
                    static_cast<std::uint8_t>(totals[1] / kSamples),
                    static_cast<std::uint8_t>(totals[2] / kSamples),
                    0xFFU};
            }
        }
    }
    center_x_ = image_width_ / 2;
    center_y_ = image_height_ / 2;
    diagnostics::log_info("world minimap built size=" + std::to_string(image_width_) + "x" +
        std::to_string(image_height_) + " ms=" +
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count()));
}

std::pair<int, int> WorldMinimap::image_position(const int world_x, const int world_y) const noexcept {
    return {((world_x - world_y + world::kWorldExtent - 1) * kTileHalfWidth + anchor_left_) /
                kDownsample,
        ((world_x + world_y) * kTileHalfHeight + anchor_top_) / kDownsample};
}

// 有名字且入口在地图内的场景入口坐标。
NODISCARD std::optional<std::pair<int, int>> named_entrance(const model::SceneMetadataRecord& scene) {
    const auto entrance_x = scene.word(model::scene_metadata_word::main_entrance_x_1);
    const auto entrance_y = scene.word(model::scene_metadata_word::main_entrance_y_1);
    if (scene_name(scene).empty() || entrance_x <= 0 || entrance_y <= 0 ||
        entrance_x >= world::kWorldExtent || entrance_y >= world::kWorldExtent) {
        return std::nullopt;
    }
    return std::pair<int, int>{entrance_x, entrance_y};
}

std::optional<std::size_t> WorldMinimap::selected_scene(
    const model::RuntimeRangerState& ranger) const {
    std::optional<std::size_t> best;
    std::int64_t best_distance = 0;
    for (std::size_t index = 0U; index < ranger.scenes.size(); ++index) {
        const auto entrance = named_entrance(ranger.scenes[index]);
        if (!entrance.has_value()) {
            continue;
        }
        const auto [x, y] = image_position(entrance->first, entrance->second);
        const auto dx = static_cast<std::int64_t>(x) - center_x_;
        const auto dy = static_cast<std::int64_t>(y) - center_y_;
        const auto distance = dx * dx + dy * dy;
        if (!best.has_value() || distance < best_distance) {
            best = index;
            best_distance = distance;
        }
    }
    return best;
}

void WorldMinimap::center_on(const int world_x, const int world_y) noexcept {
    const auto [x, y] = image_position(world_x, world_y);
    center_x_ = x;
    center_y_ = y;
    clamp_center();
}

void WorldMinimap::zoom_in() noexcept {
    zoom_ = std::min(std::max(zoom_ * kZoomStepPercent / 100, zoom_ + 1), kMaximumZoom);
    clamp_center();
}

void WorldMinimap::zoom_out() noexcept {
    zoom_ = std::max(zoom_ * 100 / kZoomStepPercent, kZoomFit);
    clamp_center();
}

void WorldMinimap::pan(const double horizontal_views, const double vertical_views) noexcept {
    const auto view_height =
        static_cast<double>(image_height_) * kZoomFit / static_cast<double>(zoom_);
    pan_remainder_x_ += horizontal_views * view_height;
    pan_remainder_y_ += vertical_views * view_height;
    const auto whole_x = static_cast<int>(pan_remainder_x_);
    const auto whole_y = static_cast<int>(pan_remainder_y_);
    pan_remainder_x_ -= whole_x;
    pan_remainder_y_ -= whole_y;
    center_x_ += whole_x;
    center_y_ += whole_y;
    clamp_center();
}

// 视野中心可到底图任意位置（含边缘），以便任何缩放下都能以人物为中心；底图外画黑色。
void WorldMinimap::clamp_center() noexcept {
    center_x_ = std::clamp(center_x_, 0, image_width_);
    center_y_ = std::clamp(center_y_, 0, image_height_);
}

bool WorldMinimap::render(
    ui::ModernUiRenderer& ui_renderer,
    const model::RuntimeRangerState& ranger,
    const std::pair<int, int> person,
    const std::optional<std::pair<int, int>> ship,
    const bool teleport_available,
    render::RgbaFramebuffer& framebuffer) const {
    if (!valid() || image_width_ <= 0 || image_height_ <= 0) {
        return false;
    }
    const auto scale = framebuffer.scale();
    const auto logical_width = framebuffer.logical_width();
    const auto logical_height = framebuffer.logical_height();
    if (!framebuffer.fill_rectangle(0, 0, static_cast<std::uint16_t>(logical_width),
            static_cast<std::uint16_t>(logical_height), kBackground)) {
        return false;
    }

    // 顶部一行标题与说明，其下按底图宽高比放到最大并居中。
    const auto title_metrics = ui_renderer.font_metrics(framebuffer, kTitleFont);
    const auto margin = std::max(2, logical_height / 50);
    const auto header = margin + static_cast<int>(title_metrics.line_height);
    const auto pixel_width = framebuffer.pixel_width();
    const auto pixel_height = framebuffer.pixel_height();
    const auto available_width = pixel_width - 2 * margin * scale;
    const auto available_height = pixel_height - (header + 2 * margin) * scale;
    const auto destination_width = static_cast<int>(std::min<std::int64_t>(available_width,
        static_cast<std::int64_t>(available_height) * image_width_ / image_height_));
    const auto destination_height = static_cast<int>(
        static_cast<std::int64_t>(destination_width) * image_height_ / image_width_);
    if (destination_width <= 0 || destination_height <= 0) {
        return false;
    }
    const auto left = (pixel_width - destination_width) / 2;
    const auto top = (header + margin) * scale + (available_height - destination_height) / 2;
    // 可见源范围：缩放后的宽高，左上角由视野中心推得。
    const auto source_left = center_x_ -
        static_cast<std::int64_t>(image_width_) * kZoomFit / (2 * zoom_);
    const auto source_top = center_y_ -
        static_cast<std::int64_t>(image_height_) * kZoomFit / (2 * zoom_);
    const auto zoomed_width = static_cast<std::int64_t>(destination_width) * zoom_ / kZoomFit;
    const auto zoomed_height = static_cast<std::int64_t>(destination_height) * zoom_ / kZoomFit;
    for (int row = 0; row < destination_height; ++row) {
        const auto source_row =
            source_top + static_cast<std::int64_t>(row) * image_height_ / zoomed_height;
        auto* target = framebuffer.row(top + row) +
            static_cast<std::size_t>(left) * compat::kModernRgbaBytesPerPixel;
        const bool row_inside = source_row >= 0 && source_row < image_height_;
        const auto* source = image_.data() +
            static_cast<std::size_t>(row_inside ? source_row : 0) *
                static_cast<std::size_t>(image_width_);
        for (int column = 0; column < destination_width; ++column) {
            const auto source_column = source_left +
                static_cast<std::int64_t>(column) * image_width_ / zoomed_width;
            const auto color = row_inside && source_column >= 0 && source_column < image_width_
                ? source[source_column] : kBackground;
            target[0] = color.red;
            target[1] = color.green;
            target[2] = color.blue;
            target[3] = color.alpha;
            target += compat::kModernRgbaBytesPerPixel;
        }
    }

    // 格(x,y)锚点投影到物理像素坐标；落在可见范围外时返回空。
    const auto project = [&](const int x, const int y) -> std::optional<std::pair<int, int>> {
        const auto [image_x, image_y] = image_position(x, y);
        const auto screen_x = (static_cast<std::int64_t>(image_x) - source_left) * zoomed_width /
            image_width_;
        const auto screen_y = (static_cast<std::int64_t>(image_y) - source_top) * zoomed_height /
            image_height_;
        if (screen_x < 0 || screen_y < 0 || screen_x >= destination_width ||
            screen_y >= destination_height) {
            return std::nullopt;
        }
        return std::pair<int, int>{left + static_cast<int>(screen_x),
            top + static_cast<int>(screen_y)};
    };
    const auto marker_size = std::max(3, destination_width / 160);
    // 地名字号按缩放倍数的平方根放大、最多两倍，直接以画面像素计算，可逐像素变化；
    // 地图放大时地名间距随之拉开。
    const auto label_scale = std::min(2.0,
        std::sqrt(static_cast<double>(zoom_) / static_cast<double>(kZoomFit)));
    const auto reference_pixels = static_cast<double>(render::scale_legacy_reference_length(
        kLabelReferenceFont * 100, logical_width, logical_height)) / 100.0;
    const auto label_font = render::rgba::FontSize{static_cast<std::uint16_t>(std::clamp<long>(
        std::lround(reference_pixels * label_scale), 1L,
        static_cast<long>(render::rgba::kMaximumFontPixelHeight)))};
    const auto label_metrics = render::rgba::font_metrics(label_font);
    struct Entrance {
        std::span<const std::uint8_t> name;
        int x{};
        int y{};
        bool selected{};
    };
    // 传送目标排在最前，地名优先占位并以黄色显示。
    const auto selected = selected_scene(ranger);
    std::vector<Entrance> entrances;
    for (std::size_t index = 0U; index < ranger.scenes.size(); ++index) {
        const auto entrance = named_entrance(ranger.scenes[index]);
        if (!entrance.has_value()) {
            continue;
        }
        if (const auto point = project(entrance->first, entrance->second)) {
            const bool is_selected = selected == index;
            const Entrance item{scene_name(ranger.scenes[index]), point->first, point->second,
                is_selected};
            if (is_selected) {
                entrances.insert(entrances.begin(), item);
            } else {
                entrances.push_back(item);
            }
            draw_marker(framebuffer, point->first, point->second,
                is_selected ? marker_size : std::max(1, marker_size / 2),
                is_selected ? kSelectedColor : kEntranceColor);
        }
    }
    // 视野中心十字，指示传送目标的选取位置。
    {
        const auto center_x = left + destination_width / 2;
        const auto center_y = top + destination_height / 2;
        const auto arm = marker_size * 2;
        for (int offset = -arm; offset <= arm; ++offset) {
            if (std::abs(offset) > marker_size / 2) {
                put_physical_pixel(framebuffer, center_x + offset, center_y, kSelectedColor);
                put_physical_pixel(framebuffer, center_x, center_y + offset, kSelectedColor);
            }
        }
    }
    // 地名避让：依次尝试入口点上、下、右、左四个位置，取第一个不与已放置地名重叠
    // 且不超出地图区域的位置；都放不下则只保留入口点。
    struct LabelBox {
        int left{};
        int top{};
        int right{};
        int bottom{};
    };
    std::vector<LabelBox> placed;
    const auto line_height = static_cast<int>(label_metrics.line_height);
    const auto gap = std::max(1, marker_size / scale);
    const auto area_left = (left + scale - 1) / scale;
    const auto area_top = (top + scale - 1) / scale;
    const auto area_right = (left + destination_width) / scale;
    const auto area_bottom = (top + destination_height) / scale;
    for (const auto& entrance : entrances) {
        const auto text_width = static_cast<int>(label_metrics.big5_width) *
            static_cast<int>(entrance.name.size() / 2U);
        const auto x = entrance.x / scale;
        const auto y = entrance.y / scale;
        const std::array<std::pair<int, int>, 4> candidates{
            std::pair{x - text_width / 2, y - gap - line_height},
            std::pair{x - text_width / 2, y + gap},
            std::pair{x + gap, y - line_height / 2},
            std::pair{x - gap - text_width, y - line_height / 2}};
        for (const auto [text_x, text_y] : candidates) {
            const LabelBox box{text_x, text_y, text_x + text_width, text_y + line_height};
            if (box.left < area_left || box.top < area_top || box.right > area_right ||
                box.bottom > area_bottom ||
                std::any_of(placed.begin(), placed.end(), [&box](const LabelBox& other) {
                    return box.left < other.right && other.left < box.right &&
                        box.top < other.bottom && other.top < box.bottom;
                })) {
                continue;
            }
            placed.push_back(box);
            static_cast<void>(ui_renderer.draw_text_big5_pixels(framebuffer, text_x, text_y,
                text::Big5TextView{entrance.name},
                entrance.selected ? kSelectedLabelText : kLabelText, label_font));
            break;
        }
    }
    if (ship.has_value() && ship->first > 0 && ship->second > 0) {
        if (const auto point = project(ship->first, ship->second)) {
            draw_marker(framebuffer, point->first, point->second, marker_size, kShipColor);
        }
    }
    if (const auto point = project(person.first, person.second)) {
        draw_marker(framebuffer, point->first, point->second, marker_size, kPersonColor);
    }

    // 标题，随后是人、船图例与操作说明。
    namespace strings = text::game_strings;
    auto cursor = margin;
    const auto big5_width = static_cast<int>(title_metrics.big5_width);
    const auto line_middle = (margin + static_cast<int>(title_metrics.line_height) / 2) * scale;
    static_cast<void>(ui_renderer.draw_text_utf8_rgba(framebuffer, cursor, margin,
        strings::kWorldMinimapTitle, kTitleText, kTitleFont));
    cursor += big5_width * 6;
    for (const auto& [label, color] : {std::pair{strings::kWorldMinimapPerson, kPersonColor},
             std::pair{strings::kWorldMinimapShip, kShipColor}}) {
        draw_marker(framebuffer, cursor * scale + marker_size, line_middle, marker_size, color);
        cursor += (2 * marker_size) / scale + big5_width / 2;
        static_cast<void>(ui_renderer.draw_text_utf8_rgba(framebuffer, cursor, margin, label,
            kHintText, kTitleFont));
        cursor += big5_width * 2;
    }
    static_cast<void>(ui_renderer.draw_text_utf8_rgba(framebuffer, cursor + big5_width, margin,
        strings::kWorldMinimapCloseHint, kHintText, kTitleFont));
    // 传送提示靠右：大地图上显示“Enter傳送：地名”，场景中说明仅限大地图。
    if (selected.has_value()) {
        std::u8string hint;
        if (teleport_available) {
            const auto name = text::decode_big5(text::Big5TextView{scene_name(ranger.scenes[*selected])});
            hint = std::u8string{strings::kWorldMinimapTeleportPrefix} + name.value_or(u8"");
        } else {
            hint = std::u8string{strings::kWorldMinimapTeleportWorldOnly};
        }
        int hint_width = 0;
        for (const auto unit : hint) {
            if (unit < 0x80U) {
                hint_width += title_metrics.ascii_width;
            } else if ((unit & 0xC0U) != 0x80U) {
                hint_width += title_metrics.big5_width;
            }
        }
        static_cast<void>(ui_renderer.draw_text_utf8_rgba(framebuffer,
            std::max(0, logical_width - margin - hint_width), margin, hint,
            teleport_available ? kSelectedLabelText : kHintText, kTitleFont));
    }
    return true;
}

}  // namespace openlegend::app
