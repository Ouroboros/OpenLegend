#include "openlegend/ui/modern_ui_renderer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>

#include "openlegend/text/big5.hpp"
#include "openlegend/text/game_strings.hpp"

namespace openlegend::ui {
namespace {

using namespace openlegend::text::game_strings;
namespace palette_colors = render::legacy_color;
namespace text_colors = render::legacy_color::text;

constexpr std::uint8_t kPanelAlpha = 96U;

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
    const int maximum_pixels) noexcept {
    std::size_t bytes = 0U;
    int pixels = 0;
    while (bytes < text.size()) {
        const bool double_byte = text[bytes] >= 0x80U && bytes + 1U < text.size();
        const int glyph_pixels = double_byte ? 16 : 8;
        if (pixels + glyph_pixels > maximum_pixels) {
            break;
        }
        pixels += glyph_pixels;
        bytes += double_byte ? 2U : 1U;
    }
    return text.first(bytes);
}

[[nodiscard]] SaveListLayout save_list_layout() noexcept {
    constexpr int kReferenceWidth = 320;
    constexpr int kReferencePanelHeight = 198;
    constexpr int kPageTextPixels = 7 * 8;
    constexpr std::array<int, 5> kColumnWeights{40, 72, 24, 87, 88};
    constexpr int kTotalColumnWeight = 311;
    const int framebuffer_width = render::RgbaFramebuffer::width;
    const int framebuffer_height = render::RgbaFramebuffer::height;
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
    const auto scaled_y = [](const int reference_y) {
        return outer_y + reference_y * panel_height / kReferencePanelHeight;
    };
    layout.title_y = scaled_y(3);
    layout.page_x = outer_x + panel_width - inner_x - kPageTextPixels;
    layout.header_y = scaled_y(21);
    layout.first_row_y = scaled_y(39);
    layout.row_step = std::max(18, 18 * panel_height / kReferencePanelHeight);
    const auto column = [&layout](const SaveListColumn value) {
        return layout.column_x[static_cast<std::size_t>(value)];
    };
    const int name_gutter = std::max(8, 8 * framebuffer_width / kReferenceWidth);
    layout.name_text_pixels = column(SaveListColumn::level) -
        column(SaveListColumn::name) - name_gutter;
    return layout;
}

[[nodiscard]] compat::Rgba8 palette_color(
    const compat::LegacyPalette& palette,
    const render::PaletteIndex index,
    const std::uint8_t alpha = compat::kOpaqueAlpha) noexcept {
    const auto source = palette[index];
    return {
        compat::expand_rgb6(source.red),
        compat::expand_rgb6(source.green),
        compat::expand_rgb6(source.blue),
        alpha,
    };
}

[[nodiscard]] render::rgba::TextColors rgba_text_colors(
    const compat::LegacyPalette& palette,
    const render::TextColors colors) noexcept {
    return {
        palette_color(palette, colors.right_shadow),
        palette_color(palette, colors.foreground),
    };
}

}  // namespace

ModernUiRenderer::ModernUiRenderer(const resource::DataRoot& data_root) {
    auto ascii = data_root.read("FONT3.E16");
    if (!ascii) {
        error_ = ascii.error;
        return;
    }
    auto big5 = data_root.read("FONT3.C16");
    if (!big5) {
        error_ = big5.error;
        return;
    }
    if (ascii.bytes.size() != 128U * 16U || big5.bytes.size() % 32U != 0U) {
        error_ = "legacy UI fonts have unexpected sizes";
        return;
    }
    ascii_font_ = std::move(ascii.bytes);
    big5_font_ = std::move(big5.bytes);
    big5_cache_.emplace(big5_font_);
}

