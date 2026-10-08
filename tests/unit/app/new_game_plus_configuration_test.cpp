#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

#include "openlegend/attributes.hpp"
#include "openlegend/app/runtime_configuration.hpp"
#include "openlegend/model/new_game_plus_configuration.hpp"
#include "test_support.hpp"

namespace {

class ConfigurationFixture {
public:
    ConfigurationFixture() {
        static std::atomic_uint64_t sequence{};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        directory_ = std::filesystem::temp_directory_path() /
            ("openlegend-ngplus-configuration-" + std::to_string(stamp) + "-" +
                std::to_string(sequence.fetch_add(1U)));
        std::filesystem::create_directories(directory_);
    }

    ~ConfigurationFixture() {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
    }

    ConfigurationFixture(const ConfigurationFixture&) = delete;

    ConfigurationFixture& operator=(const ConfigurationFixture&) = delete;

    NODISCARD std::filesystem::path path() const {
        return directory_ / "openlegend.toml";
    }

    NODISCARD const std::filesystem::path& directory() const noexcept {
        return directory_;
    }

    void write(const std::string_view value) const {
        std::ofstream output{path(), std::ios::binary | std::ios::trunc};
        output.write(value.data(), static_cast<std::streamsize>(value.size()));
    }

private:
    std::filesystem::path directory_;
};

void check_default_and_custom_limits() {
    using namespace openlegend::model;

    NewGamePlusConfiguration configuration;
    OL_CHECK(calculate_playthrough_limits(configuration, 1) == PlaythroughLimits(99, 100, 100));
    OL_CHECK(calculate_playthrough_limits(configuration, 2) == PlaythroughLimits(199, 200, 150));
    OL_CHECK(calculate_playthrough_limits(configuration, 3) == PlaythroughLimits(299, 300, 250));
    OL_CHECK(calculate_playthrough_limits(configuration, 5) == PlaythroughLimits(499, 500, 450));
    OL_CHECK(calculate_playthrough_limits(configuration, 999) == PlaythroughLimits(99'899, 99'900, 99'850));
    OL_CHECK(!calculate_playthrough_limits(configuration, 0).has_value());
    OL_CHECK(!calculate_playthrough_limits(configuration, 1000).has_value());
    configuration.hurt_cap_step = 7;
    configuration.battle_experience_percent_ng2 = 123;
    configuration.battle_experience_percent_step = 456;
    OL_CHECK(calculate_playthrough_limits(configuration, 2) == PlaythroughLimits(106, 110, 123));
    OL_CHECK(calculate_playthrough_limits(configuration, 3) == PlaythroughLimits(113, 120, 579));
    configuration.maximum_playthroughs = 2;
    configuration.hurt_cap_step = -99;
    OL_CHECK(calculate_playthrough_limits(configuration, 2) == PlaythroughLimits(0, 0, 123));
    OL_CHECK(!calculate_playthrough_limits(configuration, 3).has_value());
}

void check_overflow_and_negative_limits() {
    using namespace openlegend::model;

    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    NewGamePlusConfiguration configuration;
    configuration.hurt_cap_step = maximum;
    OL_CHECK(!calculate_playthrough_limits(configuration, 2).has_value());
    configuration.hurt_cap_step = maximum - 99;
    OL_CHECK(!calculate_playthrough_limits(configuration, 2).has_value());
    configuration.hurt_cap_step = maximum - 106;
    OL_CHECK(calculate_playthrough_limits(configuration, 2) ==
        PlaythroughLimits(maximum - 7, maximum - 7, 150));
    OL_CHECK(!calculate_playthrough_limits(configuration, 3).has_value());
    configuration.hurt_cap_step = -100;
    OL_CHECK(!calculate_playthrough_limits(configuration, 2).has_value());
    configuration.hurt_cap_step = 100;
    configuration.battle_experience_percent_step = maximum;
    OL_CHECK(!calculate_playthrough_limits(configuration, 3).has_value());
    configuration.battle_experience_percent_step = 100;
    configuration.battle_experience_percent_ng2 = -1;
    OL_CHECK(!calculate_playthrough_limits(configuration, 2).has_value());
}

void check_toml_loading_and_configuration_preservation() {
    using namespace openlegend::app;

    ConfigurationFixture fixture;
    const auto absent = load_new_game_plus_configuration(fixture.path());
    OL_CHECK(absent.status == NewGamePlusConfigurationStatus::ready);
    OL_CHECK(!absent.loaded_from_file);
    OL_CHECK(!absent.values.enabled);
    OL_CHECK(absent.values.maximum_playthroughs == 999);
    fixture.write(
        "[new_game_plus]\n"
        "enabled = true\n"
        "maximum_playthroughs = 4\n"
        "hurt_cap_step = 7\n"
        "hp_cap_step = 1001\n"
        "mp_cap_step = 1002\n"
        "attack_cap_step = 101\n"
        "defence_cap_step = 102\n"
        "use_poison_cap_step = 103\n"
        "anti_poison_cap_step = 104\n"
        "hidden_weapon_cap_step = 105\n"
        "role_level_step = 65536\n"
        "martial_level_step = 10001\n"
        "battle_experience_percent_ng2 = 123\n"
        "battle_experience_percent_step = 456\n");
    const auto loaded = load_new_game_plus_configuration(fixture.path());
    OL_CHECK(loaded.status == NewGamePlusConfigurationStatus::ready);
    OL_CHECK(loaded.loaded_from_file);
    OL_CHECK(loaded.values.enabled);
    OL_CHECK(loaded.values.maximum_playthroughs == 4);
    OL_CHECK(loaded.values.hurt_cap_step == 7);
    OL_CHECK(loaded.values.hp_cap_step == 1001);
    OL_CHECK(loaded.values.mp_cap_step == 1002);
    OL_CHECK(loaded.values.attack_cap_step == 101);
    OL_CHECK(loaded.values.defence_cap_step == 102);
    OL_CHECK(loaded.values.use_poison_cap_step == 103);
    OL_CHECK(loaded.values.anti_poison_cap_step == 104);
    OL_CHECK(loaded.values.hidden_weapon_cap_step == 105);
    OL_CHECK(loaded.values.role_level_step == 65536);
    OL_CHECK(loaded.values.martial_level_step == 10001);
    OL_CHECK(loaded.values.battle_experience_percent_ng2 == 123);
    OL_CHECK(loaded.values.battle_experience_percent_step == 456);
    const auto runtime = load_runtime_configuration(
        {}, fixture.path(), fixture.directory(), fixture.directory(), {});
    OL_CHECK(runtime.new_game_plus.status == NewGamePlusConfigurationStatus::ready);
    OL_CHECK(runtime.new_game_plus.values == loaded.values);
    std::string detail;
    OL_CHECK(save_window_configuration(fixture.path(), {960, 600}, false, detail) ==
        WindowConfigurationStatus::ready);
    const auto saved = load_new_game_plus_configuration(fixture.path());
    OL_CHECK(saved.status == NewGamePlusConfigurationStatus::ready);
    OL_CHECK(saved.values == loaded.values);
}

void check_invalid_toml_and_values() {
    using namespace openlegend::app;

    ConfigurationFixture fixture;
    fixture.write("[new_game_plus]\nenabled = true\nenabled = false\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::parse_failed);
    const auto runtime = load_runtime_configuration(
        {}, fixture.path(), fixture.directory(), fixture.directory(), {});
    OL_CHECK(runtime.new_game_plus.status == NewGamePlusConfigurationStatus::parse_failed);
    fixture.write("new_game_plus = true\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::invalid_table);
    fixture.write("[new_game_plus]\nenabled = 1\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::invalid_value);
    fixture.write("[new_game_plus]\nmaximum_playthroughs = 3.0\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::invalid_value);
    fixture.write("[new_game_plus]\nmaximum_playthroughs = 0\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::invalid_value);
    fixture.write("[new_game_plus]\nhurt_cap_step = 9223372036854775807\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::invalid_limits);
    fixture.write("[new_game_plus]\nmaximum_playthroughs = 2\nhurt_cap_step = -100\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::invalid_limits);
    fixture.write("[new_game_plus]\nbattle_experience_percent_ng2 = -1\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::invalid_limits);
    fixture.write("[new_game_plus]\npoison_cap_step = 100\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::unknown_key);
    fixture.write("[new_game_plus]\nhurt_cap_step = 9223372036854775808\n");
    OL_CHECK(load_new_game_plus_configuration(fixture.path()).status ==
        NewGamePlusConfigurationStatus::parse_failed);
}

}

int main() {
    check_default_and_custom_limits();
    check_overflow_and_negative_limits();
    check_toml_loading_and_configuration_preservation();
    check_invalid_toml_and_values();
    return openlegend::test::failures == 0 ? 0 : 1;
}
