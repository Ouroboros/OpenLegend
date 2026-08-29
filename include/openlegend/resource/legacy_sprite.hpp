#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace openlegend::resource {

struct SpriteRun {
    std::uint16_t skip{};
    std::span<const std::uint8_t> pixels;
};

struct SpriteRow {
    std::vector<SpriteRun> runs;
};

class SpriteFrameView {
public:
    [[nodiscard]] static SpriteFrameView parse(std::span<const std::uint8_t> bytes);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::uint16_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint16_t height() const noexcept { return height_; }
    [[nodiscard]] std::int16_t x_offset() const noexcept { return x_offset_; }
    [[nodiscard]] std::int16_t y_offset() const noexcept { return y_offset_; }
    [[nodiscard]] const std::vector<SpriteRow>& rows() const noexcept { return rows_; }

private:
    std::uint16_t width_{};
    std::uint16_t height_{};
    std::int16_t x_offset_{};
    std::int16_t y_offset_{};
    std::vector<SpriteRow> rows_;
    std::string error_;
};

}  // namespace openlegend::resource
