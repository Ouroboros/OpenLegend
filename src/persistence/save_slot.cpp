#include "openlegend/persistence/save_slot.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::persistence {

namespace {

struct LoadedBytes {
    PersistenceStatus status{PersistenceStatus::ready};
    std::vector<std::uint8_t> bytes;
    std::filesystem::path path;
    std::string detail;
};

[[nodiscard]] LoadedBytes read_required(const std::filesystem::path& path) {
    const auto file = resource::read_binary_file(path);
    if (!file) {
        return LoadedBytes{PersistenceStatus::read_failed, {}, path, file.error};
    }
    return LoadedBytes{PersistenceStatus::ready, file.bytes, path, {}};
}

[[nodiscard]] SnapshotLoadResult load_error(
    const PersistenceStatus status, std::filesystem::path path, std::string detail = {}) {
    SnapshotLoadResult result;
    result.status = status;
    result.path = std::move(path);
    result.detail = std::move(detail);
    return result;
}

[[nodiscard]] SnapshotWriteResult write_error(
    const PersistenceStatus status, std::filesystem::path path, std::string detail = {}) {
    SnapshotWriteResult result;
    result.status = status;
    result.path = std::move(path);
    result.detail = std::move(detail);
    return result;
}

[[nodiscard]] std::vector<std::uint8_t> encode_index(
    const std::span<const std::uint32_t> ends) {
    std::vector<std::uint8_t> bytes(ends.size() * 4U);
    for (std::size_t index = 0U; index < ends.size(); ++index) {
        const std::uint32_t value = ends[index];
        bytes[index * 4U] = static_cast<std::uint8_t>(value & 0xFFU);
        bytes[index * 4U + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
        bytes[index * 4U + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
        bytes[index * 4U + 3U] = static_cast<std::uint8_t>(value >> 24U);
    }
    return bytes;
}

[[nodiscard]] bool decode_index(
    const std::span<const std::uint8_t> bytes,
    const std::span<std::uint32_t> ends) noexcept {
    if (bytes.size() != ends.size() * 4U) {
        return false;
    }
    for (std::size_t index = 0U; index < ends.size(); ++index) {
        ends[index] = compat::read_u32le(bytes, index * 4U);
    }
    return true;
}

[[nodiscard]] bool monotonic_nonzero(const std::span<const std::uint32_t> ends) noexcept {
    std::uint32_t previous = 0U;
    for (const std::uint32_t end : ends) {
        if (end <= previous) {
            return false;
        }
        previous = end;
    }
    return true;
}

template <typename Record>
void decode_records(
    const std::span<const std::uint8_t> bytes,
    const std::size_t offset,
    std::vector<Record>& records) {
    for (std::size_t index = 0U; index < records.size(); ++index) {
        const auto begin =
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + index * Record::byte_count);
        std::copy_n(begin, Record::byte_count, records[index].bytes.begin());
    }
}

template <typename Record>
void encode_records(
    const std::vector<Record>& records, std::vector<std::uint8_t>& bytes) {
    for (const auto& record : records) {
        bytes.insert(bytes.end(), record.bytes.begin(), record.bytes.end());
    }
}

[[nodiscard]] SnapshotLoadResult decode_ranger(
    const LoadedBytes& index_file,
    const LoadedBytes& group_file,
    model::GameSnapshot& snapshot) {
    if (index_file.bytes.size() != model::kRangerCumulativeEnds.size() * 4U) {
        return load_error(
            PersistenceStatus::invalid_ranger_index_size, index_file.path);
    }
    std::array<std::uint32_t, 6> ends{};
    static_cast<void>(decode_index(index_file.bytes, ends));
    if (!monotonic_nonzero(ends)) {
        return load_error(
            PersistenceStatus::invalid_ranger_index_order, index_file.path);
    }
    if (ends != model::kRangerCumulativeEnds) {
        return load_error(PersistenceStatus::invalid_ranger_layout, index_file.path);
    }
    if (group_file.bytes.size() != ends.back()) {
        return load_error(
            PersistenceStatus::invalid_ranger_group_size, group_file.path);
    }

    const std::span<const std::uint8_t> bytes{group_file.bytes};
    std::copy_n(bytes.begin(), model::kRangerHeaderBytes, snapshot.ranger.header.bytes.begin());
    decode_records(bytes, 0U + model::kRangerCumulativeEnds[0], snapshot.ranger.roles);
    decode_records(bytes, model::kRangerCumulativeEnds[1], snapshot.ranger.items);
    decode_records(bytes, model::kRangerCumulativeEnds[2], snapshot.ranger.scenes);
    decode_records(bytes, model::kRangerCumulativeEnds[3], snapshot.ranger.magics);
    decode_records(bytes, model::kRangerCumulativeEnds[4], snapshot.ranger.shops);
    return SnapshotLoadResult{};
}

[[nodiscard]] SnapshotLoadResult decode_scene_archive(
    const LoadedBytes& index_file,
    const LoadedBytes& group_file,
    const std::size_t bytes_per_scene,
    std::array<std::uint32_t, model::kSceneCount>& ends,
    std::vector<std::uint8_t>& output) {
    if (index_file.bytes.size() != model::kSceneCount * 4U) {
        return load_error(PersistenceStatus::invalid_scene_index_size, index_file.path);
    }
    static_cast<void>(decode_index(index_file.bytes, ends));
    if (!monotonic_nonzero(ends)) {
        return load_error(PersistenceStatus::invalid_scene_index_layout, index_file.path);
    }
    for (std::size_t index = 0U; index < ends.size(); ++index) {
        if (ends[index] != static_cast<std::uint32_t>((index + 1U) * bytes_per_scene)) {
            return load_error(PersistenceStatus::invalid_scene_index_layout, index_file.path);
        }
    }
    if (group_file.bytes.size() != ends.back()) {
        return load_error(PersistenceStatus::invalid_scene_group_size, group_file.path);
    }
    output = group_file.bytes;
    return SnapshotLoadResult{};
}

[[nodiscard]] SnapshotLoadResult decode_fixed_scene_group(
    const LoadedBytes& group_file,
    const std::size_t bytes_per_scene,
    std::array<std::uint32_t, model::kSceneCount>& ends,
    std::vector<std::uint8_t>& output) {
    const std::size_t expected_size = bytes_per_scene * model::kSceneCount;
    if (group_file.bytes.size() != expected_size) {
        return load_error(PersistenceStatus::invalid_scene_group_size, group_file.path);
    }
    for (std::size_t index = 0U; index < ends.size(); ++index) {
        ends[index] = static_cast<std::uint32_t>((index + 1U) * bytes_per_scene);
    }
    output = group_file.bytes;
    return SnapshotLoadResult{};
}

[[nodiscard]] std::vector<std::uint8_t> encode_ranger(const model::RangerState& ranger) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(model::kRangerCumulativeEnds.back());
    bytes.insert(bytes.end(), ranger.header.bytes.begin(), ranger.header.bytes.end());
    encode_records(ranger.roles, bytes);
    encode_records(ranger.items, bytes);
    encode_records(ranger.scenes, bytes);
    encode_records(ranger.magics, bytes);
    encode_records(ranger.shops, bytes);
    return bytes;
}

[[nodiscard]] SnapshotWriteResult write_bytes(
    const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output) {
        return write_error(PersistenceStatus::write_failed, path);
    }
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!output) {
        return write_error(PersistenceStatus::write_failed, path);
    }
    return SnapshotWriteResult{};
}

