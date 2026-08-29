#include "openlegend/resource/legacy_assets.hpp"

#include "openlegend/compat/byte_reader.hpp"

namespace openlegend::resource {

PaletteResult parse_vga_palette(const std::span<const std::uint8_t> bytes) {
    PaletteResult result;
    if (bytes.size() != compat::kLegacyPaletteSize * 3U) {
        result.error = "VGA palette must contain exactly 768 bytes";
        return result;
    }
    for (std::size_t index = 0U; index < compat::kLegacyPaletteSize; ++index) {
        const auto offset = index * 3U;
        result.palette[index] = {bytes[offset], bytes[offset + 1U], bytes[offset + 2U]};
        if (!result.palette[index].valid()) {
            result.error = "VGA palette contains a channel above six-bit range";
            return result;
        }
    }
    return result;
}

std::int16_t Int16FileView::at(const std::size_t index) const noexcept {
    if (!valid() || index >= size()) {
        return 0;
    }
    return compat::read_i16le(bytes_, index * 2U);
}

std::span<const std::uint8_t> GlyphArchiveView::glyph(const std::size_t index) const noexcept {
    if (!valid() || index >= glyph_count()) {
        return {};
    }
    return bytes_.subspan(index * glyph_size_, glyph_size_);
}

}  // namespace openlegend::resource
