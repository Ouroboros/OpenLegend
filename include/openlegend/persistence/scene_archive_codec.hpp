#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "openlegend/attributes.hpp"

namespace openlegend::persistence {

enum class SceneArchiveKind {
    maps,
    events,
};

NODISCARD std::optional<std::string> encode_scene_archive(
    std::span<const std::uint8_t> bytes, SceneArchiveKind kind);

NODISCARD std::optional<std::vector<std::uint8_t>> decode_scene_archive(
    std::string_view base64, SceneArchiveKind kind);

}
