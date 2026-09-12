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

#include "openlegend/attributes.hpp"
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

    NODISCARD virtual PlaybackStatus music_status() const noexcept = 0;

    NODISCARD virtual bool start_music(
        std::span<const std::uint8_t> xmi,
        int legacy_volume,
        int legacy_loop_count) = 0;

    virtual void fade_music(int legacy_volume, std::chrono::milliseconds duration) noexcept = 0;

    virtual void end_music() noexcept = 0;

    NODISCARD virtual PlaybackStatus sample_status(std::size_t slot) const noexcept = 0;

    NODISCARD virtual bool start_sample(
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

    NODISCARD bool play_music(std::size_t zero_based_index);

    void fade_in_music() noexcept;

    void fade_out_music() noexcept;

    void end_music() noexcept;

    NODISCARD bool load_sample(SampleBank bank, std::size_t index);

    NODISCARD bool start_loaded_sample(SampleBank bank, std::size_t size_index);

    NODISCARD bool play_sample(SampleBank bank, std::size_t index);

    void end_sample(SampleBank bank) noexcept;

    NODISCARD std::size_t current_music() const noexcept { return current_music_; }

    NODISCARD const std::string& error() const noexcept { return error_; }

private:
    struct LoadedSample {
        std::vector<std::uint8_t> bytes;
        std::vector<std::uint16_t> sizes;
    };

    NODISCARD static std::size_t slot_for(SampleBank bank) noexcept;

    NODISCARD static std::size_t count_for(SampleBank bank) noexcept;

    NODISCARD static int volume_for(SampleBank bank) noexcept;

    NODISCARD static std::filesystem::path music_filename(std::size_t zero_based_index);

    NODISCARD static std::filesystem::path sample_filename(
        SampleBank bank, std::size_t index);

    NODISCARD LoadedSample& loaded(SampleBank bank) noexcept;

    NODISCARD const LoadedSample& loaded(SampleBank bank) const noexcept;

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

    NODISCARD bool valid() const noexcept;

    NODISCARD const std::string& error() const noexcept;

    NODISCARD std::uint32_t output_rate() const noexcept;

    NODISCARD PlaybackStatus music_status() const noexcept override;

    NODISCARD bool start_music(
        std::span<const std::uint8_t> xmi,
        int legacy_volume,
        int legacy_loop_count) override;

    void fade_music(int legacy_volume, std::chrono::milliseconds duration) noexcept override;

    void end_music() noexcept override;

    NODISCARD PlaybackStatus sample_status(std::size_t slot) const noexcept override;

    NODISCARD bool start_sample(
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
