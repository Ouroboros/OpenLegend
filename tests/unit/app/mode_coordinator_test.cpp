#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include "openlegend/app/mode_coordinator.hpp"
#include "test_support.hpp"

void run_legacy_video_tests();
void run_runtime_configuration_tests();

namespace {

class RecordingDriver final : public openlegend::app::ModeDriver {
public:
    explicit RecordingDriver(
        std::function<openlegend::app::ModeStepResult(openlegend::app::AppMode)> callback)
        : callback_(std::move(callback)) {}

    [[nodiscard]] openlegend::app::ModeStepResult step(const openlegend::app::AppMode mode) override {
        calls.push_back(mode);
        return callback_(mode);
    }

    std::vector<openlegend::app::AppMode> calls;

private:
    std::function<openlegend::app::ModeStepResult(openlegend::app::AppMode)> callback_;
};

void run_mode_coordinator_tests() {
    using namespace openlegend::app;

    {
        ModeCoordinator coordinator;
        RecordingDriver driver([](AppMode) { return ModeStepResult::stay(); });
        const auto result = coordinator.run_tick(driver);
        OL_CHECK(result.status == TickStatus::yielded);
        OL_CHECK(result.mode == AppMode::startup);
        OL_CHECK(result.steps == 1U);
        OL_CHECK(driver.calls.size() == 1U);
    }

    {
        ModeCoordinator coordinator;
        RecordingDriver driver([](const AppMode mode) {
            if (mode == AppMode::startup) {
                return ModeStepResult::continue_to(AppMode::title);
            }
            return ModeStepResult::stay();
        });
        const auto result = coordinator.run_tick(driver);
        OL_CHECK(result.status == TickStatus::yielded);
        OL_CHECK(result.mode == AppMode::title);
        OL_CHECK(result.steps == 2U);
        OL_CHECK(driver.calls == std::vector<AppMode>({AppMode::startup, AppMode::title}));
    }

    {
        ModeCoordinator coordinator{AppMode::world};
        RecordingDriver driver([](AppMode) { return ModeStepResult::yield_to(AppMode::ui); });
        const auto result = coordinator.run_tick(driver);
        OL_CHECK(result.status == TickStatus::yielded);
        OL_CHECK(coordinator.mode() == AppMode::ui);
    }

    {
        ModeCoordinator coordinator{AppMode::battle};
        RecordingDriver driver([](AppMode) { return ModeStepResult::quit(); });
        const auto result = coordinator.run_tick(driver);
        OL_CHECK(result.status == TickStatus::exited);
        OL_CHECK(!coordinator.running());
        OL_CHECK(coordinator.run_tick(driver).status == TickStatus::already_exited);
    }

    {
        ModeCoordinator coordinator;
        RecordingDriver driver([](AppMode mode) { return ModeStepResult::continue_to(mode); });
        const auto result = coordinator.run_tick(driver, 3U);
        OL_CHECK(result.status == TickStatus::transition_limit_exceeded);
        OL_CHECK(result.steps == 3U);
        OL_CHECK(coordinator.running());
    }
}

}  // namespace

int main() {
    run_mode_coordinator_tests();
    run_runtime_configuration_tests();
    run_legacy_video_tests();
    return openlegend::test::failures == 0 ? 0 : 1;
}
