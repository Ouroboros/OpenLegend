#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/app/runtime_configuration.hpp"
#include "openlegend/input/name_input_method.hpp"
#include "openlegend/model/new_game_plus_configuration.hpp"

namespace openlegend::platform::sdl3 {

class SdlRuntimePlatform;

struct LegacyRuntimeLoopSettings {
    std::filesystem::path save_directory;
    input::NameInputMethod name_input_method{input::NameInputMethod::legacy};
    std::chrono::milliseconds movement_repeat_delay{};
    std::chrono::milliseconds menu_repeat_delay{};
    std::chrono::milliseconds menu_repeat_interval{};
    std::chrono::nanoseconds fade_frame_delay{};
    app::GameResolution game_resolution;
    bool smoke_test{};
    std::chrono::nanoseconds movement_step_duration{};
    std::chrono::nanoseconds motion_frame_interval{
        std::chrono::nanoseconds{1'000'000'000} / 240};
    model::NewGamePlusConfiguration new_game_plus_configuration{};
    std::optional<std::uint16_t> last_save_slot{};
    std::filesystem::path program_configuration_path{};
    std::string save_slot_mod_key{};
    std::vector<std::uint8_t> world_map_keys{static_cast<std::uint8_t>('M'), 0x09U};
    // [paths] mod；未配置时使用程序内置的原版设定。
    std::optional<std::filesystem::path> mod_configuration{};
};

struct LegacyRuntimeLoopResult {
    int status{};
    bool ending_completed{};
};

NODISCARD LegacyRuntimeLoopResult run_legacy_runtime_loop(
    SdlRuntimePlatform& platform,
    const LegacyRuntimeLoopSettings& settings);

}  // namespace openlegend::platform::sdl3
