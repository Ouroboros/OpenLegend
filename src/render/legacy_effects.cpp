#include "openlegend/attributes.hpp"
#include "openlegend/render/legacy_effects.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/render/legacy_color.hpp"

namespace openlegend::render {
namespace {

NODISCARD bool apply_shadow_runs(
    const std::span<std::uint8_t> pixels,
    const std::span<const std::uint16_t> alternating_zero_skip_runs,
    const int byte_offset) noexcept {
    constexpr auto pixel_count = static_cast<int>(openlegend::compat::kLegacyPixelCount);
    if (pixels.size() < openlegend::compat::kLegacyPixelCount ||
        byte_offset <= -pixel_count || byte_offset >= pixel_count ||
        alternating_zero_skip_runs.empty()) {
        return false;
    }

    std::size_t destination = 0U;
    std::size_t run_index = 0U;
    auto remaining = static_cast<std::uint32_t>(pixel_count);
    std::uint32_t zero_count{};

    if (byte_offset < 0) {
        remaining = static_cast<std::uint32_t>(pixel_count + byte_offset);
        const auto shifted_first = static_cast<std::int64_t>(
            alternating_zero_skip_runs[0]) + static_cast<std::int64_t>(byte_offset);
        zero_count = static_cast<std::uint32_t>(shifted_first);
        run_index = 1U;
    } else {
        const auto prefix = static_cast<std::size_t>(byte_offset);
        std::fill_n(
            pixels.begin(), static_cast<std::ptrdiff_t>(prefix), std::uint8_t{0U});
        destination = prefix;
        remaining = static_cast<std::uint32_t>(pixel_count - byte_offset);
        zero_count = alternating_zero_skip_runs[0];
        run_index = 1U;
    }

    while (remaining != 0U) {
        const auto clamped_zero = std::min(zero_count, remaining);
        std::fill_n(
            pixels.begin() + static_cast<std::ptrdiff_t>(destination),
            static_cast<std::ptrdiff_t>(clamped_zero),
            std::uint8_t{0U});
        destination += clamped_zero;
        remaining -= clamped_zero;
        if (remaining == 0U) {
            break;
        }
        if (run_index >= alternating_zero_skip_runs.size()) {
            return false;
        }

        const auto skip_count = static_cast<std::uint32_t>(
            alternating_zero_skip_runs[run_index++]);
        if (skip_count >= remaining) {
            destination += remaining;
            remaining = 0U;
            break;
        }
        destination += skip_count;
        remaining -= skip_count;
        if (run_index >= alternating_zero_skip_runs.size()) {
            return false;
        }
        zero_count = alternating_zero_skip_runs[run_index++];
    }

    if (byte_offset < 0) {
        const auto suffix = static_cast<std::size_t>(-byte_offset);
        if (destination + suffix > pixels.size()) {
            return false;
        }
        std::fill_n(
            pixels.begin() + static_cast<std::ptrdiff_t>(destination),
            static_cast<std::ptrdiff_t>(suffix),
            std::uint8_t{0U});
    }
    return true;
}

}  // namespace

void cycle_legacy_palette(compat::LegacyPalette& palette) noexcept {
    std::rotate(
        palette.begin() + legacy_color::first_palette_cycle_begin,
        palette.begin() + legacy_color::first_palette_cycle_pivot,
        palette.begin() + legacy_color::first_palette_cycle_end);
    std::rotate(
        palette.begin() + legacy_color::second_palette_cycle_begin,
        palette.begin() + legacy_color::second_palette_cycle_pivot,
        palette.begin() + legacy_color::second_palette_cycle_end);
}

std::optional<std::vector<std::uint16_t>> parse_legacy_shadow_mask(
    const std::span<const std::uint8_t> bytes) {
    if (bytes.empty() || bytes.size() % 2U != 0U) {
        return std::nullopt;
    }
    std::vector<std::uint16_t> runs(bytes.size() / 2U);
    std::uint64_t covered{};
    for (std::size_t index = 0U; index < runs.size(); ++index) {
        runs[index] = openlegend::compat::read_u16le(bytes, index * 2U);
        covered += runs[index];
    }
    if (covered < openlegend::compat::kLegacyPixelCount) {
        return std::nullopt;
    }
    return runs;
}

bool apply_legacy_shadow_mask(
    IndexedFramebuffer& framebuffer,
    const std::span<const std::uint16_t> alternating_zero_skip_runs,
    const int byte_offset) noexcept {
    if (framebuffer.legacy_size()) {
        return apply_shadow_runs(
            framebuffer.pixels(), alternating_zero_skip_runs, byte_offset);
    }

    openlegend::compat::LegacyPixels mask{};
    std::ranges::fill(mask, std::uint8_t{1U});
    if (!apply_shadow_runs(mask, alternating_zero_skip_runs, byte_offset)) {
        return false;
    }

    // The hole around the player exists once; everything outside the centred
    // legacy window stays dark instead of tiling the mask across the view.
    const auto origin_x =
        (framebuffer.pixel_width() - IndexedFramebuffer::width) / 2;
    const auto origin_y =
        (framebuffer.pixel_height() - IndexedFramebuffer::height) / 2;
    for (int y = 0; y < framebuffer.pixel_height(); ++y) {
        auto* destination = framebuffer.row(y);
        const auto source_y = y - origin_y;
        if (source_y < 0 || source_y >= IndexedFramebuffer::height) {
            std::fill_n(destination, framebuffer.pixel_width(), std::uint8_t{0U});
            continue;
        }
        for (int x = 0; x < framebuffer.pixel_width(); ++x) {
            const auto source_x = x - origin_x;
            if (source_x < 0 || source_x >= IndexedFramebuffer::width ||
                mask[static_cast<std::size_t>(source_y) *
                        openlegend::compat::kLegacyWidth +
                    static_cast<std::size_t>(source_x)] == 0U) {
                destination[x] = 0U;
            }
        }
    }
    return true;
}

bool render_legacy_shadow_layer(
    IndexedLayer& layer,
    const std::span<const std::uint16_t> alternating_zero_skip_runs,
    const int byte_offset) noexcept {
    if (!layer.valid()) {
        return false;
    }

    openlegend::compat::LegacyPixels mask{};
    std::ranges::fill(mask, std::uint8_t{1U});
    if (!apply_shadow_runs(mask, alternating_zero_skip_runs, byte_offset)) {
        return false;
    }

    layer.clear();
    // Same rule as the framebuffer path: no tiling outside the legacy window.
    const auto origin_x =
        (layer.width() - IndexedFramebuffer::width) / 2;
    const auto origin_y =
        (layer.height() - IndexedFramebuffer::height) / 2;
    for (int y = 0; y < layer.height(); ++y) {
        const auto source_y = y - origin_y;
        const auto row_outside =
            source_y < 0 || source_y >= IndexedFramebuffer::height;
        for (int x = 0; x < layer.width(); ++x) {
            const auto source_x = x - origin_x;
            if (row_outside || source_x < 0 ||
                source_x >= IndexedFramebuffer::width ||
                mask[static_cast<std::size_t>(source_y) *
                        openlegend::compat::kLegacyWidth +
                    static_cast<std::size_t>(source_x)] == 0U) {
                layer.draw_pixel(x, y, 0U);
            }
        }
    }
    return true;
}

}  // namespace openlegend::render
