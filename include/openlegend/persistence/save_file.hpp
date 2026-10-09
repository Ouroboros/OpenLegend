#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "openlegend/attributes.hpp"

namespace openlegend::persistence {

enum class SaveFileStatus {
    ready,
    not_found,
    not_regular,
    too_large,
    io_error,
    rollback_failed,
};

struct SaveFileReadResult {
    SaveFileStatus status{SaveFileStatus::ready};
    std::optional<std::vector<std::uint8_t>> bytes;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return status == SaveFileStatus::ready && bytes.has_value();
    }
};

struct SaveFileWriteResult {
    SaveFileStatus status{SaveFileStatus::ready};
    std::filesystem::path recovery_path;
    std::string detail;

    NODISCARD explicit operator bool() const noexcept {
        return status == SaveFileStatus::ready;
    }
};

NODISCARD SaveFileReadResult read_save_file(
    const std::filesystem::path& path, std::size_t maximum_bytes);

NODISCARD SaveFileWriteResult replace_save_file(
    const std::filesystem::path& path, std::span<const std::uint8_t> bytes,
    std::size_t maximum_bytes);

}
