#pragma once

#include <optional>
#include <string>

#include "openlegend/attributes.hpp"
#include "openlegend/model/runtime_snapshot.hpp"

namespace openlegend::model {

struct PlaythroughTransitionResult {
    std::optional<RuntimeGameSnapshot> snapshot;
    std::string error;

    NODISCARD explicit operator bool() const noexcept {
        return snapshot.has_value();
    }
};

NODISCARD PlaythroughTransitionResult prepare_next_playthrough(
    const RuntimeGameSnapshot& completed, const GameSnapshot& baseline,
    const NewGamePlusConfiguration& configuration);

}
