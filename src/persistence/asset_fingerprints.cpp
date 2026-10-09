#include "openlegend/persistence/asset_fingerprints.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

#include "openlegend/attributes.hpp"
#include "openlegend/resource/binary_file.hpp"

namespace openlegend::persistence {
namespace {

constexpr std::array<std::uint32_t, 64> kSha256Constants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

void apply_sha256_block(
    std::array<std::uint32_t, 8>& digest, const std::span<const std::uint8_t, 64> bytes) noexcept {
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0U; index < 16U; ++index) {
        const auto offset = index * 4U;
        schedule[index] = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
            (static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U) |
            (static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U) |
            static_cast<std::uint32_t>(bytes[offset + 3U]);
    }
    for (std::size_t index = 16U; index < schedule.size(); ++index) {
        const auto earlier = schedule[index - 15U];
        const auto recent = schedule[index - 2U];
        const auto earlier_sigma = std::rotr(earlier, 7) ^ std::rotr(earlier, 18) ^ (earlier >> 3U);
        const auto recent_sigma = std::rotr(recent, 17) ^ std::rotr(recent, 19) ^ (recent >> 10U);
        schedule[index] = schedule[index - 16U] + earlier_sigma +
            schedule[index - 7U] + recent_sigma;
    }
    auto working = digest;
    for (std::size_t round = 0U; round < schedule.size(); ++round) {
        const auto high_sigma = std::rotr(working[4U], 6) ^
            std::rotr(working[4U], 11) ^ std::rotr(working[4U], 25);
        const auto choose = (working[4U] & working[5U]) ^ (~working[4U] & working[6U]);
        const auto low_sigma = std::rotr(working[0U], 2) ^
            std::rotr(working[0U], 13) ^ std::rotr(working[0U], 22);
        const auto majority = (working[0U] & working[1U]) ^
            (working[0U] & working[2U]) ^ (working[1U] & working[2U]);
        const auto first = working[7U] + high_sigma + choose + kSha256Constants[round] + schedule[round];
        const auto second = low_sigma + majority;
        working = {first + second, working[0U], working[1U], working[2U],
                   working[3U] + first, working[4U], working[5U], working[6U]};
    }
    for (std::size_t index = 0U; index < digest.size(); ++index) {
        digest[index] += working[index];
    }
}

NODISCARD std::optional<std::string> sha256(const std::span<const std::uint8_t> bytes) {
    if (bytes.size() > std::numeric_limits<std::uint64_t>::max() / 8U) {
        return std::nullopt;
    }
    std::array<std::uint32_t, 8> digest{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    std::size_t offset = 0U;
    while (bytes.size() - offset >= 64U) {
        apply_sha256_block(digest, std::span<const std::uint8_t, 64>{bytes.data() + offset, 64U});
        offset += 64U;
    }
    const auto remainder = bytes.subspan(offset);
    std::array<std::uint8_t, 128> padding{};
    std::ranges::copy(remainder, padding.begin());
    padding[remainder.size()] = 0x80U;
    const auto padded_size = remainder.size() < 56U ? 64U : 128U;
    const auto bit_count = static_cast<std::uint64_t>(bytes.size()) * 8U;
    for (std::size_t index = 0U; index < 8U; ++index) {
        padding[padded_size - 1U - index] = static_cast<std::uint8_t>(
            (bit_count >> (8U * index)) & 255U);
    }
    apply_sha256_block(digest, std::span<const std::uint8_t, 64>{padding.data(), 64U});
    if (padded_size == 128U) {
        apply_sha256_block(digest, std::span<const std::uint8_t, 64>{padding.data() + 64U, 64U});
    }
    constexpr auto alphabet = std::string_view{"0123456789abcdef"};
    std::string result(64U, '0');
    for (std::size_t word = 0U; word < digest.size(); ++word) {
        for (std::size_t digit = 0U; digit < 8U; ++digit) {
            result[word * 8U + digit] = alphabet[(digest[word] >> (28U - 4U * digit)) & 15U];
        }
    }
    return result;
}

}

AssetFingerprintResult fingerprint_new_game_plus_assets(const std::filesystem::path& root) {
    AssetFingerprints fingerprints;
    for (std::size_t index = 0U; index < kNewGamePlusSaveAssets.size(); ++index) {
        const auto path = root / kNewGamePlusSaveAssets[index].filename;
        const auto file = resource::read_binary_file(path);
        if (!file) {
            return {std::nullopt, path, file.error};
        }
        auto digest = sha256(file.bytes);
        if (!digest.has_value()) {
            return {std::nullopt, path, "asset file exceeds the SHA-256 length domain"};
        }
        fingerprints.sha256[index] = std::move(*digest);
    }
    return {std::move(fingerprints), {}, {}};
}

}
