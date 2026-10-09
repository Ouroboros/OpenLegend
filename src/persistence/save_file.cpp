#include "openlegend/persistence/save_file.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "openlegend/attributes.hpp"
#include "save_file_operations.hpp"

namespace openlegend::persistence {
namespace {

constexpr std::size_t kTransferBytes = 64U * 1024U;

class SaveFailure final : public std::runtime_error {
public:
    SaveFailure(const SaveFileStatus status, const std::string& message)
        : std::runtime_error(message), status_(status) {}

    NODISCARD SaveFileStatus status() const noexcept {
        return status_;
    }

private:
    SaveFileStatus status_;
};

NODISCARD SaveFileStatus read_error_status(const std::exception& error) {
    const auto* system_error = dynamic_cast<const std::system_error*>(&error);
    if (system_error != nullptr) {
        if (system_error->code() == std::errc::no_such_file_or_directory) {
            return SaveFileStatus::not_found;
        }
        if (system_error->code() == std::errc::too_many_symbolic_link_levels) {
            return SaveFileStatus::not_regular;
        }
    }
    return SaveFileStatus::io_error;
}

void write_complete(detail::SaveFileHandle& file, const std::span<const std::uint8_t> bytes) {
    if (!file.information().regular) {
        throw SaveFailure(SaveFileStatus::not_regular, "save temporary file is not regular");
    }
    std::size_t offset{};
    while (offset < bytes.size()) {
        const auto chunk = bytes.subspan(offset, std::min(kTransferBytes, bytes.size() - offset));
        const auto count = file.write(chunk);
        if (count == 0U || count > chunk.size()) {
            throw SaveFailure(SaveFileStatus::io_error, "incomplete save file write");
        }
        offset += count;
    }
    file.flush();
    file.close();
}

void cleanup_file(detail::SaveFileOperations& operations, const std::filesystem::path& path,
    SaveFileWriteResult& result) {
    try {
        operations.remove(path);
    } catch (const std::exception& error) {
        result.detail += "; temporary file cleanup failed: ";
        result.detail += error.what();
        if (result.recovery_path.empty()) {
            result.recovery_path = path;
        }
    }
}

}

SaveFileReadResult detail::read_save_file(SaveFileOperations& operations,
    const std::filesystem::path& path, const std::size_t maximum_bytes) {
    try {
        auto file = operations.open(path, SaveFileAccess::read);
        const auto information = file->information();
        if (!information.regular) {
            return {SaveFileStatus::not_regular, {}, "save path is not a regular file"};
        }
        if (information.size > maximum_bytes) {
            return {SaveFileStatus::too_large, {}, "save file exceeds its size limit"};
        }
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(information.size));
        std::size_t offset{};
        while (offset < bytes.size()) {
            const auto chunk = std::span{bytes}.subspan(offset, std::min(kTransferBytes, bytes.size() - offset));
            const auto count = file->read(chunk);
            if (count == 0U || count > chunk.size()) {
                return {SaveFileStatus::io_error, {}, "save file changed or ended during reading"};
            }
            offset += count;
        }
        std::array<std::uint8_t, 1> extra{};
        if (file->read(extra) != 0U || file->information().size != information.size) {
            return {SaveFileStatus::io_error, {}, "save file changed during reading"};
        }
        file->close();
        return {SaveFileStatus::ready, std::move(bytes), {}};
    } catch (const std::exception& error) {
        return {read_error_status(error), {}, error.what()};
    }
}

SaveFileWriteResult detail::replace_save_file(SaveFileOperations& operations,
    const std::filesystem::path& path, const std::span<const std::uint8_t> bytes,
    const std::size_t maximum_bytes) {
    if (bytes.size() > maximum_bytes) {
        return {SaveFileStatus::too_large, {}, "save document exceeds its size limit"};
    }
    if (path.filename().empty()) {
        return {SaveFileStatus::not_regular, {}, "save path has no filename"};
    }
    auto temporary = path;
    temporary += ".tmp";
    auto backup = path;
    backup += ".rollback.tmp";
    const auto directory = path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."};
    bool owns_temporary{};
    bool owns_backup{};
    bool attempted_publish{};
    bool published{};
    bool had_target{};
    std::unique_ptr<SaveFileHandle> temporary_file;
    SaveFileReadResult original;
    SaveFileWriteResult result;
    try {
        temporary_file = operations.open(temporary, SaveFileAccess::create);
        owns_temporary = true;
        auto backup_file = operations.open(backup, SaveFileAccess::create);
        owns_backup = true;
        original = detail::read_save_file(operations, path, maximum_bytes);
        had_target = static_cast<bool>(original);
        if (!had_target && original.status != SaveFileStatus::not_found) {
            throw SaveFailure(original.status, original.detail);
        }
        write_complete(*backup_file, had_target ? std::span<const std::uint8_t>{*original.bytes}
                                               : std::span<const std::uint8_t>{});
        backup_file.reset();
        write_complete(*temporary_file, bytes);
        temporary_file.reset();
        operations.sync_directory(directory);
        attempted_publish = true;
        operations.publish(temporary, path, had_target);
        published = true;
        owns_temporary = false;
        operations.sync_directory(directory);
        if (owns_backup) {
            operations.remove(backup);
            owns_backup = false;
        }
        return {};
    } catch (const SaveFailure& error) {
        result.status = error.status();
        result.detail = error.what();
    } catch (const std::exception& error) {
        result.status = SaveFileStatus::io_error;
        result.detail = error.what();
    }
    temporary_file.reset();
    if (attempted_publish && (had_target || published)) {
        try {
            if (had_target) {
                const auto current = detail::read_save_file(operations, path, maximum_bytes);
                if (!current || current.bytes != original.bytes) {
                    operations.restore(backup, path);
                    owns_backup = false;
                }
            } else {
                operations.remove(path);
            }
            operations.sync_directory(directory);
        } catch (const std::exception& error) {
            result.status = SaveFileStatus::rollback_failed;
            result.recovery_path = had_target && owns_backup ? backup : path;
            result.detail += "; previous save restoration failed: ";
            result.detail += error.what();
        }
    }
    if (owns_temporary) {
        cleanup_file(operations, temporary, result);
    }
    if (owns_backup && (!had_target || result.status != SaveFileStatus::rollback_failed)) {
        cleanup_file(operations, backup, result);
    }
    return result;
}

SaveFileReadResult read_save_file(const std::filesystem::path& path, const std::size_t maximum_bytes) {
    return detail::read_save_file(detail::native_save_file_operations(), path, maximum_bytes);
}

SaveFileWriteResult replace_save_file(const std::filesystem::path& path,
    const std::span<const std::uint8_t> bytes, const std::size_t maximum_bytes) {
    return detail::replace_save_file(detail::native_save_file_operations(), path, bytes, maximum_bytes);
}

}
