#include <chrono>
#include <iostream>
#include <string_view>

#include "openlegend/compat/legacy_video.hpp"
#include "openlegend/compat/runtime_platform.hpp"
#include "sdl_runtime_platform.hpp"

int main(const int argc, const char* const* argv) {
    using namespace openlegend;

    const bool smoke_test = argc == 2 && std::string_view{argv[1]} == "--smoke-test";

    platform::sdl3::SdlRuntimePlatform platform;
    if (!platform.valid()) {
        std::cerr << "Unable to initialize SDL3 platform\n";
        return 1;
    }

    compat::LegacyPixels pixels{};
    compat::LegacyPalette palette{};
    const compat::IndexedFrameView frame{pixels, palette};

    bool running = true;
    while (running) {
        compat::HostEvent event{};
        while (platform.poll_event(event)) {
            if (event.type == compat::HostEventType::quit) {
                running = false;
            }
        }
        if (running && !platform.present(frame)) {
            std::cerr << "Unable to present indexed framebuffer\n";
            return 2;
        }
        if (smoke_test) {
            running = false;
        } else {
            platform.delay(std::chrono::milliseconds{16});
        }
    }

    return 0;
}
