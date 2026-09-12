#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "openlegend/attributes.hpp"

namespace openlegend::resource {

class PackedArchive {
public:
    NODISCARD static PackedArchive parse(
        std::span<const std::uint8_t> index_bytes,
        std::vector<std::uint8_t> group_bytes);

    NODISCARD static PackedArchive open(
        const std::filesystem::path& index_path,
        const std::filesystem::path& group_path);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD std::size_t entry_count() const noexcept { return ranges_.size(); }

    NODISCARD std::span<const std::uint8_t> entry(std::size_t index) const noexcept;

    NODISCARD std::size_t data_size() const noexcept { return data_.size(); }

private:
    std::vector<std::uint8_t> data_;
    std::vector<std::pair<std::size_t, std::size_t>> ranges_;
    std::string error_;
};

class SentinelArchive {
public:
    NODISCARD static SentinelArchive parse(
        std::span<const std::uint8_t> index_bytes,
        std::vector<std::uint8_t> group_bytes);

    NODISCARD bool valid() const noexcept { return error_.empty(); }

    NODISCARD const std::string& error() const noexcept { return error_; }

    NODISCARD std::size_t entry_count() const noexcept { return ranges_.size(); }

    NODISCARD std::span<const std::uint8_t> entry(std::size_t index) const noexcept;

    NODISCARD std::span<const std::uint8_t> legacy_pointer_entry(
        std::size_t index) const noexcept;

private:
    std::vector<std::uint8_t> data_;
    std::vector<std::pair<std::size_t, std::size_t>> ranges_;
    std::string error_;
};

}  // namespace openlegend::resource
