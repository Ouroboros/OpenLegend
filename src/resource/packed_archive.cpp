#include "openlegend/resource/packed_archive.hpp"

#include <utility>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::resource {
namespace {

[[nodiscard]] bool valid_index_size(const std::span<const std::uint8_t> index_bytes) {
    return !index_bytes.empty() && index_bytes.size() % 4U == 0U;
}

}  // namespace

PackedArchive PackedArchive::parse(
    const std::span<const std::uint8_t> index_bytes,
    std::vector<std::uint8_t> group_bytes) {
    PackedArchive archive;
    archive.data_ = std::move(group_bytes);
    if (!valid_index_size(index_bytes)) {
        archive.error_ = "index size must be a non-zero multiple of four";
        return archive;
    }

    std::size_t begin = 0U;
    bool saw_positive_end = false;
    bool zero_tail = false;
    for (std::size_t offset = 0U; offset < index_bytes.size(); offset += 4U) {
        const auto raw_end = compat::read_u32le(index_bytes, offset);
        const auto end = static_cast<std::size_t>(raw_end);
        if (zero_tail) {
            if (end != 0U) {
                archive.error_ = "non-zero index value after zero tail";
                archive.ranges_.clear();
                return archive;
            }
            continue;
        }
        if (end == 0U && saw_positive_end) {
            zero_tail = true;
            continue;
        }
        if (end < begin || end > archive.data_.size()) {
            archive.error_ = "index offset is non-monotonic or outside group data";
            archive.ranges_.clear();
            return archive;
        }
        archive.ranges_.emplace_back(begin, end);
        begin = end;
        saw_positive_end = saw_positive_end || end != 0U;
    }

    if (begin != archive.data_.size()) {
        archive.error_ = "last valid index offset does not cover group data";
        archive.ranges_.clear();
    }
    return archive;
}

PackedArchive PackedArchive::open(
    const std::filesystem::path& index_path,
    const std::filesystem::path& group_path) {
    const auto index = read_binary_file(index_path);
    if (!index) {
        PackedArchive archive;
        archive.error_ = index.error;
        return archive;
    }
    auto group = read_binary_file(group_path);
    if (!group) {
        PackedArchive archive;
        archive.error_ = group.error;
        return archive;
    }
    return parse(index.bytes, std::move(group.bytes));
}

std::span<const std::uint8_t> PackedArchive::entry(const std::size_t index) const noexcept {
    if (index >= ranges_.size()) {
        return {};
    }
    const auto [begin, end] = ranges_[index];
    return std::span<const std::uint8_t>{data_}.subspan(begin, end - begin);
}

SentinelArchive SentinelArchive::parse(
    const std::span<const std::uint8_t> index_bytes,
    std::vector<std::uint8_t> group_bytes) {
    SentinelArchive archive;
    archive.data_ = std::move(group_bytes);
    if (!valid_index_size(index_bytes)) {
        archive.error_ = "sentinel index size must be a non-zero multiple of four";
        return archive;
    }

    const auto count = index_bytes.size() / 4U;
    if (compat::read_u32le(index_bytes, (count - 1U) * 4U) != 0U) {
        archive.error_ = "sentinel index must end with zero";
        return archive;
    }

    std::size_t begin = 0U;
    for (std::size_t index = 0U; index + 1U < count; ++index) {
        const auto end = static_cast<std::size_t>(compat::read_u32le(index_bytes, index * 4U));
        if (end < begin || end > archive.data_.size()) {
            archive.error_ = "sentinel index offset is non-monotonic or outside group data";
            archive.ranges_.clear();
            return archive;
        }
        archive.ranges_.emplace_back(begin, end);
        begin = end;
    }
    archive.ranges_.emplace_back(begin, archive.data_.size());
    return archive;
}

std::span<const std::uint8_t> SentinelArchive::entry(const std::size_t index) const noexcept {
    if (index >= ranges_.size()) {
        return {};
    }
    const auto [begin, end] = ranges_[index];
    return std::span<const std::uint8_t>{data_}.subspan(begin, end - begin);
}

}  // namespace openlegend::resource
