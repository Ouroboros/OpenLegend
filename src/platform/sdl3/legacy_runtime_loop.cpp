#include "legacy_runtime_loop.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include <SDL3/SDL.h>

#include "frame_presenter.hpp"
#include "legacy_input_coordinator.hpp"
#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/audio/legacy_audio.hpp"
#include "openlegend/battle/battle_session.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/input/key_repeat.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/scene/scene.hpp"
#include "openlegend/time/legacy_clock.hpp"
#include "sdl_audio_device.hpp"
#include "sdl_runtime_platform.hpp"

namespace openlegend::platform::sdl3 {
namespace {

constexpr std::chrono::milliseconds kSaveListPageRepeatInterval{100};

void report_runtime_error(
    const std::string_view category,
    const std::string_view message,
    const std::string_view detail = {}) {
    std::string record{category};
    record += ": ";
    record += message;
    if (!detail.empty()) {
        record += ": ";
        record += detail;
    }
    std::cerr << record << '\n';
    if (diagnostics::logging_to_file()) {
        diagnostics::log_error(record);
    }
}

[[nodiscard]] std::uint32_t make_random_seed() {
    const auto wall_time = std::chrono::system_clock::now().time_since_epoch();
    const auto whole_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(wall_time);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        wall_time - whole_seconds);
    const auto second = static_cast<std::uint8_t>(whole_seconds.count() % 60);
    const auto hundredth = static_cast<std::uint8_t>(milliseconds.count() / 10);
    return random::LegacyRandom::dos_time_seed(second, hundredth);
}

void dispatch_audio_commands(
    app::LegacyGameRuntime& game,
    audio::LegacyAudioController& legacy_audio) {
    for (const auto& command : game.take_scene_audio_commands()) {
        if (command.id < 0) {
            continue;
        }
        if (command.kind == scene::SceneAudioCommand::Kind::music) {
            const auto music_id = static_cast<std::size_t>(command.id);
            if (command.force || legacy_audio.current_music() != music_id) {
                static_cast<void>(legacy_audio.play_music(music_id));
            }
        } else {
            static_cast<void>(legacy_audio.play_sample(
                audio::SampleBank::effect,
                static_cast<std::size_t>(command.id)));
        }
    }
    for (const auto& command : game.take_battle_audio_commands()) {
        if (command.sample_id < 0) {
            continue;
        }
        const auto bank = command.bank == battle::BattleAudioBank::attack
            ? audio::SampleBank::attack
            : audio::SampleBank::effect;
        const auto sample_id = static_cast<std::size_t>(command.sample_id);
        if (command.action == battle::BattleAudioAction::load) {
            static_cast<void>(legacy_audio.load_sample(bank, sample_id));
        } else if (
            command.action == battle::BattleAudioAction::start_loaded) {
            static_cast<void>(legacy_audio.start_loaded_sample(bank, sample_id));
        } else {
            static_cast<void>(legacy_audio.play_sample(bank, sample_id));
        }
    }
}

}  // namespace

LegacyRuntimeLoopResult run_legacy_runtime_loop(
    SdlRuntimePlatform& platform,
    const LegacyRuntimeLoopSettings& settings) {
    audio::AudioMixer audio_mixer;
    audio::SystemAudioDelay audio_delay;
    audio::LegacyAudioController legacy_audio{
        resource::DataRoot{std::filesystem::current_path()},
        audio_mixer,
        audio_delay};
    SdlAudioDevice audio_device{audio_mixer};
    if (!audio_mixer.valid()) {
        diagnostics::log_warning(
            "XMI synthesizer unavailable; audio disabled: " +
            audio_mixer.error());
    } else if (!audio_device.valid()) {
        diagnostics::log_warning(
            std::string{"SDL3 audio device unavailable; audio disabled: "} +
            SDL_GetError());
    } else {
        diagnostics::log_info("audio backend ready");
    }

    const auto random_seed = make_random_seed();
    bool fade_music_on_exit = false;
    LegacyRuntimeLoopResult result{};
    {
        app::LegacyGameRuntime game{
            std::filesystem::current_path(),
            settings.save_directory,
            random_seed};
        diagnostics::log_info(
            "runtime random_seed=" + std::to_string(random_seed));
        if (!game.valid()) {
            diagnostics::log_critical(
                "game runtime initialization failed: " + game.error());
            report_runtime_error("game runtime", game.error());
            result.status = 5;
            return result;
        }
        if (!legacy_audio.play_music(16U)) {
            diagnostics::log_warning(
                "title music unavailable: " + legacy_audio.error());
        }

        LegacyInputCoordinator input_coordinator{game};
        input::KeyRepeatController key_repeat{
            settings.movement_repeat_delay,
            kSaveListPageRepeatInterval};
        FramePresenter frame_presenter;
        timing::SteadyBiosTickSource tick_source;
        timing::SteadyVgaRetraceSource retrace_source{
            settings.fade_frame_delay};
        bool running = true;
        while (running) {
            const auto frame_tick = tick_source.tick();
            const auto frame_retrace = retrace_source.tick();
            const auto input_now = std::chrono::steady_clock::now();
            key_repeat.begin_frame();
            input_coordinator.process_host_events(
                platform, key_repeat, frame_tick, input_now, running);
            input_coordinator.dispatch_repeats(key_repeat, frame_tick);
            input_coordinator.apply_game_input();

            game.advance(frame_tick);
            const bool vga_frame = game.uses_vga_retrace();
            input_coordinator.after_advance(key_repeat);
            dispatch_audio_commands(game, legacy_audio);

            running = running && game.running();
            if (running) {
                const auto presentation_status =
                    frame_presenter.present(game, platform);
                if (presentation_status != 0) {
                    result.status = presentation_status;
                    return result;
                }
                game.finish_presented_tick(tick_source.tick());
                input_coordinator.after_present();
                diagnostics::log_trace(
                    "frame presented tick=" + std::to_string(frame_tick) +
                    " view=" +
                    std::to_string(static_cast<int>(game.view())));
            }

            if (settings.smoke_test) {
                running = false;
            } else if (
                running && !game.needs_immediate_frame(tick_source.tick())) {
                if (vga_frame &&
                    settings.fade_frame_delay >
                        std::chrono::nanoseconds::zero()) {
                    static_cast<void>(timing::wait_for_tick_change(
                        retrace_source, frame_retrace));
                } else if (!vga_frame) {
                    if (input_coordinator.waits_for_menu_input()) {
                        const auto wait_timeout =
                            tick_source.time_until_next_tick();
                        if (tick_source.tick() == frame_tick) {
                            platform.wait_for_event_or_timeout(wait_timeout);
                        }
                    } else {
                        static_cast<void>(timing::wait_for_tick_change(
                            tick_source, frame_tick));
                    }
                }
            }
        }

        result.ending_completed = game.ending_complete();
        fade_music_on_exit = game.fade_music_on_exit();
        if (settings.smoke_test) {
            diagnostics::log_info("smoke test completed");
            return result;
        }
    }

    if (fade_music_on_exit) {
        legacy_audio.fade_out_music();
    }
    return result;
}

}  // namespace openlegend::platform::sdl3