[[nodiscard]] std::optional<unsigned int> slot_number(const SaveSlot slot) noexcept {
    switch (slot) {
    case SaveSlot::one:
        return 1U;
    case SaveSlot::two:
        return 2U;
    case SaveSlot::three:
        return 3U;
    }
    return std::nullopt;
}

[[nodiscard]] SaveFileSet make_file_set(
    const std::filesystem::path& root,
    const SaveFileSetKind kind,
    std::optional<SaveSlot> slot,
    const std::string& ranger,
    const std::string& scene_map,
    const std::string& scene_event) {
    return SaveFileSet{
        kind,
        slot,
        root / (ranger + ".IDX"),
        root / (ranger + ".GRP"),
        root / (scene_map + ".IDX"),
        root / (scene_map + ".GRP"),
        root / (scene_event + ".IDX"),
        root / (scene_event + ".GRP")};
}

}  // namespace

SaveFileSet baseline_file_set(const std::filesystem::path& root) {
    return make_file_set(
        root, SaveFileSetKind::baseline, std::nullopt, "RANGER", "ALLSIN", "ALLDEF");
}

SaveFileSet working_copy_file_set(const std::filesystem::path& root) {
    return make_file_set(
        root,
        SaveFileSetKind::working_copy,
        std::nullopt,
        "RANGER",
        "ALLSINBK",
        "ALLDEFBK");
}

