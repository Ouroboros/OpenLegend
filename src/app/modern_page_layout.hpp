#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <span>
#include <string>
#include <string_view>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/color.hpp"
#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/render/reference_layout.hpp"
#include "openlegend/render/rgba_framebuffer.hpp"
#include "openlegend/resource/legacy_sprite.hpp"
#include "openlegend/text/big5.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

// 现代UI层整页界面（状态页、物品页）共用的参考坐标布局与文本绘制工具。
namespace openlegend::app::page {

inline constexpr render::rgba::FontSize kModernPageFont{13};

NODISCARD inline std::u8string decimal_text(const std::int64_t value) {
    const auto text = std::to_string(value);
    return std::u8string{text.begin(), text.end()};
}

NODISCARD inline std::span<const std::uint8_t> zero_terminated(const std::span<const std::uint8_t> bytes) {
    const auto end = std::find(bytes.begin(), bytes.end(), std::uint8_t{0U});
    return bytes.first(static_cast<std::size_t>(end - bytes.begin()));
}

NODISCARD inline std::span<const std::uint8_t> fixed_text(
    const std::span<const std::uint8_t> bytes, const std::size_t begin, const std::size_t count) {
    return zero_terminated(bytes.subspan(begin, count));
}

class PageLayout {
public:
    PageLayout(
        ui::ModernUiRenderer& ui_renderer,
        const compat::LegacyPalette& palette,
        render::RgbaFramebuffer& framebuffer,
        const render::ReferenceViewport viewport,
        const render::rgba::FontSize font)
        : ui_renderer_(ui_renderer),
          palette_(palette),
          framebuffer_(framebuffer),
          viewport_(viewport),
          font_(font),
          metrics_(ui_renderer.font_metrics(framebuffer, font)) {}

    NODISCARD int x(const int reference) const noexcept {
        return viewport_.x + reference * viewport_.width / static_cast<int>(compat::kLegacyWidth);
    }

    NODISCARD int y(const int reference) const noexcept {
        return viewport_.y + reference * viewport_.height / static_cast<int>(compat::kLegacyHeight);
    }

    NODISCARD int length(const int reference) const noexcept {
        return render::scale_legacy_reference_length(
            reference, framebuffer_.logical_width(), framebuffer_.logical_height());
    }

    NODISCARD const render::rgba::FontMetrics& metrics() const noexcept { return metrics_; }

    // 测量模式：文字与头像不绘制，只记录内容的水平范围，用于先量后居中。
    void begin_measure() noexcept {
        measuring_ = true;
        extent_left_ = std::numeric_limits<int>::max();
        extent_right_ = std::numeric_limits<int>::min();
    }

    NODISCARD std::optional<std::pair<int, int>> measured_extent() const noexcept {
        return extent_left_ <= extent_right_
            ? std::optional{std::pair{extent_left_, extent_right_}} : std::nullopt;
    }

    NODISCARD int utf8_width(const std::u8string_view text) const noexcept {
        int width = 0;
        for (const auto unit : text) {
            if (unit < 0x80U) {
                width += metrics_.ascii_width;
            } else if ((unit & 0xC0U) != 0x80U) {
                width += metrics_.big5_width;
            }
        }
        return width;
    }

    NODISCARD int big5_width(const std::span<const std::uint8_t> bytes) const noexcept {
        int width = 0;
        for (std::size_t index = 0U; index < bytes.size(); ++index) {
            if (bytes[index] >= 0x80U && index + 1U < bytes.size()) {
                width += metrics_.big5_width;
                ++index;
            } else {
                width += metrics_.ascii_width;
            }
        }
        return width;
    }

    NODISCARD bool box(const int left, const int top, const int width, const int height) {
        return ui_renderer_.draw_box(framebuffer_, left, top,
            static_cast<std::uint16_t>(width), static_cast<std::uint16_t>(height), palette_);
    }

    // 超出画布的文本裁剪后绘制；字形越界不得让整页渲染失败。
    NODISCARD bool text(
        const int left, const int top, const std::u8string_view value, const render::TextColors colors) {
        if (measuring_) {
            measure(left, left + utf8_width(value));
            return true;
        }
        if (top < 0 || top + static_cast<int>(metrics_.line_height) > framebuffer_.logical_height()) {
            return true;
        }
        auto clipped = value;
        const auto limit = framebuffer_.logical_width();
        while (!clipped.empty() && left + utf8_width(clipped) > limit) {
            auto size = clipped.size();
            while (size > 0U && (static_cast<unsigned char>(clipped[size - 1U]) & 0xC0U) == 0x80U) {
                --size;
            }
            clipped = clipped.substr(0U, size > 0U ? size - 1U : 0U);
        }
        return clipped.empty() ||
            ui_renderer_.draw_text_utf8(framebuffer_, left, top, clipped, colors, palette_, font_);
    }

