#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "openlegend/input/key_repeat.hpp"
#include "openlegend/input/legacy_keyboard.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/time/legacy_clock.hpp"
#include "test_support.hpp"

namespace {

class SequenceTickSource final : public openlegend::timing::TickSource {
public:
    explicit SequenceTickSource(std::vector<std::uint32_t> values) : values_(std::move(values)) {}

    [[nodiscard]] std::uint32_t tick() const noexcept override {
        ++tick_count_;
        return values_[position_];
    }

    void idle() noexcept override {
        ++idle_count_;
        if (position_ + 1U < values_.size()) {
            ++position_;
        }
    }

    [[nodiscard]] std::size_t tick_count() const noexcept { return tick_count_; }
    [[nodiscard]] std::size_t idle_count() const noexcept { return idle_count_; }

private:
    std::vector<std::uint32_t> values_;
    mutable std::size_t position_{};
    mutable std::size_t tick_count_{};
    std::size_t idle_count_{};
};

class ReadSequenceTickSource final : public openlegend::timing::TickSource {
public:
    explicit ReadSequenceTickSource(std::vector<std::uint32_t> values)
        : values_(std::move(values)) {}

    [[nodiscard]] std::uint32_t tick() const noexcept override {
        const auto index = position_ < values_.size() ? position_ : values_.size() - 1U;
        ++position_;
        return values_[index];
    }

    void idle() noexcept override { ++idle_count_; }

