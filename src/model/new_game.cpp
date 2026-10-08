#include "openlegend/model/new_game.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "openlegend/attributes.hpp"
#include "openlegend/model/checked_arithmetic.hpp"
#include "openlegend/text/big5.hpp"

namespace openlegend::model {
namespace {

constexpr std::array<std::size_t, 12> kRolledAbilityWords{
    role_word::attack,
    role_word::speed,
    role_word::defence,
    role_word::medicine,
    role_word::use_poison,
    role_word::detoxification,
    role_word::anti_poison,
    role_word::fist,
    role_word::sword,
    role_word::knife,
    role_word::unusual,
    role_word::hidden_weapon,
};

void synchronize_current_vitals(RoleRecord& protagonist) noexcept {
    protagonist.set_word(role_word::hp, protagonist.word(role_word::maximum_hp));
    protagonist.set_word(role_word::mp, protagonist.word(role_word::maximum_mp));
}

}  // namespace

bool set_protagonist_name(
    RangerState& ranger, const std::span<const std::uint8_t> legacy_name) noexcept {
    if (ranger.roles.empty() || legacy_name.empty() ||
        legacy_name.size() > kNewGameNameMaximumBytes ||
        std::ranges::find(legacy_name, std::uint8_t{0U}) != legacy_name.end()) {
        return false;
    }

    auto& bytes = ranger.roles[0].bytes;
    auto destination = std::span<std::uint8_t>{bytes}.subspan(
        role_word::name_byte, role_word::name_bytes);
    std::ranges::fill(destination, std::uint8_t{0U});
    std::ranges::copy(legacy_name, destination.begin());
    return true;
}

void roll_protagonist_attributes(
    RoleRecord& protagonist, random::LegacyRandom& random) noexcept {
    protagonist.set_word(
        role_word::mp_type, static_cast<std::int16_t>(random.bounded(2)));
    protagonist.set_word(
        role_word::maximum_mp,
        static_cast<std::int16_t>(random.bounded(20) + 21));
    for (const auto word : kRolledAbilityWords) {
        protagonist.set_word(word, static_cast<std::int16_t>(random.bounded(10) + 21));
    }

    const auto increased_life = static_cast<std::int16_t>(random.bounded(5) + 3);
    protagonist.set_word(role_word::increased_life, increased_life);
    const auto level = protagonist.word(role_word::level);
    const auto maximum_hp = static_cast<std::int16_t>(
        static_cast<std::int32_t>(increased_life) * 3 *
            static_cast<std::int32_t>(level) +
        29);
    protagonist.set_word(role_word::maximum_hp, maximum_hp);

    const auto bucket = random.bounded(10);
    std::int32_t iq{};
    if (bucket <= 1) {
        iq = random.bounded(35) + 30;
    } else if (bucket <= 7) {
        iq = random.bounded(20) + 60;
    } else {
        iq = random.bounded(20) + 75;
    }
    protagonist.set_word(role_word::iq, static_cast<std::int16_t>(iq));
    synchronize_current_vitals(protagonist);
}

void apply_baberuth_attributes(RoleRecord& protagonist) noexcept {
    protagonist.set_word(role_word::mp_type, 2);
    protagonist.set_word(role_word::maximum_mp, 40);
    for (const auto word : kRolledAbilityWords) {
        protagonist.set_word(word, 30);
    }
    protagonist.set_word(role_word::increased_life, 10);
    protagonist.set_word(role_word::maximum_hp, 50);
    protagonist.set_word(role_word::iq, 100);
    synchronize_current_vitals(protagonist);
}

bool set_protagonist_name(
    RuntimeRangerState& ranger, const std::span<const std::uint8_t> legacy_name) {
    if (ranger.roles.empty() || legacy_name.empty() ||
        legacy_name.size() > kNewGameNameMaximumBytes) {
        return false;
    }
    auto decoded = text::decode_big5(text::Big5TextView{legacy_name});
    if (!decoded.has_value()) {
        return false;
    }
    ranger.roles[0].name = std::move(*decoded);
    return true;
}

void roll_protagonist_attributes(RoleState& protagonist, random::LegacyRandom& random) {
    auto rolled = protagonist;
    auto rolled_random = random;
    rolled.mp_type = rolled_random.bounded(2);
    rolled.maximum_mp = rolled_random.bounded(20) + 21;
    for (const auto index : kRolledAbilityWords) {
        rolled.set_word(index, rolled_random.bounded(10) + 21);
    }
    rolled.increased_life = rolled_random.bounded(5) + 3;
    const auto growth = checked_multiply(rolled.increased_life, 3);
    const auto total = growth.has_value() ? checked_multiply(*growth, rolled.level) : std::nullopt;
    const auto hp = total.has_value() ? checked_add(*total, 29) : std::nullopt;
    if (!hp.has_value()) {
        throw std::overflow_error("protagonist maximum HP");
    }
    rolled.maximum_hp = *hp;
    const auto bucket = rolled_random.bounded(10);
    if (bucket <= 1) {
        rolled.iq = rolled_random.bounded(35) + 30;
    } else if (bucket <= 7) {
        rolled.iq = rolled_random.bounded(20) + 60;
    } else {
        rolled.iq = rolled_random.bounded(20) + 75;
    }
    rolled.hp = rolled.maximum_hp;
    rolled.mp = rolled.maximum_mp;
    protagonist = std::move(rolled);
    random = rolled_random;
}

void apply_baberuth_attributes(RoleState& protagonist) {
    protagonist.mp_type = 2;
    protagonist.maximum_mp = 40;
    for (const auto index : kRolledAbilityWords) {
        protagonist.set_word(index, 30);
    }
    protagonist.increased_life = 10;
    protagonist.maximum_hp = 50;
    protagonist.iq = 100;
    protagonist.hp = protagonist.maximum_hp;
    protagonist.mp = protagonist.maximum_mp;
}

}  // namespace openlegend::model
