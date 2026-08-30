#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "openlegend/audio/legacy_audio.hpp"
#include "openlegend/resource/binary_file.hpp"
#include "test_support.hpp"

#ifndef OPENLEGEND_GAME_DATA_ROOT
#error OPENLEGEND_GAME_DATA_ROOT must name the original game data directory
#endif

namespace {

using openlegend::audio::PlaybackStatus;

struct AudioCall {
    std::string operation;
    std::size_t slot{};
    std::size_t bytes{};
    std::uint32_t rate{};
    int volume{};
    int loops{};
    std::chrono::milliseconds duration{};
    std::array<std::uint8_t, 4> prefix{};
};

class RecordingAudio final : public openlegend::audio::LegacyAudioPort {
public:
    [[nodiscard]] PlaybackStatus music_status() const noexcept override { return music; }

    [[nodiscard]] bool start_music(
        const std::span<const std::uint8_t> xmi,
        const int legacy_volume,
        const int legacy_loop_count) override {
        AudioCall call{"start_music"};
        call.bytes = xmi.size();
        call.volume = legacy_volume;
        call.loops = legacy_loop_count;
        std::copy_n(xmi.begin(), std::min(call.prefix.size(), xmi.size()), call.prefix.begin());
        calls.push_back(call);
        music = PlaybackStatus::playing;
        return true;
    }

    void fade_music(
        const int legacy_volume, const std::chrono::milliseconds duration) noexcept override {
        AudioCall call{"fade_music"};
        call.volume = legacy_volume;
        call.duration = duration;
        calls.push_back(call);
    }

    void end_music() noexcept override {
        calls.push_back(AudioCall{"end_music"});
        music = PlaybackStatus::stopped;
    }

    [[nodiscard]] PlaybackStatus sample_status(const std::size_t slot) const noexcept override {
        return slot < samples.size() ? samples[slot] : PlaybackStatus::stopped;
    }

    [[nodiscard]] bool start_sample(
        const std::size_t slot,
        const std::span<const std::uint8_t> raw_unsigned_mono,
        const std::uint32_t playback_rate,
        const int legacy_volume,
        const int legacy_loop_count) override {
        AudioCall call{"start_sample"};
        call.slot = slot;
        call.bytes = raw_unsigned_mono.size();
        call.rate = playback_rate;
        call.volume = legacy_volume;
        call.loops = legacy_loop_count;
        std::copy_n(
            raw_unsigned_mono.begin(),
            std::min(call.prefix.size(), raw_unsigned_mono.size()),
            call.prefix.begin());
        calls.push_back(call);
        samples[slot] = PlaybackStatus::playing;
        return true;
    }

    void end_sample(const std::size_t slot) noexcept override {
        AudioCall call{"end_sample"};
        call.slot = slot;
        calls.push_back(call);
        if (slot < samples.size()) {
            samples[slot] = PlaybackStatus::stopped;
        }
    }

    PlaybackStatus music{PlaybackStatus::stopped};
    std::array<PlaybackStatus, openlegend::audio::kLegacySampleSlotCount> samples{};
    std::vector<AudioCall> calls;
};

class RecordingDelay final : public openlegend::audio::AudioDelayPort {
public:
    explicit RecordingDelay(std::vector<AudioCall>& calls) : calls_(calls) {}

