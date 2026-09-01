#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "openlegend/input/legacy_keyboard.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/time/legacy_clock.hpp"
#include "test_support.hpp"

namespace {

class SequenceTickSource final : public openlegend::timing::TickSource {
public:
    explicit SequenceTickSource(std::vector<std::uint32_t> values) : values_(std::move(values)) {}

    [[nodiscard]] std::uint32_t tick() const noexcept override {
        return values_[position_];
    }

    void idle() noexcept override {
        ++idle_count_;
        if (position_ + 1U < values_.size()) {
            ++position_;
        }
    }

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
        std::pair{HostKey::left, openlegend::input::kLegacyLeftKey},
        std::pair{HostKey::up, openlegend::input::kLegacyUpKey},
        std::pair{HostKey::down, openlegend::input::kLegacyDownKey},
        std::pair{HostKey::right, openlegend::input::kLegacyRightKey}};
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

    OL_CHECK(openlegend::input::kLegacyWorldLeftKeys ==
             (std::array<std::uint8_t, 2>{0x9AU, 0x9DU}));
    OL_CHECK(openlegend::input::kLegacyWorldUpKeys ==
             (std::array<std::uint8_t, 2>{0x9EU, 0x9FU}));
    OL_CHECK(openlegend::input::kLegacyWorldDownKeys ==
             (std::array<std::uint8_t, 2>{0x97U, 0x98U}));
    OL_CHECK(openlegend::input::kLegacyWorldRightKeys ==
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

void run_timing_tests() {
    using namespace openlegend::timing;

    OL_CHECK(legacy_delay_tick_count(0) == 1);
    OL_CHECK(legacy_delay_tick_count(39) == 1);
    OL_CHECK(legacy_delay_tick_count(40) == 2);
    OL_CHECK(legacy_delay_tick_count(79) == 2);
    OL_CHECK(legacy_delay_tick_count(-1) == 1);
    OL_CHECK(legacy_delay_tick_count(-39) == 1);
    OL_CHECK(legacy_delay_tick_count(-40) == 0);
    OL_CHECK(legacy_delay_tick_count(-79) == 0);
    OL_CHECK(legacy_delay_tick_count(-80) == -1);

    SequenceTickSource rollover{{kBiosTicksPerDay - 1U, kBiosTicksPerDay - 1U, 0U}};
    OL_CHECK(wait_for_tick_change(rollover, kBiosTicksPerDay - 1U) == 0U);
    OL_CHECK(rollover.idle_count() == 2U);

    SequenceTickSource delay{{100U, 101U, 102U, 103U}};
    legacy_delay(delay, 80);
    OL_CHECK(delay.idle_count() == 3U);

    SequenceTickSource no_delay{{100U}};
    legacy_delay(no_delay, -40);
    OL_CHECK(no_delay.idle_count() == 0U);
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
    run_timing_tests();
    run_random_tests();
}
