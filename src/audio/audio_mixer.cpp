#include "openlegend/audio/legacy_audio.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#endif
#include <adlmidi.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace openlegend::audio {
namespace {

[[nodiscard]] int clamp_volume(const int legacy_volume) noexcept {
    return std::clamp(legacy_volume, 0, kLegacyMaximumVolume);
}

[[nodiscard]] std::int16_t clamp_sample(const std::int32_t sample) noexcept {
    return static_cast<std::int16_t>(std::clamp(
        sample,
        static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::min()),
        static_cast<std::int32_t>(std::numeric_limits<std::int16_t>::max())));
}

[[nodiscard]] std::string adl_detail(
    ADL_MIDIPlayer* player, const char* fallback) {
    const char* detail = player == nullptr ? adl_errorString() : adl_errorInfo(player);
    return detail == nullptr || *detail == '\0' ? fallback : detail;
}

}  // namespace

class AudioMixer::Impl {
public:
    struct SampleChannel {
        std::vector<std::uint8_t> bytes;
        double position{};
        double step{1.0};
        int volume{kLegacyMaximumVolume};
        int loops_remaining{1};
        bool playing{};
    };

    explicit Impl(const std::uint32_t requested_rate) : rate(requested_rate) {
        if (rate == 0U || rate > static_cast<std::uint32_t>(std::numeric_limits<long>::max())) {
            error = "audio output rate is outside libADLMIDI limits";
            return;
        }
        player = adl_init(static_cast<long>(rate));
        if (player == nullptr) {
            error = adl_detail(nullptr, "libADLMIDI initialization failed");
            return;
        }
        if (adl_switchEmulator(player, ADLMIDI_EMU_DOSBOX) < 0 ||
            adl_setNumChips(player, 1) < 0 || adl_setBank(player, 0) < 0) {
            error = adl_detail(player, "libADLMIDI configuration failed");
            adl_close(player);
            player = nullptr;
            return;
        }
        adl_setVolumeRangeModel(player, ADLMIDI_VolumeModel_AIL);
    }

    ~Impl() {
        if (player != nullptr) {
            adl_close(player);
        }
    }

    ADL_MIDIPlayer* player{};
    std::uint32_t rate{};
    mutable std::mutex mutex;
    std::string error;
    std::vector<std::uint8_t> music_bytes;
    std::vector<std::int16_t> music_samples;
    std::array<SampleChannel, kLegacySampleSlotCount> channels{};
    double music_volume{};
    double music_target_volume{};
    std::uint64_t fade_frames_remaining{};
    bool music_playing{};
};

AudioMixer::AudioMixer(const std::uint32_t output_rate) : impl_(std::make_unique<Impl>(output_rate)) {}

AudioMixer::~AudioMixer() = default;

bool AudioMixer::valid() const noexcept {
    return impl_->player != nullptr;
}

const std::string& AudioMixer::error() const noexcept {
    return impl_->error;
}

std::uint32_t AudioMixer::output_rate() const noexcept {
    return impl_->rate;
}

PlaybackStatus AudioMixer::music_status() const noexcept {
    std::scoped_lock lock{impl_->mutex};
    return impl_->music_playing ? PlaybackStatus::playing : PlaybackStatus::stopped;
}

bool AudioMixer::start_music(
    const std::span<const std::uint8_t> xmi,
    const int legacy_volume,
    const int legacy_loop_count) {
    std::scoped_lock lock{impl_->mutex};
    if (impl_->player == nullptr || xmi.empty()) {
        impl_->error = "cannot start empty XMI data";
        return false;
    }

    impl_->music_bytes.assign(xmi.begin(), xmi.end());
    if (adl_openData(
            impl_->player,
            impl_->music_bytes.data(),
            static_cast<unsigned long>(impl_->music_bytes.size())) < 0) {
        impl_->error = adl_detail(impl_->player, "libADLMIDI rejected XMI data");
        impl_->music_bytes.clear();
        impl_->music_playing = false;
        return false;
    }

    if (legacy_loop_count == 0) {
        adl_setLoopEnabled(impl_->player, 1);
        adl_setLoopCount(impl_->player, -1);
    } else {
        adl_setLoopEnabled(impl_->player, legacy_loop_count > 1 ? 1 : 0);
        adl_setLoopCount(impl_->player, std::max(legacy_loop_count - 1, 0));
    }
    impl_->music_volume = static_cast<double>(clamp_volume(legacy_volume));
    impl_->music_target_volume = impl_->music_volume;
    impl_->fade_frames_remaining = 0U;
    impl_->music_playing = true;
    impl_->error.clear();
    return true;
}

void AudioMixer::fade_music(
    const int legacy_volume, const std::chrono::milliseconds duration) noexcept {
    std::scoped_lock lock{impl_->mutex};
    impl_->music_target_volume = static_cast<double>(clamp_volume(legacy_volume));
    if (duration.count() <= 0) {
        impl_->music_volume = impl_->music_target_volume;
        impl_->fade_frames_remaining = 0U;
        return;
    }
    const auto milliseconds = static_cast<std::uint64_t>(duration.count());
    impl_->fade_frames_remaining =
        (static_cast<std::uint64_t>(impl_->rate) * milliseconds) / 1000U;
    if (impl_->fade_frames_remaining == 0U) {
        impl_->music_volume = impl_->music_target_volume;
    }
}

