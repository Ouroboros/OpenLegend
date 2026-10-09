#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/save_file.hpp"
#include "openlegend/persistence/toml_snapshot.hpp"

namespace openlegend::persistence {

struct TomlSlotLoadResult {
    SaveFileStatus file_status{SaveFileStatus::ready};
    TomlSaveStatus snapshot_status{TomlSaveStatus::ready};
    std::optional<TomlSave> save;
    std::filesystem::path path;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return file_status == SaveFileStatus::ready && snapshot_status == TomlSaveStatus::ready && save.has_value();
    }
};

struct TomlSlotWriteResult {
    SaveFileStatus file_status{SaveFileStatus::ready};
    TomlSaveStatus snapshot_status{TomlSaveStatus::ready};
    std::filesystem::path path;
    std::filesystem::path recovery_path;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return file_status == SaveFileStatus::ready && snapshot_status == TomlSaveStatus::ready;
    }
};

NODISCARD std::optional<std::filesystem::path> toml_slot_path(
    const std::filesystem::path& root, TomlSaveKind kind, SaveSlot slot);

NODISCARD TomlSlotLoadResult load_toml_slot(
    const std::filesystem::path& root, TomlSaveKind kind, SaveSlot slot,
    const TomlSnapshotContext& context);

NODISCARD TomlSlotWriteResult write_toml_slot(
    const std::filesystem::path& root, const model::RuntimeGameSnapshot& snapshot,
    const TomlSaveMetadata& metadata, const TomlSnapshotContext& context);

}
