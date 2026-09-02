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
    if (accepted_) {
        return NameEditStatus::completed;
    }
    if (no_candidates_) {
        clear_composition();
        return NameEditStatus::editing;
    }

    if (!candidates_.empty()) {
        if (translated_key == kEscape) {
            clear_composition();
        } else if (translated_key >= '1' && translated_key <= '8') {
            commit_candidate(static_cast<std::size_t>(translated_key - '1'));
        } else if (shift_down && translated_key == kPeriod &&
                   has_next_candidate_page()) {
            ++candidate_page_;
        } else if (shift_down && translated_key == kComma &&
                   has_previous_candidate_page()) {
            --candidate_page_;
        } else if (translated_key == kSpace) {
            if (has_next_candidate_page()) {
                ++candidate_page_;
            } else if (has_previous_candidate_page()) {
                candidate_page_ = 0;
            }
        }
        return NameEditStatus::editing;
    }

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
        if (!name_.empty()) {
            accepted_ = true;
            return NameEditStatus::completed;
        }
        return NameEditStatus::editing;
    }
    if (translated_key == kEscape) {
        clear_composition();
        return NameEditStatus::editing;
    }

    const auto limit = mode_ == NameInputMode::zhuyin
        ? model::kNewGameNameMaximumBytes - 1U
        : model::kNewGameNameMaximumBytes;
    if (name_.size() >= limit) {
        return NameEditStatus::editing;
    }
    if (mode_ == NameInputMode::alphanumeric) {
        if (is_ascii_name_key(translated_key)) {
            name_.push_back(translated_key);
            unit_sizes_.push_back(1U);
            sync_display_name();
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
    std::size_t count = 0U;
    while (count < kCandidatesPerPage && visible_candidate(count).has_value()) {
        ++count;
    }
    return count;
}

std::optional<std::array<std::uint8_t, 2>> NewGameNameEditor::visible_candidate(
    const std::size_t visible_index) const noexcept {
    if (visible_index >= kCandidatesPerPage || candidates_.empty()) {
        return std::nullopt;
    }
    const auto index = static_cast<std::int64_t>(candidate_page_) *
                           static_cast<std::int64_t>(kCandidatesPerPage) +
                       static_cast<std::int64_t>(visible_index);
    if (index >= 0) {
        if (static_cast<std::size_t>(index) >= candidates_.size()) {
            return std::nullopt;
        }
        return candidates_[static_cast<std::size_t>(index)];
    }
    const auto offset = static_cast<std::int64_t>(candidate_data_begin_) + index * 2;
    if (offset < 0 || offset + 1 >= static_cast<std::int64_t>(cfont_.size())) {
        return std::nullopt;
    }
    return std::array<std::uint8_t, 2>{
        cfont_[static_cast<std::size_t>(offset)],
        cfont_[static_cast<std::size_t>(offset + 1)]};
}

bool NewGameNameEditor::has_previous_candidate_page() const noexcept {
    if (candidates_.empty()) {
        return false;
    }
    if (candidate_page_ == 0) {
        return candidates_.size() > kCandidatesPerPage;
    }
    if (candidate_page_ > 0) {
        return true;
    }
    const auto previous_begin = static_cast<std::int64_t>(candidate_data_begin_) +
        (static_cast<std::int64_t>(candidate_page_) - 1) *
            static_cast<std::int64_t>(kCandidatesPerPage * 2U);
    return previous_begin >= 0;
}

bool NewGameNameEditor::has_next_candidate_page() const noexcept {
    if (candidates_.empty()) {
        return false;
    }
    if (candidate_page_ < 0) {
        return true;
    }
    const auto next_begin = (static_cast<std::size_t>(candidate_page_) + 1U) *
        kCandidatesPerPage;
    return next_begin < candidates_.size();
}

void NewGameNameEditor::finish_presented_frame() noexcept {
    if (!accepted_ && candidates_.empty() && !no_candidates_ &&
        name_.size() < model::kNewGameNameMaximumBytes) {
        cursor_bright_ = !cursor_bright_;
    }
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
    candidate_page_ = 0;
    candidate_data_begin_ = 0U;
    no_candidates_ = false;
}

void NewGameNameEditor::erase_last() noexcept {
    if (has_composition()) {
        if (final_ != 0) {
            final_ = 0;
        } else if (medial_ != 0) {
            medial_ = 0;
        } else if (initial_ != 0) {
            initial_ = 0;
        }
        return;
    }
    if (unit_sizes_.empty()) {
        return;
    }
    const auto byte_count = unit_sizes_.back();
    const auto retain_single_ascii_display = unit_sizes_.size() == 1U && byte_count == 1U;
    unit_sizes_.pop_back();
    name_.resize(name_.size() - byte_count);
    if (!retain_single_ascii_display) {
        sync_display_name();
    }
}

void NewGameNameEditor::lookup_candidates() {
    candidates_.clear();
    candidate_page_ = 0;
    candidate_data_begin_ = 0U;
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

    candidate_data_begin_ =
        static_cast<std::size_t>(std::distance(cfont_.begin(), match)) + 1U;
    auto cursor = candidate_data_begin_;
    while (cursor < cfont_.size() && cfont_[cursor] >= 0x40U) {
        ++cursor;
    }
    const auto count = (cursor - candidate_data_begin_) / 2U;
    candidates_.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const auto offset = candidate_data_begin_ + index * 2U;
        candidates_.push_back({cfont_[offset], cfont_[offset + 1U]});
    }
    no_candidates_ = candidates_.empty();
}

void NewGameNameEditor::commit_candidate(const std::size_t visible_index) {
    const auto selected = static_cast<std::int64_t>(candidate_page_) *
                              static_cast<std::int64_t>(kCandidatesPerPage) +
                          static_cast<std::int64_t>(visible_index) + 1;
    if (selected < 0) {
        clear_composition();
        return;
    }
    if (selected == 0 || static_cast<std::size_t>(selected) > candidates_.size() ||
        name_.size() + 2U > model::kNewGameNameMaximumBytes) {
        return;
    }
    const auto index = static_cast<std::size_t>(selected - 1);
    name_.push_back(candidates_[index][0]);
    name_.push_back(candidates_[index][1]);
    unit_sizes_.push_back(2U);
    sync_display_name();
    clear_composition();
}

void NewGameNameEditor::sync_display_name() {
    display_name_ = name_;
}

}  // namespace openlegend::ui
