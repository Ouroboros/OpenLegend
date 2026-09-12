#pragma once

#include "openlegend/attributes.hpp"

namespace openlegend::platform::sdl3 {

NODISCARD int run_sdl_application(
    int argument_count, const char* const* argument_values);

}  // namespace openlegend::platform::sdl3
