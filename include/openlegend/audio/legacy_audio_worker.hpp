#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/audio/legacy_audio.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::audio {

class LegacyAudioWorker {
public:
    LegacyAudioWorker(
        resource::DataRoot data_root,
        LegacyAudioPort& audio,
        AudioDelayPort& delay);

    ~LegacyAudioWorker();

    LegacyAudioWorker(const LegacyAudioWorker&) = delete;

    LegacyAudioWorker& operator=(const LegacyAudioWorker&) = delete;

    void play_music(std::size_t zero_based_index, bool force = false);

    void prepare_music(std::size_t zero_based_index, bool force = false);

    void switch_music(std::size_t zero_based_index, bool force = false);

    void fade_out_music();

    void load_sample(SampleBank bank, std::size_t index);

    void start_loaded_sample(SampleBank bank, std::size_t size_index);

    void play_sample(SampleBank bank, std::size_t index);

    void wait_until_idle();

    NODISCARD std::vector<std::string> take_errors();

private:
    enum class CommandKind {
        play_music,
        prepare_music,
        switch_music,
        fade_out_music,
        load_sample,
        start_loaded_sample,
        play_sample,
    };

    struct Command {
        CommandKind kind{CommandKind::play_music};
        SampleBank bank{SampleBank::effect};
        std::size_t index{};
        bool force{};
    };

    void enqueue(Command command);

    void run();

    void execute(const Command& command);

    void record_error(std::string operation);

    LegacyAudioController controller_;
    std::mutex mutex_;
    std::condition_variable work_available_;
    std::condition_variable idle_;
    std::deque<Command> commands_;
    std::vector<std::string> errors_;
    bool command_active_{};
    bool stopping_{};
    std::thread worker_;
};

}  // namespace openlegend::audio
