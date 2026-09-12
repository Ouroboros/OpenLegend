#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/render/legacy_color.hpp"
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

    NODISCARD NameEditStatus handle_key(
        std::uint8_t translated_key, bool control_down, bool shift_down);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD NameInputMode mode() const noexcept { return mode_; }

    NODISCARD std::span<const std::uint8_t> name() const noexcept { return name_; }

    NODISCARD std::span<const std::uint8_t> display_name() const noexcept {
        return display_name_;
    }

    NODISCARD std::span<const std::array<std::uint8_t, 2>> candidates() const noexcept {
        return candidates_;
    }

    NODISCARD std::int16_t candidate_page() const noexcept { return candidate_page_; }

    NODISCARD std::size_t visible_candidate_count() const noexcept;

    NODISCARD std::optional<std::array<std::uint8_t, 2>> visible_candidate(
        std::size_t visible_index) const noexcept;

    NODISCARD bool has_previous_candidate_page() const noexcept;

    NODISCARD bool has_next_candidate_page() const noexcept;

    NODISCARD bool no_candidates() const noexcept { return no_candidates_; }

    NODISCARD bool accepted() const noexcept { return accepted_; }

    NODISCARD render::PaletteIndex cursor_color() const noexcept {
        return cursor_bright_
            ? render::legacy_color::name_cursor_bright
            : render::legacy_color::name_cursor_dim;
    }

    void finish_presented_frame() noexcept;

    NODISCARD std::int16_t initial() const noexcept { return initial_; }

    NODISCARD std::int16_t medial() const noexcept { return medial_; }

    NODISCARD std::int16_t final() const noexcept { return final_; }

    NODISCARD std::int16_t tone() const noexcept { return tone_; }

private:
    struct ZhuyinKey {
        std::int16_t type{};
        std::int16_t value{};
    };

    NODISCARD static ZhuyinKey zhuyin_key(std::uint8_t translated_key) noexcept;

    NODISCARD bool has_composition() const noexcept;

    void clear_composition() noexcept;

    void erase_last() noexcept;

    void lookup_candidates();

    void commit_candidate(std::size_t visible_index);

    void sync_display_name();

    std::vector<std::uint8_t> cfont_;
    std::vector<std::uint8_t> name_;
    std::vector<std::uint8_t> display_name_;
    std::vector<std::uint8_t> unit_sizes_;
    std::vector<std::array<std::uint8_t, 2>> candidates_;
    std::string error_;
    NameInputMode mode_{NameInputMode::zhuyin};
    std::int16_t candidate_page_{};
    std::size_t candidate_data_begin_{};
    std::int16_t initial_{};
    std::int16_t medial_{};
    std::int16_t final_{};
    std::int16_t tone_{};
    bool no_candidates_{};
    bool accepted_{};
    bool cursor_bright_{};
};

}  // namespace openlegend::ui
