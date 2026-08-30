#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "openlegend/resource/binary_file.hpp"

namespace openlegend::audio {

inline constexpr std::size_t kLegacySampleSlotCount = 8;
inline constexpr std::uint32_t kLegacySampleRate = 11'025U;
inline constexpr int kLegacyMaximumVolume = 127;
inline constexpr std::size_t kLegacyMusicCount = 24;
inline constexpr std::size_t kLegacyAttackSampleCount = 24;
inline constexpr std::size_t kLegacyEffectSampleCount = 53;

enum class PlaybackStatus {
    stopped,
    playing,
};

enum class SampleBank : std::uint8_t {
    attack = 1,
    effect = 2,
};

class LegacyAudioPort {
public:
    virtual ~LegacyAudioPort() = default;

    [[nodiscard]] virtual PlaybackStatus music_status() const noexcept = 0;
    [[nodiscard]] virtual bool start_music(
        std::span<const std::uint8_t> xmi,
        int legacy_volume,
        int legacy_loop_count) = 0;
    virtual void fade_music(int legacy_volume, std::chrono::milliseconds duration) noexcept = 0;
    virtual void end_music() noexcept = 0;

    [[nodiscard]] virtual PlaybackStatus sample_status(std::size_t slot) const noexcept = 0;
    [[nodiscard]] virtual bool start_sample(
        std::size_t slot,
        std::span<const std::uint8_t> raw_unsigned_mono,
        std::uint32_t playback_rate,
        int legacy_volume,
        int legacy_loop_count) = 0;
    virtual void end_sample(std::size_t slot) noexcept = 0;
};

class AudioDelayPort {
public:
    virtual ~AudioDelayPort() = default;
    virtual void delay(std::chrono::milliseconds duration) noexcept = 0;
};

class SystemAudioDelay final : public AudioDelayPort {
public:
    void delay(std::chrono::milliseconds duration) noexcept override;
};

class LegacyAudioController {
public:
    LegacyAudioController(
        resource::DataRoot data_root, LegacyAudioPort& audio, AudioDelayPort& delay);

    [[nodiscard]] bool play_music(std::size_t zero_based_index);
    void fade_in_music() noexcept;
    void fade_out_music() noexcept;
    void end_music() noexcept;

    [[nodiscard]] bool load_sample(SampleBank bank, std::size_t index);
    [[nodiscard]] bool start_loaded_sample(SampleBank bank, std::size_t size_index);
    [[nodiscard]] bool play_sample(SampleBank bank, std::size_t index);
    void end_sample(SampleBank bank) noexcept;

    [[nodiscard]] std::size_t current_music() const noexcept { return current_music_; }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }

private:
    struct LoadedSample {
        std::vector<std::uint8_t> bytes;
        std::vector<std::uint16_t> sizes;
    };

    [[nodiscard]] static std::size_t slot_for(SampleBank bank) noexcept;
    [[nodiscard]] static std::size_t count_for(SampleBank bank) noexcept;
    [[nodiscard]] static int volume_for(SampleBank bank) noexcept;
    [[nodiscard]] static std::filesystem::path music_filename(std::size_t zero_based_index);
    [[nodiscard]] static std::filesystem::path sample_filename(
        SampleBank bank, std::size_t index);
    [[nodiscard]] LoadedSample& loaded(SampleBank bank) noexcept;
    [[nodiscard]] const LoadedSample& loaded(SampleBank bank) const noexcept;

    resource::DataRoot data_root_;
    LegacyAudioPort& audio_;
    AudioDelayPort& delay_;
    LoadedSample attack_{{}, std::vector<std::uint16_t>(kLegacyAttackSampleCount)};
    LoadedSample effect_{{}, std::vector<std::uint16_t>(kLegacyEffectSampleCount)};
    std::size_t current_music_{kLegacyMusicCount};
    std::string error_;
};

class AudioMixer final : public LegacyAudioPort {
public:
    explicit AudioMixer(std::uint32_t output_rate = kLegacySampleRate);
    ~AudioMixer() override;

    AudioMixer(const AudioMixer&) = delete;
    AudioMixer& operator=(const AudioMixer&) = delete;

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] const std::string& error() const noexcept;
    [[nodiscard]] std::uint32_t output_rate() const noexcept;

    [[nodiscard]] PlaybackStatus music_status() const noexcept override;
    [[nodiscard]] bool start_music(
        std::span<const std::uint8_t> xmi,
        int legacy_volume,
        int legacy_loop_count) override;
    void fade_music(int legacy_volume, std::chrono::milliseconds duration) noexcept override;
    void end_music() noexcept override;

    [[nodiscard]] PlaybackStatus sample_status(std::size_t slot) const noexcept override;
    [[nodiscard]] bool start_sample(
        std::size_t slot,
        std::span<const std::uint8_t> raw_unsigned_mono,
        std::uint32_t playback_rate,
        int legacy_volume,
        int legacy_loop_count) override;
    void end_sample(std::size_t slot) noexcept override;

    void render(std::span<std::int16_t> interleaved_stereo) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openlegend::audio
