#include "openlegend/resource/legacy_sprite.hpp"

#include "openlegend/compat/byte_reader.hpp"

namespace openlegend::resource {

SpriteFrameView SpriteFrameView::parse(const std::span<const std::uint8_t> bytes) {
    SpriteFrameView frame;
    if (bytes.size() < 8U) {
        frame.error_ = "sprite frame is shorter than its eight-byte header";
        return frame;
    }

    frame.width_ = compat::read_u16le(bytes, 0U);
    frame.height_ = compat::read_u16le(bytes, 2U);
    frame.x_offset_ = compat::read_i16le(bytes, 4U);
    frame.y_offset_ = compat::read_i16le(bytes, 6U);
    frame.rows_.reserve(frame.height_);

    std::size_t cursor = 8U;
    for (std::size_t row_index = 0U; row_index < frame.height_; ++row_index) {
        if (cursor >= bytes.size()) {
            frame.error_ = "sprite row length is missing";
            frame.rows_.clear();
            return frame;
        }
        const auto row_size = static_cast<std::size_t>(bytes[cursor++]);
        const auto row_end = cursor + row_size;
        if (row_end > bytes.size()) {
            frame.error_ = "sprite row exceeds frame boundary";
            frame.rows_.clear();
            return frame;
        }

        SpriteRow row;
        std::size_t x = 0U;
        while (cursor < row_end) {
            if (row_end - cursor < 2U) {
                frame.error_ = "sprite run header is truncated";
                frame.rows_.clear();
                return frame;
            }
            const auto skip = static_cast<std::size_t>(bytes[cursor++]);
            const auto count = static_cast<std::size_t>(bytes[cursor++]);
            if (count > row_end - cursor) {
                frame.error_ = "sprite run pixels exceed row boundary";
                frame.rows_.clear();
                return frame;
            }
            x += skip;
            if (x + count > frame.width_) {
                frame.error_ = "sprite run exceeds declared width";
                frame.rows_.clear();
                return frame;
            }
            row.runs.push_back({static_cast<std::uint16_t>(skip), bytes.subspan(cursor, count)});
            cursor += count;
            x += count;
        }
        frame.rows_.push_back(std::move(row));
    }

    if (cursor != bytes.size()) {
        frame.error_ = "sprite frame contains trailing bytes";
        frame.rows_.clear();
    }
    return frame;
}

}  // namespace openlegend::resource
