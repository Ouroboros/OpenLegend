#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/color.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/model/runtime_snapshot.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"
#include "openlegend/world/world_map.hpp"

namespace openlegend::app {

// 全屏世界小地图：底图用大地图地面、表面、建筑三层真实贴图按原尺寸绘制后
// 缩小为1/4保存，绘制时叠加人、船位置与各场景入口地名。
// 支持100%～800%缩放（每次5%），放大后可平移视野，地名字号随缩放放大。
// 底图首次生成后压缩写入缓存文件，资源未变时之后直接读取。
class WorldMinimap {
public:
    WorldMinimap(const resource::DataRoot& data_root, const world::WorldMapData& map,
        const std::filesystem::path& cache_path);

    // 视野中心移到大地图格(x,y)，缩放倍数不变。
    void center_on(int world_x, int world_y) noexcept;

    void zoom_in() noexcept;

    void zoom_out() noexcept;

    // 按屏幕方向平移，每步为可见高度的1/12，左右与上下距离相同。
    void pan(int horizontal_steps, int vertical_steps) noexcept;

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD bool render(
        ui::ModernUiRenderer& ui_renderer,
        const model::RuntimeRangerState& ranger,
        std::pair<int, int> person,
        std::optional<std::pair<int, int>> ship,
        render::RgbaFramebuffer& framebuffer) const;

private:
    void build(const resource::DataRoot& data_root, const world::WorldMapData& map);

    NODISCARD bool load_cache(const std::filesystem::path& path, std::uint64_t key);

    void save_cache(const std::filesystem::path& path, std::uint64_t key) const;

    // 格(x,y)锚点在底图上的像素坐标。
    NODISCARD std::pair<int, int> image_position(int world_x, int world_y) const noexcept;

    void clamp_center() noexcept;

    std::vector<compat::Rgba8> image_;
    std::string error_;
    int image_width_{};
    int image_height_{};
    int anchor_left_{};
    int anchor_top_{};
    int zoom_{1000};
    int center_x_{};
    int center_y_{};
};

}  // namespace openlegend::app
