#include "openlegend/resource/asset_file_names.hpp"

#include <cstdio>
#include <string_view>

namespace openlegend::resource {
namespace {

constexpr std::array<std::string_view, kAssetFileKindCount> kOriginalFormats{
    "SDX%03d", "SMP%03d", "WDX%03d", "WMP%03d", "FIGHT%03d.IDX", "FIGHT%03d.GRP"};

}  // namespace

std::optional<std::string> AssetFileNames::name(const AssetFileKind kind, const std::int64_t id) const {
    const auto index = static_cast<std::size_t>(kind);
    if (id < 0 || index >= kAssetFileKindCount) {
        return std::nullopt;
    }
    const auto& list = lists[index];
    if (!list.empty()) {
        return static_cast<std::size_t>(id) < list.size()
            ? std::optional<std::string>{list[static_cast<std::size_t>(id)]} : std::nullopt;
    }
    if (id > 999) {
        return std::nullopt;
    }
    std::array<char, 16> buffer{};
    static_cast<void>(std::snprintf(
        buffer.data(), buffer.size(), kOriginalFormats[index].data(), static_cast<int>(id)));
    return std::string{buffer.data()};
}

}  // namespace openlegend::resource
