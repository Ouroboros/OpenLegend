#include "openlegend/audio/legacy_audio_worker.hpp"

#include <exception>
#include <utility>

namespace openlegend::audio {

LegacyAudioWorker::LegacyAudioWorker(
    resource::DataRoot data_root,
    LegacyAudioPort& audio,
    AudioDelayPort& delay)
    : controller_(std::move(data_root), audio, delay),
      worker_([this]() { run(); }) {}

LegacyAudioWorker::~LegacyAudioWorker() {
    {
        const std::scoped_lock lock{mutex_};
        stopping_ = true;
    }
    work_available_.notify_one();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void LegacyAudioWorker::play_music(
    const std::size_t zero_based_index, const bool force) {
    enqueue(Command{
        CommandKind::play_music,
        SampleBank::effect,
        zero_based_index,
        force});
}

void LegacyAudioWorker::prepare_music(
    const std::size_t zero_based_index, const bool force) {
    enqueue(Command{
        CommandKind::prepare_music,
        SampleBank::effect,
        zero_based_index,
        force});
}

void LegacyAudioWorker::switch_music(
    const std::size_t zero_based_index, const bool force) {
    enqueue(Command{
        CommandKind::switch_music,
        SampleBank::effect,
        zero_based_index,
        force});
}

void LegacyAudioWorker::fade_out_music() {
    enqueue(Command{CommandKind::fade_out_music});
}

void LegacyAudioWorker::load_sample(
    const SampleBank bank, const std::size_t index) {
    enqueue(Command{CommandKind::load_sample, bank, index});
}

void LegacyAudioWorker::start_loaded_sample(
    const SampleBank bank, const std::size_t size_index) {
    enqueue(Command{CommandKind::start_loaded_sample, bank, size_index});
}

void LegacyAudioWorker::play_sample(
    const SampleBank bank, const std::size_t index) {
    enqueue(Command{CommandKind::play_sample, bank, index});
}

void LegacyAudioWorker::wait_until_idle() {
    std::unique_lock lock{mutex_};
    idle_.wait(lock, [this]() {
        return commands_.empty() && !command_active_;
    });
}

std::vector<std::string> LegacyAudioWorker::take_errors() {
    const std::scoped_lock lock{mutex_};
    std::vector<std::string> errors;
    errors.swap(errors_);
    return errors;
}

void LegacyAudioWorker::enqueue(Command command) {
    {
        const std::scoped_lock lock{mutex_};
        if (stopping_) {
            return;
        }
        commands_.push_back(command);
    }
    work_available_.notify_one();
}

void LegacyAudioWorker::run() {
    for (;;) {
        Command command;
        {
            std::unique_lock lock{mutex_};
            work_available_.wait(lock, [this]() {
                return stopping_ || !commands_.empty();
            });
            if (commands_.empty()) {
                return;
            }
            command = commands_.front();
            commands_.pop_front();
            command_active_ = true;
        }

        try {
            execute(command);
        } catch (const std::exception& exception) {
            const std::scoped_lock lock{mutex_};
            errors_.push_back(
                std::string{"audio worker exception: "} + exception.what());
        } catch (...) {
            const std::scoped_lock lock{mutex_};
            errors_.push_back("audio worker exception: unknown failure");
        }

        {
            const std::scoped_lock lock{mutex_};
            command_active_ = false;
            if (commands_.empty()) {
                idle_.notify_all();
            }
        }
    }
}

void LegacyAudioWorker::execute(const Command& command) {
    switch (command.kind) {
    case CommandKind::play_music:
        if (!command.force && controller_.current_music() == command.index) {
            return;
        }
        if (!controller_.play_music(command.index)) {
            record_error("music " + std::to_string(command.index));
        }
        return;
    case CommandKind::prepare_music:
        if (command.index < kLegacyMusicCount &&
            (command.force || controller_.current_music() != command.index)) {
            controller_.begin_music_fade_out();
        }
        return;
    case CommandKind::switch_music:
        if (!command.force && controller_.current_music() == command.index) {
            return;
        }
        if (!controller_.switch_music(command.index)) {
            record_error("music switch " + std::to_string(command.index));
        }
        return;
    case CommandKind::fade_out_music:
        controller_.fade_out_music();
        return;
    case CommandKind::load_sample:
        if (!controller_.load_sample(command.bank, command.index)) {
            record_error("sample load " + std::to_string(command.index));
        }
        return;
    case CommandKind::start_loaded_sample:
        if (!controller_.start_loaded_sample(command.bank, command.index)) {
            record_error(
                "loaded sample start " + std::to_string(command.index));
        }
        return;
    case CommandKind::play_sample:
        if (!controller_.play_sample(command.bank, command.index)) {
            record_error("sample play " + std::to_string(command.index));
        }
        return;
    }
}

void LegacyAudioWorker::record_error(std::string operation) {
    const std::scoped_lock lock{mutex_};
    errors_.push_back(std::move(operation) + ": " + controller_.error());
}

}  // namespace openlegend::audio
