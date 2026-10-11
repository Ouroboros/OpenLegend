#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "openlegend/attributes.hpp"

namespace openlegend::render {

class Big5GlyphCache {
public:
    explicit Big5GlyphCache(std::span<const std::uint8_t> font_bytes) noexcept;

    NODISCARD std::optional<std::span<const std::uint8_t, 32>> resolve(
        std::uint16_t big5_code) noexcept;

    NODISCARD std::size_t next_replacement_slot() const noexcept {
        return next_slot_;
    }

private:
    static constexpr std::size_t kCapacity = 4096U;

    std::span<const std::uint8_t> font_bytes_;
    // 容量须覆盖现代UI单帧的不重复字形数，否则轮转替换会每帧抙6动；
    // 数组放堆上，避免持有者作为栈对象时超出主线程栈。
    std::vector<std::uint16_t> codes_ = std::vector<std::uint16_t>(kCapacity);
    std::vector<std::uint8_t> valid_ = std::vector<std::uint8_t>(kCapacity);
    std::vector<std::array<std::uint8_t, 32>> glyphs_ =
        std::vector<std::array<std::uint8_t, 32>>(kCapacity);
    std::size_t next_slot_{0U};
};

}  // namespace openlegend::render
