#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include "openlegend/audio/legacy_audio.hpp"

namespace openlegend::platform::sdl3 {

class SdlAudioDevice final {
public:
    explicit SdlAudioDevice(audio::AudioMixer& mixer);
    ~SdlAudioDevice();

    SdlAudioDevice(const SdlAudioDevice&) = delete;
    SdlAudioDevice& operator=(const SdlAudioDevice&) = delete;

    [[nodiscard]] bool valid() const noexcept { return stream_ != nullptr; }

private:
    static void SDLCALL feed(
        void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
    void feed(SDL_AudioStream* stream, int additional_amount);

    audio::AudioMixer& mixer_;
    SDL_AudioStream* stream_{};
    std::vector<std::int16_t> buffer_;
    bool audio_subsystem_initialized_{};
};

}  // namespace openlegend::platform::sdl3