std::optional<SaveFileSet> numbered_file_set(
    const std::filesystem::path& root, const SaveSlot slot) {
    const auto number = slot_number(slot);
    if (!number.has_value()) {
        return std::nullopt;
    }
    const std::string suffix = std::to_string(*number);
    return make_file_set(
        root,
        SaveFileSetKind::numbered_slot,
        slot,
        "R" + suffix,
        "S" + suffix,
        "D" + suffix);
}

SnapshotLoadResult load_snapshot(const SaveFileSet& files) {
    const auto ranger_index = read_required(files.ranger_index);
    if (ranger_index.status != PersistenceStatus::ready) {
        return load_error(ranger_index.status, ranger_index.path, ranger_index.detail);
    }
    const auto ranger_group = read_required(files.ranger_group);
    if (ranger_group.status != PersistenceStatus::ready) {
        return load_error(ranger_group.status, ranger_group.path, ranger_group.detail);
    }
    const auto scene_map_index = read_required(files.scene_map_index);
    if (scene_map_index.status != PersistenceStatus::ready) {
        return load_error(scene_map_index.status, scene_map_index.path, scene_map_index.detail);
    }
    const auto scene_map_group = read_required(files.scene_map_group);
    if (scene_map_group.status != PersistenceStatus::ready) {
        return load_error(scene_map_group.status, scene_map_group.path, scene_map_group.detail);
    }
    const auto scene_event_index = read_required(files.scene_event_index);
    if (scene_event_index.status != PersistenceStatus::ready) {
        return load_error(scene_event_index.status, scene_event_index.path, scene_event_index.detail);
    }
    const auto scene_event_group = read_required(files.scene_event_group);
    if (scene_event_group.status != PersistenceStatus::ready) {
        return load_error(scene_event_group.status, scene_event_group.path, scene_event_group.detail);
    }

    model::GameSnapshot snapshot;
    auto result = decode_ranger(ranger_index, ranger_group, snapshot);
    if (result.status != PersistenceStatus::ready) {
        return result;
    }
    result = decode_scene_archive(
        scene_map_index,
        scene_map_group,
        model::kSceneMapBytesPerScene,
        snapshot.scene_map_ends,
        snapshot.scene_maps);
    if (result.status != PersistenceStatus::ready) {
        return result;
    }
    result = decode_scene_archive(
        scene_event_index,
        scene_event_group,
        model::kSceneEventBytesPerScene,
        snapshot.scene_event_ends,
        snapshot.scene_events);
    if (result.status != PersistenceStatus::ready) {
        return result;
    }
    if (!snapshot.valid()) {
        return load_error(PersistenceStatus::invalid_snapshot, files.ranger_group);
    }

    SnapshotLoadResult loaded;
    loaded.snapshot = std::move(snapshot);
    return loaded;
}

SnapshotLoadResult load_baseline(const std::filesystem::path& root) {
    return load_snapshot(baseline_file_set(root));
}

SnapshotLoadResult load_working_copy(const std::filesystem::path& root) {
    return load_snapshot(working_copy_file_set(root));
}

SnapshotLoadResult load_numbered_slot(
    const std::filesystem::path& root, const SaveSlot slot) {
    const auto files = numbered_file_set(root, slot);
    if (!files.has_value()) {
        return load_error(PersistenceStatus::invalid_slot, root);
    }

    const auto scene_map_group = read_required(files->scene_map_group);
    if (scene_map_group.status != PersistenceStatus::ready) {
        return load_error(scene_map_group.status, scene_map_group.path, scene_map_group.detail);
    }
    const auto scene_event_group = read_required(files->scene_event_group);
    if (scene_event_group.status != PersistenceStatus::ready) {
        return load_error(scene_event_group.status, scene_event_group.path, scene_event_group.detail);
    }
    const auto ranger_index = read_required(root / "RANGER.IDX");
    if (ranger_index.status != PersistenceStatus::ready) {
        return load_error(ranger_index.status, ranger_index.path, ranger_index.detail);
    }
    const auto ranger_group = read_required(files->ranger_group);
    if (ranger_group.status != PersistenceStatus::ready) {
        return load_error(ranger_group.status, ranger_group.path, ranger_group.detail);
    }

    model::GameSnapshot snapshot;
    auto result = decode_fixed_scene_group(
        scene_map_group,
        model::kSceneMapBytesPerScene,
        snapshot.scene_map_ends,
        snapshot.scene_maps);
    if (result.status != PersistenceStatus::ready) {
        return result;
    }
    result = decode_fixed_scene_group(
        scene_event_group,
        model::kSceneEventBytesPerScene,
        snapshot.scene_event_ends,
        snapshot.scene_events);
    if (result.status != PersistenceStatus::ready) {
        return result;
    }
    result = decode_ranger(ranger_index, ranger_group, snapshot);
    if (result.status != PersistenceStatus::ready) {
        return result;
    }

    SnapshotLoadResult loaded;
    loaded.snapshot = std::move(snapshot);
    return loaded;
}