    void delay(const std::chrono::milliseconds duration) noexcept override {
        AudioCall call{"delay"};
        call.duration = duration;
        calls_.push_back(call);
    }

private:
    std::vector<AudioCall>& calls_;
};

[[nodiscard]] std::filesystem::path numbered_name(
    const char* pattern, const std::size_t index) {
    std::array<char, 20> buffer{};
    static_cast<void>(std::snprintf(buffer.data(), buffer.size(), pattern, index));
    return buffer.data();
}

[[nodiscard]] std::uint16_t little_u16(
    const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
}

[[nodiscard]] std::uint32_t little_u32(
    const std::span<const std::uint8_t> bytes, const std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

void run_controller_tests(const openlegend::resource::DataRoot& root) {
    using namespace openlegend::audio;

    RecordingAudio port;
    RecordingDelay delay{port.calls};
    LegacyAudioController controller{root, port, delay};

    OL_CHECK(controller.play_music(0U));
    OL_CHECK(port.calls.size() == 4U);
    OL_CHECK(port.calls[0].operation == "fade_music");
    OL_CHECK(port.calls[0].volume == 0);
    OL_CHECK(port.calls[0].duration == std::chrono::milliseconds{2000});
    OL_CHECK(port.calls[1].operation == "delay");
    OL_CHECK(port.calls[1].duration == std::chrono::milliseconds{1000});
    OL_CHECK(port.calls[2].operation == "end_music");
    OL_CHECK(port.calls[3].operation == "start_music");
    OL_CHECK(port.calls[3].bytes == 6488U);
    OL_CHECK(port.calls[3].volume == 127);
    OL_CHECK(port.calls[3].loops == 0);
    const std::array<std::uint8_t, 4> form_prefix{'F', 'O', 'R', 'M'};
    OL_CHECK(port.calls[3].prefix == form_prefix);
    OL_CHECK(controller.current_music() == 0U);

    const auto before_invalid = port.calls.size();
    OL_CHECK(!controller.play_music(kLegacyMusicCount));
    OL_CHECK(port.calls.size() == before_invalid);

    port.calls.clear();
    OL_CHECK(controller.load_sample(SampleBank::attack, 0U));
    OL_CHECK(port.calls.size() == 1U);
    OL_CHECK(port.calls[0].operation == "end_sample");
    OL_CHECK(port.calls[0].slot == 1U);
    OL_CHECK(controller.start_loaded_sample(SampleBank::attack, 0U));
    OL_CHECK(port.calls.back().operation == "start_sample");
    OL_CHECK(port.calls.back().slot == 1U);
    OL_CHECK(port.calls.back().bytes == 4460U);
    OL_CHECK(port.calls.back().rate == kLegacySampleRate);
    OL_CHECK(port.calls.back().volume == 200);
    OL_CHECK(port.calls.back().loops == 1);
    const std::array<std::uint8_t, 4> riff_prefix{'R', 'I', 'F', 'F'};
    OL_CHECK(port.calls.back().prefix == riff_prefix);

    const auto before_reuse = port.calls.size();
    OL_CHECK(controller.start_loaded_sample(SampleBank::attack, 0U));
    OL_CHECK(port.calls.size() == before_reuse + 2U);
    OL_CHECK(port.calls[before_reuse].operation == "end_sample");
    OL_CHECK(port.calls[before_reuse + 1U].operation == "start_sample");

    port.calls.clear();
    OL_CHECK(controller.play_sample(SampleBank::effect, 20U));
    OL_CHECK(port.calls.size() == 2U);
    OL_CHECK(port.calls[0].operation == "end_sample");
    OL_CHECK(port.calls[1].slot == 2U);
    OL_CHECK(port.calls[1].bytes == 11158U);
    OL_CHECK(port.calls[1].volume == 400);
}

void run_asset_tests(const openlegend::resource::DataRoot& root) {
    constexpr std::array<std::size_t, openlegend::audio::kLegacyAttackSampleCount> attack_sizes{
        4460U, 4460U, 3552U, 2454U, 7676U, 9284U, 5308U, 5444U,
        5516U, 11834U, 12354U, 4076U, 9104U, 4820U, 5680U, 6948U,
        12614U, 12312U, 19880U, 8768U, 17398U, 8804U, 23456U, 19228U,
    };
    constexpr std::array<std::size_t, openlegend::audio::kLegacyEffectSampleCount> effect_sizes{
        17824U, 4896U, 9284U, 7882U, 8646U, 16704U, 14848U, 15478U, 11078U,
        15786U, 32492U, 10418U, 3326U, 6828U, 6828U, 16704U, 17400U, 16724U,
        6884U, 11050U, 11158U, 6384U, 7716U, 10260U, 4294U, 8248U, 8800U,
        12038U, 7910U, 18900U, 20700U, 18514U, 8380U, 13644U, 7460U, 9942U,
        16290U, 12792U, 12464U, 22094U, 14862U, 20204U, 18344U, 7706U, 30032U,
        10418U, 9810U, 9810U, 9154U, 18532U, 19822U, 9962U, 7716U,
    };
    constexpr std::array<std::size_t, openlegend::audio::kLegacyMusicCount> music_sizes{
        6488U, 7654U, 6046U, 15470U, 13876U, 9122U, 6558U, 3938U,
        2910U, 2508U, 18004U, 3208U, 952U, 1812U, 8082U, 760U,
        3862U, 8576U, 9718U, 6888U, 5188U, 7866U, 4132U, 4534U,
    };

    std::size_t rate_11000 = 0U;
    std::size_t rate_11025 = 0U;
    for (std::size_t index = 0; index < openlegend::audio::kLegacyAttackSampleCount; ++index) {
        const auto file = root.read(numbered_name("ATK%02zu.WAV", index));
        OL_CHECK(file);
        OL_CHECK(file.bytes.size() == attack_sizes[index]);
        OL_CHECK(file.bytes.size() >= 44U);
        OL_CHECK(std::equal(file.bytes.begin(), file.bytes.begin() + 4, "RIFF"));
        OL_CHECK(std::equal(file.bytes.begin() + 8, file.bytes.begin() + 12, "WAVE"));
        OL_CHECK(little_u16(file.bytes, 22U) == 1U);
        OL_CHECK(little_u16(file.bytes, 34U) == 8U);
        const auto rate = little_u32(file.bytes, 24U);
        rate_11000 += rate == 11000U ? 1U : 0U;
        rate_11025 += rate == 11025U ? 1U : 0U;
    }
    for (std::size_t index = 0; index < openlegend::audio::kLegacyEffectSampleCount; ++index) {
        const auto file = root.read(numbered_name("E%02zu.WAV", index));
        OL_CHECK(file);
        OL_CHECK(file.bytes.size() == effect_sizes[index]);
        OL_CHECK(file.bytes.size() >= 44U);
        OL_CHECK(std::equal(file.bytes.begin(), file.bytes.begin() + 4, "RIFF"));
        OL_CHECK(std::equal(file.bytes.begin() + 8, file.bytes.begin() + 12, "WAVE"));
        OL_CHECK(little_u16(file.bytes, 22U) == 1U);
        OL_CHECK(little_u16(file.bytes, 34U) == 8U);
        const auto rate = little_u32(file.bytes, 24U);
        rate_11000 += rate == 11000U ? 1U : 0U;
        rate_11025 += rate == 11025U ? 1U : 0U;
    }
    OL_CHECK(rate_11000 == 3U);
    OL_CHECK(rate_11025 == 74U);

    openlegend::audio::AudioMixer mixer;
    OL_CHECK(mixer.valid());
    std::vector<std::int16_t> pcm(2048U);
    for (std::size_t index = 0; index < openlegend::audio::kLegacyMusicCount; ++index) {
        const auto file = root.read(numbered_name("GAME%02zu.XMI", index + 1U));
        OL_CHECK(file);
        OL_CHECK(file.bytes.size() == music_sizes[index]);
        OL_CHECK(file.bytes.size() >= 24U);
        OL_CHECK(std::equal(file.bytes.begin(), file.bytes.begin() + 4, "FORM"));
        OL_CHECK(std::equal(file.bytes.begin() + 8, file.bytes.begin() + 12, "XDIR"));
        OL_CHECK(mixer.start_music(file.bytes, 127, 0));
        bool nonzero = false;
        for (int block = 0; block < 240 && !nonzero; ++block) {
            mixer.render(pcm);
            nonzero = std::any_of(pcm.begin(), pcm.end(), [](const std::int16_t value) {
                return value != 0;
            });
        }
        OL_CHECK(nonzero);
        mixer.end_music();
    }
}

void run_mixer_tests() {
    using namespace openlegend::audio;

    AudioMixer mixer;
    OL_CHECK(mixer.valid());
    const std::array<std::uint8_t, 3> raw{0U, 128U, 255U};
    OL_CHECK(mixer.start_sample(7U, raw, kLegacySampleRate, 400, 1));
    OL_CHECK(mixer.sample_status(7U) == PlaybackStatus::playing);
    std::array<std::int16_t, 8> pcm{};
    mixer.render(pcm);
    OL_CHECK(pcm[0] == std::numeric_limits<std::int16_t>::min());
    OL_CHECK(pcm[1] == std::numeric_limits<std::int16_t>::min());
    OL_CHECK(pcm[2] == 0);
    OL_CHECK(pcm[3] == 0);
    OL_CHECK(pcm[4] == 32512);
    OL_CHECK(pcm[5] == 32512);
    OL_CHECK(mixer.sample_status(7U) == PlaybackStatus::stopped);
    OL_CHECK(mixer.sample_status(kLegacySampleSlotCount) == PlaybackStatus::stopped);
}

}  // namespace

int main() {
    const openlegend::resource::DataRoot root{
        openlegend::test::utf8_path(OPENLEGEND_GAME_DATA_ROOT)};
    run_controller_tests(root);
    run_asset_tests(root);
    run_mixer_tests();
    return openlegend::test::failures == 0 ? 0 : 1;
}
