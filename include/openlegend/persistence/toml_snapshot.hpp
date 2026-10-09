#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/asset_fingerprints.hpp"
#include "openlegend/persistence/save_slot.hpp"

namespace openlegend::persistence {

inline constexpr std::size_t kMaximumTomlSaveBytes = 64U * 1024U * 1024U;

enum class TomlSaveKind {
    ordinary,
    completion,
};

enum class TomlSaveStatus {
    ready,
    invalid_document,
    invalid_metadata,
    incompatible_assets,
    invalid_snapshot,
};

struct TomlSaveMetadata {
    TomlSaveKind kind{TomlSaveKind::ordinary};
    SaveSlot slot{SaveSlot::one};
    std::string timestamp_utc;

    NODISCARD bool operator==(const TomlSaveMetadata&) const = default;
};

struct TomlSnapshotContext {
    const model::RangerState& baseline;
    const AssetFingerprints& fingerprints;
    const model::NewGamePlusConfiguration& configuration;
};

struct TomlSave {
    TomlSaveMetadata metadata;
    model::RuntimeGameSnapshot snapshot;
};

struct TomlSnapshotReadResult {
    TomlSaveStatus status{TomlSaveStatus::ready};
    std::optional<TomlSave> save;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return status == TomlSaveStatus::ready && save.has_value();
    }
};

struct TomlSnapshotWriteResult {
    TomlSaveStatus status{TomlSaveStatus::ready};
    std::optional<std::string> document;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return status == TomlSaveStatus::ready && document.has_value();
    }
};

NODISCARD TomlSnapshotReadResult decode_toml_snapshot(
    std::string_view document, TomlSaveKind kind, SaveSlot slot,
    const TomlSnapshotContext& context);

NODISCARD TomlSnapshotWriteResult encode_toml_snapshot(
    const model::RuntimeGameSnapshot& snapshot, const TomlSaveMetadata& metadata,
    const TomlSnapshotContext& context);

}
