#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

#include "openlegend/attributes.hpp"
#include "openlegend/model/magic_progression.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "test_support.hpp"

namespace {

NODISCARD openlegend::model::MagicRecord uniform_magic(const std::int16_t attack) {
    using namespace openlegend::model;
    MagicRecord magic;
    for (std::size_t index = 0U; index < magic_word::level_value_count; ++index) {
        magic.set_word(magic_word::attack_begin + index, attack);
        magic.set_word(magic_word::select_distance_begin + index, 1);
    }
    return magic;
}

void check_calibration_and_boundaries() {
    using namespace openlegend::model;
    std::array magics{uniform_magic(100), uniform_magic(400), uniform_magic(100)};
    magics[2U].set_word(magic_word::id, 77);
    magics[2U].set_word(magic_word::sound_id, 42);
    magics[2U].bytes[magic_word::name_byte] = 'X';
    const MagicProgression distinct{std::span{magics}.first(2U)};
    const MagicProgression duplicated{magics};
    OL_CHECK(distinct.valid() && duplicated.valid());
    OL_CHECK(duplicated.references()[0U].prototype_count == 2U);
    OL_CHECK(duplicated.references()[0U].final_effect == 250.0L);
    OL_CHECK(duplicated.references()[1U].prototype_count == 0U);
    OL_CHECK(distinct.effects(0U, 20) == duplicated.effects(0U, 20));
    OL_CHECK(duplicated.effects(0U, 20) == duplicated.effects(2U, 20));
    magics[2U].set_word(magic_word::with_poison, 1);
    const MagicProgression different_poison{magics};
    OL_CHECK(different_poison.references()[0U].prototype_count == 3U);
    magics[2U].set_word(magic_word::with_poison, 0);
    magics[2U].set_word(magic_word::attack_begin, 0);
    const MagicProgression incomplete_positive{magics};
    OL_CHECK(incomplete_positive.references()[0U].prototype_count == 2U);
    OL_CHECK(incomplete_positive.effects(2U, 20)->attack == 100);
    OL_CHECK(incomplete_positive.effects(2U, 100'000)->attack == 100);

    const std::array single{uniform_magic(100)};
    const MagicProgression simple{single};
    OL_CHECK(simple.effects(0U, 10)->attack == 100);
    OL_CHECK(simple.effects(0U, 11)->attack == 103);
    OL_CHECK(simple.effects(0U, 19)->attack == 229);
    OL_CHECK(simple.effects(0U, 20)->attack == 232);
    OL_CHECK(simple.effects(0U, 21)->attack == 236);
    OL_CHECK(simple.effects(0U, 30)->attack == 375);
    OL_CHECK(!simple.effects(0U, std::numeric_limits<std::int64_t>::max()).has_value());
    OL_CHECK(!simple.effects(0U, 0).has_value());
    OL_CHECK(!simple.effects(1U, 20).has_value());

    auto altered = single;
    altered[0U].set_word(magic_word::select_distance_begin, 20);
    const std::array competition{single[0U], altered[0U]};
    const MagicProgression different_first_geometry{competition};
    OL_CHECK(different_first_geometry.effects(0U, 20)->attack >
             different_first_geometry.effects(1U, 20)->attack);
    altered[0U].set_word(magic_word::attack_area_type, 4);
    OL_CHECK(!MagicProgression{altered}.valid());
    altered[0U].set_word(magic_word::attack_area_type, 0);
    altered[0U].set_word(magic_word::need_mp, -1);
    OL_CHECK(!MagicProgression{altered}.valid());
    altered[0U].set_word(magic_word::need_mp, 0);
    altered[0U].set_word(magic_word::hurt_type, 2);
    OL_CHECK(!MagicProgression{altered}.valid());
    const std::array zero{uniform_magic(0)};
    const MagicProgression zero_progression{zero};
    OL_CHECK(zero_progression.valid());
    OL_CHECK(zero_progression.references()[0U].prototype_count == 0U);
    OL_CHECK(zero_progression.effects(0U, std::numeric_limits<std::int64_t>::max()) == MagicEffects{});
    const std::array small{uniform_magic(1)};
    const MagicProgression small_progression{small};
    const auto large = small_progression.effects(0U, std::numeric_limits<std::int64_t>::max());
    OL_CHECK(large.has_value());
    OL_CHECK(large->attack > 1'000'000'000'000'000'000);
}

void check_original_data() {
    using namespace openlegend;
    const auto loaded = persistence::load_baseline(test::game_data_root());
    OL_CHECK(static_cast<bool>(loaded));
    if (!loaded) {
        return;
    }
    const auto& magics = loaded.snapshot->ranger.magics;
    const model::MagicProgression progression{magics};
    OL_CHECK(progression.valid());
    if (!progression.valid()) {
        return;
    }
    const auto& references = progression.references();
    OL_CHECK(references[0U].prototype_count == 84U);
    OL_CHECK(references[1U].prototype_count == 3U);
    OL_CHECK(references[0U].final_effect == 400.0L);
    OL_CHECK(references[1U].final_effect == 113.75L);
    OL_CHECK(std::abs(references[0U].strength - 22.313334L) < 0.0000005L);
    OL_CHECK(std::abs(references[1U].strength - 9.336457L) < 0.0000005L);
    for (std::size_t magic_id = 0U; magic_id < magics.size(); ++magic_id) {
        for (std::int64_t level = 1; level <= 10; ++level) {
            const auto index = static_cast<std::size_t>(level - 1);
            const model::MagicEffects expected{
                magics[magic_id].word(model::magic_word::attack_begin + index),
                magics[magic_id].word(model::magic_word::hurt_mp_begin + index),
                magics[magic_id].word(model::magic_word::add_mp_begin + index),
            };
            OL_CHECK(progression.effects(magic_id, level) == expected);
        }
        auto previous = *progression.effects(magic_id, 10);
        for (std::int64_t level = 11; level <= 10'020; ++level) {
            const auto current = progression.effects(magic_id, level);
            const auto increasing = current.has_value() &&
                current->attack >= previous.attack && current->hurt_mp >= previous.hurt_mp &&
                current->add_mp >= previous.add_mp;
            OL_CHECK(increasing);
            if (!increasing) {
                return;
            }
            previous = *current;
        }
    }
    struct AttackExample {
        std::size_t magic_id;
        std::array<std::int64_t, 8> attacks;
    };
    constexpr std::array examples{
        AttackExample{25U, {900, 2944, 5133, 7394, 9697, 12025, 14370, 16728}},
        AttackExample{60U, {1200, 3030, 4990, 7014, 9076, 11160, 13260, 15371}},
        AttackExample{22U, {770, 2149, 3625, 5151, 6704, 8274, 9857, 11447}},
        AttackExample{92U, {700, 1868, 3119, 4411, 5727, 7057, 8397, 9744}},
        AttackExample{51U, {350, 712, 1101, 1502, 1911, 2324, 2741, 3159}},
    };
    for (const auto& example : examples) {
        for (std::size_t stage = 0U; stage < example.attacks.size(); ++stage) {
            const auto level = static_cast<std::int64_t>((stage + 1U) * 10U);
            OL_CHECK(progression.effects(example.magic_id, level)->attack == example.attacks[stage]);
        }
    }
    OL_CHECK(progression.effects(29U, 20)->hurt_mp == 200);
    OL_CHECK(progression.effects(29U, 20)->add_mp == 200);
    OL_CHECK(progression.effects(28U, 20)->hurt_mp == 186);
    OL_CHECK(progression.effects(28U, 20)->add_mp == 104);
    OL_CHECK(progression.effects(27U, 20)->hurt_mp == 177);
    OL_CHECK(progression.effects(27U, 20)->add_mp == 0);
    OL_CHECK(progression.effects(27U, 10'020)->add_mp == 0);
    OL_CHECK(progression.effects(25U, 10'020)->attack > progression.effects(25U, 9'990)->attack);
    OL_CHECK(!progression.effects(25U, std::numeric_limits<std::int64_t>::max()).has_value());
}

void check_levels_and_costs() {
    using namespace openlegend::model;
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(magic_level_from_proficiency(0) == 1);
    OL_CHECK(magic_level_from_proficiency(999) == 10);
    OL_CHECK(magic_level_from_proficiency(1'000) == 11);
    OL_CHECK(magic_level_from_proficiency(5'000'000'000'499) == 50'000'000'005);
    OL_CHECK(magic_level_from_proficiency(maximum) == maximum / 100 + 1);
    OL_CHECK(!magic_level_from_proficiency(-1).has_value());
    OL_CHECK(magic_mp_cost(1, maximum) == 0);
    OL_CHECK(magic_mp_cost(2, maximum) == maximum);
    OL_CHECK(magic_mp_cost(11, 10) == 50);
    OL_CHECK(magic_mp_cost(12, 10) == 60);
    OL_CHECK(magic_mp_cost(maximum, 0) == 0);
    OL_CHECK(!magic_mp_cost(4, maximum).has_value());
    OL_CHECK(!magic_mp_cost(0, 1).has_value());
    OL_CHECK(!magic_mp_cost(2, -1).has_value());
    OL_CHECK(affordable_magic_level(20, 49, 10) == 9);
    OL_CHECK(affordable_magic_level(20, 50, 10) == 11);
    OL_CHECK(affordable_magic_level(10, 50, 10) == 10);
    OL_CHECK(affordable_magic_level(20, 0, 10) == 1);
    OL_CHECK(affordable_magic_level(maximum, 0, 0) == maximum);
    OL_CHECK(affordable_magic_level(maximum, maximum, 3) == (maximum / 3) * 2 + 1);
    OL_CHECK(!affordable_magic_level(20, -1, 10).has_value());
    OL_CHECK(!affordable_magic_level(20, 50, -1).has_value());
}

}

int main() {
    check_calibration_and_boundaries();
    check_original_data();
    check_levels_and_costs();
    return openlegend::test::failures == 0 ? 0 : 1;
}
