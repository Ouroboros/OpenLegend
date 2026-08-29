#pragma once

#include <chrono>

#include "openlegend/compat/legacy_video.hpp"

namespace openlegend::compat {

enum class HostEventType {
    none,
    quit,
};

struct HostEvent {
    HostEventType type{HostEventType::none};
};

class RuntimePlatform {
public:
    virtual ~RuntimePlatform() = default;

    [[nodiscard]] virtual bool poll_event(HostEvent& event) = 0;
    [[nodiscard]] virtual bool present(IndexedFrameView frame) = 0;
    virtual void delay(std::chrono::milliseconds duration) = 0;
};

}  // namespace openlegend::compat