    // 按字形实际笔画把文字放在矩形正中；取不到笔画范围时按占位宽度居中。
    NODISCARD bool centered_text(
        const int left, const int top, const int width, const int height,
        const std::u8string_view value, const render::TextColors colors) {
        const auto ink = ui_renderer_.text_ink_bounds_utf8(framebuffer_, value, font_);
        const auto ink_left = ink.has_value() ? ink->left : 0;
        const auto ink_width = ink.has_value() ? ink->right - ink->left : utf8_width(value);
        const auto ink_top = ink.has_value() ? ink->top : 0;
        const auto ink_height = ink.has_value()
            ? ink->bottom - ink->top : static_cast<int>(metrics_.pixel_height);
        return text(left + (width - ink_width) / 2 - ink_left,
            top + (height - ink_height) / 2 - ink_top, value, colors);
    }

    NODISCARD bool big5(
        const int left, const int top, const std::span<const std::uint8_t> value,
        const render::TextColors colors) {
        if (measuring_) {
            measure(left, left + big5_width(zero_terminated(value)));
            return true;
        }
        if (top < 0 || top + static_cast<int>(metrics_.line_height) > framebuffer_.logical_height()) {
            return true;
        }
        const auto limit = framebuffer_.logical_width();
        std::size_t fit = 0U;
        int width = left;
        while (fit < value.size() && value[fit] != 0U) {
            const auto wide = value[fit] >= 0x80U && fit + 1U < value.size();
            const auto advance = static_cast<int>(wide ? metrics_.big5_width : metrics_.ascii_width);
            if (width + advance > limit) {
                break;
            }
            width += advance;
            fit += wide ? 2U : 1U;
        }
        return fit == 0U ||
            ui_renderer_.draw_text_big5(
                framebuffer_, left, top, text::Big5TextView{value.first(fit)}, colors, palette_, font_);
    }

    NODISCARD bool portrait(const resource::SpriteFrameView& frame, const int left, const int top) {
        if (measuring_) {
            measure(left, left + scaled_width(static_cast<int>(frame.width())));
            return true;
        }
        for (std::size_t row = 0U; row < frame.rows().size(); ++row) {
            const auto row_top = top + scaled_height(static_cast<int>(row));
            const auto row_bottom = top + scaled_height(static_cast<int>(row) + 1);
            int column = 0;
            for (const auto& run : frame.rows()[row].runs) {
                column += static_cast<int>(run.skip);
                for (const auto index : run.pixels) {
                    const auto column_left = left + scaled_width(column);
                    const auto column_right = left + scaled_width(column + 1);
                    const auto color = palette_[index];
                    if (column_right > column_left && row_bottom > row_top &&
                        !framebuffer_.fill_rectangle(column_left, row_top,
                            static_cast<std::uint16_t>(column_right - column_left),
                            static_cast<std::uint16_t>(row_bottom - row_top),
                            compat::Rgba8{compat::expand_rgb6(color.red), compat::expand_rgb6(color.green),
                                compat::expand_rgb6(color.blue), compat::kOpaqueAlpha})) {
                        return false;
                    }
                    ++column;
                }
            }
        }
        return true;
    }

    NODISCARD int scaled_width(const int reference) const noexcept {
        return reference * viewport_.width / static_cast<int>(compat::kLegacyWidth);
    }

    NODISCARD int scaled_height(const int reference) const noexcept {
        return reference * viewport_.height / static_cast<int>(compat::kLegacyHeight);
    }

private:
    void measure(const int left, const int right) noexcept {
        extent_left_ = std::min(extent_left_, left);
        extent_right_ = std::max(extent_right_, right);
    }

    bool measuring_{};
    int extent_left_{std::numeric_limits<int>::max()};
    int extent_right_{std::numeric_limits<int>::min()};
    ui::ModernUiRenderer& ui_renderer_;
    const compat::LegacyPalette& palette_;
    render::RgbaFramebuffer& framebuffer_;
    render::ReferenceViewport viewport_;
    render::rgba::FontSize font_;
    render::rgba::FontMetrics metrics_;
};

struct ContentArea {
    int left{};
    int right{};
    int top{};
    int bottom{};
};

}  // namespace openlegend::app::page
