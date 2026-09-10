#pragma once

#include <array>
#include <cstdint>

namespace openlegend::render {

using PaletteIndex = std::uint8_t;

struct TextColors {
    PaletteIndex right_shadow{};
    PaletteIndex foreground{};

    [[nodiscard]] static constexpr TextColors from_legacy_packed(
        const std::uint16_t packed) noexcept {
        return {
            static_cast<PaletteIndex>(packed & 0x00FFU),
            static_cast<PaletteIndex>(packed >> 8U),
        };
    }

    [[nodiscard]] constexpr std::uint16_t legacy_packed() const noexcept {
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(foreground) << 8U |
            static_cast<std::uint16_t>(right_shadow));
    }

    friend constexpr bool operator==(const TextColors&, const TextColors&) = default;
};

namespace legacy_color {

inline constexpr PaletteIndex black = 0x00U;
inline constexpr PaletteIndex name_cursor_dim = 0x07U;
inline constexpr PaletteIndex name_cursor_bright = 0x09U;
inline constexpr PaletteIndex attribute_highlight_background = 0x15U;
inline constexpr PaletteIndex scroll_indicator = 0x63U;
inline constexpr PaletteIndex panel_outline = 0xFFU;
inline constexpr PaletteIndex menu_background = black;
inline constexpr PaletteIndex inventory_outline = black;
inline constexpr PaletteIndex inventory_selection_outline = panel_outline;
inline constexpr PaletteIndex battle_flash_standard = panel_outline;
inline constexpr PaletteIndex battle_flash_damage_kind_two = 0x2FU;
inline constexpr PaletteIndex battle_flash_alternate = 0x4EU;
inline constexpr PaletteIndex first_palette_cycle_begin = 224U;
inline constexpr PaletteIndex first_palette_cycle_pivot = 231U;
inline constexpr PaletteIndex first_palette_cycle_end = 232U;
inline constexpr PaletteIndex second_palette_cycle_begin = 244U;
inline constexpr PaletteIndex second_palette_cycle_pivot = 252U;
inline constexpr PaletteIndex second_palette_cycle_end = 253U;

namespace text {

inline constexpr TextColors transparent{0x00U, 0x00U};
inline constexpr TextColors notice{0x05U, 0x07U};
inline constexpr TextColors moderate_injury{0x10U, 0x0EU};
inline constexpr TextColors negative_value{0x14U, 0x10U};
inline constexpr TextColors severe_injury{0x16U, 0x14U};
inline constexpr TextColors death_detail{0x17U, 0x15U};
inline constexpr TextColors normal{0x15U, 0x17U};
inline constexpr TextColors new_game_prompt_inactive = normal;
inline constexpr TextColors attribute_normal = normal;
inline constexpr TextColors candidate{0x19U, 0x17U};
inline constexpr TextColors attribute_question{0x06U, 0x08U};
inline constexpr TextColors attribute_highlight{0x1DU, 0x1FU};
inline constexpr TextColors menu_normal{0x21U, 0x23U};
inline constexpr TextColors hp_separator{0x22U, 0x23U};
inline constexpr TextColors poison{0x32U, 0x30U};
inline constexpr TextColors severe_poison{0x37U, 0x35U};
inline constexpr TextColors yin_mp{0x4EU, 0x50U};
inline constexpr TextColors alternate_mp{0x53U, 0x50U};
inline constexpr TextColors selected{0x63U, 0x66U};
inline constexpr TextColors scene_dialogue{0x00U, 0x64U};
inline constexpr std::array<TextColors, 6> battle_damage_numbers{
    TextColors{0x00U, 0x00U},
    TextColors{0x14U, 0x10U},
    TextColors{0x32U, 0x30U},
    TextColors{0x93U, 0x91U},
    TextColors{0x05U, 0x07U},
    TextColors{0x53U, 0x50U},
};
inline constexpr TextColors death_name{0x6EU, 0x6CU};
inline constexpr TextColors alternate_action{0x93U, 0x91U};
inline constexpr TextColors name_value{0x03U, 0x05U};

inline constexpr TextColors dialogue = normal;
inline constexpr TextColors save_list_normal = menu_normal;
inline constexpr TextColors save_list_selected = selected;

}  // namespace text
}  // namespace legacy_color
}  // namespace openlegend::render
