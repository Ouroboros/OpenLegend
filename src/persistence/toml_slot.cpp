#include "openlegend/persistence/toml_slot.hpp"

#include <cstdint>
#include <exception>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "openlegend/attributes.hpp"
#include "save_file_operations.hpp"

namespace openlegend::persistence {
namespace {

void ensure_directory(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> missing;
    auto current = directory;
    for (;;) {
        std::error_code error;
        const bool exists = std::filesystem::exists(current, error);
        if (error) {
            throw std::system_error(error, "inspect save directory");
        }
        if (exists) {
            if (!std::filesystem::is_directory(current)) {
                throw std::system_error(std::make_error_code(std::errc::not_a_directory), "save directory");
            }
            break;
        }
        missing.push_back(current);
        const auto parent = current.has_parent_path() ? current.parent_path() : std::filesystem::path{"."};
        if (parent == current) {
            throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory), "save directory root");
        }
        current = parent;
    }
    for (auto iterator = missing.rbegin(); iterator != missing.rend(); ++iterator) {
        static_cast<void>(std::filesystem::create_directory(*iterator));
        const auto parent = iterator->has_parent_path() ? iterator->parent_path() : std::filesystem::path{"."};
        detail::native_save_file_operations().sync_directory(parent);
    }
}

}

std::optional<std::filesystem::path> toml_slot_path(
    const std::filesystem::path& root, const TomlSaveKind kind, const SaveSlot slot) {
    const auto index = static_cast<unsigned int>(slot);
    if (index >= kNumberedSaveSlotCount ||
        (kind != TomlSaveKind::ordinary && kind != TomlSaveKind::completion)) {
        return std::nullopt;
    }
    auto filename = std::to_string(index + 1U);
    filename.insert(0U, 3U - filename.size(), '0');
    filename += ".toml";
    return root / (kind == TomlSaveKind::ordinary ? "ngplus" : "completed") / filename;
}

TomlSlotLoadResult load_toml_slot(const std::filesystem::path& root,
    const TomlSaveKind kind, const SaveSlot slot, const TomlSnapshotContext& context) {
    TomlSlotLoadResult result;
    if (!context.configuration.enabled) {
        result.snapshot_status = TomlSaveStatus::invalid_snapshot;
        result.detail = "TOML saves are disabled";
        return result;
    }
    const auto path = toml_slot_path(root, kind, slot);
    if (!path.has_value()) {
        result.snapshot_status = TomlSaveStatus::invalid_metadata;
        result.detail = "invalid TOML save kind or slot";
        return result;
    }
    result.path = *path;
    const auto document = read_save_file(*path, kMaximumTomlSaveBytes);
    if (!document) {
        result.file_status = document.status;
        result.detail = document.detail;
        return result;
    }
    const std::string_view text{reinterpret_cast<const char*>(document.bytes->data()), document.bytes->size()};
    auto decoded = decode_toml_snapshot(text, kind, slot, context);
    result.snapshot_status = decoded.status;
    result.save = std::move(decoded.save);
    result.detail = std::move(decoded.detail);
    return result;
}

TomlSlotWriteResult write_toml_slot(const std::filesystem::path& root,
    const model::RuntimeGameSnapshot& snapshot, const TomlSaveMetadata& metadata,
    const TomlSnapshotContext& context) {
    TomlSlotWriteResult result;
    auto encoded = encode_toml_snapshot(snapshot, metadata, context);
    if (!encoded) {
        result.snapshot_status = encoded.status;
        result.detail = std::move(encoded.detail);
        return result;
    }
    const auto path = toml_slot_path(root, metadata.kind, metadata.slot);
    if (!path.has_value()) {
        result.snapshot_status = TomlSaveStatus::invalid_metadata;
        result.detail = "invalid TOML save kind or slot";
        return result;
    }
    result.path = *path;
    try {
        ensure_directory(path->parent_path());
        const auto bytes = std::span{reinterpret_cast<const std::uint8_t*>(encoded.document->data()), encoded.document->size()};
        auto written = replace_save_file(*path, bytes, kMaximumTomlSaveBytes);
        result.file_status = written.status;
        result.recovery_path = std::move(written.recovery_path);
        result.detail = std::move(written.detail);
    } catch (const std::exception& error) {
        result.file_status = SaveFileStatus::io_error;
        result.detail = error.what();
    }
    return result;
}

}
