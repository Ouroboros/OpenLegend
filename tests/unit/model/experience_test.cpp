#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/model/experience.hpp"
#include "test_support.hpp"

namespace {

void check_original_table_and_extension() {
    using namespace openlegend;
    const battle::BattleData data{resource::DataRoot{test::game_data_root()}, 4};
    OL_CHECK(data.valid());
    if (!data.valid()) {
        return;
    }
    const auto thresholds = data.experience_thresholds();
    constexpr std::array<std::uint16_t, 30> expected{
        0, 50, 150, 300, 500, 750, 1050, 1400, 1800, 2250,
        2750, 3850, 5050, 6350, 7750, 9250, 10850, 12550, 14350, 16750,
        18250, 21400, 24700, 28150, 31750, 35500, 39400, 43450, 47650, 52000};
    for (std::size_t index = 0U; index < expected.size(); ++index) {
        const auto level = static_cast<std::int64_t>(index + 1U);
        OL_CHECK(thresholds[index] == expected[index]);
        OL_CHECK(model::level_experience_requirement(thresholds, level) == expected[index]);
        OL_CHECK(model::level_for_experience(thresholds, 1, expected[index]) == level);
        if (index > 0U) {
            OL_CHECK(model::level_for_experience(thresholds, 1, expected[index] - 1) == level - 1);
        }
    }
    OL_CHECK(model::level_experience_requirement(thresholds, 31) == 56'500);
    OL_CHECK(model::level_experience_requirement(thresholds, 60) == 252'250);
    OL_CHECK(model::level_experience_requirement(thresholds, 90) == 587'500);
    std::int64_t cumulative = thresholds.back();
    const auto last_index = thresholds.size() - 1U;
    std::int64_t cost = thresholds[last_index] - thresholds[last_index - 1U];
    const auto step = cost - (thresholds[last_index - 1U] - thresholds[last_index - 2U]);
    for (std::int64_t level = 31; level <= 10'020; ++level) {
        cost += step;
        cumulative += cost;
        OL_CHECK(model::level_experience_requirement(thresholds, level) == cumulative);
    }
    OL_CHECK(model::level_for_experience(thresholds, 1, cumulative) == 10'020);
    OL_CHECK(model::level_for_experience(thresholds, 50, 0) == 50);
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(!model::level_experience_requirement(thresholds, maximum).has_value());
    const auto highest = model::level_for_experience(thresholds, 1, maximum);
    OL_CHECK(highest.has_value());
    if (highest.has_value()) {
        OL_CHECK(model::level_experience_requirement(thresholds, *highest).has_value());
        OL_CHECK(!model::level_experience_requirement(thresholds, *highest + 1).has_value());
    }
}

void check_next_level_display() {
    using namespace openlegend;
    battle::BattleRenderer renderer{resource::DataRoot{test::game_data_root()}, 0};
    OL_CHECK(renderer.valid());
    if (!renderer.valid()) {
        return;
    }
    auto ranger = std::make_unique<model::RuntimeRangerState>();
    constexpr std::array<std::pair<std::int64_t, std::u8string_view>, 4> cases{{
        {29, u8" 52000"}, {30, u8" 56500"}, {59, u8"252250"}, {89, u8"587500"}}};
    for (const auto& [level, text] : cases) {
        ranger->roles[0U].level = level;
        render::IndexedFramebuffer actual;
        render::IndexedFramebuffer expected;
        actual.clear(0);
        expected.clear(0);
        OL_CHECK(renderer.render_character_status(*ranger, 0, 0U, actual));
        OL_CHECK(renderer.draw_box(expected, 55, 0, 210U, 200U));
        OL_CHECK(renderer.draw_text_utf8(expected, 97, 175, text));
        for (std::size_t row = 175U; row < 191U; ++row) {
            for (std::size_t column = 97U; column < 145U; ++column) {
                const auto offset = row * render::IndexedFramebuffer::width + column;
                OL_CHECK(actual.pixels()[offset] == expected.pixels()[offset]);
            }
        }
    }
}

void check_derived_rules_and_invalid_inputs() {
    using namespace openlegend::model;
    constexpr std::array<std::uint16_t, 3> different{0, 10, 30};
    OL_CHECK(level_experience_requirement(different, 4) == 60);
    OL_CHECK(level_experience_requirement(different, 5) == 100);
    OL_CHECK(level_for_experience(different, 1, 99) == 4);
    OL_CHECK(!level_experience_requirement(different, 0).has_value());
    OL_CHECK(!level_for_experience(different, 0, 10).has_value());
    OL_CHECK(!level_for_experience(different, 1, -1).has_value());
    OL_CHECK(!experience_thresholds_valid({}));
    OL_CHECK(!experience_thresholds_valid(std::array<std::uint16_t, 2>{0, 10}));
    OL_CHECK(!experience_thresholds_valid(std::array<std::uint16_t, 3>{1, 10, 30}));
    OL_CHECK(!experience_thresholds_valid(std::array<std::uint16_t, 3>{0, 10, 10}));
    OL_CHECK(!experience_thresholds_valid(std::array<std::uint16_t, 3>{0, 10, 15}));
    constexpr std::array<std::uint16_t, 3> linear{0, 1, 2};
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(level_experience_requirement(linear, maximum) == maximum - 1);
    OL_CHECK(level_for_experience(linear, 1, maximum) == maximum);
}

}

int main() {
    check_original_table_and_extension();
    check_derived_rules_and_invalid_inputs();
    check_next_level_display();
    return openlegend::test::failures == 0 ? 0 : 1;
}
