#pragma once

#include <chrono>
#include <filesystem>

#include "openlegend/attributes.hpp"
#include "openlegend/app/runtime_configuration.hpp"
#include "openlegend/input/name_input_method.hpp"

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
};

struct LegacyRuntimeLoopResult {
    int status{};
    bool ending_completed{};
};

NODISCARD LegacyRuntimeLoopResult run_legacy_runtime_loop(
    SdlRuntimePlatform& platform,
    const LegacyRuntimeLoopSettings& settings);

}  // namespace openlegend::platform::sdl3
