#include "openlegend/model/hurt.hpp"

#include "openlegend/model/checked_arithmetic.hpp"

namespace openlegend::model {

std::optional<std::strong_ordering> compare_hurt_percentage(
    const std::int64_t hurt,
    const std::int64_t denominator,
    const std::int64_t percentage) noexcept {
    if (hurt < 0 || denominator < 0 || percentage < 0) {
        return std::nullopt;
    }
    if (denominator == 0) {
        if (hurt != 0) {
            return std::nullopt;
        }
        return std::int64_t{0} <=> percentage;
    }
    const auto scaled_hurt = checked_multiply(100, hurt);
    const auto threshold = checked_multiply(percentage, denominator);
    if (!scaled_hurt.has_value() || !threshold.has_value()) {
        return std::nullopt;
    }
    return *scaled_hurt <=> *threshold;
}

std::optional<HurtBand> hurt_band(
    const std::int64_t hurt, const std::int64_t denominator) noexcept {
    const auto lower = compare_hurt_percentage(hurt, denominator, 33);
    const auto upper = compare_hurt_percentage(hurt, denominator, 66);
    if (!lower.has_value() || !upper.has_value()) {
        return std::nullopt;
    }
    return *upper > 0 ? HurtBand::severe : *lower > 0 ? HurtBand::moderate : HurtBand::low;
}

}
