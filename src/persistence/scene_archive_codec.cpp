#include "openlegend/persistence/scene_archive_codec.hpp"

#include <array>
#include <cstddef>
#include <memory>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#include "openlegend/attributes.hpp"
#include "openlegend/model/game_snapshot.hpp"

namespace openlegend::persistence {
namespace {

constexpr auto kBase64Alphabet = std::string_view{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};
constexpr auto kMaximumEncodedBytes = std::size_t{64U * 1024U * 1024U};

NODISCARD std::size_t archive_byte_count(const SceneArchiveKind kind) noexcept {
    switch (kind) {
    case SceneArchiveKind::maps:
        return model::kSceneCount * model::kSceneMapBytesPerScene;
    case SceneArchiveKind::events:
        return model::kSceneCount * model::kSceneEventBytesPerScene;
    }
    return 0U;
}

NODISCARD int base64_value(const char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return character - 'A';
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 26;
    }
    if (character >= '0' && character <= '9') {
        return character - '0' + 52;
    }
    if (character == '+') {
        return 62;
    }
    return character == '/' ? 63 : -1;
}

NODISCARD std::string encode_base64(const std::span<const std::uint8_t> bytes) {
    std::string result;
    const auto characters = (bytes.size() + 2U) / 3U * 4U;
    result.reserve(characters + (characters + 75U) / 76U);
    std::size_t column = 0U;
    for (std::size_t offset = 0U; offset < bytes.size(); offset += 3U) {
        const std::uint32_t first = bytes[offset];
        const auto second = offset + 1U < bytes.size() ? bytes[offset + 1U] : 0U;
        const auto third = offset + 2U < bytes.size() ? bytes[offset + 2U] : 0U;
        result.push_back(kBase64Alphabet[first >> 2U]);
        result.push_back(kBase64Alphabet[((first & 3U) << 4U) | (second >> 4U)]);
        result.push_back(offset + 1U < bytes.size()
            ? kBase64Alphabet[((second & 15U) << 2U) | (third >> 6U)] : '=');
        result.push_back(offset + 2U < bytes.size() ? kBase64Alphabet[third & 63U] : '=');
        column += 4U;
        if (column == 76U) {
            result.push_back('\n');
            column = 0U;
        }
    }
    if (column != 0U) {
        result.push_back('\n');
    }
    return result;
}

NODISCARD std::optional<std::vector<std::uint8_t>> decode_base64(
    const std::string_view text) {
    if (text.empty() || text.size() > kMaximumEncodedBytes) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() / 4U * 3U);
    std::array<char, 4> quartet{};
    std::size_t length = 0U;
    bool padded = false;
    for (const auto character : text) {
        if (character == '\r' || character == '\n') {
            continue;
        }
        if (padded) {
            return std::nullopt;
        }
        quartet[length++] = character;
        if (length != quartet.size()) {
            continue;
        }
        length = 0U;
        const auto first = base64_value(quartet[0U]);
        const auto second = base64_value(quartet[1U]);
        if (first < 0 || second < 0) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>((first << 2U) | (second >> 4U)));
        if (quartet[2U] == '=') {
            if (quartet[3U] != '=' || (second & 15) != 0) {
                return std::nullopt;
            }
            padded = true;
            continue;
        }
        const auto third = base64_value(quartet[2U]);
        if (third < 0) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>(((second & 15) << 4U) | (third >> 2U)));
        if (quartet[3U] == '=') {
            if ((third & 3) != 0) {
                return std::nullopt;
            }
            padded = true;
            continue;
        }
        const auto fourth = base64_value(quartet[3U]);
        if (fourth < 0) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::uint8_t>(((third & 3) << 6U) | fourth));
    }
    if (length != 0U || bytes.empty()) {
        return std::nullopt;
    }
    return bytes;
}

}

std::optional<std::string> encode_scene_archive(
    const std::span<const std::uint8_t> bytes, const SceneArchiveKind kind) {
    const auto expected_size = archive_byte_count(kind);
    if (expected_size == 0U || bytes.size() != expected_size) {
        return std::nullopt;
    }
    const std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context{
        ZSTD_createCCtx(), &ZSTD_freeCCtx};
    if (!context ||
        ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, 3)) ||
        ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_contentSizeFlag, 1)) ||
        ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, 1))) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> compressed(ZSTD_compressBound(bytes.size()));
    const auto size = ZSTD_compress2(
        context.get(), compressed.data(), compressed.size(), bytes.data(), bytes.size());
    if (ZSTD_isError(size)) {
        return std::nullopt;
    }
    compressed.resize(size);
    return encode_base64(compressed);
}

std::optional<std::vector<std::uint8_t>> decode_scene_archive(
    const std::string_view base64, const SceneArchiveKind kind) {
    const auto expected_size = archive_byte_count(kind);
    if (expected_size == 0U) {
        return std::nullopt;
    }
    const auto compressed = decode_base64(base64);
    if (!compressed.has_value()) {
        return std::nullopt;
    }
    ZSTD_FrameHeader header{};
    if (ZSTD_getFrameHeader(&header, compressed->data(), compressed->size()) != 0U ||
        header.frameType != ZSTD_frame || header.frameContentSize != expected_size ||
        header.checksumFlag != 1U || header.dictID != 0U) {
        return std::nullopt;
    }
    const auto frame_size = ZSTD_findFrameCompressedSize(compressed->data(), compressed->size());
    if (ZSTD_isError(frame_size) || frame_size != compressed->size()) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes(expected_size);
    const auto size = ZSTD_decompress(
        bytes.data(), bytes.size(), compressed->data(), compressed->size());
    if (ZSTD_isError(size) || size != expected_size) {
        return std::nullopt;
    }
    return bytes;
}

}
