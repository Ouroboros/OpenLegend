#include "openlegend/attributes.hpp"
#include "openlegend/text/big5.hpp"

#include <algorithm>
#include <array>

namespace openlegend::text {
namespace {

constexpr std::u32string_view kUnicodeCharacters =
    U"一三下不中之了二人仍休住体你使修個候值備儲先入內兵出刀利刪到前劍力功加助動勝十升去參又取口只合名向否命"
    U"品員嗎器在地增壞多夜夠大夫失姓字存學宮宿少屬巧已帶常幫度待得御復德必性恢息意態成戰戲所技抗抱招拳指掌提"
    U"換揮損擇擊改攻敗數方是時暗書替最會有望未束某查格樣檔次歉此武殊毒求沒治注減滿為無物特狀獲現球生用當療白"
    U"的目真睡知禦移程稍種空第筆等系級結統經編練耍聲能自與船英處號行術裝製要覺解誰請識讀資質路蹤載輕輸這造進遊"
    U"過道適選配醫門開間閱防除陰陽隊離音須驗鬥點！（），．／：＜＞？ＮＹ";

constexpr std::array<std::uint16_t, 238> kBig5Codes{
    0xA440U, 0xA454U, 0xA455U, 0xA4A3U, 0xA4A4U, 0xA4A7U, 0xA446U, 0xA447U, 0xA448U, 0xA4B4U,
    0xA5F0U, 0xA6EDU, 0xCA5EU, 0xA741U, 0xA8CFU, 0xADD7U, 0xADD3U, 0xADD4U, 0xADC8U, 0xB3C6U, 0xC078U,
    0xA5FDU, 0xA44AU, 0xA4BAU, 0xA74CU, 0xA558U, 0xA44DU, 0xA751U, 0xA752U, 0xA8ECU, 0xAB65U, 0xBC43U,
    0xA44FU, 0xA55CU, 0xA55BU, 0xA755U, 0xB0CAU, 0xB3D3U, 0xA451U, 0xA4C9U, 0xA568U, 0xB0D1U,
    0xA453U, 0xA8FAU, 0xA466U, 0xA575U, 0xA658U, 0xA657U, 0xA656U, 0xA75FU, 0xA952U, 0xAB7EU,
    0xADFBU, 0xB6DCU, 0xBEB9U, 0xA662U, 0xA661U, 0xBC57U, 0xC361U, 0xA668U, 0xA95DU, 0xB0F7U,
    0xA46AU, 0xA4D2U, 0xA5A2U, 0xA96DU, 0xA672U, 0xA673U, 0xBEC7U, 0xAE63U, 0xB14AU, 0xA4D6U,
    0xC4DDU, 0xA5A9U, 0xA477U, 0xB161U, 0xB160U, 0xC0B0U, 0xABD7U, 0xABDDU, 0xB16FU, 0xB173U,
    0xB45FU, 0xBC77U, 0xA5B2U, 0xA9CAU, 0xABECU, 0xAEA7U, 0xB74EU, 0xBA41U, 0xA6A8U, 0xBED4U, 0xC0B8U,
    0xA9D2U, 0xA7DEU, 0xA7DCU, 0xA9EAU, 0xA9DBU, 0xAEB1U, 0xABFCU, 0xB478U, 0xB4A3U, 0xB4ABU, 0xB4A7U,
    0xB76CU, 0xBEDCU, 0xC0BBU, 0xA7EFU, 0xA7F0U, 0xB1D1U, 0xBCC6U, 0xA4E8U, 0xAC4FU, 0xAEC9U,
    0xB774U, 0xAED1U, 0xB4C0U, 0xB3CCU, 0xB77CU, 0xA6B3U, 0xB1E6U, 0xA5BCU, 0xA7F4U, 0xAC59U, 0xAC64U,
    0xAEE6U, 0xBCCBU, 0xC0C9U, 0xA6B8U, 0xBA70U, 0xA6B9U, 0xAA5AU, 0xAEEDU, 0xAC72U, 0xA844U,
    0xA853U, 0xAA76U, 0xAA60U, 0xB4EEU, 0xBAA1U, 0xACB0U, 0xB54CU, 0xAAABU, 0xAF53U, 0xAAACU,
    0xC0F2U, 0xB27BU, 0xB279U, 0xA5CDU, 0xA5CEU, 0xB7EDU, 0xC0F8U, 0xA5D5U, 0xAABAU, 0xA5D8U, 0xAF75U,
    0xBACEU, 0xAABEU, 0xBF6DU, 0xB2BEU, 0xB57BU, 0xB579U, 0xBAD8U, 0xAAC5U, 0xB2C4U, 0xB5A7U,
    0xB5A5U, 0xA874U, 0xAFC5U, 0xB5B2U, 0xB2CEU, 0xB867U, 0xBD73U, 0xBD6DU, 0xAD41U, 0xC16EU, 0xAFE0U,
    0xA6DBU, 0xBB50U, 0xB2EEU, 0xAD5EU, 0xB342U, 0xB8B9U, 0xA6E6U, 0xB34EU, 0xB8CBU, 0xBB73U,
    0xAD6EU, 0xC4B1U, 0xB8D1U, 0xBDD6U, 0xBDD0U, 0xC3D1U, 0xC5AAU, 0xB8EAU, 0xBDE8U, 0xB8F4U,
    0xC2DCU, 0xB8FCU, 0xBBB4U, 0xBFE9U, 0xB36FU, 0xB379U, 0xB669U, 0xB943U, 0xB94CU, 0xB944U, 0xBE41U,
    0xBFEFU, 0xB074U, 0xC2E5U, 0xAAF9U, 0xB67DU, 0xB6A1U, 0xBE5CU, 0xA8BEU, 0xB0A3U, 0xB3B1U,
    0xB6A7U, 0xB6A4U, 0xC2F7U, 0xADB5U, 0xB6B7U, 0xC5E7U, 0xB0ABU, 0xC249U, 0xA149U, 0xA15DU,
    0xA15EU, 0xA141U, 0xA144U, 0xA1FEU, 0xA147U, 0xA1D5U, 0xA1D6U, 0xA148U, 0xA2DCU, 0xA2E7U,
};

static_assert(kUnicodeCharacters.size() == kBig5Codes.size());

NODISCARD constexpr bool continuation(const std::uint8_t value) noexcept {
    return value >= 0x80U && value <= 0xBFU;
}

}  // namespace

std::optional<char32_t> decode_next_utf8(
    const std::u8string_view value,
    std::size_t& offset) noexcept {
    if (offset >= value.size()) {
        return std::nullopt;
    }

    const auto byte = [&value](const std::size_t index) {
        return static_cast<std::uint8_t>(value[index]);
    };
    const auto first = byte(offset);
    if (first <= 0x7FU) {
        ++offset;
        return static_cast<char32_t>(first);
    }

    std::size_t length{};
    char32_t code_point{};
    if (first >= 0xC2U && first <= 0xDFU) {
        length = 2U;
        code_point = static_cast<char32_t>(first & 0x1FU);
    } else if (first >= 0xE0U && first <= 0xEFU) {
        length = 3U;
        code_point = static_cast<char32_t>(first & 0x0FU);
    } else if (first >= 0xF0U && first <= 0xF4U) {
        length = 4U;
        code_point = static_cast<char32_t>(first & 0x07U);
    } else {
        return std::nullopt;
    }
    if (value.size() - offset < length) {
        return std::nullopt;
    }
    for (std::size_t index = 1U; index < length; ++index) {
        const auto next = byte(offset + index);
        if (!continuation(next)) {
            return std::nullopt;
        }
        code_point = static_cast<char32_t>((code_point << 6U) | (next & 0x3FU));
    }
    if ((length == 3U && code_point < 0x800U) ||
        (length == 4U && code_point < 0x10000U) ||
        (code_point >= 0xD800U && code_point <= 0xDFFFU) ||
        code_point > 0x10FFFFU) {
        return std::nullopt;
    }
    offset += length;
    return code_point;
}

std::optional<std::uint16_t> big5_code_for_unicode(const char32_t code_point) noexcept {
    if (code_point <= 0x7FU) {
        return static_cast<std::uint16_t>(code_point);
    }
    const auto found = std::lower_bound(
        kUnicodeCharacters.begin(), kUnicodeCharacters.end(), code_point);
    if (found == kUnicodeCharacters.end() || *found != code_point) {
        return std::nullopt;
    }
    return kBig5Codes[static_cast<std::size_t>(found - kUnicodeCharacters.begin())];
}

bool append_big5(
    std::vector<std::uint8_t>& destination,
    const std::u8string_view utf8_text) {
    const auto original_size = destination.size();
    destination.reserve(destination.size() + utf8_text.size());
    std::size_t offset = 0U;
    while (offset < utf8_text.size()) {
        const auto code_point = decode_next_utf8(utf8_text, offset);
        if (!code_point.has_value()) {
            destination.resize(original_size);
            return false;
        }
        const auto code = big5_code_for_unicode(*code_point);
        if (!code.has_value() || *code == 0U) {
            destination.resize(original_size);
            return false;
        }
        if (*code <= 0x7FU) {
            destination.push_back(static_cast<std::uint8_t>(*code));
        } else {
            destination.push_back(static_cast<std::uint8_t>(*code >> 8U));
            destination.push_back(static_cast<std::uint8_t>(*code));
        }
    }
    return true;
}

std::optional<std::vector<std::uint8_t>> encode_big5(
    const std::u8string_view utf8_text) {
    std::vector<std::uint8_t> result;
    if (!append_big5(result, utf8_text)) {
        return std::nullopt;
    }
    return result;
}

}  // namespace openlegend::text
