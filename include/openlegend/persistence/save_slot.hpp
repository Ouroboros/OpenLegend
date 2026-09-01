#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "openlegend/model/game_snapshot.hpp"

namespace openlegend::persistence {

enum class SaveSlot : unsigned int {
    one = 0U,
    two = 1U,
    three = 2U,
};

enum class SaveFileSetKind {
    baseline,
    working_copy,
    numbered_slot,
};

struct SaveFileSet {
    SaveFileSetKind kind{SaveFileSetKind::baseline};
    std::optional<SaveSlot> slot;
    std::filesystem::path ranger_index;
    std::filesystem::path ranger_group;
    std::filesystem::path scene_map_index;
    std::filesystem::path scene_map_group;
    std::filesystem::path scene_event_index;
    std::filesystem::path scene_event_group;
};

[[nodiscard]] SaveFileSet baseline_file_set(const std::filesystem::path& root);
[[nodiscard]] SaveFileSet working_copy_file_set(const std::filesystem::path& root);
[[nodiscard]] std::optional<SaveFileSet> numbered_file_set(
    const std::filesystem::path& root, SaveSlot slot);

enum class PersistenceStatus {
    ready,
    invalid_slot,
    read_failed,
    invalid_ranger_index_size,
    invalid_ranger_index_order,
    invalid_ranger_layout,
    invalid_ranger_group_size,
    invalid_scene_index_size,
    invalid_scene_index_layout,
    invalid_scene_group_size,
    invalid_snapshot,
    write_failed,
};

struct SnapshotLoadResult {
    PersistenceStatus status{PersistenceStatus::ready};
    std::optional<model::GameSnapshot> snapshot;
    std::filesystem::path path;
    std::string detail;

    [[nodiscard]] explicit operator bool() const noexcept {
        return status == PersistenceStatus::ready && snapshot.has_value();
    }
};

struct SnapshotWriteResult {
    PersistenceStatus status{PersistenceStatus::ready};
    std::filesystem::path path;
    std::string detail;

    [[nodiscard]] explicit operator bool() const noexcept {
        return status == PersistenceStatus::ready;
    }
};

[[nodiscard]] SnapshotLoadResult load_snapshot(const SaveFileSet& files);
[[nodiscard]] SnapshotLoadResult load_baseline(const std::filesystem::path& root);
[[nodiscard]] SnapshotLoadResult load_working_copy(const std::filesystem::path& root);
[[nodiscard]] SnapshotLoadResult load_numbered_slot(
    const std::filesystem::path& root, SaveSlot slot);

[[nodiscard]] SnapshotWriteResult write_snapshot(
    const SaveFileSet& files, const model::GameSnapshot& snapshot);
[[nodiscard]] SnapshotWriteResult write_numbered_slot_scene_archives(
    const std::filesystem::path& root, SaveSlot slot, const model::GameSnapshot& snapshot);
[[nodiscard]] SnapshotWriteResult write_numbered_slot_ranger(
    const std::filesystem::path& root, SaveSlot slot, const model::GameSnapshot& snapshot);
[[nodiscard]] SnapshotWriteResult write_numbered_slot(
    const std::filesystem::path& root, SaveSlot slot, const model::GameSnapshot& snapshot);

[[nodiscard]] std::string_view persistence_status_message(PersistenceStatus status) noexcept;

}  // namespace openlegend::persistence
