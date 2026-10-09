#include "openlegend/model/magic_progression.hpp"

#include <algorithm>
#include <cmath>

#include "openlegend/attributes.hpp"
#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {
namespace {

NODISCARD MagicEffects original_effects(const MagicRecord& magic, const std::size_t index) noexcept {
    return {
        magic.word(magic_word::attack_begin + index),
        magic.word(magic_word::hurt_mp_begin + index),
        magic.word(magic_word::add_mp_begin + index),
    };
}

NODISCARD long double combined_effect(const MagicRecord& magic, const std::size_t index) noexcept {
    const auto effects = original_effects(magic, index);
    return magic.word(magic_word::hurt_type) == 0
        ? static_cast<long double>(effects.attack)
        : static_cast<long double>(effects.hurt_mp) + 0.75L * effects.add_mp;
}

NODISCARD long double geometry_weight(const MagicRecord& magic, const std::size_t index) noexcept {
    const auto distance = static_cast<long double>(magic.word(magic_word::select_distance_begin + index));
    const auto radius = static_cast<long double>(magic.word(magic_word::attack_distance_begin + index));
    long double covered = 1.0L;
    switch (magic.word(magic_word::attack_area_type)) {
    case 1:
        covered = std::max(1.0L, distance);
        break;
    case 2:
        covered = std::max(1.0L, 4.0L * distance);
        break;
    case 3:
        covered = std::max(1.0L, (2.0L * radius + 1.0L) * (2.0L * radius + 1.0L));
        break;
    default:
        break;
    }
    return std::min(25.0L, 1.0L + 0.4L * std::sqrt(covered - 1.0L)) *
        (1.0L + 0.04L * std::max(distance - 1.0L, 0.0L));
}

NODISCARD long double growth_strength(const MagicRecord& magic) noexcept {
    long double logarithms = 0.0L;
    for (std::size_t index = 0U; index < magic_word::level_value_count; ++index) {
        const auto effect = combined_effect(magic, index);
        if (effect <= 0.0L) {
            return 0.0L;
        }
        logarithms += std::log(effect / geometry_weight(magic, index));
    }
    const auto sustained = std::exp(logarithms / magic_word::level_value_count);
    const auto cost = static_cast<long double>(magic.word(magic_word::need_mp));
    const auto compensation = cost == 0.0L
        ? 1.0L : std::min(3.0L, std::pow(1.0L + 5.0L * cost / 15.0L, 0.75L));
    return std::pow(sustained, 0.6L) * std::pow(compensation, 0.9L);
}

NODISCARD bool same_prototype(const MagicRecord& left, const MagicRecord& right) noexcept {
    constexpr auto offset = magic_word::hurt_type * 2U;
    return std::ranges::equal(
        std::span{left.bytes}.subspan(offset), std::span{right.bytes}.subspan(offset));
}

NODISCARD long double median(std::vector<long double>& values) {
    std::ranges::sort(values);
    const auto middle = values.size() / 2U;
    return values.size() % 2U == 0U
        ? (values[middle - 1U] + values[middle]) / 2.0L : values[middle];
}

NODISCARD long double growth_endpoint(const long double stage) noexcept {
    return stage + 0.25L * stage * stage / (stage + 3.0L);
}

NODISCARD long double growth_progress(const std::int64_t level) noexcept {
    const auto stages = static_cast<long double>((level - 10) / 10);
    const auto fraction = static_cast<long double>((level - 10) % 10) / 10.0L;
    const auto smooth = fraction * fraction * (3.0L - 2.0L * fraction);
    const auto beginning = growth_endpoint(stages);
    return beginning + (growth_endpoint(stages + 1.0L) - beginning) * smooth;
}

NODISCARD std::optional<std::int64_t> floor_effect(const long double value) noexcept {
    const auto floored = std::floor(value);
    const auto exclusive_maximum = std::ldexp(1.0L, 63);
    if (!std::isfinite(floored) || floored < -exclusive_maximum || floored >= exclusive_maximum) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(floored);
}

}

