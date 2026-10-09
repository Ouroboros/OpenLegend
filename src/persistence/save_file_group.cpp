#include "openlegend/persistence/save_file.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>

#include "openlegend/attributes.hpp"
#include "save_file_operations.hpp"

namespace openlegend::persistence {
namespace {

struct PendingSaveFile {
    SaveFileReplacement replacement;
    std::filesystem::path temporary;
    std::filesystem::path backup;
    std::unique_ptr<detail::SaveFileHandle> temporary_file;
    std::unique_ptr<detail::SaveFileHandle> backup_file;
    SaveFileReadResult original;
    bool owns_temporary{};
    bool owns_backup{};
    bool attempted_publish{};
    bool published{};
    bool keep_backup{};
};

NODISCARD bool reserved_filename(const std::filesystem::path& path) {
    auto filename = path.filename().u8string();
    for (auto& character : filename) {
        if (character >= u8'A' && character <= u8'Z') {
            character = static_cast<char8_t>(character + (u8'a' - u8'A'));
        }
    }
    return filename.empty() || filename == u8"." || filename == u8".." || filename.ends_with(u8".tmp");
}

void write_complete(detail::SaveFileHandle& file, const std::span<const std::uint8_t> bytes) {
    if (!file.information().regular) {
        throw std::runtime_error("save temporary file is not regular");
    }
    std::size_t offset{};
    while (offset < bytes.size()) {
        const auto chunk = bytes.subspan(offset, std::min<std::size_t>(65536U, bytes.size() - offset));
        const auto count = file.write(chunk);
        if (count == 0U || count > chunk.size()) {
            throw std::runtime_error("incomplete save file write");
        }
        offset += count;
    }
    file.flush();
    file.close();
}

void record_recovery(SaveFileGroupWriteResult& result, const std::filesystem::path& path) {
    if (std::find(result.recovery_paths.begin(), result.recovery_paths.end(), path) ==
        result.recovery_paths.end()) {
        result.recovery_paths.push_back(path);
    }
}

void cleanup_file(detail::SaveFileOperations& operations, const std::filesystem::path& path,
    SaveFileGroupWriteResult& result) {
    try {
        operations.remove(path);
    } catch (const std::exception& error) {
        if (result.status == SaveFileStatus::ready) {
            result.status = SaveFileStatus::io_error;
            result.path = path;
        }
        result.detail += "; temporary file cleanup failed: ";
        result.detail += error.what();
        record_recovery(result, path);
    }
}

void restore_files(detail::SaveFileOperations& operations, std::vector<PendingSaveFile>& pending,
    const std::filesystem::path& directory, SaveFileGroupWriteResult& result) {
    for (auto cursor = pending.rbegin(); cursor != pending.rend(); ++cursor) {
        auto& file = *cursor;
        if (!file.attempted_publish || (!file.original && !file.published)) {
            continue;
        }
        try {
            if (file.original) {
                const auto current = detail::read_save_file(
                    operations, file.replacement.path, file.replacement.maximum_bytes);
                if (!current || current.bytes != file.original.bytes) {
                    operations.restore(file.backup, file.replacement.path);
                    file.owns_backup = false;
                }
            } else {
                operations.remove(file.replacement.path);
            }
            operations.sync_directory(directory);
        } catch (const std::exception& error) {
            result.status = SaveFileStatus::rollback_failed;
            file.keep_backup = file.owns_backup;
            record_recovery(result, file.keep_backup ? file.backup : file.replacement.path);
            if (!file.original) {
                record_recovery(result, file.replacement.path);
            }
            result.detail += "; previous save restoration failed: ";
            result.detail += error.what();
        }
    }
}

}

SaveFileGroupWriteResult detail::replace_save_files(
    SaveFileOperations& operations, const std::span<const SaveFileReplacement> files) {
    SaveFileGroupWriteResult result;
    if (files.empty()) {
        result.status = SaveFileStatus::io_error;
        result.detail = "save file group is empty";
        return result;
    }
    std::vector<PendingSaveFile> pending;
    std::filesystem::path directory;
    try {
        pending.reserve(files.size());
        for (const auto& replacement : files) {
            result.path = replacement.path;
            if (replacement.bytes.size() > replacement.maximum_bytes) {
                result.status = SaveFileStatus::too_large;
                result.detail = "save document exceeds its size limit";
                return result;
            }
            const auto normalized = replacement.path.lexically_normal();
            const auto parent = normalized.has_parent_path()
                ? normalized.parent_path() : std::filesystem::path{"."};
            if (reserved_filename(replacement.path) || (!directory.empty() && parent != directory)) {
                result.status = SaveFileStatus::not_regular;
                result.detail = "save file group requires ordinary filenames in one directory";
                return result;
            }
            directory = parent;
            PendingSaveFile file;
            file.replacement = replacement;
            file.temporary = replacement.path;
            file.temporary += ".tmp";
            file.backup = replacement.path;
            file.backup += ".rollback.tmp";
            pending.push_back(std::move(file));
        }
        for (auto& file : pending) {
            result.path = file.replacement.path;
            file.temporary_file = operations.open(file.temporary, SaveFileAccess::create);
            file.owns_temporary = true;
            file.backup_file = operations.open(file.backup, SaveFileAccess::create);
            file.owns_backup = true;
        }
        for (auto& file : pending) {
            result.path = file.replacement.path;
            file.original = detail::read_save_file(
                operations, file.replacement.path, file.replacement.maximum_bytes);
            if (!file.original && file.original.status != SaveFileStatus::not_found) {
                result.status = file.original.status;
                throw std::runtime_error(file.original.detail);
            }
            write_complete(*file.backup_file, file.original
                ? std::span<const std::uint8_t>{*file.original.bytes} : std::span<const std::uint8_t>{});
            file.backup_file.reset();
            write_complete(*file.temporary_file, file.replacement.bytes);
            file.temporary_file.reset();
        }
        result.path = directory;
        operations.sync_directory(directory);
        for (auto& file : pending) {
            result.path = file.replacement.path;
            file.attempted_publish = true;
            operations.publish(file.temporary, file.replacement.path, static_cast<bool>(file.original));
            file.published = true;
            file.owns_temporary = false;
        }
        result.path = directory;
        operations.sync_directory(directory);
        result.committed = true;
        result.path.clear();
    } catch (const std::exception& error) {
        if (result.status == SaveFileStatus::ready) {
            result.status = SaveFileStatus::io_error;
        }
        result.detail = error.what();
    }
    for (auto& file : pending) {
        file.temporary_file.reset();
        file.backup_file.reset();
    }
    if (!result.committed) {
        restore_files(operations, pending, directory, result);
    }
    for (auto& file : pending) {
        if (file.owns_temporary) {
            cleanup_file(operations, file.temporary, result);
        }
        if (file.owns_backup && !file.keep_backup) {
            cleanup_file(operations, file.backup, result);
        }
    }
    return result;
}

SaveFileGroupWriteResult replace_save_files(const std::span<const SaveFileReplacement> files) {
    return detail::replace_save_files(detail::native_save_file_operations(), files);
}

}
