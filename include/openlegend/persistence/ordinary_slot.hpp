#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/toml_slot.hpp"

namespace openlegend::persistence {

enum class OrdinarySaveFormat { legacy, toml };

struct OrdinarySlotContext {
    const model::RangerState& baseline;
    std::span<const std::uint8_t> ranger_index;
    const model::NewGamePlusConfiguration& configuration;
    const AssetFingerprints* fingerprints{};
};

struct OrdinarySlotSelection {
    PersistenceStatus status{PersistenceStatus::ready};
    OrdinarySaveFormat format{OrdinarySaveFormat::legacy};
    std::filesystem::path path;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return status == PersistenceStatus::ready;
    }
};

struct OrdinarySlotLoadResult {
    PersistenceStatus status{PersistenceStatus::ready};
    OrdinarySaveFormat format{OrdinarySaveFormat::legacy};
    std::optional<model::RuntimeGameSnapshot> snapshot;
    std::optional<TomlSaveMetadata> metadata;
    std::filesystem::path path;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return status == PersistenceStatus::ready && snapshot.has_value();
    }
};

NODISCARD OrdinarySlotSelection select_ordinary_slot(
    const std::filesystem::path& root, SaveSlot slot, bool enabled);

NODISCARD OrdinarySlotLoadResult load_ordinary_slot(
    const std::filesystem::path& root, SaveSlot slot, const OrdinarySlotContext& context);

NODISCARD SnapshotWriteResult write_ordinary_slot(
    const std::filesystem::path& root, SaveSlot slot, const model::RuntimeGameSnapshot& snapshot,
    const OrdinarySlotContext& context, std::string timestamp_utc);

NODISCARD SnapshotWriteResult delete_ordinary_slot(
    const std::filesystem::path& root, SaveSlot slot, bool enabled);

}