SnapshotWriteResult write_snapshot(
    const SaveFileSet& files, const model::GameSnapshot& snapshot) {
    if (!snapshot.valid()) {
        return write_error(PersistenceStatus::invalid_snapshot, files.ranger_group);
    }

    const auto scene_map_index = encode_index(snapshot.scene_map_ends);
    auto result = write_bytes(files.scene_map_index, scene_map_index);
    if (!result) {
        return result;
    }
    result = write_bytes(files.scene_map_group, snapshot.scene_maps);
    if (!result) {
        return result;
    }

    const auto scene_event_index = encode_index(snapshot.scene_event_ends);
    result = write_bytes(files.scene_event_index, scene_event_index);
    if (!result) {
        return result;
    }
    result = write_bytes(files.scene_event_group, snapshot.scene_events);
    if (!result) {
        return result;
    }

    const auto ranger_index = encode_index(model::kRangerCumulativeEnds);
    result = write_bytes(files.ranger_index, ranger_index);
    if (!result) {
        return result;
    }
    const auto ranger_group = encode_ranger(snapshot.ranger);
    result = write_bytes(files.ranger_group, ranger_group);
    if (!result) {
        return result;
    }
    return SnapshotWriteResult{};
}

SnapshotWriteResult write_numbered_slot_scene_archives(
    const std::filesystem::path& root,
    const SaveSlot slot,
    const model::GameSnapshot& snapshot) {
    const auto files = numbered_file_set(root, slot);
    if (!files.has_value()) {
        return write_error(PersistenceStatus::invalid_slot, root);
    }
    if (!snapshot.valid()) {
        return write_error(PersistenceStatus::invalid_snapshot, files->scene_map_group);
    }

    auto result = write_bytes(files->scene_map_group, snapshot.scene_maps);
    if (!result) {
        return result;
    }
    return write_bytes(files->scene_event_group, snapshot.scene_events);
}

SnapshotWriteResult write_numbered_slot_ranger(
    const std::filesystem::path& root,
    const SaveSlot slot,
    const model::GameSnapshot& snapshot) {
    const auto files = numbered_file_set(root, slot);
    if (!files.has_value()) {
        return write_error(PersistenceStatus::invalid_slot, root);
    }
    if (!snapshot.valid()) {
        return write_error(PersistenceStatus::invalid_snapshot, files->ranger_group);
    }

    const auto ranger_group = encode_ranger(snapshot.ranger);
    return write_bytes(files->ranger_group, ranger_group);
}

SnapshotWriteResult write_numbered_slot(
    const std::filesystem::path& root,
    const SaveSlot slot,
    const model::GameSnapshot& snapshot) {
    auto result = write_numbered_slot_scene_archives(root, slot, snapshot);
    if (!result) {
        return result;
    }
    return write_numbered_slot_ranger(root, slot, snapshot);
}

std::string_view persistence_status_message(const PersistenceStatus status) noexcept {
    switch (status) {
    case PersistenceStatus::ready:
        return "ready";
    case PersistenceStatus::invalid_slot:
        return "save slot must be 1, 2, or 3";
    case PersistenceStatus::read_failed:
        return "cannot read save file";
    case PersistenceStatus::invalid_ranger_index_size:
        return "RANGER index must contain six 32-bit cumulative ends";
    case PersistenceStatus::invalid_ranger_index_order:
        return "RANGER cumulative ends must be strictly increasing";
    case PersistenceStatus::invalid_ranger_layout:
        return "RANGER six-section layout does not match the DOS data";
    case PersistenceStatus::invalid_ranger_group_size:
        return "RANGER group size does not match its index";
    case PersistenceStatus::invalid_scene_index_size:
        return "scene index must contain 100 cumulative ends";
    case PersistenceStatus::invalid_scene_index_layout:
        return "scene index record size does not match the DOS data";
    case PersistenceStatus::invalid_scene_group_size:
        return "scene group size does not match its index";
    case PersistenceStatus::invalid_snapshot:
        return "game snapshot has invalid scene storage";
    case PersistenceStatus::write_failed:
        return "cannot write save file";
    }
    return "unknown persistence status";
}

}  // namespace openlegend::persistence
