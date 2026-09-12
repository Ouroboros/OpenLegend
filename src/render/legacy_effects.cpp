#include "openlegend/render/legacy_effects.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "openlegend/compat/byte_reader.hpp"

namespace openlegend::render {
namespace {

[[nodiscard]] bool apply_shadow_runs(
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

[[nodiscard]] int positive_remainder(
    const int value, const int divisor) noexcept {
    const auto remainder = value % divisor;
    return remainder < 0 ? remainder + divisor : remainder;
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
    if (framebuffer.legacy_size()) {
        return apply_shadow_runs(
            framebuffer.pixels(), alternating_zero_skip_runs, byte_offset);
    }

    openlegend::compat::LegacyPixels mask{};
    std::ranges::fill(mask, std::uint8_t{1U});
    if (!apply_shadow_runs(mask, alternating_zero_skip_runs, byte_offset)) {
        return false;
    }

    const auto origin_x =
        (framebuffer.pixel_width() - IndexedFramebuffer::width) / 2;
    const auto origin_y =
        (framebuffer.pixel_height() - IndexedFramebuffer::height) / 2;
    for (int y = 0; y < framebuffer.pixel_height(); ++y) {
        auto* destination = framebuffer.row(y);
        const auto source_y = positive_remainder(
            y - origin_y, IndexedFramebuffer::height);
        for (int x = 0; x < framebuffer.pixel_width(); ++x) {
            const auto source_x = positive_remainder(
                x - origin_x, IndexedFramebuffer::width);
            const auto source_index =
                static_cast<std::size_t>(source_y) *
                    openlegend::compat::kLegacyWidth +
                static_cast<std::size_t>(source_x);
            if (mask[source_index] == 0U) {
                destination[x] = 0U;
            }
        }
    }
    return true;
}

}  // namespace openlegend::render
