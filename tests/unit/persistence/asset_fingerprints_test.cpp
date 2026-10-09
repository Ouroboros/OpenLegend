#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "openlegend/persistence/asset_fingerprints.hpp"
#include "test_support.hpp"

namespace {

constexpr std::array<std::string_view, 9> kFilenames{
    "RANGER.IDX", "RANGER.GRP", "ALLSIN.IDX", "ALLSIN.GRP", "ALLDEF.IDX",
    "ALLDEF.GRP", "KDEF.IDX", "KDEF.GRP", "WAR.STA"};
constexpr std::array<std::string_view, 9> kFixtureHashes{
    "5e3283e916d3c42f5b9658a063ca78fa46aac2e127367680558cfa71910bf61d",
    "bbd2f5bb4f3e179cc35ff9cc2f3e9ba32a1cc8981b335f500d9a358e4cbba843",
    "22235e9b6d156bd9ef14ebf1f26ad8c80651378577cc3db50ae7eea14c1ce97e",
    "b0df4e2673831b86c8053a302fad63cbacfb7952ef63429e6456476c8599c71e",
    "0474ce4aa8f05f26fa34a65269e77b8e84bc8cad951da531cb086627c38373ce",
    "ac9b16f0cbdda45f24c7857c6e0cb18e760ce1b8cb49f223ed1b15aba4884145",
    "ceed7831cac7af05835437ae5f10140ff06223749ff2d8dd1554ee1ebe4f6afd",
    "1260489e9c64a1e65a39bfdc5977444df4803706b629f178748964d0c412fb88",
    "c3cff65a6ba22dbcf87bc3660c209bfa72ba8845ae7f68a9d0fab1def79c1189",
};

void write_bytes(const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    OL_CHECK(output);
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    output.close();
    OL_CHECK(output);
}

void check_fixture_hashes(const std::filesystem::path& root) {
    using openlegend::persistence::fingerprint_new_game_plus_assets;
    for (std::size_t index = 0U; index < kFilenames.size(); ++index) {
        write_bytes(root / kFilenames[index],
                    std::vector<std::uint8_t>(55U + index, static_cast<std::uint8_t>(index + 1U)));
    }
    const auto original = fingerprint_new_game_plus_assets(root);
    OL_CHECK(original);
    if (!original) {
        return;
    }
    OL_CHECK(original.path.empty() && original.error.empty());
    for (std::size_t index = 0U; index < kFilenames.size(); ++index) {
        OL_CHECK(original.fingerprints->sha256[index] == kFixtureHashes[index]);
        auto bytes = std::vector<std::uint8_t>(55U + index, static_cast<std::uint8_t>(index + 1U));
        bytes.back() ^= 1U;
        write_bytes(root / kFilenames[index], bytes);
        const auto changed = fingerprint_new_game_plus_assets(root);
        OL_CHECK(changed);
        if (changed) {
            for (std::size_t compared = 0U; compared < kFilenames.size(); ++compared) {
                OL_CHECK((changed.fingerprints->sha256[compared] == kFixtureHashes[compared]) ==
                    (index != compared));
            }
        }
        bytes.back() ^= 1U;
        write_bytes(root / kFilenames[index], bytes);
    }
}

void check_padding_vectors(const std::filesystem::path& root) {
    using openlegend::persistence::fingerprint_new_game_plus_assets;
    struct HashVector {
        std::size_t size;
        std::string_view expected;
    };
    constexpr std::array cases{
        HashVector{0U, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        HashVector{1U, "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d"},
        HashVector{55U, "463eb28e72f82e0a96c0a4cc53690c571281131f672aa229e0d45ae59b598b59"},
        HashVector{56U, "da2ae4d6b36748f2a318f23e7ab1dfdf45acdc9d049bd80e59de82a60895f562"},
        HashVector{63U, "29af2686fd53374a36b0846694cc342177e428d1647515f078784d69cdb9e488"},
        HashVector{64U, "fdeab9acf3710362bd2658cdc9a29e8f9c757fcf9811603a8c447cd1d9151108"},
        HashVector{65U, "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781"},
        HashVector{119U, "da18797ed7c3a777f0847f429724a2d8cd5138e6ed2895c3fa1a6d39d18f7ec6"},
        HashVector{120U, "f52b23db1fbb6ded89ef42a23ce0c8922c45f25c50b568a93bf1c075420bbb7c"},
        HashVector{127U, "92ca0fa6651ee2f97b884b7246a562fa71250fedefe5ebf270d31c546bfea976"},
        HashVector{128U, "471fb943aa23c511f6f72f8d1652d9c880cfa392ad80503120547703e56a2be5"},
        HashVector{129U, "5099c6a56203f9687f7d33f4bfdf576d31dc91f6b695ecea38b2770c87631135"},
    };
    for (const auto& test_case : cases) {
        std::vector<std::uint8_t> bytes(test_case.size);
        for (std::size_t index = 0U; index < bytes.size(); ++index) {
            bytes[index] = static_cast<std::uint8_t>(index & 255U);
        }
        write_bytes(root / "RANGER.IDX", bytes);
        const auto result = fingerprint_new_game_plus_assets(root);
        OL_CHECK(result);
        if (result) {
            OL_CHECK(result.fingerprints->sha256[0U] == test_case.expected);
        }
    }
    write_bytes(root / "RANGER.IDX", std::array<std::uint8_t, 3>{'a', 'b', 'c'});
    const auto abc = fingerprint_new_game_plus_assets(root);
    OL_CHECK(abc && abc.fingerprints->sha256[0U] ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    write_bytes(root / "RANGER.IDX", std::vector<std::uint8_t>(1'000'000U, 'a'));
    const auto million = fingerprint_new_game_plus_assets(root);
    OL_CHECK(million && million.fingerprints->sha256[0U] ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    write_bytes(root / "RANGER.IDX", std::vector<std::uint8_t>(55U, 1U));
}

void check_read_failures(const std::filesystem::path& root) {
    using openlegend::persistence::fingerprint_new_game_plus_assets;
    for (std::size_t index = 0U; index < kFilenames.size(); ++index) {
        const auto path = root / kFilenames[index];
        OL_CHECK(std::filesystem::remove(path));
        const auto missing = fingerprint_new_game_plus_assets(root);
        OL_CHECK(!missing && !missing.fingerprints.has_value());
        OL_CHECK(missing.path == path && !missing.error.empty());
        OL_CHECK(std::filesystem::create_directory(path));
        const auto directory = fingerprint_new_game_plus_assets(root);
        OL_CHECK(!directory && !directory.fingerprints.has_value());
        OL_CHECK(directory.path == path && !directory.error.empty());
        OL_CHECK(std::filesystem::remove(path));
        write_bytes(path, std::vector<std::uint8_t>(55U + index, static_cast<std::uint8_t>(index + 1U)));
    }
}

void check_current_assets() {
    constexpr std::array<std::string_view, 9> expected{
        "52c1545c8c0aa4d7919916ae840fb15f2772c567811ab9b9056e55b2e17ff92c",
        "07b99e3c1676e18691f00d6dfe713121faa6f7429e43666bc83e1568cecb68ab",
        "6d35d9c9b233cd389261d58ca2f17d4f12f6dba1f429ac8ff6217ce3b10ab94a",
        "830ae313ccabe310a16d330eac83647a9c81a6c23efce6069ca87dc653f0e154",
        "99ad387b0ec7790ea684e3d9edf4777b35273703108b1c2ba0d584ddffc51c20",
        "3633122f6a43f0b5dd390c2fa2516766d735a064ca955c8766b73232230a4480",
        "bf02eccf1dab01fd0ae033cdf807f56b978a71a6c2ab9be05e366239b485104b",
        "135c5e097a7fe561ee931046e1bebf55de9b469678e3d111d8e9f2c6bb600e06",
        "98e3f66912c5ba4a0be00aaeff3462eb8c99f4d591d92a754930070dde9649b6",
    };
    const auto result = openlegend::persistence::fingerprint_new_game_plus_assets(
        openlegend::test::game_data_root());
    OL_CHECK(result);
    if (result) {
        for (std::size_t index = 0U; index < expected.size(); ++index) {
            OL_CHECK(result.fingerprints->sha256[index] == expected[index]);
        }
    }
}

}

int main() {
    const auto root = std::filesystem::path{OPENLEGEND_TEST_OUTPUT_ROOT} / "asset-fingerprints";
    std::filesystem::remove_all(root);
    OL_CHECK(std::filesystem::create_directories(root));
    check_fixture_hashes(root);
    check_padding_vectors(root);
    check_read_failures(root);
    check_current_assets();
    std::filesystem::remove_all(root);
    return openlegend::test::failures == 0 ? 0 : 1;
}
