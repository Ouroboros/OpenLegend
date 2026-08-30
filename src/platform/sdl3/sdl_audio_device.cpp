#include "sdl_audio_device.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace openlegend::platform::sdl3 {

SdlAudioDevice::SdlAudioDevice(audio::AudioMixer& mixer) : mixer_(mixer) {
    if (!mixer_.valid() || !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        return;
    }
    audio_subsystem_initialized_ = true;

    const SDL_AudioSpec specification{
        SDL_AUDIO_S16,
        2,
        static_cast<int>(mixer_.output_rate()),
    };
    stream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, &SdlAudioDevice::feed, this);
    if (stream_ != nullptr && !SDL_ResumeAudioStreamDevice(stream_)) {
        SDL_DestroyAudioStream(stream_);
        stream_ = nullptr;
    }
}

SdlAudioDevice::~SdlAudioDevice() {
    SDL_DestroyAudioStream(stream_);
    if (audio_subsystem_initialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void SDLCALL SdlAudioDevice::feed(
    void* userdata,
    SDL_AudioStream* stream,
    const int additional_amount,
    const int total_amount) {
    static_cast<void>(total_amount);
    static_cast<SdlAudioDevice*>(userdata)->feed(stream, additional_amount);
}

void SdlAudioDevice::feed(SDL_AudioStream* stream, const int additional_amount) {
    if (additional_amount <= 0) {
        return;
    }
    constexpr std::size_t bytes_per_frame = sizeof(std::int16_t) * 2U;
    const auto requested_bytes = static_cast<std::size_t>(additional_amount);
    const auto frames = (requested_bytes + bytes_per_frame - 1U) / bytes_per_frame;
    buffer_.resize(frames * 2U);
    mixer_.render(buffer_);
    static_cast<void>(SDL_PutAudioStreamData(
        stream,
        buffer_.data(),
        static_cast<int>(std::min<std::size_t>(
            buffer_.size() * sizeof(std::int16_t),
            static_cast<std::size_t>(std::numeric_limits<int>::max())))));
}

}  // namespace openlegend::platform::sdl3
