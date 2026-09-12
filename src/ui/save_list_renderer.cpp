#include "openlegend/ui/save_list_renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>

#include "openlegend/render/legacy_color.hpp"
#include "openlegend/render/reference_layout.hpp"
#include "openlegend/text/big5.hpp"
#include "openlegend/text/game_strings.hpp"
#include "openlegend/text/game_text.hpp"
#include "openlegend/ui/modern_ui_renderer.hpp"

namespace openlegend::ui {
namespace {

using namespace openlegend::text::game_strings;
namespace text_colors = render::legacy_color::text;

enum class SaveListColumn : std::size_t {
    slot,
    name,
    level,
    location,
    saved_at,
    count,
};

struct SaveListLayout {
    int panel_x{};
    int panel_y{};
    std::uint16_t panel_width{};
    std::uint16_t panel_height{};
    int title_y{};
    int page_x{};
    int header_y{};
    int first_row_y{};
    int row_step{};
    int name_text_pixels{};
    std::array<int, static_cast<std::size_t>(SaveListColumn::count)> column_x{};
};

void append_number(std::u8string& text, const std::int32_t value) {
    std::array<char, 16> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    for (const auto* cursor = buffer.data(); cursor != converted.ptr; ++cursor) {
        text.push_back(static_cast<char8_t>(*cursor));
    }
}

[[nodiscard]] std::u8string zero_padded_number(
    const std::uint32_t value, const int width) {
    std::array<char, 16> buffer{};
    const auto converted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value);
    const auto count = static_cast<int>(converted.ptr - buffer.data());
    std::u8string text;
    text.reserve(static_cast<std::size_t>(std::max(width, count)));
    for (int index = count; index < width; ++index) {
        text.push_back(u8'0');
    }
    for (const auto* cursor = buffer.data(); cursor != converted.ptr; ++cursor) {
        text.push_back(static_cast<char8_t>(*cursor));
    }
    return text;
}

[[nodiscard]] std::span<const std::uint8_t> legacy_text_prefix(
    const std::span<const std::uint8_t> text,
    const int maximum_pixels,
    const render::rgba::FontMetrics metrics) noexcept {
    std::size_t bytes = 0U;
    int pixels = 0;
    while (bytes < text.size()) {
        const auto first = text[bytes];
        const bool double_byte = first >= 0x80U && bytes + 1U < text.size();
        const int glyph_pixels = double_byte
            ? metrics.big5_width
            : first == static_cast<std::uint8_t>('_')
                ? metrics.underscore_advance
                : metrics.ascii_width;
        if (pixels + glyph_pixels > maximum_pixels) {
            break;
        }
        pixels += glyph_pixels;
        bytes += double_byte ? 2U : 1U;
    }
    return text.first(bytes);
}

[[nodiscard]] SaveListLayout save_list_layout(
    const render::RgbaFramebuffer& framebuffer,
    const render::rgba::FontMetrics metrics) noexcept {
    constexpr int kReferenceWidth = 320;
    constexpr int kReferencePanelHeight = 198;
    constexpr std::array<int, 5> kColumnWeights{40, 72, 24, 87, 88};
    constexpr int kTotalColumnWeight = 311;
    const int framebuffer_width = framebuffer.logical_width();
    const int framebuffer_height = framebuffer.logical_height();
    const int outer_x = std::max(1, framebuffer_width / kReferenceWidth);
    const int outer_y = std::max(1, framebuffer_height / 200);
    const int inner_x = std::max(6, 6 * framebuffer_width / kReferenceWidth);
    const int panel_width = framebuffer_width - 2 * outer_x;
    const int panel_height = framebuffer_height - 2 * outer_y;
    const int content_x = outer_x + inner_x;
    const int content_width = panel_width - inner_x - outer_x;

    SaveListLayout layout;
    layout.panel_x = outer_x;
    layout.panel_y = outer_y;
    layout.panel_width = static_cast<std::uint16_t>(panel_width);
    layout.panel_height = static_cast<std::uint16_t>(panel_height);
    int accumulated_weight = 0;
    for (std::size_t column = 0U; column < layout.column_x.size(); ++column) {
        layout.column_x[column] =
            content_x + content_width * accumulated_weight / kTotalColumnWeight;
        accumulated_weight += kColumnWeights[column];
    }
    const auto scaled_y = [outer_y, panel_height](const int reference_y) {
        return outer_y + reference_y * panel_height / kReferencePanelHeight;
    };
    layout.title_y = scaled_y(3);
    layout.page_x = outer_x + panel_width - inner_x -
        7 * static_cast<int>(metrics.ascii_width);
    layout.header_y = scaled_y(21);
    layout.first_row_y = scaled_y(39);
    layout.row_step = std::max(
        static_cast<int>(metrics.line_height) + 2,
        18 * panel_height / kReferencePanelHeight);
    const auto column = [&layout](const SaveListColumn value) {
        return layout.column_x[static_cast<std::size_t>(value)];
    };
    const int name_gutter = std::max(
        static_cast<int>(metrics.ascii_width),
        8 * framebuffer_width / kReferenceWidth);
    layout.name_text_pixels = column(SaveListColumn::level) -
        column(SaveListColumn::name) - name_gutter;
    return layout;
}

}  // namespace

