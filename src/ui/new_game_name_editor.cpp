#include "openlegend/ui/new_game_name_editor.hpp"

#include <algorithm>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/model/new_game.hpp"

namespace openlegend::ui {
namespace {

constexpr std::uint8_t kBackspace = 0x08U;
constexpr std::uint8_t kEnter = 0x0DU;
constexpr std::uint8_t kEscape = 0x1BU;
constexpr std::uint8_t kSpace = 0x20U;
constexpr std::uint8_t kComma = 0x2CU;
constexpr std::uint8_t kPeriod = 0x2EU;
constexpr std::size_t kCandidatesPerPage = 8U;
constexpr std::size_t kCfontBytes = 29'674U;
constexpr std::size_t kCfontBoundaryCount = 111U;

[[nodiscard]] constexpr bool is_ascii_name_key(const std::uint8_t key) noexcept {
    return (key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z');
}

}  // namespace

NewGameNameEditor::NewGameNameEditor(const resource::DataRoot& data_root) {
    auto cfont = data_root.read("CFONT");
    if (!cfont) {
        error_ = cfont.error;
        return;
    }
    if (cfont.bytes.size() != kCfontBytes) {
        error_ = "CFONT does not have the original 29674-byte layout";
        return;
    }
    const auto bytes = std::span<const std::uint8_t>{cfont.bytes};
    const auto first_data = compat::read_u16le(bytes, 0U);
    if (first_data != kCfontBoundaryCount * 2U) {
        error_ = "CFONT cumulative boundary table has an unexpected size";
        return;
    }
    std::uint16_t previous = first_data;
    for (std::size_t index = 1U; index < kCfontBoundaryCount; ++index) {
        const auto boundary = compat::read_u16le(bytes, index * 2U);
        if (boundary < previous || boundary > bytes.size()) {
            error_ = "CFONT cumulative boundaries are invalid";
            return;
        }
        previous = boundary;
    }
    cfont_ = std::move(cfont.bytes);
}

NameEditStatus NewGameNameEditor::handle_key(
    const std::uint8_t translated_key,
    const bool control_down,
    const bool shift_down) {
    if (!valid()) {
        return NameEditStatus::editing;
    }
    no_candidates_ = false;

    if (control_down && translated_key == kSpace) {
        mode_ = mode_ == NameInputMode::zhuyin ? NameInputMode::alphanumeric
                                               : NameInputMode::zhuyin;
        clear_composition();
        return NameEditStatus::editing;
    }
    if (translated_key == kBackspace) {
        erase_last();
        return NameEditStatus::editing;
    }
    if (translated_key == kEnter) {
        return name_.empty() ? NameEditStatus::editing : NameEditStatus::completed;
    }
    if (translated_key == kEscape) {
        clear_composition();
        return NameEditStatus::editing;
    }

    if (!candidates_.empty()) {
        if (translated_key >= '1' && translated_key <= '8') {
            commit_candidate(static_cast<std::size_t>(translated_key - '1'));
        } else if ((shift_down && translated_key == kPeriod) || translated_key == kSpace) {
            const auto next_page = candidate_page_ + 1U;
            if (next_page * kCandidatesPerPage < candidates_.size()) {
                candidate_page_ = next_page;
            } else if (translated_key == kSpace && candidate_page_ != 0U) {
                candidate_page_ = 0U;
            }
        } else if (shift_down && translated_key == kComma && candidate_page_ != 0U) {
            --candidate_page_;
        }
        return NameEditStatus::editing;
    }

    if (name_.size() >= model::kNewGameNameMaximumBytes) {
        return NameEditStatus::editing;
    }
    if (mode_ == NameInputMode::alphanumeric) {
        if (is_ascii_name_key(translated_key)) {
            name_.push_back(translated_key);
            unit_sizes_.push_back(1U);
        }
        return NameEditStatus::editing;
    }

    if (translated_key == kSpace) {
        if (has_composition()) {
            lookup_candidates();
        }
        return NameEditStatus::editing;
    }

    const auto mapping = zhuyin_key(translated_key);
    switch (mapping.type) {
    case 1: initial_ = mapping.value; break;
    case 2: medial_ = mapping.value; break;
    case 3: final_ = mapping.value; break;
    case 4:
        tone_ = mapping.value;
        lookup_candidates();
        break;
    default: break;
    }
    return NameEditStatus::editing;
}

std::size_t NewGameNameEditor::visible_candidate_count() const noexcept {
    const auto begin = candidate_page_ * kCandidatesPerPage;
    if (begin >= candidates_.size()) {
        return 0U;
    }
    return std::min(kCandidatesPerPage, candidates_.size() - begin);
}

NewGameNameEditor::ZhuyinKey NewGameNameEditor::zhuyin_key(
    const std::uint8_t translated_key) noexcept {
    switch (translated_key) {
    case 0x2C: return {3, 4};
    case 0x2D: return {3, 13};
    case 0x2E: return {3, 8};
    case 0x2F: return {3, 12};
    case '0': return {3, 9};
    case '1': return {1, 1};
    case '2': return {1, 5};
    case '3': return {4, 3};
    case '4': return {4, 4};
    case '5': return {1, 15};
    case '6': return {4, 2};
    case '7': return {4, 1};
    case '8': return {3, 1};
    case '9': return {3, 5};
    case 0x3A:
    case 0x3B: return {3, 11};
    case 0x3C: return {3, 4};
    case 0x3E: return {3, 8};
    case 0x3F: return {3, 12};
    case 'A': return {1, 3};
    case 'B': return {1, 18};
    case 'C': return {1, 11};
    case 'D': return {1, 10};
    case 'E': return {1, 9};
    case 'F': return {1, 13};
    case 'G': return {1, 17};
    case 'H': return {1, 20};
    case 'I': return {3, 2};
    case 'J': return {2, 2};
    case 'K': return {3, 3};
    case 'L': return {3, 7};
    case 'M': return {2, 3};
    case 'N': return {1, 21};
    case 'O': return {3, 6};
    case 'P': return {3, 10};
    case 'Q': return {1, 2};
    case 'R': return {1, 12};
    case 'S': return {1, 7};
    case 'T': return {1, 16};
    case 'U': return {2, 1};
    case 'V': return {1, 14};
    case 'W': return {1, 6};
    case 'X': return {1, 8};
    case 'Y': return {1, 19};
    case 'Z': return {1, 4};
    default: return {};
    }
}

bool NewGameNameEditor::has_composition() const noexcept {
    return initial_ != 0 || medial_ != 0 || final_ != 0 || tone_ != 0;
}

void NewGameNameEditor::clear_composition() noexcept {
    initial_ = 0;
    medial_ = 0;
    final_ = 0;
    tone_ = 0;
    candidates_.clear();
    candidate_page_ = 0U;
    no_candidates_ = false;
}

void NewGameNameEditor::erase_last() noexcept {
    if (!candidates_.empty() || has_composition()) {
        if (!candidates_.empty()) {
            candidates_.clear();
            candidate_page_ = 0U;
        } else if (tone_ != 0) {
            tone_ = 0;
        } else if (final_ != 0) {
            final_ = 0;
        } else if (medial_ != 0) {
            medial_ = 0;
        } else {
            initial_ = 0;
        }
        return;
    }
    if (unit_sizes_.empty()) {
        return;
    }
    const auto byte_count = unit_sizes_.back();
    unit_sizes_.pop_back();
    name_.resize(name_.size() - byte_count);
}

void NewGameNameEditor::lookup_candidates() {
    candidates_.clear();
    candidate_page_ = 0U;
    const auto index = static_cast<std::size_t>(initial_) * 5U +
                       static_cast<std::size_t>(tone_);
    if (index + 1U >= kCfontBoundaryCount) {
        no_candidates_ = true;
        return;
    }

    const auto bytes = std::span<const std::uint8_t>{cfont_};
    const auto begin = static_cast<std::size_t>(compat::read_u16le(bytes, index * 2U));
    const auto end = static_cast<std::size_t>(compat::read_u16le(bytes, (index + 1U) * 2U));
    const auto packed = static_cast<std::uint8_t>(
        (static_cast<std::uint8_t>(medial_) << 4U) + static_cast<std::uint8_t>(final_));
    const auto match = std::find(
        cfont_.begin() + static_cast<std::ptrdiff_t>(begin),
        cfont_.begin() + static_cast<std::ptrdiff_t>(end),
        packed);
    if (match == cfont_.begin() + static_cast<std::ptrdiff_t>(end)) {
        no_candidates_ = true;
        return;
    }

    auto cursor = static_cast<std::size_t>(std::distance(cfont_.begin(), match)) + 1U;
    while (cursor + 1U < end && cfont_[cursor] >= 0x40U && cfont_[cursor + 1U] >= 0x40U) {
        candidates_.push_back({cfont_[cursor], cfont_[cursor + 1U]});
        cursor += 2U;
    }
    no_candidates_ = candidates_.empty();
}

void NewGameNameEditor::commit_candidate(const std::size_t visible_index) {
    const auto index = candidate_page_ * kCandidatesPerPage + visible_index;
    if (index >= candidates_.size() ||
        name_.size() + 2U > model::kNewGameNameMaximumBytes) {
        return;
    }
    name_.push_back(candidates_[index][0]);
    name_.push_back(candidates_[index][1]);
    unit_sizes_.push_back(2U);
    clear_composition();
}

}  // namespace openlegend::ui
