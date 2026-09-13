#include "legacy_runtime_loop.hpp"

#include <algorithm>
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
#include "openlegend/attributes.hpp"
#include "openlegend/app/legacy_game_runtime.hpp"
#include "openlegend/audio/legacy_audio.hpp"
#include "openlegend/battle/battle_session.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/motion/authoritative_motion.hpp"
#include "openlegend/input/key_repeat.hpp"
#include "openlegend/random/legacy_random.hpp"
#include "openlegend/scene/scene.hpp"
#include "openlegend/time/legacy_clock.hpp"
#include "sdl_audio_device.hpp"
#include "sdl_runtime_platform.hpp"

namespace openlegend::platform::sdl3 {
namespace {

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

NODISCARD std::uint32_t make_random_seed() {
    const auto wall_time = std::chrono::system_clock::now().time_since_epoch();
    const auto whole_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(wall_time);
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        wall_time - whole_seconds);
    const auto second = static_cast<std::uint8_t>(whole_seconds.count() % 60);
    const auto hundredth = static_cast<std::uint8_t>(milliseconds.count() / 10);
    return random::LegacyRandom::dos_time_seed(second, hundredth);
}

NODISCARD constexpr std::string_view game_view_name(
    const app::LegacyGameView view) noexcept {
    switch (view) {
    case app::LegacyGameView::title: return "title";
    case app::LegacyGameView::name_entry: return "name_entry";
    case app::LegacyGameView::attributes: return "attributes";
    case app::LegacyGameView::world: return "world";
    case app::LegacyGameView::scene: return "scene";
    case app::LegacyGameView::battle: return "battle";
    case app::LegacyGameView::game_menu: return "game_menu";
    case app::LegacyGameView::error: return "error";
    case app::LegacyGameView::exited: return "exited";
    }
    return "unknown";
}

struct FadeTiming {
    bool active{};
    std::uint64_t sequence{};
    app::LegacyGameView start_view{app::LegacyGameView::title};
    std::chrono::steady_clock::time_point started_at{};
    std::chrono::nanoseconds presentation_time{};
    std::chrono::nanoseconds wait_time{};
    std::chrono::nanoseconds configured_frame_delay{};
    std::size_t presented_frames{};
};

void begin_fade_timing(
    FadeTiming& timing,
    const app::LegacyGameView view,
    const std::chrono::nanoseconds configured_frame_delay) {
    timing.active = true;
    ++timing.sequence;
    timing.start_view = view;
    timing.started_at = std::chrono::steady_clock::now();
    timing.presentation_time = std::chrono::nanoseconds::zero();
    timing.wait_time = std::chrono::nanoseconds::zero();
    timing.configured_frame_delay = configured_frame_delay;
    timing.presented_frames = 0U;
    diagnostics::log_info(
        "fade timing started sequence=" + std::to_string(timing.sequence) +
        " view=" + std::string{game_view_name(view)} +
        " configured_frame_delay_us=" + std::to_string(
            std::chrono::duration_cast<std::chrono::microseconds>(
                configured_frame_delay).count()));
}

void finish_fade_timing(
    FadeTiming& timing,
    const app::LegacyGameView end_view) {
    const auto total_time = std::chrono::steady_clock::now() - timing.started_at;
    const auto accounted_time = timing.presentation_time + timing.wait_time;
    const auto other_time = total_time > accounted_time
        ? total_time - accounted_time
        : std::chrono::steady_clock::duration::zero();
    const auto total_us =
        std::chrono::duration_cast<std::chrono::microseconds>(total_time).count();
    const auto presentation_us = std::chrono::duration_cast<std::chrono::microseconds>(
        timing.presentation_time).count();
    const auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
        timing.wait_time).count();
    const auto other_us =
        std::chrono::duration_cast<std::chrono::microseconds>(other_time).count();
    const auto average_presentation_us = timing.presented_frames == 0U
        ? 0
        : presentation_us / static_cast<std::int64_t>(timing.presented_frames);
    diagnostics::log_info(
        "fade timing completed sequence=" + std::to_string(timing.sequence) +
        " start_view=" + std::string{game_view_name(timing.start_view)} +
        " end_view=" + std::string{game_view_name(end_view)} +
        " frames=" + std::to_string(timing.presented_frames) +
        " total_us=" + std::to_string(total_us) +
        " present_us=" + std::to_string(presentation_us) +
        " wait_us=" + std::to_string(wait_us) +
        " other_us=" + std::to_string(other_us) +
        " average_present_us=" + std::to_string(average_presentation_us) +
        " configured_frame_delay_us=" + std::to_string(
            std::chrono::duration_cast<std::chrono::microseconds>(
                timing.configured_frame_delay).count()));
    timing.active = false;
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
            random_seed,
            settings.game_resolution,
            settings.name_input_method,
            settings.movement_step_duration};
        diagnostics::log_info(
            "runtime random_seed=" + std::to_string(random_seed));
        if (!game.valid()) {
            diagnostics::log_critical(
                "game runtime initialization failed: " + game.error());
            report_runtime_error("game runtime", game.error());
            result.status = 5;
            return result;
        }
        const bool native_motion_enabled =
            settings.movement_step_duration > std::chrono::nanoseconds::zero();
        if (!legacy_audio.play_music(16U)) {
            diagnostics::log_warning(
                "title music unavailable: " + legacy_audio.error());
        }

        LegacyInputCoordinator input_coordinator{game};
        input::KeyRepeatController key_repeat{
            settings.movement_repeat_delay,
            settings.menu_repeat_delay,
            settings.menu_repeat_interval};
        FramePresenter frame_presenter;
        timing::SteadyBiosTickSource tick_source;
        timing::SteadyFadeFrameSource fade_frame_source{
            settings.fade_frame_delay};
        FadeTiming fade_timing;
        auto previous_motion_time = std::chrono::steady_clock::now();
        auto motion_frame_deadline = previous_motion_time;
        auto weather_frame_deadline = previous_motion_time;
        auto previous_weather_presentation_time = previous_motion_time;
        std::uint64_t timed_motion_sequence = 0U;
        std::uint64_t observed_weather_revision = game.weather_revision();
        std::uint64_t observed_weather_position_revision =
            game.weather_position_revision();
        std::int64_t weather_offset_x{};
        std::int64_t weather_subpixel_numerator{};
        std::uint32_t last_advanced_tick{};
        bool advanced_tick_known = false;
        bool motion_was_active = false;
        bool weather_was_active = false;
        bool motion_vsync_available = native_motion_enabled;
        bool motion_vsync_status_logged = false;
        bool vsync_disable_warning_logged = false;
        bool running = true;
        while (running) {
            const auto motion_time = std::chrono::steady_clock::now();
            const auto active_motion_sequence = game.motion_sequence();
            const bool motion_active_at_frame_start =
                active_motion_sequence != 0U;
            if (active_motion_sequence == 0U) {
                previous_motion_time = motion_time;
                timed_motion_sequence = 0U;
            } else if (active_motion_sequence != timed_motion_sequence) {
                previous_motion_time = motion_time;
                timed_motion_sequence = active_motion_sequence;
            } else {
                game.advance_motion(motion_time - previous_motion_time);
                previous_motion_time = std::chrono::steady_clock::now();
                timed_motion_sequence = game.motion_sequence();
            }
            const auto frame_tick = tick_source.tick();
            const auto fade_frame_tick = fade_frame_source.tick();
            const auto input_now = std::chrono::steady_clock::now();
            key_repeat.begin_frame();
            input_coordinator.synchronize_repeat_context(
                key_repeat, input_now);
            input_coordinator.process_host_events(
                platform, key_repeat, frame_tick, running);
            input_coordinator.dispatch_repeats(key_repeat, frame_tick);
            input_coordinator.apply_game_input();

            const bool motion_completed_this_frame =
                motion_active_at_frame_start && !game.motion_active();
            const bool repeated_weather_frame =
                game.weather_presentation_active() && advanced_tick_known &&
                frame_tick == last_advanced_tick &&
                !motion_completed_this_frame;
            bool logic_advanced = false;
            if (!game.motion_active() && !repeated_weather_frame) {
                game.advance(frame_tick);
                last_advanced_tick = frame_tick;
                advanced_tick_known = true;
                logic_advanced = true;
            }
            bool motion_active = game.motion_active();
            const auto motion_sequence = game.motion_sequence();
            if (motion_active && motion_sequence != timed_motion_sequence) {
                timed_motion_sequence = motion_sequence;
                previous_motion_time = std::chrono::steady_clock::now();
            }
            if (motion_active && !motion_was_active && motion_vsync_available) {
                motion_vsync_available = platform.set_vsync_enabled(true);
                if (!motion_vsync_status_logged) {
                    diagnostics::log_info(
                        motion_vsync_available
                            ? "native motion presentation uses renderer VSync"
                            : "native motion presentation uses independent deadline");
                    motion_vsync_status_logged = true;
                }
            } else if (!motion_active && platform.vsync_enabled()) {
                if (platform.set_vsync_enabled(false)) {
                    vsync_disable_warning_logged = false;
                } else if (!vsync_disable_warning_logged) {
                    diagnostics::log_warning(
                        "unable to disable renderer VSync after native motion; retrying");
                    vsync_disable_warning_logged = true;
                }
            }
            const bool weather_active = game.weather_presentation_active();
            const auto weather_revision = game.weather_revision();
            const auto weather_position_revision =
                game.weather_position_revision();
            const auto presentation_time = std::chrono::steady_clock::now();
            const auto revision_delta =
                weather_revision - observed_weather_revision;
            const auto position_revision_delta =
                weather_position_revision - observed_weather_position_revision;
            const bool weather_content_changed =
                revision_delta != position_revision_delta;
            const auto weather_offset_before = weather_offset_x;
            if (!weather_active) {
                weather_offset_x = 0;
                weather_subpixel_numerator = 0;
            } else if (!weather_was_active || weather_content_changed) {
                weather_offset_x = 0;
                weather_subpixel_numerator = 0;
            } else {
                const auto elapsed = std::clamp(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        presentation_time - previous_weather_presentation_time),
                    std::chrono::nanoseconds::zero(),
                    timing::kBiosTickDuration);
                const auto duration_count = timing::kBiosTickDuration.count();
                weather_subpixel_numerator +=
                    motion::kFixedUnitsPerGridUnit * elapsed.count();
                weather_offset_x +=
                    weather_subpixel_numerator / duration_count;
                weather_subpixel_numerator %= duration_count;
                weather_offset_x -= static_cast<std::int64_t>(
                    position_revision_delta) *
                    motion::kFixedUnitsPerGridUnit;
            }
            if (revision_delta != 0U ||
                weather_active != weather_was_active) {
                std::string record{
                    "weather_phase active=" +
                    std::to_string(static_cast<int>(weather_active)) +
                    " was_active=" +
                    std::to_string(static_cast<int>(weather_was_active)) +
                    " revision=" + std::to_string(weather_revision) +
                    " revision_delta=" + std::to_string(revision_delta) +
                    " position_revision=" +
                    std::to_string(weather_position_revision) +
                    " position_delta=" +
                    std::to_string(position_revision_delta) +
                    " content_changed=" +
                    std::to_string(static_cast<int>(weather_content_changed)) +
                    " offset_before=" +
                    std::to_string(weather_offset_before) +
                    " offset_after=" + std::to_string(weather_offset_x) +
                    " motion_active=" +
                    std::to_string(static_cast<int>(motion_active)) +
                    " motion_sequence=" + std::to_string(motion_sequence)};
                if (motion_active) {
                    const auto presentation = game.motion_presentation();
                    record +=
                        " motion_domain=" + std::to_string(
                            static_cast<int>(presentation.domain)) +
                        " motion_position=" +
                        std::to_string(presentation.position.x) + "," +
                        std::to_string(presentation.position.y) + "," +
                        std::to_string(presentation.position.height) +
                        " destination_phase=" + std::to_string(
                            static_cast<int>(
                                presentation.destination_depth_phase));
                }
                diagnostics::log_debug(record);
            }
            observed_weather_revision = weather_revision;
            observed_weather_position_revision = weather_position_revision;
            previous_weather_presentation_time = presentation_time;
            const bool fade_frame = game.uses_fade_frame_clock();
            if (!settings.smoke_test) {
                if (fade_frame && !fade_timing.active) {
                    begin_fade_timing(
                        fade_timing, game.view(), settings.fade_frame_delay);
                } else if (!fade_frame && fade_timing.active) {
                    finish_fade_timing(fade_timing, game.view());
                }
            }
            input_coordinator.after_advance(key_repeat);
            dispatch_audio_commands(game, legacy_audio);

            running = running && game.running();
            if (running) {
                const auto presentation_started_at =
                    std::chrono::steady_clock::now();
                const auto presentation_status = frame_presenter.present(
                    game,
                    platform,
                    !weather_active || logic_advanced,
                    weather_offset_x);
                if (fade_frame && fade_timing.active) {
                    fade_timing.presentation_time +=
                        std::chrono::steady_clock::now() - presentation_started_at;
                    ++fade_timing.presented_frames;
                }
                if (presentation_status != 0) {
                    result.status = presentation_status;
                    return result;
                }
                bool presented_tick_finished = false;
                if ((!motion_active &&
                     (!weather_active || logic_advanced)) ||
                    game.world_motion_endpoint_present_pending()) {
                    game.finish_presented_tick(tick_source.tick());
                    presented_tick_finished = true;
                }
                if (presented_tick_finished && motion_completed_this_frame) {
                    const auto resumed_at = std::chrono::steady_clock::now();
                    if (game.resume_motion_after_endpoint_presented(
                            resumed_at - motion_time)) {
                        motion_active = true;
                        timed_motion_sequence = game.motion_sequence();
                        previous_motion_time = resumed_at;
                        motion_frame_deadline =
                            resumed_at + settings.motion_frame_interval;
                        if (motion_vsync_available &&
                            !platform.vsync_enabled()) {
                            motion_vsync_available =
                                platform.set_vsync_enabled(true);
                        }
                    }
                }
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
                if (motion_active) {
                    const auto now = std::chrono::steady_clock::now();
                    if (!motion_was_active) {
                        motion_frame_deadline =
                            now + settings.motion_frame_interval;
                    }
                    if (!platform.vsync_enabled() &&
                        settings.motion_frame_interval >
                            std::chrono::nanoseconds::zero()) {
                        if (motion_frame_deadline > now) {
                            platform.wait_for_event_or_timeout(
                                motion_frame_deadline - now);
                        }
                        const auto after_wait = std::chrono::steady_clock::now();
                        motion_frame_deadline += settings.motion_frame_interval;
                        if (motion_frame_deadline <= after_wait) {
                            motion_frame_deadline =
                                after_wait + settings.motion_frame_interval;
                        }
                    }
                } else if (weather_active) {
                    const auto now = std::chrono::steady_clock::now();
                    if (!weather_was_active || motion_was_active) {
                        weather_frame_deadline =
                            now + settings.motion_frame_interval;
                    }
                    if (settings.motion_frame_interval >
                        std::chrono::nanoseconds::zero()) {
                        if (weather_frame_deadline > now) {
                            platform.wait_for_event_or_timeout(
                                weather_frame_deadline - now);
                        }
                        const auto after_wait = std::chrono::steady_clock::now();
                        weather_frame_deadline += settings.motion_frame_interval;
                        if (weather_frame_deadline <= after_wait) {
                            weather_frame_deadline =
                                after_wait + settings.motion_frame_interval;
                        }
                    }
                } else if (fade_frame &&
                    settings.fade_frame_delay >
                        std::chrono::nanoseconds::zero()) {
                    const auto wait_started_at =
                        std::chrono::steady_clock::now();
                    static_cast<void>(timing::wait_for_tick_change(
                        fade_frame_source, fade_frame_tick));
                    if (fade_timing.active) {
                        fade_timing.wait_time +=
                            std::chrono::steady_clock::now() - wait_started_at;
                    }
                } else if (!fade_frame) {
                    if (input_coordinator.waits_for_menu_input()) {
                        const auto wait_now = std::chrono::steady_clock::now();
                        input_coordinator.synchronize_repeat_context(
                            key_repeat, wait_now);
                        auto wait_timeout = tick_source.time_until_next_tick();
                        if (const auto repeat_timeout =
                                key_repeat.time_until_menu_repeat(wait_now)) {
                            wait_timeout = std::min(wait_timeout, *repeat_timeout);
                        }
                        if (tick_source.tick() == frame_tick) {
                            platform.wait_for_event_or_timeout(wait_timeout);
                        }
                    } else {
                        static_cast<void>(timing::wait_for_tick_change(
                            tick_source, frame_tick));
                    }
                }
            }
            motion_was_active = motion_active;
            weather_was_active = weather_active;
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
