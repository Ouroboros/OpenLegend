#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace openlegend::resource {

struct BinaryFile {
    std::vector<std::uint8_t> bytes;
    std::string error;

    [[nodiscard]] explicit operator bool() const noexcept { return error.empty(); }
};

[[nodiscard]] BinaryFile read_binary_file(const std::filesystem::path& path);

class DataRoot {
public:
    explicit DataRoot(std::filesystem::path root) : root_(std::move(root)) {}

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return root_; }
    [[nodiscard]] BinaryFile read(const std::filesystem::path& relative) const;

private:
    std::filesystem::path root_;
};

}  // namespace openlegend::resource
