#include "openlegend/audio/legacy_audio.hpp"

#include <array>
#include <cstdio>
#include <limits>
#include <thread>
#include <utility>

namespace openlegend::audio {

void SystemAudioDelay::delay(const std::chrono::milliseconds duration) noexcept {
    if (duration.count() > 0) {
        std::this_thread::sleep_for(duration);
    }
}

LegacyAudioController::LegacyAudioController(
    resource::DataRoot data_root, LegacyAudioPort& audio, AudioDelayPort& delay)
    : data_root_(std::move(data_root)), audio_(audio), delay_(delay) {}

bool LegacyAudioController::play_music(const std::size_t zero_based_index) {
    error_.clear();
    if (zero_based_index >= kLegacyMusicCount) {
        error_ = "music index is outside GAME01.XMI..GAME24.XMI";
        return false;
    }

    fade_out_music();
    audio_.end_music();

    auto file = data_root_.read(music_filename(zero_based_index));
    if (!file) {
        error_ = std::move(file.error);
        return false;
    }
    if (!audio_.start_music(file.bytes, kLegacyMaximumVolume, 0)) {
        error_ = "XMI backend rejected the selected music";
        return false;
    }
    current_music_ = zero_based_index;
    return true;
}

void LegacyAudioController::fade_in_music() noexcept {
    audio_.fade_music(kLegacyMaximumVolume, std::chrono::milliseconds{2000});
}

void LegacyAudioController::fade_out_music() noexcept {
    audio_.fade_music(0, std::chrono::milliseconds{2000});
    delay_.delay(std::chrono::milliseconds{1000});
}

void LegacyAudioController::end_music() noexcept {
    audio_.end_music();
}

bool LegacyAudioController::load_sample(const SampleBank bank, const std::size_t index) {
    error_.clear();
    if (index >= count_for(bank)) {
        error_ = "sample index is outside the selected legacy bank";
        return false;
    }

    audio_.end_sample(slot_for(bank));
    auto file = data_root_.read(sample_filename(bank, index));
    if (!file) {
        error_ = std::move(file.error);
        return false;
    }
    if (file.bytes.size() > std::numeric_limits<std::uint16_t>::max()) {
        error_ = "sample file exceeds the legacy 16-bit size table";
        return false;
    }

    auto& destination = loaded(bank);
    destination.sizes[index] = static_cast<std::uint16_t>(file.bytes.size());
    destination.bytes = std::move(file.bytes);
    return true;
}

bool LegacyAudioController::start_loaded_sample(
    const SampleBank bank, const std::size_t size_index) {
    error_.clear();
    if (size_index >= count_for(bank)) {
        error_ = "sample size index is outside the selected legacy bank";
        return false;
    }

    const auto slot = slot_for(bank);
    if (audio_.sample_status(slot) == PlaybackStatus::playing) {
        audio_.end_sample(slot);
    }

    const auto& source = loaded(bank);
    const auto legacy_size = static_cast<std::size_t>(source.sizes[size_index]);
    if (legacy_size == 0U || legacy_size > source.bytes.size()) {
        error_ = "selected sample buffer and legacy size table do not match";
        return false;
    }
    if (!audio_.start_sample(
            slot,
            std::span<const std::uint8_t>{source.bytes}.first(legacy_size),
            kLegacySampleRate,
            volume_for(bank),
            1)) {
        error_ = "audio backend rejected the selected raw sample";
        return false;
    }
    return true;
}

bool LegacyAudioController::play_sample(const SampleBank bank, const std::size_t index) {
    return load_sample(bank, index) && start_loaded_sample(bank, index);
}

void LegacyAudioController::end_sample(const SampleBank bank) noexcept {
    audio_.end_sample(slot_for(bank));
}

std::size_t LegacyAudioController::slot_for(const SampleBank bank) noexcept {
    return static_cast<std::size_t>(bank);
}

std::size_t LegacyAudioController::count_for(const SampleBank bank) noexcept {
    return bank == SampleBank::attack ? kLegacyAttackSampleCount : kLegacyEffectSampleCount;
}

int LegacyAudioController::volume_for(const SampleBank bank) noexcept {
    return bank == SampleBank::attack ? 200 : 400;
}

std::filesystem::path LegacyAudioController::music_filename(const std::size_t zero_based_index) {
    std::array<char, 16> name{};
    static_cast<void>(
        std::snprintf(name.data(), name.size(), "GAME%02zu.XMI", zero_based_index + 1U));
    return name.data();
}

std::filesystem::path LegacyAudioController::sample_filename(
    const SampleBank bank, const std::size_t index) {
    std::array<char, 16> name{};
    const char* pattern = bank == SampleBank::attack ? "ATK%02zu.WAV" : "E%02zu.WAV";
    static_cast<void>(std::snprintf(name.data(), name.size(), pattern, index));
    return name.data();
}

LegacyAudioController::LoadedSample& LegacyAudioController::loaded(
    const SampleBank bank) noexcept {
    return bank == SampleBank::attack ? attack_ : effect_;
}

const LegacyAudioController::LoadedSample& LegacyAudioController::loaded(
    const SampleBank bank) const noexcept {
    return bank == SampleBank::attack ? attack_ : effect_;
}

}  // namespace openlegend::audio
