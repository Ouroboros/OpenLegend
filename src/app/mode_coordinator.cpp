#include "openlegend/app/mode_coordinator.hpp"

namespace openlegend::app {

TickResult ModeCoordinator::run_tick(ModeDriver& driver, const std::size_t transition_budget) {
    if (!running_) {
        return {TickStatus::already_exited, mode_, 0U};
    }
    if (transition_budget == 0U) {
        return {TickStatus::transition_limit_exceeded, mode_, 0U};
    }

    for (std::size_t step_count = 1U; step_count <= transition_budget; ++step_count) {
        const auto result = driver.step(mode_);
        if (result.next_mode.has_value()) {
            mode_ = *result.next_mode;
        }

        switch (result.control) {
        case StepControl::yield:
            return {TickStatus::yielded, mode_, step_count};
        case StepControl::exit:
            mode_ = AppMode::exit;
            running_ = false;
            return {TickStatus::exited, mode_, step_count};
        case StepControl::continue_same_tick:
            break;
        }
    }

    return {TickStatus::transition_limit_exceeded, mode_, transition_budget};
}

}  // namespace openlegend::app