bool SaveListRenderer::render(
    const SaveListMode mode,
    const std::uint16_t selection,
    const std::span<const SaveListEntry> entries,
    const compat::LegacyPalette& palette,
    ModernUiRenderer& ui_renderer,
    render::RgbaFramebuffer& framebuffer) const {
    const auto metrics = ui_renderer.font_metrics(framebuffer);
    const auto layout = save_list_layout(framebuffer, metrics);
    const auto column = [&layout](const SaveListColumn value) {
        return layout.column_x[static_cast<std::size_t>(value)];
    };
    if (!ui_renderer.valid() || entries.size() != kSaveListPageSize ||
        save_list_page(selection) >= kSaveListPageCount ||
        !ui_renderer.draw_box(
            framebuffer,
            layout.panel_x,
            layout.panel_y,
            layout.panel_width,
            layout.panel_height,
            palette)) {
        return false;
    }

    const auto& title = mode == SaveListMode::load ? kLoadListTitle : kSaveListTitle;
    auto page = zero_padded_number(save_list_page(selection) + 1U, 3);
    page.push_back(u8'/');
    const auto page_count = zero_padded_number(kSaveListPageCount, 3);
    page.insert(page.end(), page_count.begin(), page_count.end());
    if (!ui_renderer.draw_text_utf8(
            framebuffer,
            column(SaveListColumn::slot),
            layout.title_y,
            title,
            text_colors::notice,
            palette) ||
        !ui_renderer.draw_text_utf8(
            framebuffer,
            layout.page_x,
            layout.title_y,
            page,
            text_colors::notice,
            palette) ||
        !ui_renderer.draw_text_utf8(
            framebuffer,
            column(SaveListColumn::slot),
            layout.header_y,
            kSlotNumberHeader,
            text_colors::normal,
            palette) ||
        !ui_renderer.draw_text_utf8(
            framebuffer,
            column(SaveListColumn::name),
            layout.header_y,
            kNameHeader,
            text_colors::normal,
            palette) ||
        !ui_renderer.draw_text_utf8(
            framebuffer,
            column(SaveListColumn::level),
            layout.header_y,
            kLevelHeader,
            text_colors::normal,
            palette) ||
        !ui_renderer.draw_text_utf8(
            framebuffer,
            column(SaveListColumn::location),
            layout.header_y,
            kLocationHeader,
            text_colors::normal,
            palette) ||
        !ui_renderer.draw_text_utf8(
            framebuffer,
            column(SaveListColumn::saved_at),
            layout.header_y,
            kSavedAtHeader,
            text_colors::normal,
            palette)) {
        return false;
    }

    for (std::size_t row = 0U; row < entries.size(); ++row) {
        const auto& entry = entries[row];
        if (entry.state == SaveListEntryState::hidden) {
            continue;
        }
        const int y = layout.first_row_y + static_cast<int>(row) * layout.row_step;
        const auto colors = entry.slot == selection
            ? text_colors::save_list_selected
            : text_colors::save_list_normal;
        const auto slot = zero_padded_number(
            static_cast<std::uint32_t>(entry.slot) + 1U, 3);
        if (!ui_renderer.draw_text_utf8(
                framebuffer,
                column(SaveListColumn::slot),
                y,
                slot,
                colors,
                palette)) {
            return false;
        }
        if (entry.state == SaveListEntryState::empty) {
            if (!ui_renderer.draw_text_utf8(
                    framebuffer,
                    column(SaveListColumn::location),
                    y,
                    kEmptySaveLabel,
                    colors,
                    palette)) {
                return false;
            }
            continue;
        }
        if (entry.state == SaveListEntryState::damaged) {
            if (!ui_renderer.draw_text_utf8(
                    framebuffer,
                    column(SaveListColumn::location),
                    y,
                    kDamagedSaveLabel,
                    colors,
                    palette)) {
                return false;
            }
            continue;
        }
        std::u8string level;
        append_number(level, entry.level);
        const auto saved_at = text::utf8_from_ascii(entry.saved_at);
        if (!ui_renderer.draw_text_big5(
                framebuffer,
                column(SaveListColumn::name),
                y,
                text::Big5TextView{legacy_text_prefix(
                    entry.protagonist_name,
                    layout.name_text_pixels,
                    metrics)},
                colors,
                palette) ||
            !ui_renderer.draw_text_utf8(
                framebuffer,
                column(SaveListColumn::level),
                y,
                level,
                colors,
                palette) ||
            !ui_renderer.draw_text_mixed(
                framebuffer,
                column(SaveListColumn::location),
                y,
                entry.location,
                colors,
                palette) ||
            !ui_renderer.draw_text_utf8(
                framebuffer,
                column(SaveListColumn::saved_at),
                y,
                saved_at,
                colors,
                palette)) {
            return false;
        }
    }
    return true;
}