void AudioMixer::end_music() noexcept {
    std::scoped_lock lock{impl_->mutex};
    impl_->music_playing = false;
    impl_->music_bytes.clear();
    impl_->fade_frames_remaining = 0U;
    if (impl_->player != nullptr) {
        adl_reset(impl_->player);
    }
}

PlaybackStatus AudioMixer::sample_status(const std::size_t slot) const noexcept {
    std::scoped_lock lock{impl_->mutex};
    return slot < impl_->channels.size() && impl_->channels[slot].playing
        ? PlaybackStatus::playing
        : PlaybackStatus::stopped;
}

bool AudioMixer::start_sample(
    const std::size_t slot,
    const std::span<const std::uint8_t> raw_unsigned_mono,
    const std::uint32_t playback_rate,
    const int legacy_volume,
    const int legacy_loop_count) {
    std::scoped_lock lock{impl_->mutex};
    if (slot >= impl_->channels.size() || raw_unsigned_mono.empty() || playback_rate == 0U ||
        impl_->rate == 0U) {
        impl_->error = "invalid raw sample request";
        return false;
    }

    auto& channel = impl_->channels[slot];
    channel.bytes.assign(raw_unsigned_mono.begin(), raw_unsigned_mono.end());
    channel.position = 0.0;
    channel.step = static_cast<double>(playback_rate) / static_cast<double>(impl_->rate);
    channel.volume = legacy_volume;
    channel.loops_remaining = legacy_loop_count;
    channel.playing = true;
    impl_->error.clear();
    return true;
}

void AudioMixer::end_sample(const std::size_t slot) noexcept {
    std::scoped_lock lock{impl_->mutex};
    if (slot >= impl_->channels.size()) {
        return;
    }
    auto& channel = impl_->channels[slot];
    channel.playing = false;
    channel.bytes.clear();
    channel.position = 0.0;
}

void AudioMixer::render(const std::span<std::int16_t> interleaved_stereo) noexcept {
    std::fill(interleaved_stereo.begin(), interleaved_stereo.end(), std::int16_t{0});
    if ((interleaved_stereo.size() & 1U) != 0U) {
        return;
    }

    std::scoped_lock lock{impl_->mutex};
    const auto frames = interleaved_stereo.size() / 2U;
    impl_->music_samples.assign(interleaved_stereo.size(), std::int16_t{0});
    if (impl_->music_playing && impl_->player != nullptr) {
        const auto requested = static_cast<int>(std::min<std::size_t>(
            impl_->music_samples.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
        const auto generated = adl_play(impl_->player, requested, impl_->music_samples.data());
        if (generated <= 0 || adl_atEnd(impl_->player) > 0) {
            impl_->music_playing = false;
        }
    }

    for (std::size_t frame = 0; frame < frames; ++frame) {
        if (impl_->fade_frames_remaining > 0U) {
            impl_->music_volume +=
                (impl_->music_target_volume - impl_->music_volume) /
                static_cast<double>(impl_->fade_frames_remaining);
            --impl_->fade_frames_remaining;
        }

        const auto music_gain = impl_->music_volume / static_cast<double>(kLegacyMaximumVolume);
        std::int32_t left = static_cast<std::int32_t>(
            std::lround(static_cast<double>(impl_->music_samples[frame * 2U]) * music_gain));
        std::int32_t right = static_cast<std::int32_t>(
            std::lround(static_cast<double>(impl_->music_samples[frame * 2U + 1U]) * music_gain));

        for (auto& channel : impl_->channels) {
            if (!channel.playing || channel.bytes.empty()) {
                continue;
            }
            auto source_index = static_cast<std::size_t>(channel.position);
            if (source_index >= channel.bytes.size()) {
                if (channel.loops_remaining == 0 || channel.loops_remaining > 1) {
                    if (channel.loops_remaining > 1) {
                        --channel.loops_remaining;
                    }
                    channel.position = std::fmod(
                        channel.position, static_cast<double>(channel.bytes.size()));
                    source_index = static_cast<std::size_t>(channel.position);
                } else {
                    channel.playing = false;
                    continue;
                }
            }

            const auto centered = static_cast<std::int32_t>(channel.bytes[source_index]) - 128;
            const auto gain = static_cast<double>(clamp_volume(channel.volume)) /
                              static_cast<double>(kLegacyMaximumVolume);
            const auto sample = static_cast<std::int32_t>(
                std::lround(static_cast<double>(centered * 256) * gain));
            left += sample;
            right += sample;
            channel.position += channel.step;
        }

        interleaved_stereo[frame * 2U] = clamp_sample(left);
        interleaved_stereo[frame * 2U + 1U] = clamp_sample(right);
    }
}

}  // namespace openlegend::audio
