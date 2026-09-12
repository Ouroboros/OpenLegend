#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/text/big5.hpp"

namespace openlegend::text {

NODISCARD std::u8string utf8_from_ascii(std::string_view text);

class GameText {
public:
    void append_utf8(std::u8string_view text);

    void append_ascii(std::string_view text);

    void append_legacy(Big5TextView text);

    NODISCARD bool empty() const noexcept { return segments_.empty(); }

    NODISCARD std::optional<std::size_t> legacy_width_units() const noexcept;

    NODISCARD std::size_t trailing_ascii_digit_count() const noexcept;

    void clear() noexcept { segments_.clear(); }

private:
    using OwnedSegment = std::variant<std::u8string, std::vector<std::uint8_t>>;

    std::vector<OwnedSegment> segments_;

    friend bool encode_game_text(
        const GameText& text,
        std::vector<std::uint8_t>& encoded);
};

NODISCARD bool encode_game_text(
    const GameText& text,
    std::vector<std::uint8_t>& encoded);

}  // namespace openlegend::text