bool SaveListRenderer::render_delete_confirmation(
    const std::uint16_t selection,
    const compat::LegacyPalette& palette,
    ModernUiRenderer& ui_renderer,
    render::RgbaFramebuffer& framebuffer) const {
    constexpr int kReferenceBoxWidth = 196;
    constexpr int kReferenceBoxHeight = 31;
    const auto box_width = render::scale_legacy_reference_length(
        kReferenceBoxWidth,
        framebuffer.logical_width(),
        framebuffer.logical_height());
    const auto box_height = render::scale_legacy_reference_length(
        kReferenceBoxHeight,
        framebuffer.logical_width(),
        framebuffer.logical_height());
    const int x = (framebuffer.logical_width() - box_width) / 2;
    const int y = (framebuffer.logical_height() - box_height) / 2;
    const auto text_inset_x = render::scale_legacy_reference_length(
        10, framebuffer.logical_width(), framebuffer.logical_height());
    const auto text_inset_y = render::scale_legacy_reference_length(
        7, framebuffer.logical_width(), framebuffer.logical_height());
    std::u8string prompt{kDeleteSavePrompt};
    prompt.push_back(u8' ');
    const auto slot = zero_padded_number(
        static_cast<std::uint32_t>(selection) + 1U, 3);
    prompt.append(slot);
    prompt.append(kYesNoPrompt);
    return ui_renderer.draw_box(
               framebuffer,
               x,
               y,
               static_cast<std::uint16_t>(box_width),
               static_cast<std::uint16_t>(box_height),
               palette) &&
        ui_renderer.draw_text_utf8(
            framebuffer,
            x + text_inset_x,
            y + text_inset_y,
            prompt,
            text_colors::notice,
            palette);
}

bool SaveListRenderer::render_io_wait(
    const compat::LegacyPalette& palette,
    ModernUiRenderer& ui_renderer,
    render::RgbaFramebuffer& framebuffer) const {
    const auto viewport = render::fit_legacy_reference_viewport(
        framebuffer.logical_width(), framebuffer.logical_height());
    const auto scale = [&framebuffer](const int value) {
        return render::scale_legacy_reference_length(
            value, framebuffer.logical_width(), framebuffer.logical_height());
    };
    const auto x = viewport.x + scale(154);
    const auto y = viewport.y + scale(18);
    return ui_renderer.draw_box(
               framebuffer,
               x,
               y,
               static_cast<std::uint16_t>(scale(68)),
               static_cast<std::uint16_t>(scale(31)),
               palette) &&
        ui_renderer.draw_text_utf8(
            framebuffer,
            viewport.x + scale(158),
            viewport.y + scale(25),
            kIoWaitLabel,
            text_colors::notice,
            palette);
}

}  // namespace openlegend::ui
