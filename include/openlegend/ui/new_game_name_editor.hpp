#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "openlegend/resource/binary_file.hpp"

namespace openlegend::ui {

enum class NameInputMode {
    zhuyin,
    alphanumeric,
};

enum class NameEditStatus {
    editing,
    completed,
};

class NewGameNameEditor {
public:
    explicit NewGameNameEditor(const resource::DataRoot& data_root);

    [[nodiscard]] NameEditStatus handle_key(
        std::uint8_t translated_key, bool control_down, bool shift_down);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] NameInputMode mode() const noexcept { return mode_; }
    [[nodiscard]] std::span<const std::uint8_t> name() const noexcept { return name_; }
    [[nodiscard]] std::span<const std::array<std::uint8_t, 2>> candidates() const noexcept {
        return candidates_;
    }
    [[nodiscard]] std::size_t candidate_page() const noexcept { return candidate_page_; }
    [[nodiscard]] std::size_t visible_candidate_count() const noexcept;
    [[nodiscard]] bool no_candidates() const noexcept { return no_candidates_; }
    [[nodiscard]] std::int16_t initial() const noexcept { return initial_; }
    [[nodiscard]] std::int16_t medial() const noexcept { return medial_; }
    [[nodiscard]] std::int16_t final() const noexcept { return final_; }
    [[nodiscard]] std::int16_t tone() const noexcept { return tone_; }

private:
    struct ZhuyinKey {
        std::int16_t type{};
        std::int16_t value{};
    };

    [[nodiscard]] static ZhuyinKey zhuyin_key(std::uint8_t translated_key) noexcept;
    [[nodiscard]] bool has_composition() const noexcept;
    void clear_composition() noexcept;
    void erase_last() noexcept;
    void lookup_candidates();
    void commit_candidate(std::size_t visible_index);

    std::vector<std::uint8_t> cfont_;
    std::vector<std::uint8_t> name_;
    std::vector<std::uint8_t> unit_sizes_;
    std::vector<std::array<std::uint8_t, 2>> candidates_;
    std::string error_;
    NameInputMode mode_{NameInputMode::zhuyin};
    std::size_t candidate_page_{};
    std::int16_t initial_{};
    std::int16_t medial_{};
    std::int16_t final_{};
    std::int16_t tone_{};
    bool no_candidates_{};
};

}  // namespace openlegend::ui
