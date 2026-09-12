#pragma once

#include <cstddef>
#include <optional>

#include "openlegend/attributes.hpp"

namespace openlegend::app {

enum class AppMode {
    startup,
    title,
    world,
    scene,
    ui,
    battle,
    exit,
};

enum class StepControl {
    yield,
    continue_same_tick,
    exit,
};

struct ModeStepResult {
    StepControl control{StepControl::yield};
    std::optional<AppMode> next_mode{};

    NODISCARD static constexpr ModeStepResult stay() noexcept {
        return {};
    }

    NODISCARD static constexpr ModeStepResult yield_to(const AppMode mode) noexcept {
        return {StepControl::yield, mode};
    }

    NODISCARD static constexpr ModeStepResult continue_to(const AppMode mode) noexcept {
        return {StepControl::continue_same_tick, mode};
    }

    NODISCARD static constexpr ModeStepResult quit() noexcept {
        return {StepControl::exit, AppMode::exit};
    }
};

class ModeDriver {
public:
    virtual ~ModeDriver() = default;

    NODISCARD virtual ModeStepResult step(AppMode mode) = 0;
};

enum class TickStatus {
    yielded,
    exited,
    transition_limit_exceeded,
    already_exited,
};

struct TickResult {
    TickStatus status{TickStatus::yielded};
    AppMode mode{AppMode::startup};
    std::size_t steps{};
};

class ModeCoordinator {
public:
    explicit constexpr ModeCoordinator(const AppMode initial_mode = AppMode::startup) noexcept
        : mode_(initial_mode), running_(initial_mode != AppMode::exit) {}

    NODISCARD TickResult run_tick(ModeDriver& driver, std::size_t transition_budget = 16U);

    NODISCARD constexpr AppMode mode() const noexcept { return mode_; }

    NODISCARD constexpr bool running() const noexcept { return running_; }

private:
    AppMode mode_;
    bool running_;
};

}  // namespace openlegend::app
