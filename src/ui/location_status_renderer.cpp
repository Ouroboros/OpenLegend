#include "openlegend/ui/location_status_renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>

#include "openlegend/render/legacy_color.hpp"
#include "openlegend/text/big5.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

namespace openlegend::ui {
namespace {

namespace text_colors = render::legacy_color::text;

constexpr render::rgba::FontSize kLocationStatusFontSize{10U};

struct RelativeInset {
    std::uint16_t numerator{};
    std::uint16_t denominator{1U};

    [[nodiscard]] constexpr int resolve(const int extent) const noexcept {
        if (extent <= 0 || numerator == 0U || denominator == 0U) {
            return 0;
        }
        const auto scaled =
            (static_cast<std::uint32_t>(extent) * numerator + denominator / 2U) /
            denominator;
        return std::max(1, static_cast<int>(scaled));
    }
};

constexpr RelativeInset kLocationStatusLeftInset{1U, 80U};
constexpr RelativeInset kLocationStatusBottomInset{1U, 50U};
static_assert(kLocationStatusLeftInset.resolve(320) == 4);
static_assert(kLocationStatusLeftInset.resolve(640) == 8);
static_assert(kLocationStatusBottomInset.resolve(200) == 4);
static_assert(kLocationStatusBottomInset.resolve(400) == 8);

}  // namespace

bool LocationStatusRenderer::render(
    const std::span<const std::uint8_t> legacy_name,
    const int location_x,
    const int location_y,
    const compat::LegacyPalette& palette,
    ModernUiRenderer& ui_renderer,
    render::RgbaFramebuffer& framebuffer) const {
    if (!ui_renderer.valid()) {
        return false;
    }

    std::array<std::uint8_t, 64> status{};
    if (legacy_name.size() >= status.size()) {
        return false;
    }
    auto length = legacy_name.size();
    std::copy(legacy_name.begin(), legacy_name.end(), status.begin());
    if (!legacy_name.empty()) {
        status[length++] = static_cast<std::uint8_t>(' ');
    }
    const auto append_coordinate = [&status, &length](const int value) {
        std::array<char, 16> digits{};
        const auto converted = std::to_chars(
            digits.data(), digits.data() + digits.size(), value);
        if (converted.ec != std::errc{} ||
            length + static_cast<std::size_t>(converted.ptr - digits.data()) >
                status.size()) {
            return false;
        }
        for (const auto* cursor = digits.data(); cursor != converted.ptr; ++cursor) {
            status[length++] = static_cast<std::uint8_t>(*cursor);
        }
        return true;
    };
    if (!append_coordinate(location_x) || length >= status.size()) {
        return false;
    }
    status[length++] = static_cast<std::uint8_t>(',');
    if (!append_coordinate(location_y)) {
        return false;
    }

    const auto metrics = ui_renderer.font_metrics(kLocationStatusFontSize);
    const auto left = kLocationStatusLeftInset.resolve(
        render::RgbaFramebuffer::width);
    const auto bottom = kLocationStatusBottomInset.resolve(
        render::RgbaFramebuffer::height);
    return ui_renderer.draw_text_big5(
        framebuffer,
        left,
        render::RgbaFramebuffer::height - bottom - metrics.line_height,
        text::Big5TextView{std::span<const std::uint8_t>{status}.first(length)},
        text_colors::location_status,
        palette,
        kLocationStatusFontSize);
}

}  // namespace openlegend::ui
