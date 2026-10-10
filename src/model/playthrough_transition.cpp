#include "openlegend/model/playthrough_transition.hpp"

#include <utility>

#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {

PlaythroughTransitionResult prepare_next_playthrough(
    const RuntimeGameSnapshot& completed, const GameSnapshot& baseline,
    const NewGamePlusConfiguration& configuration) {
    if (!configuration.enabled || !calculate_playthrough_limits(configuration, 1)) {
        return {{}, "NG+ configuration is disabled or invalid"};
    }
    if (completed.origin != SnapshotOrigin::new_game_plus || !completed.valid_for_persistence()) {
        return {{}, "Completed playthrough snapshot is invalid"};
    }
    if (!completed.ranger.matches_legacy_definitions(baseline.ranger)) {
        return {{}, "Completed definitions do not match the new playthrough baseline"};
    }
    const auto next = checked_add(completed.playthrough, 1);
    if (!next || !calculate_playthrough_limits(configuration, *next)) {
        return {{}, "Next playthrough exceeds the configured limit"};
    }
    auto candidate = decode_legacy_snapshot(baseline, configuration, &baseline.ranger);
    if (!candidate || candidate->ranger.roles.size() != completed.ranger.roles.size()) {
        return {{}, "New playthrough baseline is invalid"};
    }
    candidate->origin = SnapshotOrigin::new_game_plus;
    candidate->playthrough = *next;
    for (std::size_t index = 0U; index < candidate->ranger.roles.size(); ++index) {
        auto& destination = candidate->ranger.roles[index];
        const auto& previous = completed.ranger.roles[index];
        if (previous.ever_joined) {
            auto inherited = previous;
            inherited.sex = destination.sex;
            inherited.frames = destination.frames;
            inherited.morality = destination.morality;
            inherited.attack_twice = destination.attack_twice;
            inherited.fame = destination.fame;
            inherited.taking_items = destination.taking_items;
            inherited.taking_counts = destination.taking_counts;
            inherited.hp = inherited.maximum_hp;
            inherited.mp = inherited.maximum_mp;
            inherited.physical_power = 100;
            inherited.hurt = 0;
            inherited.poison = 0;
            inherited.equipment.fill(ItemId{-1});
            inherited.practice_item = ItemId{-1};
            inherited.item_experience = 0;
            destination = std::move(inherited);
        }
        destination.make_item_experience = 0;
    }
    auto& inventory = candidate->ranger.header;
    for (std::size_t slot = 0U; slot < kInventoryCount; ++slot) {
        inventory.set_inventory(slot, ItemId{-1}, 0);
    }
    std::size_t destination_slot{};
    for (std::size_t slot = 0U; slot < kInventoryCount; ++slot) {
        const auto item = completed.ranger.header.inventory_item(slot);
        if (item.value < 0) {
            break;
        }
        const auto type = candidate->ranger.items[static_cast<std::size_t>(item.value)].word(item_word::item_type);
        if (type == 3 || type == 4 || item.value == 174) {
            inventory.set_inventory(destination_slot++, item, completed.ranger.header.inventory_count(slot));
        }
    }
    if (!candidate->valid_for_persistence()) {
        return {{}, "Inherited playthrough violates the game state rules"};
    }
    return {std::move(candidate), {}};
}

}
