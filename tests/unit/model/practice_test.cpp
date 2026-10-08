#include <array>
#include <cstdint>
#include <limits>
#include <memory>

#include "openlegend/battle/battle_data.hpp"
#include "openlegend/battle/battle_renderer.hpp"
#include "openlegend/battle/battle_setup.hpp"
#include "openlegend/model/practice.hpp"
#include "test_support.hpp"

namespace {

void check_rules_and_costs(const openlegend::battle::BattleData& data) {
    using namespace openlegend::model;
    const auto& rules = data.practice_rules();
    OL_CHECK(practice_rules_valid(rules));
    OL_CHECK(rules.aptitude_base == 7);
    OL_CHECK(rules.aptitude_step == 15);
    OL_CHECK(rules.unlearned_level == 1);
    OL_CHECK(rules.unassociated_first_level == 2);
    OL_CHECK(rules.reward_numerator == 8);
    OL_CHECK(rules.reward_denominator == 10);
    OL_CHECK(practice_experience_requirement(10, 60, 2, rules) == 60);
    OL_CHECK(training_experience_reward(9, rules) == 7);
    OL_CHECK(!practice_experience_requirement(10, -1, 2, rules).has_value());
    OL_CHECK(!practice_experience_requirement(10, 101, 2, rules).has_value());
    OL_CHECK(!practice_experience_requirement(32767, 0,
        std::numeric_limits<std::int64_t>::max(), rules).has_value());
    const PracticeRules different{12, 10, 2, 3, 3, 4};
    OL_CHECK(practice_experience_requirement(10, 60, 2, different) == 120);
    OL_CHECK(training_experience_reward(11, different) == 8);
    OL_CHECK(!practice_rules_valid(PracticeRules{}));

    RoleState role;
    role.iq = 60;
    ItemRecord item;
    item.set_word(item_word::magic_id, -1);
    item.set_word(item_word::need_experience, 10);
    role.no_magic_count[5U] = 5;
    OL_CHECK(manual_experience_requirement(role, item, 5U, rules)->experience == 210);
    OL_CHECK(manual_experience_requirement(role, item, 6U, rules)->experience == 60);
    OL_CHECK(manual_experience_requirement(role, item, 6U, different)->experience == 180);
    role.no_magic_count[5U] = std::numeric_limits<std::int64_t>::max();
    OL_CHECK(!manual_experience_requirement(role, item, 5U, rules).has_value());
    item.set_word(item_word::magic_id, 2);
    role.magic_ids[0U] = MagicId{2};
    role.magic_ids[1U] = MagicId{2};
    role.magic_levels[0U] = 199;
    role.magic_levels[1U] = 10'000;
    OL_CHECK(manual_experience_requirement(role, item, 5U, rules)->experience == 60);
    role.magic_levels[0U] = 1'000'099;
    OL_CHECK(manual_experience_requirement(role, item, 5U, rules)->experience == 300'030);
}

void check_manual_transactions(openlegend::battle::BattleData& data) {
    using namespace openlegend;
    auto ranger = std::make_unique<model::RuntimeRangerState>();
    auto& role = ranger->roles[0U];
    auto& item = ranger->items[5U];
    role.iq = 60;
    role.practice_item = model::ItemId{5};
    role.maximum_hp = 5'000'000'000;
    role.maximum_mp = 6'000'000'000;
    role.attack = 7'000'000'000;
    item.set_word(model::item_word::magic_id, -1);
    item.set_word(model::item_word::need_experience, 10);
    item.set_word(model::item_word::add_maximum_hp, 10);
    item.set_word(model::item_word::add_maximum_mp, 20);
    item.set_word(model::item_word::add_attack, 80);
    for (const auto count : std::array<std::int64_t, 3>{0, 1, 4'000'000'000}) {
        role.no_magic_count[5U] = count;
        const auto expected_cost = 30 * (count + 2);
        role.item_experience = expected_cost - 1;
        battle::BattleSetup setup{data, *ranger};
        OL_CHECK(setup.valid());
        const auto before = role;
        const auto pending = setup.apply_battle_practice(0U, false);
        OL_CHECK(pending.has_value());
        OL_CHECK(!pending->practiced);
        OL_CHECK(role == before);
        role.item_experience = expected_cost + 1'000'000;
        const auto success = setup.apply_battle_practice(0U, false);
        OL_CHECK(success.has_value());
        OL_CHECK(success->practiced);
        OL_CHECK(success->required_experience == expected_cost);
        OL_CHECK(role.item_experience == 0);
        OL_CHECK(role.no_magic_count[5U] == count + 1);
        OL_CHECK(role.no_magic_count[6U] == 0);
        OL_CHECK(ranger->roles[1U].no_magic_count[5U] == 0);
        OL_CHECK(role.maximum_hp == before.maximum_hp + 10);
        OL_CHECK(role.maximum_mp == before.maximum_mp + 20);
        OL_CHECK(role.attack == before.attack + 80);
    }
    item.set_word(model::item_word::magic_id, 2);
    role.magic_ids[0U] = model::MagicId{2};
    role.magic_levels[0U] = 1'000'099;
    role.item_experience = 300'030;
    const auto history = role.no_magic_count;
    battle::BattleSetup high_setup{data, *ranger};
    const auto high = high_setup.apply_battle_practice(0U, false);
    OL_CHECK(high.has_value());
    OL_CHECK(high->practiced);
    OL_CHECK(role.magic_levels[0U] == 1'000'199);
    OL_CHECK(role.no_magic_count == history);

    for (const auto field : {&model::RoleState::maximum_hp, &model::RoleState::attack,
                             &model::RoleState::attack_with_poison}) {
        auto candidate = std::make_unique<model::RuntimeRangerState>(*ranger);
        candidate->roles[0U].*field = std::numeric_limits<std::int64_t>::max();
        candidate->roles[0U].item_experience = std::numeric_limits<std::int64_t>::max();
        candidate->items[5U].set_word(model::item_word::add_attack_with_poison, 1);
        const auto before = candidate->roles[0U];
        battle::BattleSetup setup{data, *candidate};
        OL_CHECK(!setup.apply_battle_practice(0U, false).has_value());
        OL_CHECK(candidate->roles[0U] == before);
    }
    role.magic_levels[0U] = std::numeric_limits<std::int64_t>::max();
    role.item_experience = std::numeric_limits<std::int64_t>::max();
    const auto before = role;
    battle::BattleSetup overflow_setup{data, *ranger};
    OL_CHECK(!overflow_setup.apply_battle_practice(0U, false).has_value());
    OL_CHECK(role == before);
}

void check_cost_display(const openlegend::resource::DataRoot& data_root) {
    using namespace openlegend;
    auto ranger = std::make_unique<model::RuntimeRangerState>();
    auto& role = ranger->roles[0U];
    role.iq = 60;
    role.practice_item = model::ItemId{5};
    role.no_magic_count[5U] = 3;
    ranger->items[5U].set_word(model::item_word::magic_id, -1);
    ranger->items[5U].set_word(model::item_word::need_experience, 10);
    battle::BattleRenderer renderer{data_root, 0};
    render::IndexedFramebuffer actual;
    render::IndexedFramebuffer expected;
    actual.clear(0);
    expected.clear(0);
    OL_CHECK(renderer.render_character_status(*ranger, 0, 1U, actual));
    OL_CHECK(renderer.draw_box(expected, 55, 0, 210U, 200U));
    OL_CHECK(renderer.draw_text_utf8(expected, 108, 175, u8"  150", render::legacy_color::text::menu_normal));
    for (std::size_t row = 175U; row < 191U; ++row) {
        for (std::size_t column = 108U; column < 148U; ++column) {
            const auto offset = row * render::IndexedFramebuffer::width + column;
            OL_CHECK(actual.pixels()[offset] == expected.pixels()[offset]);
        }
    }
}

}

int main() {
    const openlegend::resource::DataRoot data_root{openlegend::test::game_data_root()};
    openlegend::battle::BattleData data{data_root, 4};
    OL_CHECK(data.valid());
    if (data.valid()) {
        check_rules_and_costs(data);
        check_manual_transactions(data);
        check_cost_display(data_root);
    }
    return openlegend::test::failures == 0 ? 0 : 1;
}
