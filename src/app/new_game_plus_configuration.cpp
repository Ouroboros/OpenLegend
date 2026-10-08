#include "new_game_plus_configuration.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "openlegend/attributes.hpp"

namespace openlegend::app {
namespace {

struct IntegerField {
    std::string_view name;
    std::int64_t model::NewGamePlusConfiguration::* value;
};

constexpr auto integer_fields = std::to_array<IntegerField>({
    {"maximum_playthroughs", &model::NewGamePlusConfiguration::maximum_playthroughs},
    {"hurt_cap_step", &model::NewGamePlusConfiguration::hurt_cap_step},
    {"hp_cap_step", &model::NewGamePlusConfiguration::hp_cap_step},
    {"mp_cap_step", &model::NewGamePlusConfiguration::mp_cap_step},
    {"attack_cap_step", &model::NewGamePlusConfiguration::attack_cap_step},
    {"defence_cap_step", &model::NewGamePlusConfiguration::defence_cap_step},
    {"use_poison_cap_step", &model::NewGamePlusConfiguration::use_poison_cap_step},
    {"anti_poison_cap_step", &model::NewGamePlusConfiguration::anti_poison_cap_step},
    {"hidden_weapon_cap_step", &model::NewGamePlusConfiguration::hidden_weapon_cap_step},
    {"role_level_step", &model::NewGamePlusConfiguration::role_level_step},
    {"martial_level_step", &model::NewGamePlusConfiguration::martial_level_step},
    {"battle_experience_percent_ng2", &model::NewGamePlusConfiguration::battle_experience_percent_ng2},
    {"battle_experience_percent_step", &model::NewGamePlusConfiguration::battle_experience_percent_step},
});

NODISCARD NewGamePlusConfigurationLoadResult configuration_error(
    const NewGamePlusConfigurationStatus status, const std::string_view field) {
    NewGamePlusConfigurationLoadResult result;
    result.status = status;
    result.detail = std::string{NewGamePlusConfigurationLoadResult::toml_table_name};
    if (!field.empty()) {
        result.detail += '.';
        result.detail += field;
    }
    return result;
}

NODISCARD bool valid_derived_limits(const model::NewGamePlusConfiguration& values) noexcept {
    if (!model::calculate_playthrough_limits(values, 1).has_value() ||
        !model::calculate_playthrough_limits(values, values.maximum_playthroughs).has_value()) {
        return false;
    }
    return values.maximum_playthroughs < 2 ||
        model::calculate_playthrough_limits(values, 2).has_value();
}

}

NewGamePlusConfigurationLoadResult detail::new_game_plus_configuration_from_document(
    const toml::table& document) {
    const auto* node = document.get(NewGamePlusConfigurationLoadResult::toml_table_name);
    if (node == nullptr) {
        return {};
    }
    const auto* table = node->as_table();
    if (table == nullptr) {
        return configuration_error(NewGamePlusConfigurationStatus::invalid_table, {});
    }
    for (const auto& [key, value] : *table) {
        static_cast<void>(value);
        const auto& known = NewGamePlusConfigurationLoadResult::toml_field_order;
        if (std::find(known.begin(), known.end(), key.str()) == known.end()) {
            return configuration_error(NewGamePlusConfigurationStatus::unknown_key, key.str());
        }
    }
    model::NewGamePlusConfiguration values;
    if (const auto* enabled = table->get("enabled")) {
        const auto* boolean = enabled->as_boolean();
        if (boolean == nullptr) {
            return configuration_error(NewGamePlusConfigurationStatus::invalid_value, "enabled");
        }
        values.enabled = boolean->get();
    }
    for (const auto& field : integer_fields) {
        const auto* value = table->get(field.name);
        if (value == nullptr) {
            continue;
        }
        const auto* integer = value->as_integer();
        if (integer == nullptr) {
            return configuration_error(NewGamePlusConfigurationStatus::invalid_value, field.name);
        }
        values.*field.value = integer->get();
    }
    if (values.maximum_playthroughs < 1) {
        return configuration_error(NewGamePlusConfigurationStatus::invalid_value, "maximum_playthroughs");
    }
    if (!valid_derived_limits(values)) {
        return configuration_error(NewGamePlusConfigurationStatus::invalid_limits, {});
    }
    NewGamePlusConfigurationLoadResult result;
    result.values = values;
    result.loaded_from_file = true;
    return result;
}

std::string_view new_game_plus_configuration_status_message(
    const NewGamePlusConfigurationStatus status) noexcept {
    switch (status) {
    case NewGamePlusConfigurationStatus::ready:
        return "ready";
    case NewGamePlusConfigurationStatus::read_failed:
        return "cannot read NG+ configuration";
    case NewGamePlusConfigurationStatus::parse_failed:
        return "cannot parse NG+ configuration";
    case NewGamePlusConfigurationStatus::invalid_table:
        return "new_game_plus must be a table";
    case NewGamePlusConfigurationStatus::invalid_value:
        return "invalid NG+ configuration value";
    case NewGamePlusConfigurationStatus::invalid_limits:
        return "NG+ configuration produces invalid or overflowing limits";
    case NewGamePlusConfigurationStatus::unknown_key:
        return "unknown NG+ configuration key";
    }
    return "unknown NG+ configuration status";
}

}
