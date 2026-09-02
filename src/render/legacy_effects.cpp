#include "openlegend/render/legacy_effects.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "openlegend/compat/byte_reader.hpp"

namespace openlegend::render {
namespace {

void decrement_palette(openlegend::compat::LegacyPalette& palette) noexcept {
    for (auto& color : palette) {
        if (color.red != 0U) {
            --color.red;
        }
        if (color.green != 0U) {
            --color.green;
        }
        if (color.blue != 0U) {
            --color.blue;
        }
    }
}

}  // namespace

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
    constexpr auto pixel_count = static_cast<int>(openlegend::compat::kLegacyPixelCount);
    if (byte_offset <= -pixel_count || byte_offset >= pixel_count ||
        alternating_zero_skip_runs.empty()) {
        return false;
    }

    auto pixels = framebuffer.pixels();
    std::size_t destination = 0U;
    std::size_t run_index = 0U;
    auto remaining = static_cast<std::uint32_t>(pixel_count);
    std::uint32_t zero_count{};

    if (byte_offset < 0) {
        remaining = static_cast<std::uint32_t>(pixel_count + byte_offset);
        const auto shifted_first = static_cast<std::int64_t>(alternating_zero_skip_runs[0]) +
                                   static_cast<std::int64_t>(byte_offset);
        zero_count = static_cast<std::uint32_t>(shifted_first);
        run_index = 1U;
    } else {
        const auto prefix = static_cast<std::size_t>(byte_offset);
        std::fill_n(pixels.begin(), static_cast<std::ptrdiff_t>(prefix), std::uint8_t{0U});
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

        const auto skip_count = static_cast<std::uint32_t>(alternating_zero_skip_runs[run_index++]);
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

std::vector<openlegend::compat::LegacyPalette> legacy_fade_to_black(
    const openlegend::compat::LegacyPalette& palette) {
    std::vector<openlegend::compat::LegacyPalette> sequence;
    sequence.reserve(64U);
    auto current = palette;
    for (int step = 0; step < 64; ++step) {
        decrement_palette(current);
        sequence.push_back(current);
    }
    return sequence;
}

std::vector<openlegend::compat::LegacyPalette> legacy_fade_from_black(
    const openlegend::compat::LegacyPalette& palette) {
    std::vector<openlegend::compat::LegacyPalette> sequence;
    sequence.reserve(65U);
    for (int decrement_count = 64; decrement_count > 0; --decrement_count) {
        auto current = palette;
        for (int step = 0; step < decrement_count; ++step) {
            decrement_palette(current);
        }
        sequence.push_back(current);
    }
    sequence.push_back(palette);
    return sequence;
}

}  // namespace openlegend::render
