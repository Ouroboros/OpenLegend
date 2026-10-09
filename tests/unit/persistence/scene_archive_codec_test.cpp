#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/save_slot.hpp"
#include "openlegend/persistence/scene_archive_codec.hpp"
#include "test_support.hpp"

namespace {

constexpr auto kAlphabet = std::string_view{
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"};

NODISCARD std::string reference_base64(const std::span<const std::uint8_t> bytes) {
    std::string result;
    std::uint32_t accumulator = 0U;
    unsigned int bits = 0U;
    for (const auto byte : bytes) {
        accumulator = (accumulator << 8U) | byte;
        bits += 8U;
        while (bits >= 6U) {
            bits -= 6U;
            result.push_back(kAlphabet[(accumulator >> bits) & 63U]);
        }
    }
    if (bits != 0U) {
        result.push_back(kAlphabet[(accumulator << (6U - bits)) & 63U]);
    }
    while (result.size() % 4U != 0U) {
        result.push_back('=');
    }
    return result;
}

NODISCARD std::vector<std::uint8_t> reference_decode(const std::string_view text) {
    std::vector<std::uint8_t> bytes;
    std::uint32_t accumulator = 0U;
    unsigned int bits = 0U;
    for (const auto character : text) {
        if (character == '\r' || character == '\n' || character == '=') {
            continue;
        }
        const auto value = kAlphabet.find(character);
        OL_CHECK(value != std::string_view::npos);
        if (value == std::string_view::npos) {
            return {};
        }
        accumulator = (accumulator << 6U) | static_cast<std::uint32_t>(value);
        bits += 6U;
        if (bits >= 8U) {
            bits -= 8U;
            bytes.push_back(static_cast<std::uint8_t>((accumulator >> bits) & 255U));
        }
    }
    return bytes;
}

NODISCARD std::vector<std::uint8_t> reference_frame(
    const std::span<const std::uint8_t> bytes, const bool checksum, const bool content_size) {
    const std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)> context{
        ZSTD_createCCtx(), &ZSTD_freeCCtx};
    OL_CHECK(context != nullptr);
    if (!context) {
        return {};
    }
    OL_CHECK(!ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_compressionLevel, 1)));
    OL_CHECK(!ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_checksumFlag, checksum)));
    OL_CHECK(!ZSTD_isError(ZSTD_CCtx_setParameter(context.get(), ZSTD_c_contentSizeFlag, content_size)));
    std::vector<std::uint8_t> frame(ZSTD_compressBound(bytes.size()));
    const auto size = ZSTD_compress2(
        context.get(), frame.data(), frame.size(), bytes.data(), bytes.size());
    OL_CHECK(!ZSTD_isError(size));
    if (ZSTD_isError(size)) {
        return {};
    }
    frame.resize(size);
    return frame;
}

