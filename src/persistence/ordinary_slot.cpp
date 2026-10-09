#include "openlegend/persistence/ordinary_slot.hpp"

#include <system_error>
#include <utility>

namespace openlegend::persistence {

OrdinarySlotSelection select_ordinary_slot(
    const std::filesystem::path& root, const SaveSlot slot, const bool enabled) {
    OrdinarySlotSelection result;
    const auto legacy = numbered_file_set(root, slot);
    if (!legacy) {
        result.status = PersistenceStatus::invalid_slot;
        result.path = root;
        return result;
    }
    result.path = legacy->ranger_group;
    if (!enabled) {
        return result;
    }
    const auto toml = toml_slot_path(root, TomlSaveKind::ordinary, slot);
    std::error_code error;
    const auto status = std::filesystem::symlink_status(*toml, error);
    if (error == std::errc::no_such_file_or_directory || (!error && !std::filesystem::exists(status))) {
        return result;
    }
    result.format = OrdinarySaveFormat::toml;
    result.path = *toml;
    if (error) {
        result.status = PersistenceStatus::read_failed;
        result.detail = error.message();
    }
    return result;
}

OrdinarySlotLoadResult load_ordinary_slot(
    const std::filesystem::path& root, const SaveSlot slot, const OrdinarySlotContext& context) {
    auto selected = select_ordinary_slot(root, slot, context.configuration.enabled);
    OrdinarySlotLoadResult result;
    result.status = selected.status;
    result.format = selected.format;
    result.path = std::move(selected.path);
    result.detail = std::move(selected.detail);
    if (!selected) {
        return result;
    }
    if (result.format == OrdinarySaveFormat::toml) {
        if (context.fingerprints == nullptr) {
            result.status = PersistenceStatus::incompatible_assets;
            result.detail = "save asset fingerprints are unavailable";
            return result;
        }
        const TomlSnapshotContext toml_context{
            context.baseline, *context.fingerprints, context.configuration};
        auto loaded = load_toml_slot(root, TomlSaveKind::ordinary, slot, toml_context);
        if (!loaded) {
            result.status = loaded.snapshot_status == TomlSaveStatus::incompatible_assets
                ? PersistenceStatus::incompatible_assets : PersistenceStatus::invalid_toml;
            result.detail = std::move(loaded.detail);
            return result;
        }
        result.metadata = std::move(loaded.save->metadata);
        result.snapshot = std::move(loaded.save->snapshot);
        return result;
    }
    auto loaded = load_numbered_slot(root, slot, context.ranger_index);
    if (!loaded) {
        result.status = loaded.status;
        result.path = std::move(loaded.path);
        result.detail = std::move(loaded.detail);
        return result;
    }
    result.snapshot = model::decode_legacy_snapshot(std::move(*loaded.snapshot), context.configuration,
        context.configuration.enabled ? &context.baseline : nullptr);
    if (!result.snapshot) {
        result.status = PersistenceStatus::invalid_snapshot;
        result.detail = "Legacy save is incompatible with current game rules";
    }
    return result;
}

SnapshotWriteResult write_ordinary_slot(
    const std::filesystem::path& root, const SaveSlot slot, const model::RuntimeGameSnapshot& snapshot,
    const OrdinarySlotContext& context, std::string timestamp_utc) {
    if (!context.configuration.enabled) {
        return write_numbered_slot(root, slot, snapshot);
    }
    SnapshotWriteResult result;
    if (context.fingerprints == nullptr) {
        result.status = PersistenceStatus::incompatible_assets;
        result.path = root;
        result.detail = "save asset fingerprints are unavailable";
        return result;
    }
    const TomlSnapshotContext toml_context{
        context.baseline, *context.fingerprints, context.configuration};
    auto written = write_toml_slot(root, snapshot,
        {TomlSaveKind::ordinary, slot, std::move(timestamp_utc)}, toml_context);
    result.path = std::move(written.path);
    result.detail = std::move(written.detail);
    if (!written) {
        result.status = written.file_status == SaveFileStatus::rollback_failed ? PersistenceStatus::rollback_failed
            : written.snapshot_status == TomlSaveStatus::incompatible_assets ? PersistenceStatus::incompatible_assets
            : written.snapshot_status != TomlSaveStatus::ready ? PersistenceStatus::invalid_snapshot
            : PersistenceStatus::write_failed;
        if (!written.recovery_path.empty()) {
            result.recovery_paths.push_back(std::move(written.recovery_path));
        }
    }
    return result;
}

SnapshotWriteResult delete_ordinary_slot(
    const std::filesystem::path& root, const SaveSlot slot, const bool enabled) {
    auto selected = select_ordinary_slot(root, slot, enabled);
    if (!selected) {
        SnapshotWriteResult result;
        result.status = selected.status;
        result.path = std::move(selected.path);
        result.detail = std::move(selected.detail);
        return result;
    }
    if (selected.format == OrdinarySaveFormat::legacy) {
        return delete_numbered_slot(root, slot);
    }
    SnapshotWriteResult result;
    result.path = std::move(selected.path);
    std::error_code error;
    static_cast<void>(std::filesystem::remove(result.path, error));
    if (error) {
        result.status = PersistenceStatus::delete_failed;
        result.detail = error.message();
    }
    return result;
}

}
