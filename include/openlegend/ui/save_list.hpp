#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/text/game_text.hpp"

namespace openlegend::ui {

inline constexpr std::uint16_t kSaveSlotCount =
    static_cast<std::uint16_t>(persistence::kNumberedSaveSlotCount);
inline constexpr std::uint16_t kSaveListPageSize = 8U;
inline constexpr std::uint16_t kSaveListPageCount =
    (kSaveSlotCount + kSaveListPageSize - 1U) / kSaveListPageSize;

enum class SaveListMode {
    load,
    save,
};

enum class SaveListEntryState {
    hidden,
    empty,
    ready,
    damaged,
};

struct SaveListEntry {
    std::uint16_t slot{};
    SaveListEntryState state{SaveListEntryState::empty};
    std::vector<std::uint8_t> protagonist_name;
    std::int16_t level{};
    text::GameText location;
    std::string saved_at;
};

NODISCARD constexpr std::uint16_t save_list_page(
    const std::uint16_t selection) noexcept {
    return static_cast<std::uint16_t>(selection / kSaveListPageSize);
}

}  // namespace openlegend::ui