MagicProgression::MagicProgression(const std::span<const MagicRecord> magics)
    : magics_{magics.begin(), magics.end()}, increments_(magics.size()) {
    std::vector<long double> strengths;
    strengths.reserve(magics_.size());
    std::array<std::vector<long double>, 2> final_effects;
    std::array<std::vector<long double>, 2> prototype_strengths;
    for (std::size_t index = 0U; index < magics_.size(); ++index) {
        const auto& magic = magics_[index];
        const auto type = magic.word(magic_word::hurt_type);
        const auto shape = magic.word(magic_word::attack_area_type);
        if (type < 0 || type > 1 || shape < 0 || shape > 3 || magic.word(magic_word::need_mp) < 0) {
            error_ = "magic growth type, shape or MP cost is invalid";
            return;
        }
        const auto strength = growth_strength(magic);
        if (!std::isfinite(strength)) {
            error_ = "magic growth strength is not finite";
            return;
        }
        strengths.push_back(strength);
        if (strength <= 0.0L) {
            continue;
        }
        bool duplicate = false;
        for (std::size_t previous = 0U; previous < index; ++previous) {
            if (same_prototype(magic, magics_[previous])) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            const auto category = static_cast<std::size_t>(type);
            final_effects[category].push_back(combined_effect(magic, 9U));
            prototype_strengths[category].push_back(strength);
        }
    }
    for (std::size_t category = 0U; category < references_.size(); ++category) {
        if (!final_effects[category].empty()) {
            references_[category] = {
                final_effects[category].size(), median(final_effects[category]),
                median(prototype_strengths[category]),
            };
        }
    }
    for (std::size_t index = 0U; index < magics_.size(); ++index) {
        if (strengths[index] <= 0.0L) {
            continue;
        }
        const auto category = static_cast<std::size_t>(magics_[index].word(magic_word::hurt_type));
        const auto& reference = references_[category];
        increments_[index] = reference.final_effect * 1.25L * strengths[index] / reference.strength;
        if (!std::isfinite(increments_[index])) {
            error_ = "magic growth increment is not finite";
            return;
        }
    }
}

bool MagicProgression::definitions_match(const std::span<const MagicRecord> magics) const noexcept {
    return std::ranges::equal(magics_, magics);
}

std::optional<MagicEffects> MagicProgression::effects(
    const std::size_t magic_id, const std::int64_t level) const noexcept {
    if (!valid() || magic_id >= magics_.size() || level < 1) {
        return std::nullopt;
    }
    const auto& magic = magics_[magic_id];
    if (level <= static_cast<std::int64_t>(magic_word::level_value_count)) {
        return original_effects(magic, static_cast<std::size_t>(level - 1));
    }
    auto result = original_effects(magic, 9U);
    if (increments_[magic_id] == 0.0L) {
        return result;
    }
    const auto original = combined_effect(magic, 9U);
    const auto expanded = original + increments_[magic_id] * growth_progress(level);
    if (!std::isfinite(expanded)) {
        return std::nullopt;
    }
    if (magic.word(magic_word::hurt_type) == 0) {
        const auto attack = floor_effect(expanded);
        if (!attack.has_value()) {
            return std::nullopt;
        }
        result.attack = *attack;
    } else {
        const auto ratio = expanded / original;
        const auto hurt_mp = floor_effect(result.hurt_mp * ratio);
        const auto add_mp = floor_effect(result.add_mp * ratio);
        if (!hurt_mp.has_value() || !add_mp.has_value()) {
            return std::nullopt;
        }
        result.hurt_mp = *hurt_mp;
        result.add_mp = *add_mp;
    }
    return result;
}

std::optional<std::int64_t> magic_level_from_proficiency(const std::int64_t proficiency) noexcept {
    return proficiency < 0 ? std::nullopt : std::optional<std::int64_t>{proficiency / 100 + 1};
}

std::optional<std::int64_t> magic_mp_cost(
    const std::int64_t level, const std::int64_t base_cost) noexcept {
    return level < 1 || base_cost < 0 ? std::nullopt : checked_multiply(level / 2, base_cost);
}

std::optional<std::int64_t> affordable_magic_level(
    const std::int64_t learned_level, const std::int64_t mp, const std::int64_t base_cost) noexcept {
    if (learned_level < 1 || mp < 0 || base_cost < 0) {
        return std::nullopt;
    }
    if (base_cost == 0 || mp / base_cost >= learned_level / 2) {
        return learned_level;
    }
    return (mp / base_cost) * 2 + 1;
}

}
