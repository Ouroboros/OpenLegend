#include "asset_fingerprint_check.hpp"

#include <cctype>

#include "openlegend/persistence/asset_fingerprints.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::app {

std::string check_asset_fingerprints(
    const toml::table& assets, const std::filesystem::path& data_root) {
    for (const auto& [key, node] : assets) {
        const auto file = resource::read_binary_file(data_root / std::string{key.str()});
        if (!file) {
            return std::string{key.str()} + ": " + file.error;
        }
        const auto actual = persistence::sha256_hex(file.bytes);
        auto expected = node.value<std::string>().value_or(std::string{});
        for (auto& character : expected) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (!actual.has_value() || *actual != expected) {
            return std::string{key.str()} + " SHA-256 mismatch actual=" +
                actual.value_or(std::string{});
        }
    }
    return {};
}

}  // namespace openlegend::app
