#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/save_file.hpp"

namespace openlegend::persistence::detail {

enum class SaveFileAccess { read, create };

struct SaveFileInformation {
    bool regular{};
    std::uint64_t size{};
};

class SaveFileHandle {
public:
    virtual ~SaveFileHandle() = default;

    NODISCARD virtual SaveFileInformation information() = 0;

    NODISCARD virtual std::size_t read(std::span<std::uint8_t> bytes) = 0;

    NODISCARD virtual std::size_t write(std::span<const std::uint8_t> bytes) = 0;

    virtual void flush() = 0;

    virtual void close() = 0;
};

class SaveFileOperations {
public:
    virtual ~SaveFileOperations() = default;

    NODISCARD virtual std::unique_ptr<SaveFileHandle> open(
        const std::filesystem::path& path, SaveFileAccess access) = 0;

    virtual void publish(const std::filesystem::path& temporary,
        const std::filesystem::path& target, bool replacing) = 0;

    virtual void restore(const std::filesystem::path& backup,
        const std::filesystem::path& target) = 0;

    virtual void remove(const std::filesystem::path& path) = 0;

    virtual void sync_directory(const std::filesystem::path& path) = 0;
};

NODISCARD SaveFileOperations& native_save_file_operations();

NODISCARD SaveFileReadResult read_save_file(
    SaveFileOperations& operations, const std::filesystem::path& path,
    std::size_t maximum_bytes);

NODISCARD SaveFileGroupWriteResult replace_save_files(
    SaveFileOperations& operations, std::span<const SaveFileReplacement> files);

NODISCARD SaveFileWriteResult replace_save_file(
    SaveFileOperations& operations, const std::filesystem::path& path,
    std::span<const std::uint8_t> bytes, std::size_t maximum_bytes);

}
