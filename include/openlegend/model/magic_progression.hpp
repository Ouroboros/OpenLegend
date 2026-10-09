#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/model/game_snapshot.hpp"

namespace openlegend::model {

struct MagicEffects {
    std::int64_t attack{};
    std::int64_t hurt_mp{};
    std::int64_t add_mp{};

    NODISCARD bool operator==(const MagicEffects&) const = default;
};

struct MagicGrowthReference {
    std::size_t prototype_count{};
    long double final_effect{};
    long double strength{};
};

class MagicProgression {
public:
    explicit MagicProgression(std::span<const MagicRecord> magics);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD const std::array<MagicGrowthReference, 2>& references() const noexcept {
        return references_;
    }

    NODISCARD bool definitions_match(std::span<const MagicRecord> magics) const noexcept;

    NODISCARD std::optional<MagicEffects> effects(
        std::size_t magic_id, std::int64_t level) const noexcept;

private:
    std::vector<MagicRecord> magics_;
    std::vector<long double> increments_;
    std::array<MagicGrowthReference, 2> references_{};
    std::string error_;
};

NODISCARD std::optional<std::int64_t> magic_level_from_proficiency(
    std::int64_t proficiency) noexcept;

NODISCARD std::optional<std::int64_t> magic_mp_cost(
    std::int64_t level, std::int64_t base_cost) noexcept;

NODISCARD std::optional<std::int64_t> affordable_magic_level(
    std::int64_t learned_level, std::int64_t mp, std::int64_t base_cost) noexcept;

}