bool ModernUiRenderer::render_location_status(
    const std::span<const std::uint8_t> legacy_name,
    const int location_x,
    const int location_y,
    const compat::LegacyPalette& palette,
    render::RgbaFramebuffer& framebuffer) {
    if (!valid()) {
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
    return draw_text_big5(
        framebuffer,
        4,
        render::RgbaFramebuffer::height - 20,
        text::Big5TextView{std::span<const std::uint8_t>{status}.first(length)},
        text_colors::location_status,
        palette);
}

bool ModernUiRenderer::render_save_list(
    const SaveListMode mode,
    const std::uint16_t selection,
    const std::span<const SaveListEntry> entries,
    const compat::LegacyPalette& palette,
    render::RgbaFramebuffer& framebuffer) {
    const auto layout = save_list_layout();
    const auto column = [&layout](const SaveListColumn value) {
        return layout.column_x[static_cast<std::size_t>(value)];
    };
    if (entries.size() != kSaveListPageSize ||
        save_list_page(selection) >= kSaveListPageCount ||
        !draw_box(
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
    if (!draw_text_utf8(
            framebuffer,
            column(SaveListColumn::slot),
            layout.title_y,
            title,
            text_colors::notice,
            palette) ||
        !draw_text_utf8(
            framebuffer,
            layout.page_x,
            layout.title_y,
            page,
            text_colors::notice,
            palette) ||
        !draw_text_utf8(
            framebuffer,
            column(SaveListColumn::slot),
            layout.header_y,
            kSlotNumberHeader,
            text_colors::normal,
            palette) ||
        !draw_text_utf8(
            framebuffer,
            column(SaveListColumn::name),
            layout.header_y,
            kNameHeader,
            text_colors::normal,
            palette) ||
        !draw_text_utf8(
            framebuffer,
            column(SaveListColumn::level),
            layout.header_y,
            kLevelHeader,
            text_colors::normal,
            palette) ||
        !draw_text_utf8(
            framebuffer,
            column(SaveListColumn::location),
            layout.header_y,
            kLocationHeader,
            text_colors::normal,
            palette) ||
        !draw_text_utf8(
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
        if (!draw_text_utf8(
                framebuffer,
                column(SaveListColumn::slot),
                y,
                slot,
                colors,
                palette)) {
            return false;
        }
        if (entry.state == SaveListEntryState::empty) {
            if (!draw_text_utf8(
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
            if (!draw_text_utf8(
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
        if (!draw_text_big5(
                framebuffer,
                column(SaveListColumn::name),
                y,
                text::Big5TextView{legacy_text_prefix(
                    entry.protagonist_name, layout.name_text_pixels)},
                colors,
                palette) ||
            !draw_text_utf8(
                framebuffer,
                column(SaveListColumn::level),
                y,
                level,
                colors,
                palette) ||
            !draw_text_mixed(
                framebuffer,
                column(SaveListColumn::location),
                y,
                entry.location,
                colors,
                palette) ||
            !draw_text_utf8(
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

bool ModernUiRenderer::render_save_delete_confirmation(
    const std::uint16_t selection,
    const compat::LegacyPalette& palette,
    render::RgbaFramebuffer& framebuffer) {
    constexpr std::uint16_t kBoxWidth = 196U;
    constexpr std::uint16_t kBoxHeight = 31U;
    const int x = (render::RgbaFramebuffer::width - kBoxWidth) / 2;
    const int y = (render::RgbaFramebuffer::height - kBoxHeight) / 2;
    std::u8string prompt{kDeleteSavePrompt};
    prompt.push_back(u8' ');
    const auto slot = zero_padded_number(
        static_cast<std::uint32_t>(selection) + 1U, 3);
    prompt.append(slot);
    prompt.append(kYesNoPrompt);
    return draw_box(framebuffer, x, y, kBoxWidth, kBoxHeight, palette) &&
        draw_text_utf8(
            framebuffer,
            x + 10,
            y + 7,
            prompt,
            text_colors::notice,
            palette);
}

bool ModernUiRenderer::render_io_wait(
    const compat::LegacyPalette& palette,
    render::RgbaFramebuffer& framebuffer) {
    return draw_box(framebuffer, 154, 18, 68U, 31U, palette) &&
        draw_text_utf8(
            framebuffer,
            158,
            25,
            kIoWaitLabel,
            text_colors::notice,
            palette);
}

bool ModernUiRenderer::draw_text_utf8(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::u8string_view text,
    const render::TextColors colors,
    const compat::LegacyPalette& palette) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::rgba::draw_text_utf8(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        rgba_text_colors(palette, colors));
}

bool ModernUiRenderer::draw_text_mixed(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::GameText& text,
    const render::TextColors colors,
    const compat::LegacyPalette& palette) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::rgba::draw_text_mixed(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        rgba_text_colors(palette, colors));
}

bool ModernUiRenderer::draw_text_big5(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const text::Big5TextView text,
    const render::TextColors colors,
    const compat::LegacyPalette& palette) {
    if (!big5_cache_.has_value()) {
        return false;
    }
    return render::rgba::draw_text_big5(
        framebuffer,
        x,
        y,
        text,
        ascii_font_,
        *big5_cache_,
        rgba_text_colors(palette, colors));
}

bool ModernUiRenderer::draw_box(
    render::RgbaFramebuffer& framebuffer,
    const int x,
    const int y,
    const std::uint16_t width,
    const std::uint16_t height,
    const compat::LegacyPalette& palette) {
    if (width <= 10U || height <= 10U || x < 0 || y < 0 ||
        x + static_cast<int>(width) > render::RgbaFramebuffer::width ||
        y + static_cast<int>(height) > render::RgbaFramebuffer::height) {
        return false;
    }
    const auto w = static_cast<int>(width);
    const auto h = static_cast<int>(height);
    const auto panel = palette_color(palette, palette_colors::black, kPanelAlpha);
    const auto blend = [&framebuffer, panel](
                           const int left,
                           const int top,
                           const int rectangle_width,
                           const int rectangle_height) {
        return framebuffer.blend_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width),
            static_cast<std::uint16_t>(rectangle_height),
            panel);
    };
    if (!blend(x + 5, y, w - 10, 1) ||
        !blend(x + 4, y + 1, w - 8, 1) ||
        !blend(x + 3, y + 2, w - 6, 1) ||
        !blend(x + 2, y + 3, w - 4, 1) ||
        !blend(x + 1, y + 4, w - 2, 1) ||
        !blend(x, y + 5, w, h - 10) ||
        !blend(x + 1, y + h - 5, w - 2, 1) ||
        !blend(x + 2, y + h - 4, w - 4, 1) ||
        !blend(x + 3, y + h - 3, w - 6, 1) ||
        !blend(x + 4, y + h - 2, w - 8, 1) ||
        !blend(x + 5, y + h - 1, w - 10, 1)) {
        return false;
    }

    const auto outline = palette_color(palette, palette_colors::panel_outline);
    const auto fill = [&framebuffer, outline](
                          const int left,
                          const int top,
                          const int rectangle_width,
                          const int rectangle_height) {
        return framebuffer.fill_rectangle(
            left,
            top,
            static_cast<std::uint16_t>(rectangle_width),
            static_cast<std::uint16_t>(rectangle_height),
            outline);
    };
    return fill(x + 5, y + 1, w - 10, 1) &&
        fill(x + 4, y + 2, 1, 2) && fill(x + w - 5, y + 2, 1, 2) &&
        fill(x + 2, y + 4, 2, 1) && fill(x + w - 4, y + 4, 2, 1) &&
        fill(x + 1, y + 5, 1, h - 10) &&
        fill(x + w - 2, y + 5, 1, h - 10) &&
        fill(x + 2, y + h - 5, 2, 1) &&
        fill(x + w - 4, y + h - 5, 2, 1) &&
        fill(x + 4, y + h - 4, 1, 2) &&
        fill(x + w - 5, y + h - 4, 1, 2) &&
        fill(x + 5, y + h - 2, w - 10, 1);
}

}  // namespace openlegend::ui