void check_archive(
    const std::vector<std::uint8_t>& bytes, const openlegend::persistence::SceneArchiveKind kind) {
    using namespace openlegend::persistence;
    const auto encoded = encode_scene_archive(bytes, kind);
    OL_CHECK(encoded.has_value());
    if (!encoded.has_value()) {
        return;
    }
    OL_CHECK(!encoded->empty() && encoded->back() == '\n');
    std::size_t line_start = 0U;
    while (line_start < encoded->size()) {
        const auto line_end = encoded->find('\n', line_start);
        OL_CHECK(line_end != std::string::npos);
        if (line_end == std::string::npos) {
            return;
        }
        const auto line_size = line_end - line_start;
        OL_CHECK(line_size > 0U && line_size <= 76U);
        OL_CHECK(line_end + 1U == encoded->size() || line_size == 76U);
        line_start = line_end + 1U;
    }
    const auto frame = reference_decode(*encoded);
    OL_CHECK(frame.size() > 5U);
    if (frame.size() <= 5U) {
        return;
    }
    ZSTD_FrameHeader header{};
    OL_CHECK(ZSTD_getFrameHeader(&header, frame.data(), frame.size()) == 0U);
    OL_CHECK(header.frameType == ZSTD_frame);
    OL_CHECK(header.frameContentSize == bytes.size());
    OL_CHECK(header.checksumFlag == 1U && header.dictID == 0U);
    OL_CHECK(ZSTD_findFrameCompressedSize(frame.data(), frame.size()) == frame.size());
    std::vector<std::uint8_t> independent(bytes.size());
    OL_CHECK(ZSTD_decompress(independent.data(), independent.size(), frame.data(), frame.size()) ==
        bytes.size());
    OL_CHECK(independent == bytes);
    const auto decoded = decode_scene_archive(*encoded, kind);
    OL_CHECK(decoded.has_value() && *decoded == bytes);
    std::string crlf;
    for (const auto character : *encoded) {
        if (character == '\n') {
            crlf.push_back('\r');
        }
        crlf.push_back(character);
    }
    const auto decoded_crlf = decode_scene_archive(crlf, kind);
    OL_CHECK(decoded_crlf.has_value() && *decoded_crlf == bytes);
    const auto reference = reference_frame(bytes, true, true);
    const auto decoded_reference = decode_scene_archive(reference_base64(reference), kind);
    OL_CHECK(decoded_reference.has_value() && *decoded_reference == bytes);
    OL_CHECK(!decode_scene_archive(
        reference_base64(reference_frame(bytes, false, true)), kind).has_value());
    OL_CHECK(!decode_scene_archive(
        reference_base64(reference_frame(bytes, true, false)), kind).has_value());

    auto corrupted = frame;
    corrupted.back() ^= 1U;
    OL_CHECK(!decode_scene_archive(reference_base64(corrupted), kind).has_value());
    corrupted = frame;
    corrupted.pop_back();
    OL_CHECK(!decode_scene_archive(reference_base64(corrupted), kind).has_value());
    corrupted = frame;
    corrupted.push_back(0U);
    OL_CHECK(!decode_scene_archive(reference_base64(corrupted), kind).has_value());
    corrupted = frame;
    const auto empty_frame = reference_frame({}, true, true);
    corrupted.insert(corrupted.end(), empty_frame.begin(), empty_frame.end());
    OL_CHECK(!decode_scene_archive(reference_base64(corrupted), kind).has_value());
    corrupted = frame;
    constexpr std::array<std::uint8_t, 8> skippable{0x50U, 0x2AU, 0x4DU, 0x18U, 0U, 0U, 0U, 0U};
    corrupted.insert(corrupted.end(), skippable.begin(), skippable.end());
    OL_CHECK(!decode_scene_archive(reference_base64(corrupted), kind).has_value());
    OL_CHECK(!decode_scene_archive(reference_base64(skippable), kind).has_value());
    corrupted = reference_frame(std::span{bytes}.first(bytes.size() - 1U), true, true);
    ZSTD_FrameHeader shorter_header{};
    OL_CHECK(ZSTD_getFrameHeader(&shorter_header, corrupted.data(), corrupted.size()) == 0U);
    OL_CHECK(shorter_header.headerSize >= 4U);
    if (shorter_header.headerSize >= 4U) {
        for (std::size_t byte = 0U; byte < 4U; ++byte) {
            corrupted[shorter_header.headerSize - 4U + byte] = static_cast<std::uint8_t>(
                (bytes.size() >> (8U * byte)) & 255U);
        }
        OL_CHECK(!decode_scene_archive(reference_base64(corrupted), kind).has_value());
    }

    bool checked_one_padding = false;
    bool checked_two_padding = false;
    for (const std::size_t dictionary_bytes : {0U, 1U, 2U}) {
        auto with_zero_dictionary = frame;
        if (dictionary_bytes != 0U) {
            const auto dictionary_offset = (frame[4U] & 32U) != 0U ? 5 : 6;
            with_zero_dictionary[4U] |= static_cast<std::uint8_t>(dictionary_bytes);
            with_zero_dictionary.insert(
                with_zero_dictionary.begin() + dictionary_offset, dictionary_bytes, 0U);
        }
        const auto text = reference_base64(with_zero_dictionary);
        const auto valid = decode_scene_archive(text, kind);
        OL_CHECK(valid.has_value() && *valid == bytes);
        const auto padding = text.find('=');
        if (padding != std::string::npos) {
            auto invalid_padding = text;
            const auto last_value = kAlphabet.find(text[padding - 1U]);
            invalid_padding[padding - 1U] = kAlphabet[last_value + 1U];
            OL_CHECK(!decode_scene_archive(invalid_padding, kind).has_value());
            checked_one_padding = checked_one_padding || padding + 1U == text.size();
            checked_two_padding = checked_two_padding || padding + 2U == text.size();
        }
        if (dictionary_bytes != 0U) {
            const auto dictionary_offset = (frame[4U] & 32U) != 0U ? 5U : 6U;
            with_zero_dictionary[dictionary_offset] = 1U;
            OL_CHECK(!decode_scene_archive(reference_base64(with_zero_dictionary), kind).has_value());
        }
    }
    OL_CHECK(checked_one_padding && checked_two_padding);
    for (const auto character : std::array{' ', '\t', '\v', '\f', '-', '_', '%', '\0', '\x7F'}) {
        auto invalid = *encoded;
        invalid.insert(invalid.begin() + 4, character);
        OL_CHECK(!decode_scene_archive(invalid, kind).has_value());
    }
    for (const auto text : {"", "A", "AAA", "=AAA", "A===", "AA=A", "AB==", "AAB="}) {
        OL_CHECK(!decode_scene_archive(text, kind).has_value());
    }
    OL_CHECK(!decode_scene_archive(*encoded + "AAAA", kind).has_value());
    const auto other_kind = kind == SceneArchiveKind::maps
        ? SceneArchiveKind::events : SceneArchiveKind::maps;
    OL_CHECK(!decode_scene_archive(*encoded, other_kind).has_value());
    OL_CHECK(!encode_scene_archive(bytes, other_kind).has_value());
    OL_CHECK(!encode_scene_archive(std::span{bytes}.first(bytes.size() - 1U), kind).has_value());
    OL_CHECK(!decode_scene_archive(*encoded, static_cast<SceneArchiveKind>(2)).has_value());
}

}

int main() {
    using namespace openlegend::persistence;
    const auto baseline = load_baseline(openlegend::test::game_data_root());
    OL_CHECK(baseline);
    if (baseline) {
        check_archive(baseline.snapshot->scene_maps, SceneArchiveKind::maps);
        check_archive(baseline.snapshot->scene_events, SceneArchiveKind::events);
    }
    OL_CHECK(!encode_scene_archive({}, SceneArchiveKind::maps).has_value());
    OL_CHECK(!encode_scene_archive({}, static_cast<SceneArchiveKind>(2)).has_value());
    const std::string oversized(64U * 1024U * 1024U + 1U, '\n');
    OL_CHECK(!decode_scene_archive(oversized, SceneArchiveKind::events).has_value());
    return openlegend::test::failures == 0 ? 0 : 1;
}
