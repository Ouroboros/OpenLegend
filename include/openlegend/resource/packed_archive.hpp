#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace openlegend::resource {

class PackedArchive {
public:
    [[nodiscard]] static PackedArchive parse(
        std::span<const std::uint8_t> index_bytes,
        std::vector<std::uint8_t> group_bytes);
    [[nodiscard]] static PackedArchive open(
        const std::filesystem::path& index_path,
        const std::filesystem::path& group_path);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t entry_count() const noexcept { return ranges_.size(); }
    [[nodiscard]] std::span<const std::uint8_t> entry(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t data_size() const noexcept { return data_.size(); }

private:
    std::vector<std::uint8_t> data_;
    std::vector<std::pair<std::size_t, std::size_t>> ranges_;
    std::string error_;
};

class SentinelArchive {
public:
    [[nodiscard]] static SentinelArchive parse(
        std::span<const std::uint8_t> index_bytes,
        std::vector<std::uint8_t> group_bytes);

    [[nodiscard]] bool valid() const noexcept { return error_.empty(); }
    [[nodiscard]] const std::string& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t entry_count() const noexcept { return ranges_.size(); }
    [[nodiscard]] std::span<const std::uint8_t> entry(std::size_t index) const noexcept;

private:
    std::vector<std::uint8_t> data_;
    std::vector<std::pair<std::size_t, std::size_t>> ranges_;
    std::string error_;
};

}  // namespace openlegend::resource
