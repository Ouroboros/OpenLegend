#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::resource {

struct PaletteResult {
    compat::LegacyPalette palette{};
    std::string error;
    NODISCARD explicit operator bool() const noexcept { return error.empty(); }
};

NODISCARD PaletteResult parse_vga_palette(std::span<const std::uint8_t> bytes);

class Int16FileView {
public:
    explicit Int16FileView(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    NODISCARD bool valid() const noexcept { return bytes_.size() % 2U == 0U; }

    NODISCARD std::size_t size() const noexcept { return bytes_.size() / 2U; }

    NODISCARD std::int16_t at(std::size_t index) const noexcept;

private:
    std::span<const std::uint8_t> bytes_;
};

class GlyphArchiveView {
public:
    GlyphArchiveView(std::span<const std::uint8_t> bytes, std::size_t glyph_size)
        : bytes_(bytes), glyph_size_(glyph_size) {}

    NODISCARD bool valid() const noexcept {
        return glyph_size_ != 0U && bytes_.size() % glyph_size_ == 0U;
    }

    NODISCARD std::size_t glyph_count() const noexcept {
        return valid() ? bytes_.size() / glyph_size_ : 0U;
    }

    NODISCARD std::span<const std::uint8_t> glyph(std::size_t index) const noexcept;

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t glyph_size_{};
};

}  // namespace openlegend::resource
