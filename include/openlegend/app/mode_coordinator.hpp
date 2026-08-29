#pragma once

#include <cstddef>
#include <optional>

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

    [[nodiscard]] static constexpr ModeStepResult stay() noexcept {
        return {};
    }

    [[nodiscard]] static constexpr ModeStepResult yield_to(const AppMode mode) noexcept {
        return {StepControl::yield, mode};
    }

    [[nodiscard]] static constexpr ModeStepResult continue_to(const AppMode mode) noexcept {
        return {StepControl::continue_same_tick, mode};
    }

    [[nodiscard]] static constexpr ModeStepResult quit() noexcept {
        return {StepControl::exit, AppMode::exit};
    }
};

class ModeDriver {
public:
    virtual ~ModeDriver() = default;
    [[nodiscard]] virtual ModeStepResult step(AppMode mode) = 0;
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

    [[nodiscard]] TickResult run_tick(ModeDriver& driver, std::size_t transition_budget = 16U);
    [[nodiscard]] constexpr AppMode mode() const noexcept { return mode_; }
    [[nodiscard]] constexpr bool running() const noexcept { return running_; }

private:
    AppMode mode_;
    bool running_;
};

}  // namespace openlegend::app