    [[nodiscard]] std::size_t tick_count() const noexcept { return position_; }
    [[nodiscard]] std::size_t idle_count() const noexcept { return idle_count_; }

private:
    std::vector<std::uint32_t> values_;
    mutable std::size_t position_{};
    std::size_t idle_count_{};
};

void run_keyboard_tests() {
    using openlegend::compat::HostKey;
    using openlegend::input::LegacyKeyboard;

    constexpr std::array<std::uint8_t, openlegend::input::kLegacyTranslationSize> expected{
        0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
        0x2D, 0x3D, 0x08, 0x09, 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49,
        0x4F, 0x50, 0x5B, 0x5D, 0x0D, 0x82, 0x41, 0x53, 0x44, 0x46, 0x47, 0x48,
        0x4A, 0x4B, 0x4C, 0x3B, 0x27, 0x60, 0x83, 0x5C, 0x5A, 0x58, 0x43, 0x56,
        0x42, 0x4E, 0x4D, 0x2C, 0x2E, 0x2F, 0x84, 0x2A, 0x85, 0x20, 0x86, 0xC9,
        0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0x87, 0x88, 0x9D,
        0x9E, 0x9F, 0x2D, 0x9A, 0x9B, 0x9C, 0x2B, 0x97, 0x98, 0x99, 0x96, 0x89,
    };
    OL_CHECK(LegacyKeyboard::translation_table() == expected);

    LegacyKeyboard keyboard;
    keyboard.handle_host_key(HostKey::q, true);
    OL_CHECK(keyboard.last_raw_scan_code() == 0x10U);
    OL_CHECK(keyboard.last_key() == 0x51U);
    OL_CHECK(keyboard.state(0x51U) == 3U);
    OL_CHECK(keyboard.down(0x51U));
    OL_CHECK(keyboard.edge(0x51U));

    keyboard.handle_host_key(HostKey::q, true);
    OL_CHECK(keyboard.state(0x51U) == 5U);
    OL_CHECK(keyboard.last_key() == 0x51U);
    keyboard.consume_edge(0x51U);
    OL_CHECK(keyboard.state(0x51U) == 4U);
    OL_CHECK(!keyboard.edge(0x51U));

    keyboard.handle_host_key(HostKey::a, true);
    OL_CHECK(keyboard.state(0x41U) == 3U);
    OL_CHECK(keyboard.last_key() == 0x41U);
    keyboard.handle_host_key(HostKey::q, false);
    OL_CHECK(keyboard.state(0x51U) == 0U);
    OL_CHECK(keyboard.state(0x41U) == 3U);
    OL_CHECK(keyboard.last_key() == 0U);

    LegacyKeyboard odd_saturation;
    odd_saturation.handle_host_key(HostKey::q, true);
    for (int index = 0; index < 127; ++index) {
        odd_saturation.handle_host_key(HostKey::q, true);
    }
    OL_CHECK(odd_saturation.state(0x51U) == 255U);
    odd_saturation.handle_host_key(HostKey::q, true);
    OL_CHECK(odd_saturation.state(0x51U) == 255U);

    LegacyKeyboard even_saturation;
    even_saturation.handle_host_key(HostKey::q, true);
    even_saturation.consume_edge(0x51U);
    for (int index = 0; index < 126; ++index) {
        even_saturation.handle_host_key(HostKey::q, true);
    }
    OL_CHECK(even_saturation.state(0x51U) == 254U);
    even_saturation.handle_host_key(HostKey::q, true);
    OL_CHECK(even_saturation.state(0x51U) == 254U);

    LegacyKeyboard aliased;
    aliased.handle_scan_code(0x54U);
    OL_CHECK(aliased.last_raw_scan_code() == 0x54U);
    OL_CHECK(aliased.last_key() == 0x54U);
    OL_CHECK(aliased.state(0x54U) == 3U);
    aliased.handle_scan_code(0xD4U);
    OL_CHECK(aliased.state(0x54U) == 3U);
    OL_CHECK(aliased.state(0xD4U) == 0U);
    OL_CHECK(aliased.last_key() == 0U);

    LegacyKeyboard extended;
    extended.handle_host_key(HostKey::f11, true);
    OL_CHECK(extended.state(0x00U) == 3U);
    extended.handle_host_key(HostKey::up, true);
    OL_CHECK(extended.state(0x00U) == 0U);
    OL_CHECK(extended.state(0x9EU) == 3U);
    OL_CHECK(extended.last_raw_scan_code() == 0x48U);
    OL_CHECK(extended.last_key() == 0x9EU);
    extended.handle_host_key(HostKey::up, false);
    OL_CHECK(extended.state(0x00U) == 0U);
    OL_CHECK(extended.state(0x9EU) == 0U);
    OL_CHECK(extended.last_raw_scan_code() == 0xC8U);
    OL_CHECK(extended.last_key() == 0U);

    LegacyKeyboard arrows;
    constexpr std::array<std::pair<HostKey, std::uint8_t>, 4> arrow_states{
        std::pair{HostKey::left, openlegend::input::legacy_key::left},
        std::pair{HostKey::up, openlegend::input::legacy_key::up},
        std::pair{HostKey::down, openlegend::input::legacy_key::down},
        std::pair{HostKey::right, openlegend::input::legacy_key::right}};
    constexpr std::array<std::uint8_t, 4> expected_arrow_states{0x9AU, 0x9EU, 0x98U, 0x9CU};
    for (std::size_t index = 0U; index < arrow_states.size(); ++index) {
        OL_CHECK(arrow_states[index].second == expected_arrow_states[index]);
        arrows.handle_host_key(arrow_states[index].first, true);
        OL_CHECK(arrows.down(arrow_states[index].second));
        arrows.handle_host_key(arrow_states[index].first, false);
        OL_CHECK(!arrows.down(arrow_states[index].second));
    }
    OL_CHECK(!arrows.down(0x9BU));
    OL_CHECK(!arrows.down(0x9DU));

    LegacyKeyboard consumed_repeat;
    consumed_repeat.handle_host_key(HostKey::down, true);
    OL_CHECK(consumed_repeat.last_key() == openlegend::input::legacy_key::down);
    consumed_repeat.clear_state(openlegend::input::legacy_key::down);
    consumed_repeat.clear_last_key();
    consumed_repeat.handle_host_key(HostKey::down, true);
    OL_CHECK(consumed_repeat.last_key() == openlegend::input::legacy_key::down);
    OL_CHECK(consumed_repeat.state(openlegend::input::legacy_key::down) == 3U);

    LegacyKeyboard confirmation_states;
    confirmation_states.handle_host_key(HostKey::enter, true);
    confirmation_states.handle_host_key(HostKey::space, true);
    confirmation_states.handle_host_key(HostKey::insert, true);
    for (const auto translated_key : openlegend::input::legacy_key::confirmation) {
        OL_CHECK(confirmation_states.down(translated_key));
    }
    confirmation_states.clear_confirmation_states();
    for (const auto translated_key : openlegend::input::legacy_key::confirmation) {
        OL_CHECK(!confirmation_states.down(translated_key));
    }

    OL_CHECK(openlegend::input::legacy_key::world_left ==
             (std::array<std::uint8_t, 2>{0x9AU, 0x9DU}));
    OL_CHECK(openlegend::input::legacy_key::world_up ==
             (std::array<std::uint8_t, 2>{0x9EU, 0x9FU}));
    OL_CHECK(openlegend::input::legacy_key::world_down ==
             (std::array<std::uint8_t, 2>{0x97U, 0x98U}));
    OL_CHECK(openlegend::input::legacy_key::world_right ==
             (std::array<std::uint8_t, 2>{0x99U, 0x9CU}));

    LegacyKeyboard keypad_directions;
    constexpr std::array<std::pair<HostKey, std::uint8_t>, 4> keypad_states{
        std::pair{HostKey::home, 0x9DU},
        std::pair{HostKey::page_up, 0x9FU},
        std::pair{HostKey::end, 0x97U},
        std::pair{HostKey::page_down, 0x99U}};
    for (const auto& [key, translated] : keypad_states) {
        keypad_directions.handle_host_key(key, true);
        OL_CHECK(keypad_directions.down(translated));
        keypad_directions.clear_state(translated);
        OL_CHECK(!keypad_directions.down(translated));
        keypad_directions.handle_host_key(key, true);
        OL_CHECK(keypad_directions.state(translated) == 3U);
        keypad_directions.handle_host_key(key, false);
        OL_CHECK(!keypad_directions.down(translated));
    }

    using openlegend::input::LegacyWorldDirectionInput;
    LegacyKeyboard world_priority;
    world_priority.handle_host_key(HostKey::home, true);
    world_priority.handle_host_key(HostKey::up, true);
    OL_CHECK(world_priority.world_direction() == LegacyWorldDirectionInput::left);
    world_priority.consume_world_direction(LegacyWorldDirectionInput::left);
    OL_CHECK(!world_priority.down(0x9AU));
    OL_CHECK(!world_priority.down(0x9DU));
    OL_CHECK(world_priority.down(0x9EU));
    OL_CHECK(world_priority.world_direction() == LegacyWorldDirectionInput::up);
    world_priority.consume_world_direction(LegacyWorldDirectionInput::up);
    OL_CHECK(world_priority.world_direction() == LegacyWorldDirectionInput::none);
    world_priority.handle_host_key(HostKey::home, true);
    OL_CHECK(world_priority.state(0x9DU) == 3U);
    OL_CHECK(world_priority.world_direction() == LegacyWorldDirectionInput::left);

    LegacyKeyboard all_directions;
    constexpr std::array<HostKey, 8> all_direction_keys{
        HostKey::left,
        HostKey::home,
        HostKey::up,
        HostKey::page_up,
        HostKey::end,
        HostKey::down,
        HostKey::page_down,
        HostKey::right};
    for (const auto key : all_direction_keys) {
        all_directions.handle_host_key(key, true);
    }
    for (const auto translated_keys : {
             openlegend::input::legacy_key::world_left,
             openlegend::input::legacy_key::world_up,
             openlegend::input::legacy_key::world_down,
             openlegend::input::legacy_key::world_right}) {
        OL_CHECK(all_directions.down(translated_keys[0]));
        OL_CHECK(all_directions.down(translated_keys[1]));
    }
    OL_CHECK(all_directions.last_key() != 0U);
    all_directions.clear_scene_exit_key_states();
    OL_CHECK(all_directions.last_key() == 0U);
    OL_CHECK(all_directions.world_direction() == LegacyWorldDirectionInput::none);
    for (const auto translated_keys : {
             openlegend::input::legacy_key::world_left,
             openlegend::input::legacy_key::world_up,
             openlegend::input::legacy_key::world_down,
             openlegend::input::legacy_key::world_right}) {
        OL_CHECK(!all_directions.down(translated_keys[0]));
        OL_CHECK(!all_directions.down(translated_keys[1]));
    }

    LegacyKeyboard print_screen;
    print_screen.handle_host_key(HostKey::print_screen, true);
    OL_CHECK(print_screen.state(0x00U) == 0U);
    OL_CHECK(print_screen.state(0x83U) == 3U);
    OL_CHECK(print_screen.state(0x2AU) == 3U);
    OL_CHECK(print_screen.last_key() == 0x2AU);
    print_screen.handle_host_key(HostKey::print_screen, false);
    OL_CHECK(print_screen.state(0x00U) == 0U);
    OL_CHECK(print_screen.state(0x83U) == 0U);
    OL_CHECK(print_screen.state(0x2AU) == 0U);
    OL_CHECK(print_screen.last_key() == 0U);

    LegacyKeyboard pause;
    pause.handle_host_key(HostKey::pause, true);
    OL_CHECK(pause.state(0x00U) == 0U);
    OL_CHECK(pause.state(0x82U) == 0U);
    OL_CHECK(pause.state(0x87U) == 0U);
    OL_CHECK(pause.last_raw_scan_code() == 0xC5U);
    OL_CHECK(pause.last_key() == 0U);
}

void run_key_repeat_tests() {
    using openlegend::compat::HostKey;
    using openlegend::input::DirectionRepeatContext;
    using openlegend::input::KeyRepeatController;
    using namespace std::chrono_literals;

    const KeyRepeatController::TimePoint start{};

    KeyRepeatController movement{100ms, 80ms, 40ms};
    movement.set_context(DirectionRepeatContext::movement, start);
    movement.begin_frame();
    OL_CHECK(movement.handle_key_down(HostKey::left, false, start));
    OL_CHECK(!movement.handle_key_down(HostKey::left, true, start + 50ms));
    OL_CHECK(movement.take_movement_repeat(start + 100ms) == std::nullopt);
    movement.begin_frame();
    OL_CHECK(movement.take_movement_repeat(start + 99ms) == std::nullopt);
    OL_CHECK(movement.take_movement_repeat(start + 100ms) == HostKey::left);

    movement.begin_frame();
    OL_CHECK(movement.handle_key_down(HostKey::up, false, start + 110ms));
    OL_CHECK(movement.take_movement_repeat(start + 110ms) == std::nullopt);
    movement.begin_frame();
    OL_CHECK(movement.take_movement_repeat(start + 111ms) == HostKey::up);

    KeyRepeatController early_turn{100ms, 80ms, 40ms};
    early_turn.set_context(DirectionRepeatContext::movement, start);
    early_turn.begin_frame();
    OL_CHECK(early_turn.handle_key_down(HostKey::up, false, start));
    early_turn.begin_frame();
    OL_CHECK(early_turn.handle_key_down(HostKey::left, false, start + 50ms));
    early_turn.begin_frame();
    OL_CHECK(early_turn.take_movement_repeat(start + 99ms) == std::nullopt);
    OL_CHECK(early_turn.take_movement_repeat(start + 100ms) == HostKey::left);

    KeyRepeatController chord{100ms, 80ms, 40ms};
    chord.set_context(DirectionRepeatContext::movement, start);
    chord.begin_frame();
    OL_CHECK(chord.handle_key_down(HostKey::up, false, start));
    chord.begin_frame();
    OL_CHECK(chord.handle_key_down(HostKey::left, false, start + 10ms));
    chord.begin_frame();
    chord.handle_key_up(HostKey::left, start + 20ms);
    OL_CHECK(chord.take_movement_repeat(start + 20ms) == HostKey::up);
    chord.handle_key_up(HostKey::up, start + 21ms);
    chord.begin_frame();
    OL_CHECK(chord.take_movement_repeat(start + 200ms) == std::nullopt);

    KeyRepeatController interrupted{100ms, 80ms, 40ms};
    interrupted.set_context(DirectionRepeatContext::movement, start);
    interrupted.begin_frame();
    OL_CHECK(interrupted.handle_key_down(HostKey::left, false, start));
    interrupted.set_context(DirectionRepeatContext::none, start + 150ms);
    OL_CHECK(!interrupted.handle_key_down(HostKey::left, true, start + 175ms));
    interrupted.set_context(DirectionRepeatContext::movement, start + 200ms);
    interrupted.begin_frame();
    OL_CHECK(interrupted.take_movement_repeat(start + 249ms) == std::nullopt);
    OL_CHECK(interrupted.take_movement_repeat(start + 250ms) == HostKey::left);

    KeyRepeatController cross_context{100ms, 80ms, 40ms};
    cross_context.set_context(DirectionRepeatContext::movement, start);
    cross_context.begin_frame();
    OL_CHECK(cross_context.handle_key_down(HostKey::left, false, start));
    cross_context.set_context(DirectionRepeatContext::menu, start + 10ms);
    OL_CHECK(!cross_context.handle_key_down(HostKey::left, true, start + 20ms));
    OL_CHECK(!cross_context.handle_key_down(HostKey::left, false, start + 20ms));
    cross_context.begin_frame();
    OL_CHECK(cross_context.take_menu_repeat(start + 200ms) == std::nullopt);
    cross_context.set_context(DirectionRepeatContext::movement, start + 210ms);
    cross_context.begin_frame();
    OL_CHECK(cross_context.take_movement_repeat(start + 500ms) == std::nullopt);
    cross_context.handle_key_up(HostKey::left, start + 510ms);
    cross_context.begin_frame();
    OL_CHECK(cross_context.handle_key_down(HostKey::left, false, start + 520ms));

    KeyRepeatController menu{100ms, 80ms, 40ms};
    menu.set_context(DirectionRepeatContext::menu, start);
    menu.begin_frame();
    OL_CHECK(menu.handle_key_down(HostKey::down, false, start));
    OL_CHECK(!menu.handle_key_down(HostKey::down, true, start + 50ms));
    OL_CHECK(menu.take_menu_repeat(start + 100ms) == std::nullopt);
    OL_CHECK(menu.time_until_menu_repeat(start + 20ms) == 60ms);
    menu.begin_frame();
    OL_CHECK(menu.take_menu_repeat(start + 79ms) == std::nullopt);
    OL_CHECK(menu.take_menu_repeat(start + 80ms) == HostKey::down);
    OL_CHECK(menu.time_until_menu_repeat(start + 80ms) == 40ms);
    menu.begin_frame();
    OL_CHECK(menu.take_menu_repeat(start + 119ms) == std::nullopt);
    OL_CHECK(menu.take_menu_repeat(start + 120ms) == HostKey::down);

    menu.begin_frame();
    OL_CHECK(menu.handle_key_down(HostKey::up, false, start + 130ms));
    menu.begin_frame();
    OL_CHECK(menu.take_menu_repeat(start + 209ms) == std::nullopt);
    OL_CHECK(menu.take_menu_repeat(start + 210ms) == HostKey::up);
    menu.begin_frame();
    menu.handle_key_up(HostKey::up, start + 220ms);
    OL_CHECK(menu.take_menu_repeat(start + 299ms) == std::nullopt);
    OL_CHECK(menu.take_menu_repeat(start + 300ms) == HostKey::down);

    KeyRepeatController menu_subcontexts{100ms, 80ms, 40ms};
    menu_subcontexts.set_context(DirectionRepeatContext::menu, start);
    menu_subcontexts.begin_frame();
    OL_CHECK(menu_subcontexts.handle_key_down(HostKey::down, false, start));
    menu_subcontexts.set_context(DirectionRepeatContext::save_list, start + 10ms);
    menu_subcontexts.begin_frame();
    OL_CHECK(menu_subcontexts.take_menu_repeat(start + 79ms) == std::nullopt);
    OL_CHECK(menu_subcontexts.take_menu_repeat(start + 80ms) == HostKey::down);
    menu_subcontexts.set_context(DirectionRepeatContext::menu, start + 90ms);
    menu_subcontexts.begin_frame();
    OL_CHECK(menu_subcontexts.take_menu_repeat(start + 119ms) == std::nullopt);
    OL_CHECK(menu_subcontexts.take_menu_repeat(start + 120ms) == HostKey::down);

    KeyRepeatController page_navigation{100ms, 80ms, 40ms};
    page_navigation.set_context(DirectionRepeatContext::save_list, start);
    page_navigation.begin_frame();
    OL_CHECK(page_navigation.handle_key_down(HostKey::page_down, false, start));
    OL_CHECK(!page_navigation.handle_key_down(
        HostKey::page_down, true, start + 50ms));
    page_navigation.begin_frame();
    OL_CHECK(
        page_navigation.take_menu_repeat(start + 80ms) == HostKey::page_down);
    page_navigation.handle_key_up(HostKey::page_down, start + 90ms);
    page_navigation.begin_frame();
    OL_CHECK(page_navigation.handle_key_down(
        HostKey::keypad_9, false, start + 100ms));
    OL_CHECK(page_navigation.handle_key_down(
        HostKey::home, false, start + 100ms));
    OL_CHECK(!page_navigation.handle_key_down(
        HostKey::home, true, start + 110ms));
    OL_CHECK(page_navigation.handle_key_down(
        HostKey::keypad_7, false, start + 120ms));
    OL_CHECK(!page_navigation.handle_key_down(
        HostKey::keypad_7, true, start + 130ms));

    KeyRepeatController battle_cursor{100ms, 80ms, 40ms};
    battle_cursor.set_context(DirectionRepeatContext::battle_cursor, start);
    battle_cursor.begin_frame();
    OL_CHECK(battle_cursor.handle_key_down(HostKey::right, false, start));
    battle_cursor.begin_frame();
    OL_CHECK(battle_cursor.take_movement_repeat(start + 100ms) == HostKey::right);
    battle_cursor.begin_frame();
    OL_CHECK(battle_cursor.handle_key_down(
        HostKey::up, false, start + 110ms));
    battle_cursor.begin_frame();
    OL_CHECK(battle_cursor.take_movement_repeat(start + 111ms) == HostKey::right);
    battle_cursor.handle_key_up(HostKey::up, start + 120ms);
    battle_cursor.begin_frame();
    OL_CHECK(battle_cursor.take_movement_repeat(start + 121ms) == HostKey::right);
    OL_CHECK(battle_cursor.handle_key_down(
        HostKey::up, false, start + 130ms));
    battle_cursor.handle_key_up(HostKey::right, start + 140ms);
    battle_cursor.begin_frame();
    OL_CHECK(battle_cursor.take_movement_repeat(start + 140ms) == HostKey::up);

    KeyRepeatController movement_keypad{100ms, 80ms, 40ms};
    movement_keypad.set_context(DirectionRepeatContext::movement, start);
    movement_keypad.begin_frame();
    OL_CHECK(movement_keypad.handle_key_down(HostKey::keypad_1, false, start));
    OL_CHECK(!movement_keypad.handle_key_down(
        HostKey::keypad_1, true, start + 50ms));
}

void run_timing_tests() {
    using namespace openlegend::timing;

    constexpr std::array caller_counts{
        std::pair{1, 1},
        std::pair{17, 1},
        std::pair{30, 1},
        std::pair{40, 2},
        std::pair{50, 2},
        std::pair{100, 3},
        std::pair{300, 8},
        std::pair{340, 9},
        std::pair{500, 13},
        std::pair{2000, 51},
    };
    for (const auto [argument, expected] : caller_counts) {
        OL_CHECK(legacy_delay_tick_count(argument) == expected);
    }

    OL_CHECK(legacy_delay_tick_count(std::numeric_limits<std::int32_t>::min()) ==
             -53'687'090);
    OL_CHECK(legacy_delay_tick_count(-80) == -1);
    OL_CHECK(legacy_delay_tick_count(-79) == 0);
    OL_CHECK(legacy_delay_tick_count(-40) == 0);
    OL_CHECK(legacy_delay_tick_count(-39) == 1);
    OL_CHECK(legacy_delay_tick_count(-1) == 1);
    OL_CHECK(legacy_delay_tick_count(0) == 1);
    OL_CHECK(legacy_delay_tick_count(39) == 1);
    OL_CHECK(legacy_delay_tick_count(40) == 2);
    OL_CHECK(legacy_delay_tick_count(79) == 2);
    OL_CHECK(legacy_delay_tick_count(80) == 3);
    OL_CHECK(legacy_delay_tick_count(std::numeric_limits<std::int32_t>::max()) ==
             53'687'092);

    SequenceTickSource rollover{{kBiosTicksPerDay - 1U, kBiosTicksPerDay - 1U, 0U}};
    OL_CHECK(wait_for_tick_change(rollover, kBiosTicksPerDay - 1U) == 0U);
    OL_CHECK(rollover.tick_count() == 3U);
    OL_CHECK(rollover.idle_count() == 2U);

    SequenceTickSource delay{{100U, 101U, 102U, 103U}};
    legacy_delay(delay, 80);
    OL_CHECK(delay.tick_count() == 9U);
    OL_CHECK(delay.idle_count() == 3U);

    ReadSequenceTickSource exact_trace{{
        100U, 100U, 100U, 101U,
        101U, 101U, 102U,
        kBiosTicksPerDay - 1U, kBiosTicksPerDay - 1U, 0U,
    }};
    legacy_delay(exact_trace, 80);
    OL_CHECK(exact_trace.tick_count() == 10U);
    OL_CHECK(exact_trace.idle_count() == 4U);

    SequenceTickSource no_delay{{100U}};
    legacy_delay(no_delay, -40);
    OL_CHECK(no_delay.tick_count() == 0U);
    OL_CHECK(no_delay.idle_count() == 0U);

    SteadyBiosTickSource steady;
    OL_CHECK(steady.time_until_next_tick() > std::chrono::nanoseconds::zero());
    OL_CHECK(
        steady.time_until_next_tick() <= std::chrono::milliseconds{60});
    const auto captured_tick = steady.tick();
    steady.idle();
    OL_CHECK(steady.tick() != captured_tick);
}

void check_random_vector(
    const std::uint32_t seed,
    const std::array<std::uint16_t, 10>& expected,
    const std::uint32_t final_state) {
    openlegend::random::LegacyRandom random{seed};
    for (const auto value : expected) {
        OL_CHECK(random.next() == value);
    }
    OL_CHECK(random.state() == final_state);
}

void run_random_tests() {
    using openlegend::random::LegacyRandom;
    OL_CHECK(LegacyRandom::dos_time_seed(59U, 99U) == 5999U);

    LegacyRandom bounded{1U};
    OL_CHECK(bounded.bounded(-1) == 0);
    OL_CHECK(bounded.bounded(1) == 0);
    OL_CHECK(bounded.bounded(30'001) == 0);
    OL_CHECK(bounded.state() == 1U);
    OL_CHECK(bounded.bounded(2) == 0);
    OL_CHECK(bounded.state() == 1103527590U);
    OL_CHECK(bounded.bounded(30'000) == 5758);
    OL_CHECK(bounded.state() == 2524885223U);

    check_random_vector(
        0U,
        {0U, 21468U, 9988U, 22117U, 3498U, 16927U, 16045U, 19741U, 12122U, 8410U},
        551188310U);
    check_random_vector(
        1U,
        {16838U, 5758U, 10113U, 17515U, 31051U, 5627U, 23010U, 7419U, 16212U, 4086U},
        267834847U);
    check_random_vector(
        5999U,
        {22023U, 21564U, 10621U, 8352U, 13846U, 27280U, 21825U, 23901U, 5153U, 23205U},
        1520812989U);
    check_random_vector(
        0xFFFFFFFFU,
        {15929U, 4409U, 9862U, 26718U, 8713U, 28226U, 9080U, 32063U, 8032U, 12734U},
        834541773U);
}

}  // namespace

void run_legacy_runtime_tests() {
    run_keyboard_tests();
    run_key_repeat_tests();
    run_timing_tests();
    run_random_tests();
}
