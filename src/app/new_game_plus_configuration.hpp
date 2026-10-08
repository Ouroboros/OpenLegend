#pragma once

#include <toml++/toml.hpp>

#include "openlegend/attributes.hpp"
#include "openlegend/app/runtime_configuration.hpp"

namespace openlegend::app::detail {

NODISCARD NewGamePlusConfigurationLoadResult new_game_plus_configuration_from_document(
    const toml::table& document);

}
