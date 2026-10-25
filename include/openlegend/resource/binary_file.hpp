#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/resource/asset_file_names.hpp"

namespace openlegend::resource {

struct BinaryFile {
    std::vector<std::uint8_t> bytes;
    std::string error;

    NODISCARD explicit operator bool() const noexcept { return error.empty(); }
};

NODISCARD BinaryFile read_binary_file(const std::filesystem::path& path);

class DataRoot {
public:
    explicit DataRoot(std::filesystem::path root) : root_(std::move(root)) {}

    NODISCARD const std::filesystem::path& path() const noexcept { return root_; }

    NODISCARD BinaryFile read(const std::filesystem::path& relative) const;

    // 按编号打开的资源文件名列表；DataRoot副本一并携带。
    NODISCARD const AssetFileNames& asset_names() const noexcept { return asset_names_; }

    void set_asset_names(AssetFileNames names) { asset_names_ = std::move(names); }

private:
    std::filesystem::path root_;
    AssetFileNames asset_names_;
};

}  // namespace openlegend::resource
