#include "openlegend/scene/scene.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "openlegend/compat/byte_reader.hpp"
#include "openlegend/diagnostics/log.hpp"
#include "openlegend/render/legacy_font_renderer.hpp"
#include "openlegend/render/rle_sprite_renderer.hpp"
#include "openlegend/resource/legacy_assets.hpp"
#include "openlegend/resource/legacy_sprite.hpp"

namespace openlegend::scene {
namespace {

constexpr std::array<std::int16_t, 4> kPlayerFrameBase{5002, 5016, 5030, 5044};
constexpr std::array<std::int16_t, 9> kBlockedEarthLow{
    358, 374, 458, 506, 818, 838, 934, 1016, 1324};
constexpr std::array<std::int16_t, 9> kBlockedEarthHigh{
    362, 380, 464, 610, 824, 838, 936, 1022, 1348};
constexpr std::array<std::int16_t, 11> kWeatherSceneIds{5, 7, 10, 41, 42, 46, 65, 66, 67, 72, 79};
constexpr std::array<std::int16_t, 30> kTournamentHeadIds{
    8, 21, 23, 31, 32, 43, 7, 11, 14, 20, 33, 34, 10, 12, 19,
    22, 56, 68, 13, 55, 62, 67, 70, 71, 26, 57, 60, 64, 3, 69};
constexpr std::array<std::array<std::uint8_t, 11>, 4> kProgressMenuItems{{
    {0xB8U, 0xFCU, 0xA4U, 0x4AU, 0xB6U, 0x69U, 0xABU, 0xD7U, 0xA4U, 0x40U, 0x00U},
    {0xB8U, 0xFCU, 0xA4U, 0x4AU, 0xB6U, 0x69U, 0xABU, 0xD7U, 0xA4U, 0x47U, 0x00U},
    {0xB8U, 0xFCU, 0xA4U, 0x4AU, 0xB6U, 0x69U, 0xABU, 0xD7U, 0xA4U, 0x54U, 0x00U},
    {0xC2U, 0xF7U, 0xB6U, 0x7DU, 0xBAU, 0xCEU, 0xC4U, 0xB1U, 0xA5U, 0x68U, 0x00U},
}};
constexpr std::array<std::uint8_t, 13> kDeathLocationText{
    0xA6U, 0x62U, 0xA6U, 0x61U, 0xB2U, 0x79U, 0xAAU, 0xBAU, 0xACU, 0x59U, 0xB3U, 0x42U, 0x00U};
constexpr std::array<std::uint8_t, 17> kDeathMissingText{
    0xB7U, 0xEDU, 0xA6U, 0x61U, 0xA4U, 0x48U, 0xA4U, 0x66U, 0xAAU, 0xBAU, 0xA5U, 0xA2U, 0xC2U, 0xDCU, 0xBCU, 0xC6U, 0x00U};
constexpr std::array<std::uint8_t, 17> kDeathAnotherText{
    0xA4U, 0x53U, 0xA6U, 0x68U, 0xA4U, 0x46U, 0xA4U, 0x40U, 0xB5U, 0xA7U, 0xA1U, 0x44U, 0xA1U, 0x44U, 0xA1U, 0x44U, 0x00U};
constexpr std::array<std::uint8_t, 23> kExitPrompt{
    0xAFU, 0x75U, 0xADU, 0x6EU, 0xC2U, 0xF7U, 0xB6U, 0x7DU, 0xB9U, 0x43U, 0xC0U, 0xB8U,
    0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU, 0xA2U, 0xDCU, 0xA1U, 0x5EU, 0x00U};
constexpr std::array<std::uint8_t, 23> kBattleQuestion{
    0xACU, 0x4FU, 0xA7U, 0x5FU, 0xBBU, 0x50U, 0xA4U, 0xA7U, 0xB9U, 0x4CU, 0xA9U, 0xDBU,
    0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU, 0xA2U, 0xDCU, 0xA1U, 0x5EU, 0x00U};
constexpr std::array<std::uint8_t, 23> kJoinQuestion{
    0xACU, 0x4FU, 0xA7U, 0x5FU, 0xADU, 0x6EU, 0xA8U, 0x44U, 0xA5U, 0x5BU, 0xA4U, 0x4AU,
    0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU, 0xA2U, 0xDCU, 0xA1U, 0x5EU, 0x00U};
constexpr std::array<std::uint8_t, 23> kRestQuestion{
    0xACU, 0x4FU, 0xA7U, 0x5FU, 0xA6U, 0xEDU, 0xB1U, 0x4AU, 0xB9U, 0x4CU, 0xA9U, 0x5DU,
    0xA1U, 0x5DU, 0xA2U, 0xE7U, 0xA1U, 0xFEU, 0xA2U, 0xDCU, 0xA1U, 0x5EU, 0x00U};
constexpr std::array<std::int16_t, 20> kEndingCreditIds{
    6, 8, 10, 12, 14, 16, 18, 20, 22, 24,
    26, 28, 30, 32, 34, 36, 38, 40, 42, 44};
constexpr std::array<int, 20> kEndingCreditX{
    60, 60, 60, 60, 115, 115, 115, 115, 115, 115,
    115, 115, 115, 115, 115, 115, 115, 105, 135, 56};
constexpr std::array<int, 20> kEndingCreditInitialY{
    210, 386, 551, 698, 950, 1111, 1255, 1391, 1557, 1718,
    1772, 1938, 2087, 2195, 2335, 2479, 2642, 2743, 3055, 3301};
constexpr std::array<std::size_t, 68> kInstructionWidths{
    1, 4, 3, 14, 4, 3, 5, 1, 2, 3, 2, 3, 1, 1, 1, 2, 4,
    6, 4, 3, 3, 2, 1, 3, 1, 5, 6, 4, 6, 6, 5, 4, 3, 4,
    3, 5, 4, 2, 5, 2, 2, 4, 3, 4, 7, 3, 3, 3, 3, 3, 8,
    1, 1, 1, 1, 5, 2, 1, 1, 1, 6, 3, 7, 3, 1, 1, 2, 2};

[[nodiscard]] SceneDate current_local_date() noexcept {
    const auto now = std::time(nullptr);
    std::tm value{};
#if defined(_WIN32)
    if (localtime_s(&value, &now) != 0) {
        return {};
    }
#else
    if (localtime_r(&now, &value) == nullptr) {
        return {};
    }
#endif
    return SceneDate{value.tm_year + 1900, value.tm_mon + 1, value.tm_mday};
}

[[nodiscard]] constexpr std::uint16_t legacy_delay_ticks(const int delay) noexcept {
    return static_cast<std::uint16_t>(delay / 40 + 1);
}

[[nodiscard]] constexpr std::size_t tile_index(const int x, const int y) noexcept {
    return static_cast<std::size_t>(y) * model::kSceneCoordinateCount +
           static_cast<std::size_t>(x);
}

[[nodiscard]] constexpr std::string_view direction_name(
    const SceneDirection direction) noexcept {
    switch (direction) {
    case SceneDirection::up: return "up";
    case SceneDirection::right: return "right";
    case SceneDirection::left: return "left";
    case SceneDirection::down: return "down";
    }
    return "unknown";
}

[[nodiscard]] constexpr std::pair<int, int> direction_delta(
    const SceneDirection direction) noexcept {
    switch (direction) {
    case SceneDirection::up: return {0, -1};
    case SceneDirection::right: return {1, 0};
    case SceneDirection::left: return {-1, 0};
    case SceneDirection::down: return {0, 1};
    }
    return {0, 0};
}

[[nodiscard]] bool blocked_earth(const std::int16_t value) noexcept {
    for (std::size_t index = 0U; index < kBlockedEarthLow.size(); ++index) {
        if (value >= kBlockedEarthLow[index] && value <= kBlockedEarthHigh[index]) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::vector<std::uint8_t> ascii_message(const std::string& text) {
    std::vector<std::uint8_t> result;
    result.reserve(text.size() + 1U);
    for (const auto character : text) {
        result.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(character)));
    }
    result.push_back(0U);
    return result;
}

[[nodiscard]] std::vector<std::uint8_t> status_notice_message(
    const bool fame,
    const std::int16_t value) {
    constexpr std::array<std::uint8_t, 18> morality_prefix{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xAAU, 0xBAU, 0xABU,
        0x7EU, 0xBCU, 0x77U, 0xABU, 0xFCU, 0xBCU, 0xC6U, 0xACU, 0xB0U};
    constexpr std::array<std::uint8_t, 20> fame_prefix{
        0xA7U, 0x41U, 0xB2U, 0x7BU, 0xA6U, 0x62U, 0xADU, 0xD3U, 0xA4U, 0x48U,
        0xC1U, 0x6EU, 0xB1U, 0xE6U, 0xABU, 0xFCU, 0xBCU, 0xC6U, 0xACU, 0xB0U};
    std::vector<std::uint8_t> result;
    const auto prefix = fame ? std::span<const std::uint8_t>{fame_prefix}
                             : std::span<const std::uint8_t>{morality_prefix};
    result.assign(prefix.begin(), prefix.end());
    std::array<char, 16> formatted{};
    std::snprintf(
        formatted.data(), formatted.size(), fame ? "%4d" : "%5d", static_cast<int>(value));
    for (const auto character : formatted) {
        result.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(character)));
        if (character == '\0') {
            break;
        }
    }
    return result;
}

[[nodiscard]] std::int16_t wrapping_add(
    const std::int16_t value,
    const std::int16_t delta) noexcept {
    const auto bits = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(value) + static_cast<std::uint16_t>(delta));
    return static_cast<std::int16_t>(
        bits < 0x8000U ? static_cast<int>(bits) : static_cast<int>(bits) - 0x10000);
}

[[nodiscard]] std::int16_t clamped_add(
    const std::int16_t value,
    const std::int16_t delta,
    const std::int16_t minimum,
    const std::int16_t maximum) noexcept {
    return std::clamp(wrapping_add(value, delta), minimum, maximum);
}

[[nodiscard]] bool draw_dialogue_text(
    render::IndexedFramebuffer& framebuffer,
    int x,
    const int y,
    const std::span<const std::uint8_t> zero_terminated_text,
    const std::span<const std::uint8_t> ascii_font,
    render::Big5GlyphCache& big5_cache,
    const std::uint8_t right_shadow,
    const std::uint8_t foreground) noexcept {
    if (ascii_font.size() < 128U * 16U) {
        return false;
    }
    const auto draw_glyph = [&framebuffer, y, right_shadow, foreground](
                                const int glyph_x,
                                const std::span<const std::uint8_t> glyph,
                                const int glyph_width) {
        auto pixels = framebuffer.pixels();
        for (int row = 0; row < 16; ++row) {
            for (int byte_index = 0; byte_index < glyph_width / 8; ++byte_index) {
                const auto bits = glyph[static_cast<std::size_t>(row * glyph_width / 8 + byte_index)];
                for (int bit = 0; bit < 8; ++bit) {
                    if ((bits & static_cast<std::uint8_t>(0x80U >> bit)) == 0U) {
                        continue;
                    }
                    const auto destination = static_cast<std::ptrdiff_t>(
                        (y + row) * render::IndexedFramebuffer::width + glyph_x +
                        byte_index * 8 + bit);
                    if (destination < 0 ||
                        static_cast<std::size_t>(destination + 1) >= pixels.size()) {
                        return false;
                    }
                    pixels[static_cast<std::size_t>(destination)] = foreground;
                    pixels[static_cast<std::size_t>(destination + 1)] = right_shadow;
                }
            }
        }
        return true;
    };

    for (std::size_t index = 0U; index < zero_terminated_text.size();) {
        const auto first = zero_terminated_text[index++];
        if (first == 0U) {
            return true;
        }
        if (first > 0x7FU) {
            if (index >= zero_terminated_text.size()) {
                return false;
            }
            const auto second = zero_terminated_text[index++];
            const auto code = static_cast<std::uint16_t>(
                static_cast<std::uint16_t>(first) << 8U | static_cast<std::uint16_t>(second));
            const auto glyph = big5_cache.resolve(code);
            if (!glyph || !draw_glyph(x, *glyph, 16)) {
                return false;
            }
            x += 16;
            continue;
        }
        const auto glyph_offset = static_cast<std::size_t>(first) * 16U;
        const auto glyph = std::span<const std::uint8_t, 16>{
            ascii_font.data() + glyph_offset, 16U};
        if (!draw_glyph(x, glyph, 8)) {
            return false;
        }
        x += 8;
    }
    return false;
}

}  // namespace

std::vector<std::vector<std::uint8_t>> paginate_dialogue(
    const std::span<const std::uint8_t> zero_terminated_text) {
    std::vector<std::vector<std::uint8_t>> pages;
    std::vector<std::uint8_t> page;
    page.reserve(zero_terminated_text.size());
    std::size_t line_break_count = 0U;
    const auto flush_page = [&pages, &page, &line_break_count]() {
        page.push_back(0U);
        pages.push_back(std::move(page));
        page.clear();
        line_break_count = 0U;
    };

    for (std::size_t index = 0U;
         index < zero_terminated_text.size() && zero_terminated_text[index] != 0U;) {
        const auto first = zero_terminated_text[index];
        if (first == static_cast<std::uint8_t>('*')) {
            page.push_back(first);
            ++line_break_count;
            ++index;
            if (line_break_count == 3U) {
                flush_page();
            }
            continue;
        }
        const auto token_size = first > 0x7FU ? 2U : 1U;
        if (index + token_size > zero_terminated_text.size()) {
            break;
        }
        page.insert(
            page.end(),
            zero_terminated_text.begin() + static_cast<std::ptrdiff_t>(index),
            zero_terminated_text.begin() + static_cast<std::ptrdiff_t>(index + token_size));
        index += token_size;
    }
    flush_page();
    return pages;
}

SceneAssets::SceneAssets(const resource::DataRoot& data_root)
    : talks_(resource::PackedArchive::open(
          data_root.path() / "TALK.IDX", data_root.path() / "TALK.GRP")),
      scripts_(resource::PackedArchive::open(
          data_root.path() / "KDEF.IDX", data_root.path() / "KDEF.GRP")) {
    if (!talks_.valid()) {
        error_ = talks_.error();
    } else if (!scripts_.valid()) {
        error_ = scripts_.error();
    } else if (talks_.entry_count() != kTalkCount) {
        error_ = "TALK archive does not contain 2977 records";
    } else if (scripts_.entry_count() != kEventScriptCount) {
        error_ = "KDEF archive does not contain 1018 scripts";
    }
}

std::vector<std::uint8_t> SceneAssets::talk(const std::size_t talk_id) const {
    const auto source = talks_.entry(talk_id);
    std::vector<std::uint8_t> result;
    result.reserve(source.size());
    for (const auto value : source) {
        if (value == 0U) {
            break;
        }
        result.push_back(static_cast<std::uint8_t>(value ^ 0xFFU));
    }
    result.push_back(0U);
    return result;
}

std::vector<std::int16_t> SceneAssets::script(const std::size_t script_id) const {
    const auto source = scripts_.entry(script_id);
    if (source.size() % 2U != 0U) {
        return {};
    }
    std::vector<std::int16_t> result(source.size() / 2U);
    for (std::size_t index = 0U; index < result.size(); ++index) {
        result[index] = compat::read_i16le(source, index * 2U);
    }
    return result;
}

SceneSession::SceneSession(
    const resource::DataRoot& data_root,
    model::GameSnapshot& snapshot,
    random::LegacyRandom& random,
    const std::int16_t scene_id,
    const bool use_jump_entrance,
    std::optional<SceneDate> death_date_override,
    const std::int16_t periodic_counter)
    : data_root_(data_root),
      snapshot_(snapshot),
      random_(random),
      death_date_override_(death_date_override),
      assets_(data_root),
      portraits_(resource::PackedArchive::open(
          data_root.path() / "HDGRP.IDX", data_root.path() / "HDGRP.GRP")),
      weather_sprites_(resource::PackedArchive::open(
          data_root.path() / "CLOUD.IDX", data_root.path() / "CLOUD.GRP")),
      scene_id_(scene_id),
      periodic_counter_(periodic_counter) {
    if (!snapshot_.valid()) {
        error_ = "scene session requires a valid game snapshot";
        return;
    }
    if (!assets_.valid()) {
        error_ = assets_.error();
        return;
    }
    if (!portraits_.valid()) {
        error_ = portraits_.error();
        return;
    }
    if (!weather_sprites_.valid()) {
        error_ = weather_sprites_.error();
        return;
    }
    if (scene_id_ < 0 || static_cast<std::size_t>(scene_id_) >= model::kSceneCount) {
        error_ = "scene id is outside the 100-scene archive";
        return;
    }
    if (!load_scene_sprites()) {
        return;
    }
    const auto palette_file = data_root_.read("MMAP.COL");
    if (!palette_file) {
        error_ = palette_file.error;
        return;
    }
    const auto palette = resource::parse_vga_palette(palette_file.bytes);
    if (!palette) {
        error_ = palette.error;
        return;
    }
    palette_ = palette.palette;
    for (int red = 0; red < 16; ++red) {
        for (int green = 0; green < 16; ++green) {
            for (int blue = 0; blue < 16; ++blue) {
                auto best_distance = 30'000;
                std::uint8_t best_index{};
                for (std::size_t palette_index = 0U; palette_index < palette_.size();
                     ++palette_index) {
                    const auto target_red = red * 4 + 2;
                    const auto target_green = green * 4 + 2;
                    const auto target_blue = blue * 4 + 2;
                    const auto red_delta = target_red - palette_[palette_index].red;
                    const auto green_delta = target_green - palette_[palette_index].green;
                    const auto blue_delta = target_blue - palette_[palette_index].blue;
                    const auto distance = red_delta * red_delta + green_delta * green_delta +
                                          blue_delta * blue_delta;
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = static_cast<std::uint8_t>(palette_index);
                    }
                }
                rgb4_lookup_[static_cast<std::size_t>(red * 256 + green * 16 + blue)] = best_index;
            }
        }
    }
    weather_enabled_ = std::find(kWeatherSceneIds.begin(), kWeatherSceneIds.end(), scene_id_) !=
                       kWeatherSceneIds.end();
    auto ascii = data_root_.read("FONT.X16");
    auto big5 = data_root_.read("FONT.C16");
    if (!ascii) {
        error_ = ascii.error;
        return;
    }
    if (!big5) {
        error_ = big5.error;
        return;
    }
    ascii_font_ = std::move(ascii.bytes);
    big5_font_ = std::move(big5.bytes);

    if (static_cast<std::size_t>(scene_id_) < snapshot_.ranger.scenes.size()) {
        const auto& metadata = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)];
        const auto x_word = use_jump_entrance ? model::scene_metadata_word::jump_return_x
                                              : model::scene_metadata_word::entrance_x;
        const auto y_word = use_jump_entrance ? model::scene_metadata_word::jump_return_y
                                              : model::scene_metadata_word::entrance_y;
        scene_x_ = std::clamp<int>(metadata.word(x_word), 0, kSceneExtent - 1);
        scene_y_ = std::clamp<int>(metadata.word(y_word), 0, kSceneExtent - 1);
    } else {
        scene_x_ = std::clamp<int>(snapshot_.ranger.header.word(model::header_word::sub_map_x), 0, kSceneExtent - 1);
        scene_y_ = std::clamp<int>(snapshot_.ranger.header.word(model::header_word::sub_map_y), 0, kSceneExtent - 1);
    }
    direction_ = static_cast<SceneDirection>(std::clamp<std::int16_t>(
        snapshot_.ranger.header.word(model::header_word::face_towards), 0, 3));
    update_view_origin();
    commit_header();
    queue_scene_music(model::scene_metadata_word::entrance_music);
    idle_tick();
    continuation_ = PendingContinuation::scene_entry;
    pending_ = current_result(SceneStepKind::fade_from_black);
}

SceneStepResult SceneSession::current_result(const SceneStepKind kind) const noexcept {
    SceneStepResult result;
    result.kind = kind;
    result.scene_id = scene_id_;
    result.scene_x = static_cast<std::int16_t>(scene_x_);
    result.scene_y = static_cast<std::int16_t>(scene_y_);
    return result;
}

SceneStepResult SceneSession::show_scene_title() {
    pending_ = current_result(SceneStepKind::scene_title);
    pending_text_.clear();
    if (static_cast<std::size_t>(scene_id_) < snapshot_.ranger.scenes.size()) {
        const auto& bytes = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)].bytes;
        pending_text_.assign(bytes.begin() + 2, bytes.begin() + 12);
        pending_text_.push_back(0U);
    }
    return pending_;
}

bool SceneSession::load_scene_sprites() {
    std::array<char, 7> index_name{};
    std::array<char, 7> group_name{};
    static_cast<void>(std::snprintf(index_name.data(), index_name.size(), "SDX%03d", scene_id_));
    static_cast<void>(std::snprintf(group_name.data(), group_name.size(), "SMP%03d", scene_id_));
    const auto index = data_root_.read(index_name.data());
    auto group = data_root_.read(group_name.data());
    if (!index) {
        error_ = index.error;
        return false;
    }
    if (!group) {
        error_ = group.error;
        return false;
    }
    sprites_ = resource::SentinelArchive::parse(index.bytes, std::move(group.bytes));
    if (!sprites_.valid()) {
        error_ = sprites_.error();
        return false;
    }
    return true;
}

SceneStepResult SceneSession::tick(
    const std::optional<SceneDirection> direction,
    const bool interact_requested,
    const bool ui_requested) {
    if (!valid() || pending_.kind != SceneStepKind::stay) {
        return pending_;
    }
    idle_tick();
    auto result = current_result(SceneStepKind::stay);
    if (direction.has_value()) {
        result = move(*direction);
    } else if (interact_requested) {
        result = interact();
    } else if (ui_requested) {
        result = open_ui();
    }
    if (result.kind != SceneStepKind::stay && result.kind != SceneStepKind::moved) {
        tick_continuation_ = TickContinuation::after_action;
        tick_fallback_ = SceneStepKind::stay;
        return result;
    }
    return finish_tick_after_action(result.kind);
}

SceneStepResult SceneSession::move(const SceneDirection direction) {
    if (!valid() || pending_.kind != SceneStepKind::stay) {
        diagnostics::log_debug(
            "scene move ignored scene=" + std::to_string(scene_id_) +
            " pending=" + std::to_string(static_cast<int>(pending_.kind)));
        return pending_;
    }
    const auto source_x = scene_x_;
    const auto source_y = scene_y_;
    player_frame_override_.reset();
    direction_ = direction;
    walk_frame_offset_ = static_cast<std::int16_t>(walk_frame_offset_ + 2);
    if (walk_frame_offset_ > 12) {
        walk_frame_offset_ = 2;
    }
    const auto [delta_x, delta_y] = direction_delta(direction);
    const auto target_x = std::clamp(scene_x_ + delta_x, 0, kSceneExtent - 1);
    const auto target_y = std::clamp(scene_y_ + delta_y, 0, kSceneExtent - 1);
    if (!target_is_walkable(target_x, target_y)) {
        diagnostics::log_info(
            "scene blocked scene=" + std::to_string(scene_id_) +
            " direction=" + std::string{direction_name(direction)} +
            " from=" + std::to_string(source_x) + "," + std::to_string(source_y) +
            " target=" + std::to_string(target_x) + "," + std::to_string(target_y) +
            " frame=" + std::to_string(player_frame()) +
            " earth=" + std::to_string(scene_value(
                scene_id_, static_cast<std::int16_t>(model::SceneLayer::earth),
                static_cast<std::int16_t>(target_x), static_cast<std::int16_t>(target_y))) +
            " building=" + std::to_string(scene_value(
                scene_id_, static_cast<std::int16_t>(model::SceneLayer::building),
                static_cast<std::int16_t>(target_x), static_cast<std::int16_t>(target_y))) +
            " height=" + std::to_string(scene_value(
                scene_id_, static_cast<std::int16_t>(model::SceneLayer::building_height),
                static_cast<std::int16_t>(target_x), static_cast<std::int16_t>(target_y))));
        commit_header();
        return current_result(SceneStepKind::stay);
    }
    scene_x_ = target_x;
    scene_y_ = target_y;
    if (weather_active_) {
        for (auto& particle : weather_) {
            if (delta_y != 0) {
                particle.x = static_cast<std::int16_t>(particle.x + 18 * delta_y);
                particle.y = static_cast<std::int16_t>(particle.y - 9 * delta_y);
            } else {
                particle.x = static_cast<std::int16_t>(particle.x - 18 * delta_x);
                particle.y = static_cast<std::int16_t>(particle.y - 9 * delta_x);
            }
        }
    }
    update_view_origin();
    commit_header();
    diagnostics::log_info(
        "scene moved scene=" + std::to_string(scene_id_) +
        " direction=" + std::string{direction_name(direction)} +
        " from=" + std::to_string(source_x) + "," + std::to_string(source_y) +
        " to=" + std::to_string(scene_x_) + "," + std::to_string(scene_y_) +
        " frame=" + std::to_string(player_frame()) +
        " view_origin=" + std::to_string(view_origin_x_) + "," +
        std::to_string(view_origin_y_));

    return current_result(SceneStepKind::moved);
}

SceneStepResult SceneSession::interact() {
    if (!valid() || pending_.kind != SceneStepKind::stay) {
        return pending_;
    }
    const auto [delta_x, delta_y] = direction_delta(direction_);
    const auto x = scene_x_ + delta_x;
    const auto y = scene_y_ + delta_y;
    const auto event = event_at(x, y);
    if (event.has_value()) {
        event_context_ = EventContext{
            *event, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), -1};
        const auto script = event_field(scene_id_, *event, model::SceneEventField::event_1);
        if (script.has_value() && *script > 0) {
            (void)prepare_event(
                *script, *event, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), -1);
        }
    }
    pending_ = current_result(SceneStepKind::present);
    return pending_;
}

SceneStepResult SceneSession::use_item(const std::int16_t item_id) {
    if (!valid() || pending_.kind != SceneStepKind::stay) {
        return pending_;
    }
    const auto [delta_x, delta_y] = direction_delta(direction_);
    const auto x = scene_x_ + delta_x;
    const auto y = scene_y_ + delta_y;
    const auto event = event_at(x, y);
    if (!event.has_value()) {
        return current_result(SceneStepKind::stay);
    }
    event_context_ = EventContext{
        *event, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), item_id};
    const auto script = event_field(scene_id_, *event, model::SceneEventField::event_2);
    if (script.has_value() && *script > 0) {
        (void)prepare_event(
            *script, *event, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), item_id);
    }
    pending_ = current_result(SceneStepKind::present);
    return pending_;
}

SceneStepResult SceneSession::use_menu_item(const std::int16_t item_id) {
    if (!valid() || pending_.kind != SceneStepKind::open_ui) {
        return pending_;
    }
    pending_menu_item_ = item_id;
    pending_ = current_result(SceneStepKind::present);
    return pending_;
}

SceneStepResult SceneSession::open_ui() noexcept {
    pending_ = current_result(SceneStepKind::open_ui);
    return pending_;
}

SceneStepResult SceneSession::begin_event(
    const std::int16_t script_id,
    const std::int16_t event_index,
    const std::int16_t event_x,
    const std::int16_t event_y,
    const std::int16_t item_id) {
    if (!prepare_event(script_id, event_index, event_x, event_y, item_id)) {
        return current_result(SceneStepKind::stay);
    }
    return run_event();
}

bool SceneSession::prepare_event(
    const std::int16_t script_id,
    const std::int16_t event_index,
    const std::int16_t event_x,
    const std::int16_t event_y,
    const std::int16_t item_id) {
    if (!valid() || script_id <= 0 || static_cast<std::size_t>(script_id) >= assets_.script_count()) {
        return false;
    }
    event_context_ = EventContext{event_index, event_x, event_y, item_id};
    script_ = assets_.script(static_cast<std::size_t>(script_id));
    if (script_.empty()) {
        error_ = "KDEF script is empty or has odd byte length";
        return false;
    }
    program_counter_ = 0;
    event_active_ = true;
    pending_ = current_result(SceneStepKind::stay);
    pending_text_.clear();
    continuation_ = PendingContinuation::none;
    pan_state_.reset();
    picture_animation_state_.reset();
    scripted_walk_state_.reset();
    dual_picture_animation_state_.reset();
    three_statue_animation_state_.reset();
    load_menu_state_.reset();
    death_menu_state_.reset();
    ending_state_.reset();
    tournament_trial_state_.reset();
    queued_outputs_.clear();
    return true;
}

SceneStepResult SceneSession::resume(const SceneResponse response, const int value) {
    if (!valid()) {
        return current_result(SceneStepKind::stay);
    }
    const auto previous_kind = pending_.kind;
    const auto previous_question = pending_.question;
    const auto previous_shop_id = pending_.shop_id;
    pending_ = current_result(SceneStepKind::stay);
    pending_text_.clear();

    if (previous_kind == SceneStepKind::fade_to_black &&
        continuation_ == PendingContinuation::scene_jump) {
        return complete_scene_jump();
    }
    if (previous_kind == SceneStepKind::fade_to_black &&
        continuation_ == PendingContinuation::scene_exit) {
        continuation_ = PendingContinuation::none;
        snapshot_.ranger.header.set_word(model::header_word::in_sub_map, 0);
        if (exit_music_override_ >= 0) {
            audio_commands_.push_back(SceneAudioCommand{
                SceneAudioCommand::Kind::music, exit_music_override_, true});
        } else {
            queue_scene_music(model::scene_metadata_word::exit_music);
        }
        exit_music_override_ = -1;
        weather_enabled_ = false;
        weather_active_ = false;
        pending_ = current_result(SceneStepKind::return_world);
        return pending_;
    }
    if (previous_kind == SceneStepKind::fade_from_black &&
        continuation_ == PendingContinuation::scene_entry) {
        continuation_ = PendingContinuation::none;
        return show_scene_title();
    }
    if (previous_kind == SceneStepKind::scene_title) {
        continuation_ = PendingContinuation::scene_title;
        pending_ = current_result(SceneStepKind::present);
        return pending_;
    }
    if (previous_kind == SceneStepKind::present && pending_menu_item_.has_value()) {
        const auto item_id = *pending_menu_item_;
        pending_menu_item_.reset();
        const auto result = use_item(item_id);
        if (result.kind == SceneStepKind::stay) {
            pending_ = current_result(SceneStepKind::open_ui);
            return pending_;
        }
        menu_item_event_active_ = true;
        return result;
    }
    if (previous_kind == SceneStepKind::present &&
        tick_continuation_ == TickContinuation::after_scene_present) {
        return finish_tick_after_scene_present(tick_fallback_);
    }
    if (previous_kind == SceneStepKind::return_world || previous_kind == SceneStepKind::quit) {
        pending_.kind = previous_kind;
        return pending_;
    }
    if (previous_kind == SceneStepKind::open_ui) {
        if (tick_continuation_ == TickContinuation::after_action) {
            return finish_tick_after_action(tick_fallback_);
        }
        pending_.kind = previous_kind;
        return pending_;
    }
    if (load_menu_state_.has_value()) {
        return advance_load_menu(value);
    }
    if (death_menu_state_.has_value()) {
        return advance_death_menu(value);
    }
    if (ending_state_.has_value()) {
        return advance_ending();
    }
    if (pan_state_.has_value()) {
        if (auto frame = advance_pan_frame(); frame.has_value()) {
            return *frame;
        }
    }
    if (picture_animation_state_.has_value()) {
        if (auto frame = advance_picture_animation_frame(); frame.has_value()) {
            return *frame;
        }
    }
    if (scripted_walk_state_.has_value()) {
        if (auto frame = advance_scripted_walk_frame(); frame.has_value()) {
            return *frame;
        }
    }
    if (dual_picture_animation_state_.has_value()) {
        if (auto frame = advance_dual_picture_animation_frame(); frame.has_value()) {
            return *frame;
        }
    }
    if (three_statue_animation_state_.has_value()) {
        if (auto frame = advance_three_statue_animation_frame(); frame.has_value()) {
            return *frame;
        }
    }
    if (tournament_trial_state_.has_value() && queued_outputs_.empty()) {
        if (auto step = advance_tournament_trial(previous_kind, response); step.has_value()) {
            return *step;
        }
    }

    if (continuation_ == PendingContinuation::scene_title) {
        continuation_ = PendingContinuation::none;
        return run_auto_event(SceneStepKind::stay);
    }

    if (continuation_ == PendingContinuation::shop_feedback && queued_outputs_.empty()) {
        close_shop_events();
        continuation_ = PendingContinuation::none;
    }

    if (continuation_ == PendingContinuation::conditional) {
        const auto accepted = response == SceneResponse::yes;
        const auto offset = accepted ? true_offset_ : false_offset_;
        if (previous_kind == SceneStepKind::question &&
            previous_question == SceneQuestion::join) {
            true_offset_ = offset;
            continuation_ = PendingContinuation::conditional_after_present;
            pending_ = current_result(SceneStepKind::present);
            return pending_;
        }
        program_counter_ += offset;
        continuation_ = PendingContinuation::none;
    } else if (continuation_ == PendingContinuation::conditional_after_present) {
        program_counter_ += true_offset_;
        continuation_ = PendingContinuation::none;
    } else if (continuation_ == PendingContinuation::battle) {
        const auto victory = response == SceneResponse::battle_victory;
        program_counter_ += victory ? true_offset_ : false_offset_;
        continuation_ = PendingContinuation::none;
    } else if (continuation_ == PendingContinuation::shop) {
        bool has_feedback = false;
        if (response == SceneResponse::yes && previous_shop_id >= 0 &&
            static_cast<std::size_t>(previous_shop_id) < snapshot_.ranger.shops.size() &&
            value >= 0 && value < 5) {
            auto& shop = snapshot_.ranger.shops[static_cast<std::size_t>(previous_shop_id)];
            const auto slot = static_cast<std::size_t>(value);
            const auto item_id = shop.word(model::shop_word::item_id_begin + slot);
            const auto stock = shop.word(model::shop_word::total_begin + slot);
            const auto price = shop.word(model::shop_word::price_begin + slot);
            if (stock > 0 && first_inventory_count(174).value_or(0) >= price) {
                change_first_inventory(174, static_cast<std::int16_t>(-price));
                add_inventory(item_id, 1);
                shop.set_word(
                    model::shop_word::total_begin + slot,
                    static_cast<std::int16_t>(stock - 1));
                queue_dialogue(2976, 111, 0);
            } else {
                queue_dialogue(2975, 111, 0);
            }
            has_feedback = true;
        }
        if (has_feedback) {
            continuation_ = PendingContinuation::shop_feedback;
        } else {
            close_shop_events();
            continuation_ = PendingContinuation::none;
        }
    } else if (previous_kind == SceneStepKind::battle &&
               response == SceneResponse::battle_defeat && !queued_outputs_.empty()) {
        queued_outputs_.clear();
        event_active_ = false;
        pending_ = current_result(SceneStepKind::quit);
        return pending_;
    }

    if (!queued_outputs_.empty()) {
        return emit_queued();
    }
    if (event_active_) {
        const auto result = run_event();
        if (result.kind != SceneStepKind::stay || event_active_) {
            return result;
        }
    }
    clear_event();
    if (menu_item_event_active_) {
        menu_item_event_active_ = false;
        pending_ = current_result(SceneStepKind::open_ui);
        return pending_;
    }
    if (tick_continuation_ == TickContinuation::after_action) {
        return finish_tick_after_action(tick_fallback_);
    }
    if (tick_continuation_ == TickContinuation::after_auto_event) {
        return finish_tick_after_auto_event(tick_fallback_);
    }
    return current_result(SceneStepKind::stay);
}

SceneStepResult SceneSession::run_event() {
    while (event_active_) {
        if (!queued_outputs_.empty()) {
            return emit_queued();
        }
        if (program_counter_ < 0 || static_cast<std::size_t>(program_counter_) >= script_.size()) {
            error_ = "KDEF program counter left the script record";
            clear_event();
            return current_result(SceneStepKind::stay);
        }
        const auto opcode = script_[static_cast<std::size_t>(program_counter_)];
        if (opcode == -1) {
            clear_event();
            return current_result(SceneStepKind::stay);
        }
        if (opcode < 0 || opcode >= static_cast<std::int16_t>(kInstructionWidths.size())) {
            pending_ = current_result(SceneStepKind::stay);
            return pending_;
        }
        const auto width = kInstructionWidths[static_cast<std::size_t>(opcode)];
        if (static_cast<std::size_t>(program_counter_) + width > script_.size()) {
            error_ = "KDEF instruction crosses the record boundary";
            clear_event();
            return current_result(SceneStepKind::stay);
        }
        const auto base = static_cast<std::size_t>(program_counter_);
        const auto argument = [this, base](const std::size_t index) {
            return script_[base + index];
        };
        const auto conditional = [this](
                                     const bool value,
                                     const std::size_t instruction_width,
                                     const std::int16_t yes_offset,
                                     const std::int16_t no_offset) {
            program_counter_ += static_cast<std::ptrdiff_t>(instruction_width) +
                                static_cast<std::ptrdiff_t>(value ? yes_offset : no_offset);
        };

        switch (opcode) {
        case 0:
            program_counter_ += 1;
            pending_ = current_result(SceneStepKind::present);
            return pending_;
        case 1:
            program_counter_ += 4;
            queue_dialogue(argument(1), argument(2), argument(3));
            return emit_queued();
        case 2:
            program_counter_ += 3;
            add_inventory(argument(1), argument(2));
            update_book_event_if_ready();
            queue_notice(ascii_message("item " + std::to_string(argument(1)) + " " + std::to_string(argument(2))));
            return emit_queued();
        case 3: {
            std::array<std::int16_t, 13> arguments{};
            for (std::size_t index = 0U; index < arguments.size(); ++index) {
                arguments[index] = argument(index + 1U);
            }
            modify_event(arguments);
            program_counter_ += 14;
            break;
        }
        case 4:
            conditional(event_context_.item_id == argument(1), 4U, argument(2), argument(3));
            break;
        case 5:
        case 9:
        case 11: {
            true_offset_ = argument(1);
            false_offset_ = argument(2);
            program_counter_ += 3;
            continuation_ = PendingContinuation::conditional;
            pending_ = current_result(SceneStepKind::question);
            pending_.question = opcode == 5 ? SceneQuestion::battle
                                            : (opcode == 9 ? SceneQuestion::join : SceneQuestion::rest);
            const auto& question_text = opcode == 5 ? kBattleQuestion
                                        : (opcode == 9 ? kJoinQuestion : kRestQuestion);
            pending_text_.assign(question_text.begin(), question_text.end());
            return pending_;
        }
        case 6:
            true_offset_ = argument(2);
            false_offset_ = argument(3);
            battle_get_exp_ = argument(4);
            program_counter_ += 5;
            continuation_ = PendingContinuation::battle;
            pending_ = current_result(SceneStepKind::battle);
            pending_.battle_id = argument(1);
            pending_.battle_get_exp = battle_get_exp_;
            return pending_;
        case 7:
            clear_event();
            return current_result(SceneStepKind::stay);
        case 8:
            exit_music_override_ = argument(1);
            program_counter_ += 2;
            break;
        case 10: {
            program_counter_ += 2;
            for (std::size_t index = 1U; index < model::kTeamMemberCount; ++index) {
                if (snapshot_.ranger.header.team_member(index).value > 0) {
                    continue;
                }
                snapshot_.ranger.header.set_team_member(index, model::CharacterId{argument(1)});
                break;
            }
            if (argument(1) >= 0 && static_cast<std::size_t>(argument(1)) < snapshot_.ranger.roles.size()) {
                auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(argument(1))];
                for (std::size_t slot = 0U; slot < model::role_word::taking_item_count; ++slot) {
                    const auto item_id = role.word(model::role_word::taking_item_begin + slot);
                    const auto count = role.word(model::role_word::taking_item_count_begin + slot);
                    if (item_id < 0) {
                        continue;
                    }
                    add_inventory(item_id, count);
                    role.set_word(model::role_word::taking_item_begin + slot, -1);
                    role.set_word(model::role_word::taking_item_count_begin + slot, 0);
                    queue_notice(ascii_message("item " + std::to_string(item_id) + " " + std::to_string(count)));
                }
                clear_role_personal_items(argument(1));
            }
            if (!queued_outputs_.empty()) {
                return emit_queued();
            }
            break;
        }
        case 12: {
            auto party_end = model::kTeamMemberCount;
            for (std::size_t slot = 1U; slot < model::kTeamMemberCount; ++slot) {
                if (snapshot_.ranger.header.team_member(slot).value <= 0) {
                    party_end = slot;
                    break;
                }
            }
            for (std::size_t slot = 0U; slot < party_end; ++slot) {
                const auto role_id = snapshot_.ranger.header.team_member(slot).value;
                if (role_id < 0 || static_cast<std::size_t>(role_id) >= snapshot_.ranger.roles.size()) {
                    continue;
                }
                auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
                if (role.word(model::role_word::hurt) < 33 &&
                    role.word(model::role_word::poison) == 0) {
                    role.set_word(model::role_word::hurt, 0);
                    role.set_word(model::role_word::physical_power, 100);
                    role.set_word(model::role_word::mp, role.word(model::role_word::maximum_mp));
                    role.set_word(model::role_word::hp, role.word(model::role_word::maximum_hp));
                }
            }
            program_counter_ += 1;
            break;
        }
        case 13:
            program_counter_ += 1;
            pending_ = current_result(SceneStepKind::fade_from_black);
            return pending_;
        case 14:
            program_counter_ += 1;
            pending_ = current_result(SceneStepKind::fade_to_black);
            return pending_;
        case 15:
            program_counter_ += static_cast<std::ptrdiff_t>(width);
            return start_death_menu();
        case 24:
            program_counter_ += static_cast<std::ptrdiff_t>(width);
            return start_load_menu();
        case 16:
            conditional(party_contains(argument(1)), 4U, argument(2), argument(3));
            break;
        case 17:
            set_scene_value(argument(1), argument(2), argument(3), argument(4), argument(5));
            program_counter_ += 6;
            break;
        case 18:
            conditional(inventory_contains_id(argument(1)), 4U, argument(2), argument(3));
            break;
        case 19:
            scene_x_ = std::clamp<int>(argument(1), 0, kSceneExtent - 1);
            scene_y_ = std::clamp<int>(argument(2), 0, kSceneExtent - 1);
            update_view_origin();
            commit_header();
            program_counter_ += 3;
            break;
        case 20:
            conditional(
                snapshot_.ranger.header.team_member(model::kTeamMemberCount - 1U).value > 0,
                3U, argument(1), argument(2));
            break;
        case 21:
            remove_team_role(argument(1));
            program_counter_ += 2;
            break;
        case 22:
            for (std::size_t index = 0U; index < model::kTeamMemberCount; ++index) {
                const auto role_id = snapshot_.ranger.header.team_member(index).value;
                if ((index == 0U || role_id > 0) &&
                    role_id >= 0 && static_cast<std::size_t>(role_id) < snapshot_.ranger.roles.size()) {
                    snapshot_.ranger.roles[static_cast<std::size_t>(role_id)].set_word(model::role_word::mp, 0);
                }
            }
            program_counter_ += 1;
            break;
        case 23:
            if (argument(1) >= 0 && static_cast<std::size_t>(argument(1)) < snapshot_.ranger.roles.size()) {
                snapshot_.ranger.roles[static_cast<std::size_t>(argument(1))].set_word(model::role_word::use_poison, argument(2));
            }
            program_counter_ += 3;
            break;
        case 25:
            pan_state_ = PanState{
                argument(1),
                argument(2),
                argument(3),
                argument(4),
                argument(3) < argument(1) ? -1 : 1,
                argument(4) < argument(2) ? -1 : 1,
            };
            program_counter_ += 5;
            if (auto frame = advance_pan_frame(); frame.has_value()) {
                return *frame;
            }
            break;
        case 26: {
            const auto target_scene = argument(1) == -2 ? scene_id_ : argument(1);
            const auto target_event = argument(2) == -2 ? event_context_.event_index : argument(2);
            for (std::size_t field = 0U; field < 3U; ++field) {
                const auto event_field_id = static_cast<model::SceneEventField>(
                    static_cast<std::size_t>(model::SceneEventField::event_1) + field);
                const auto value = event_field(target_scene, target_event, event_field_id).value_or(0);
                set_event_field(
                    target_scene, target_event, event_field_id,
                    wrapping_add(value, argument(3U + field)));
            }
            program_counter_ += 6;
            break;
        }
        case 27:
            picture_animation_state_ = PictureAnimationState{
                argument(1), argument(2), argument(3)};
            program_counter_ += 4;
            if (auto frame = advance_picture_animation_frame(); frame.has_value()) {
                return *frame;
            }
            break;
        case 28: {
            const auto role = argument(1);
            const auto morality = role >= 0 && static_cast<std::size_t>(role) < snapshot_.ranger.roles.size()
                                      ? snapshot_.ranger.roles[static_cast<std::size_t>(role)].word(model::role_word::morality)
                                      : std::int16_t{};
            conditional(morality >= argument(2) && morality <= argument(3), 6U, argument(4), argument(5));
            break;
        }
        case 29: {
            const auto role = argument(1);
            const auto attack = role >= 0 && static_cast<std::size_t>(role) < snapshot_.ranger.roles.size()
                                   ? snapshot_.ranger.roles[static_cast<std::size_t>(role)].word(model::role_word::attack)
                                   : std::int16_t{};
            conditional(attack >= argument(2), 6U, argument(4), argument(5));
            break;
        }
        case 30:
            scripted_walk_state_ = ScriptedWalkState{
                argument(1),
                argument(2),
                argument(3),
                argument(4),
                argument(3) < argument(1) ? -1 : 1,
                argument(4) < argument(2) ? -1 : 1,
            };
            program_counter_ += 5;
            if (auto frame = advance_scripted_walk_frame(); frame.has_value()) {
                return *frame;
            }
            break;
        case 31:
            conditional(
                first_inventory_count(174).value_or(0) >= argument(1),
                4U, argument(2), argument(3));
            break;
        case 32:
            change_first_inventory(argument(1), argument(2));
            program_counter_ += 3;
            break;
        case 33: {
            const auto role_id = argument(1);
            if (role_id >= 0 && static_cast<std::size_t>(role_id) < snapshot_.ranger.roles.size()) {
                auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
                std::size_t destination = 0U;
                for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
                    if (role.word(model::role_word::magic_id_begin + slot) == 0) {
                        destination = slot;
                        break;
                    }
                }
                role.set_word(model::role_word::magic_id_begin + destination, argument(2));
                role.set_word(model::role_word::magic_level_begin + destination, 0);
            }
            program_counter_ += 4;
            if (argument(3) == 0) {
                queue_notice(ascii_message("learn " + std::to_string(argument(2))));
                return emit_queued();
            }
            break;
        }
        case 34:
        case 45:
        case 47: {
            const auto role_id = argument(1);
            if (role_id >= 0 && static_cast<std::size_t>(role_id) < snapshot_.ranger.roles.size()) {
                auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
                const auto field = opcode == 34 ? model::role_word::iq
                                  : opcode == 45 ? model::role_word::speed
                                                 : model::role_word::attack;
                const auto before = role.word(field);
                const auto after = clamped_add(before, argument(2), 0, 100);
                role.set_word(field, after);
                if (after > before) {
                    queue_notice(ascii_message("role " + std::to_string(role_id) + " +" + std::to_string(after - before)));
                }
            }
            program_counter_ += 3;
            if (!queued_outputs_.empty()) {
                return emit_queued();
            }
            break;
        }
        case 46:
        case 48: {
            const auto role_id = argument(1);
            if (role_id >= 0 && static_cast<std::size_t>(role_id) < snapshot_.ranger.roles.size()) {
                auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
                const auto maximum_field = opcode == 46 ? model::role_word::maximum_mp
                                                        : model::role_word::maximum_hp;
                const auto current_field = opcode == 46 ? model::role_word::mp
                                                        : model::role_word::hp;
                const auto before = role.word(current_field);
                const auto maximum = static_cast<std::int16_t>(
                    role.word(maximum_field) + argument(2));
                role.set_word(maximum_field, maximum);
                role.set_word(current_field, maximum);
                const auto gain = static_cast<int>(maximum) - static_cast<int>(before);
                if (gain > 0 && (opcode == 46 || party_contains(role_id))) {
                    queue_notice(ascii_message(
                        "role " + std::to_string(role_id) + " +" + std::to_string(gain)));
                }
            }
            program_counter_ += 3;
            if (!queued_outputs_.empty()) {
                return emit_queued();
            }
            break;
        }
        case 35: {
            const auto role_id = argument(1);
            if (role_id >= 0 && static_cast<std::size_t>(role_id) < snapshot_.ranger.roles.size()) {
                auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
                auto destination = argument(2);
                if (destination == -1) {
                    destination = 0;
                    for (std::size_t slot = 0U; slot < model::role_word::magic_count; ++slot) {
                        if (role.word(model::role_word::magic_id_begin + slot) == 0) {
                            destination = static_cast<std::int16_t>(slot);
                            break;
                        }
                    }
                }
                if (destination >= 0 &&
                    destination < static_cast<std::int16_t>(model::role_word::magic_count)) {
                    role.set_word(
                        model::role_word::magic_id_begin + static_cast<std::size_t>(destination),
                        argument(3));
                    role.set_word(
                        model::role_word::magic_level_begin + static_cast<std::size_t>(destination),
                        argument(4));
                }
            }
            program_counter_ += 5;
            break;
        }
        case 36: {
            const auto sex = snapshot_.ranger.roles.empty() ? -1 : snapshot_.ranger.roles[0].word(model::role_word::sexual);
            conditional(sex == argument(1), 4U, argument(2), argument(3));
            break;
        }
        case 37:
            if (!snapshot_.ranger.roles.empty()) {
                auto& role = snapshot_.ranger.roles[0];
                role.set_word(model::role_word::morality, clamped_add(role.word(model::role_word::morality), argument(1), 0, 100));
            }
            program_counter_ += 2;
            break;
        case 38: {
            const auto target_scene = argument(1) == -2 ? scene_id_ : argument(1);
            for (int y = 0; y < kSceneExtent; ++y) {
                for (int x = 0; x < kSceneExtent; ++x) {
                    if (scene_value(target_scene, argument(2), static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)) == argument(3)) {
                        set_scene_value(target_scene, argument(2), static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), argument(4));
                    }
                }
            }
            program_counter_ += 5;
            break;
        }
        case 39:
            if (argument(1) >= 0 && static_cast<std::size_t>(argument(1)) < snapshot_.ranger.scenes.size()) {
                snapshot_.ranger.scenes[static_cast<std::size_t>(argument(1))].set_word(model::scene_metadata_word::entrance_condition, 0);
            }
            program_counter_ += 2;
            break;
        case 40:
            player_frame_override_.reset();
            direction_ = static_cast<SceneDirection>(std::clamp<std::int16_t>(argument(1), 0, 3));
            walk_frame_offset_ = 0;
            commit_header();
            program_counter_ += 2;
            break;
        case 41:
            add_role_item(argument(1), argument(2), argument(3));
            program_counter_ += 4;
            break;
        case 42: {
            bool present = false;
            for (std::size_t index = 0U; index < model::kTeamMemberCount; ++index) {
                const auto role_id = snapshot_.ranger.header.team_member(index).value;
                if (role_id >= 0 && static_cast<std::size_t>(role_id) < snapshot_.ranger.roles.size() &&
                    snapshot_.ranger.roles[static_cast<std::size_t>(role_id)].word(model::role_word::sexual) == 1) {
                    present = true;
                }
            }
            conditional(present, 3U, argument(1), argument(2));
            break;
        }
        case 43:
            conditional(inventory_contains_id(argument(1)), 4U, argument(2), argument(3));
            break;
        case 44:
            dual_picture_animation_state_ = DualPictureAnimationState{
                argument(1), argument(2), argument(3), argument(4), argument(5)};
            program_counter_ += 7;
            if (auto frame = advance_dual_picture_animation_frame(); frame.has_value()) {
                return *frame;
            }
            break;
        case 49:
            if (argument(1) >= 0 && static_cast<std::size_t>(argument(1)) < snapshot_.ranger.roles.size()) {
                snapshot_.ranger.roles[static_cast<std::size_t>(argument(1))].set_word(model::role_word::mp_type, argument(2));
            }
            program_counter_ += 3;
            break;
        case 50: {
            bool all = true;
            for (std::size_t index = 1U; index <= 5U; ++index) {
                all = all && inventory_contains_id(argument(index));
            }
            conditional(all, 8U, argument(6), argument(7));
            break;
        }
        case 51:
            program_counter_ += 1;
            queue_dialogue(static_cast<std::int16_t>(2547 + random_.bounded(18)), 114, 0);
            return emit_queued();
        case 52:
        case 53: {
            const auto fame = opcode == 53;
            const auto field = fame ? model::role_word::fame : model::role_word::morality;
            const auto value = snapshot_.ranger.roles.empty() ? 0 : snapshot_.ranger.roles[0].word(field);
            program_counter_ += 1;
            queue_notice(status_notice_message(fame, value), opcode);
            return emit_queued();
        }
        case 54:
            for (auto& scene : snapshot_.ranger.scenes) {
                scene.set_word(model::scene_metadata_word::entrance_condition, 0);
            }
            for (const auto& [scene_index, condition] : std::array<std::pair<std::size_t, std::int16_t>, 4>{
                     std::pair<std::size_t, std::int16_t>{2U, 2}, {38U, 2}, {75U, 1}, {80U, 1}}) {
                if (scene_index < snapshot_.ranger.scenes.size()) {
                    snapshot_.ranger.scenes[scene_index].set_word(model::scene_metadata_word::entrance_condition, condition);
                }
            }
            program_counter_ += 1;
            break;
        case 55:
            conditional(event_field(scene_id_, argument(1), model::SceneEventField::event_1).value_or(0) == argument(2),
                        5U, argument(3), argument(4));
            break;
        case 56:
            if (!snapshot_.ranger.roles.empty()) {
                auto& role = snapshot_.ranger.roles[0];
                role.set_word(
                    model::role_word::fame,
                    wrapping_add(role.word(model::role_word::fame), argument(1)));
                update_book_event_if_ready();
            }
            program_counter_ += 2;
            break;
        case 57:
            three_statue_animation_state_ = ThreeStatueAnimationState{};
            program_counter_ += 1;
            if (auto frame = advance_three_statue_animation_frame(); frame.has_value()) {
                return *frame;
            }
            break;
        case 58:
            program_counter_ += 1;
            tournament_trial_state_ = TournamentTrialState{};
            if (auto step = advance_tournament_trial(
                    SceneStepKind::stay, SceneResponse::acknowledge); step.has_value()) {
                return *step;
            }
            break;
        case 59: {
            for (int index = 6; index > 0; --index) {
                const auto role_id = index == 6
                                         ? snapshot_.ranger.header.inventory_item(0U).value
                                         : snapshot_.ranger.header.team_member(
                                               static_cast<std::size_t>(index)).value;
                if (role_id > 0) {
                    remove_team_role(role_id);
                }
            }
            constexpr std::array<std::pair<std::int16_t, std::int16_t>, 36> targets{
                std::pair<std::int16_t, std::int16_t>{0, 0},
                {49, 2}, {4, 1}, {44, 0}, {44, 1}, {37, 5}, {30, 0}, {59, 0},
                {40, 3}, {56, 1}, {1, 7}, {1, 8}, {1, 10}, {40, 7}, {40, 8},
                {77, 0}, {54, 0}, {62, 3}, {62, 4}, {60, 2}, {60, 15}, {52, 1},
                {61, 0}, {61, 8}, {78, 0}, {18, 0}, {18, 1}, {69, 0}, {69, 1},
                {45, 0}, {52, 2}, {42, 6}, {42, 7}, {8, 8}, {7, 6}, {80, 1},
            };
            for (const auto [target_scene, target_event] : targets) {
                const std::array<std::int16_t, 13> arguments{
                    target_scene, target_event,
                    0, 0, -1, -1, -1, -1, -1, -1, 0, -2, -2,
                };
                modify_event(arguments);
            }
            program_counter_ += 1;
            break;
        }
        case 60: {
            const auto target_scene = argument(1) == -2 ? scene_id_ : argument(1);
            const auto current = event_field(
                target_scene, argument(2), model::SceneEventField::current_picture).value_or(-1);
            conditional(current == argument(3), 6U, argument(4), argument(5));
            break;
        }
        case 61: {
            bool all = true;
            for (std::int16_t event = 11; event < 25; ++event) {
                all = all && event_field(scene_id_, event, model::SceneEventField::current_picture).value_or(-1) == 4664;
            }
            conditional(all, 3U, argument(1), argument(2));
            break;
        }
        case 62:
            player_frame_override_ = -86;
            dual_picture_animation_state_ = DualPictureAnimationState{
                argument(1), argument(2), argument(3), argument(4), argument(5), true, true};
            program_counter_ += 7;
            if (auto frame = advance_dual_picture_animation_frame(); frame.has_value()) {
                return *frame;
            }
            break;
        case 63:
            if (argument(1) >= 0 && static_cast<std::size_t>(argument(1)) < snapshot_.ranger.roles.size()) {
                snapshot_.ranger.roles[static_cast<std::size_t>(argument(1))].set_word(model::role_word::sexual, argument(2));
            }
            program_counter_ += 3;
            break;
        case 64: {
            const auto shop_id = scene_id_ <= 1 ? 0 : scene_id_ == 3 ? 1 : scene_id_ == 40 ? 2 : scene_id_ == 60 ? 3 : scene_id_ == 61 ? 4 : -1;
            program_counter_ += 1;
            queue_dialogue(2974, 111, 0);
            auto shop = current_result(SceneStepKind::shop);
            shop.shop_id = static_cast<std::int16_t>(shop_id);
            queued_outputs_.push_back(QueuedOutput{shop, {}});
            return emit_queued();
        }
        case 65: {
            const auto hide = [this](const std::int16_t event) {
                const std::array<std::int16_t, 13> event_change{
                    -2, event, 0, 0, -1, -1, -1, -1, -1, -1, -2, -2, -2};
                modify_event(event_change);
            };
            if (scene_id_ == 1) {
                hide(16); hide(17); hide(18);
            } else if (scene_id_ == 3) {
                hide(14); hide(15); hide(16);
            } else if (scene_id_ == 40) {
                hide(20); hide(21); hide(22);
            } else if (scene_id_ == 60) {
                hide(16); hide(17); hide(18);
            } else if (scene_id_ == 61) {
                hide(9); hide(10); hide(11); hide(12);
            }
            constexpr std::array<std::pair<std::int16_t, std::int16_t>, 5> targets{
                std::pair<std::int16_t, std::int16_t>{1, 16}, {3, 14}, {40, 20}, {60, 16}, {61, 9}};
            const auto target = targets[static_cast<std::size_t>(random_.bounded(5))];
            const std::array<std::int16_t, 13> event_change{
                target.first, target.second, 1, 1, 938, -1, -1, 8256, 8256, 8256, -2, -2, -2};
            modify_event(event_change);
            program_counter_ += 1;
            break;
        }
        case 66:
            audio_commands_.push_back(SceneAudioCommand{
                SceneAudioCommand::Kind::music, argument(1), true});
            program_counter_ += 2;
            break;
        case 67:
            audio_commands_.push_back(SceneAudioCommand{SceneAudioCommand::Kind::wave, argument(1)});
            program_counter_ += 2;
            break;
        default:
            program_counter_ += static_cast<std::ptrdiff_t>(width);
            break;
        }
    }
    return current_result(SceneStepKind::stay);
}

SceneStepResult SceneSession::run_auto_event(const SceneStepKind fallback) {
    const auto event = event_at(scene_x_, scene_y_);
    if (!event.has_value()) {
        pending_ = current_result(SceneStepKind::stay);
        return current_result(fallback);
    }
    const auto script = event_field(scene_id_, *event, model::SceneEventField::event_3);
    if (!script.has_value() || *script == -1) {
        pending_ = current_result(SceneStepKind::stay);
        return current_result(fallback);
    }
    event_context_ = EventContext{
        *event,
        static_cast<std::int16_t>(scene_x_),
        static_cast<std::int16_t>(scene_y_),
        -1};
    if (*script > 0) {
        (void)prepare_event(
            *script,
            *event,
            static_cast<std::int16_t>(scene_x_),
            static_cast<std::int16_t>(scene_y_),
            -1);
    }
    pending_ = current_result(SceneStepKind::present);
    return pending_;
}

SceneStepResult SceneSession::finish_tick_after_action(const SceneStepKind fallback) {
    tick_continuation_ = TickContinuation::after_scene_present;
    tick_fallback_ = fallback;
    pending_ = current_result(SceneStepKind::present);
    return pending_;
}

SceneStepResult SceneSession::finish_tick_after_scene_present(const SceneStepKind fallback) {
    tick_continuation_ = TickContinuation::none;
    periodic_counter_ = static_cast<std::int16_t>((periodic_counter_ + 1) % 5);
    if (periodic_counter_ == 1) {
        cycle_palette();
        periodic_tick();
    }
    const auto result = run_auto_event(fallback);
    if (result.kind == SceneStepKind::present) {
        tick_continuation_ = TickContinuation::after_auto_event;
        tick_fallback_ = fallback;
        return result;
    }
    return resolve_scene_transition(fallback);
}

SceneStepResult SceneSession::finish_tick_after_auto_event(const SceneStepKind fallback) {
    tick_continuation_ = TickContinuation::none;
    return resolve_scene_transition(fallback);
}

SceneStepResult SceneSession::resolve_scene_transition(const SceneStepKind fallback) {
    if (static_cast<std::size_t>(scene_id_) >= snapshot_.ranger.scenes.size()) {
        return current_result(fallback);
    }
    const auto& metadata = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)];
    for (std::size_t index = 0U; index < model::scene_metadata_word::exit_count; ++index) {
        if (scene_x_ == metadata.word(model::scene_metadata_word::exit_x_begin + index) &&
            scene_y_ == metadata.word(model::scene_metadata_word::exit_y_begin + index)) {
            diagnostics::log_info(
                "scene exit scene=" + std::to_string(scene_id_) +
                " x=" + std::to_string(scene_x_) +
                " y=" + std::to_string(scene_y_));
            continuation_ = PendingContinuation::scene_exit;
            pending_ = current_result(SceneStepKind::fade_to_black);
            return pending_;
        }
    }
    const auto jump_scene = metadata.word(model::scene_metadata_word::jump_scene);
    if (jump_scene < 0 ||
        scene_x_ != metadata.word(model::scene_metadata_word::jump_x) ||
        scene_y_ != metadata.word(model::scene_metadata_word::jump_y)) {
        return current_result(fallback);
    }
    if (static_cast<std::size_t>(jump_scene) >= snapshot_.ranger.scenes.size()) {
        error_ = "scene jump target is outside the 84-scene metadata table";
        return current_result(SceneStepKind::stay);
    }
    const auto use_jump_entrance =
        metadata.word(model::scene_metadata_word::main_entrance_x_1) == 0 &&
        metadata.word(model::scene_metadata_word::main_entrance_y_1) == 0;
    pending_jump_ = PendingJump{jump_scene, use_jump_entrance};
    continuation_ = PendingContinuation::scene_jump;
    pending_ = current_result(SceneStepKind::fade_to_black);
    return pending_;
}

SceneStepResult SceneSession::complete_scene_jump() {
    if (!pending_jump_.has_value()) {
        continuation_ = PendingContinuation::none;
        return current_result(SceneStepKind::stay);
    }
    const auto jump = *pending_jump_;
    pending_jump_.reset();
    const auto previous_scene = scene_id_;
    scene_id_ = jump.scene_id;
    clear_event();
    tick_continuation_ = TickContinuation::none;
    animation_counter_ = 0;
    walk_frame_offset_ = 0;
    player_frame_override_.reset();
    weather_enabled_ = std::find(kWeatherSceneIds.begin(), kWeatherSceneIds.end(), scene_id_) !=
                       kWeatherSceneIds.end();
    weather_active_ = false;
    if (!load_scene_sprites()) {
        return current_result(SceneStepKind::stay);
    }
    const auto& target = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)];
    scene_x_ = std::clamp<int>(
        target.word(jump.use_jump_entrance ? model::scene_metadata_word::jump_return_x
                                           : model::scene_metadata_word::entrance_x),
        0,
        kSceneExtent - 1);
    scene_y_ = std::clamp<int>(
        target.word(jump.use_jump_entrance ? model::scene_metadata_word::jump_return_y
                                           : model::scene_metadata_word::entrance_y),
        0,
        kSceneExtent - 1);
    update_view_origin();
    commit_header();
    diagnostics::log_info(
        "scene jump from=" + std::to_string(previous_scene) +
        " to=" + std::to_string(scene_id_) +
        " target=" + std::to_string(scene_x_) + "," + std::to_string(scene_y_));
    queue_scene_music(model::scene_metadata_word::entrance_music);
    idle_tick();
    continuation_ = PendingContinuation::scene_entry;
    pending_ = current_result(SceneStepKind::fade_from_black);
    return pending_;
}

void SceneSession::queue_scene_music(const std::size_t metadata_word) {
    if (static_cast<std::size_t>(scene_id_) >= snapshot_.ranger.scenes.size()) {
        return;
    }
    const auto music = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)].word(
        metadata_word);
    if (music < 0) {
        return;
    }
    audio_commands_.push_back(SceneAudioCommand{SceneAudioCommand::Kind::music, music});
}

void SceneSession::periodic_tick() {
    if (valid() && weather_enabled_ && pending_.kind == SceneStepKind::stay) {
        update_weather();
    }
}

void SceneSession::cycle_palette() {
    std::rotate(palette_.begin() + 224, palette_.begin() + 231, palette_.begin() + 232);
    std::rotate(palette_.begin() + 244, palette_.begin() + 252, palette_.begin() + 253);
}

void SceneSession::update_weather() {
    if (weather_active_) {
        bool all_done = true;
        for (auto& particle : weather_) {
            particle.x = static_cast<std::int16_t>(particle.x + 1);
            if (particle.x <= 500) {
                all_done = false;
            }
        }
        if (!all_done) {
            return;
        }
        weather_active_ = false;
    }
    if (random_.bounded(1) != 0) {
        return;
    }
    weather_active_ = true;
    for (auto& particle : weather_) {
        particle.kind = static_cast<std::int16_t>(random_.bounded(4));
    }
    for (auto& particle : weather_) {
        particle.speed = static_cast<std::int16_t>(random_.bounded(3) + 6);
    }
    for (auto& particle : weather_) {
        particle.x = static_cast<std::int16_t>(random_.bounded(100) - 300);
    }
    for (std::size_t index = 0U; index < weather_.size(); ++index) {
        if (random_.bounded(2) != 0) {
            weather_[index].y = static_cast<std::int16_t>(
                -3000 - 1000 * static_cast<std::int32_t>(index));
        } else {
            weather_[index].y = static_cast<std::int16_t>(
                random_.bounded(50) + static_cast<std::int32_t>(index) * 75);
        }
    }
}

void SceneSession::idle_tick() {
    if (!valid() || pending_.kind != SceneStepKind::stay) {
        return;
    }
    animation_counter_ = static_cast<std::int16_t>((animation_counter_ + 1) % 1000);
    for (int x = 0; x < kSceneExtent; ++x) {
        for (int y = 0; y < kSceneExtent; ++y) {
            const auto event = event_at(x, y);
            if (!event.has_value()) {
                continue;
            }
            const auto first_picture = event_field(
                scene_id_, *event, model::SceneEventField::current_picture).value_or(0);
            if (first_picture <= 0) {
                continue;
            }
            const auto end_picture = event_field(
                scene_id_, *event, model::SceneEventField::end_picture).value_or(0);
            auto displayed_picture = event_field(
                scene_id_, *event, model::SceneEventField::begin_picture).value_or(0);
            const auto delay = event_field(
                scene_id_, *event, model::SceneEventField::picture_delay).value_or(0);
            if (displayed_picture >= end_picture) {
                displayed_picture = first_picture;
            }
            if (displayed_picture > first_picture && animation_counter_ % 4 == 0 &&
                displayed_picture < end_picture) {
                displayed_picture = static_cast<std::int16_t>(displayed_picture + 2);
            }
            if (delay <= animation_counter_ % 100 && displayed_picture == first_picture &&
                displayed_picture < end_picture) {
                displayed_picture = static_cast<std::int16_t>(displayed_picture + 2);
            }
            set_event_field(
                scene_id_, *event, model::SceneEventField::begin_picture, displayed_picture);
        }
    }
}

bool SceneSession::target_is_walkable(const int x, const int y) const noexcept {
    if (x < 0 || x >= kSceneExtent || y < 0 || y >= kSceneExtent) {
        return false;
    }
    if (scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::building),
                    static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)) != 0) {
        return false;
    }
    const auto current_height = scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::building_height),
                                            static_cast<std::int16_t>(scene_x_), static_cast<std::int16_t>(scene_y_));
    const auto target_height = scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::building_height),
                                           static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
    if (static_cast<int>(target_height) - static_cast<int>(current_height) >= 10) {
        return false;
    }
    const auto event = event_at(x, y);
    if (event.has_value() &&
        event_field(scene_id_, *event, model::SceneEventField::cannot_walk).value_or(0) != 0) {
        return false;
    }
    return !blocked_earth(scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::earth),
                                      static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)));
}

std::optional<std::int16_t> SceneSession::event_at(const int x, const int y) const noexcept {
    if (x < 0 || x >= kSceneExtent || y < 0 || y >= kSceneExtent) {
        return std::nullopt;
    }
    const auto value = scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::event_index),
                                   static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
    if (value < 0 || static_cast<std::size_t>(value) >= model::kSceneEventCount) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::int16_t> SceneSession::event_field(
    const std::int16_t scene_id,
    const std::int16_t event_index,
    const model::SceneEventField field) const noexcept {
    if (scene_id < 0 || event_index < 0) {
        return std::nullopt;
    }
    return snapshot_.event_value(
        static_cast<std::size_t>(scene_id), static_cast<std::size_t>(event_index), field);
}

void SceneSession::set_event_field(
    const std::int16_t scene_id,
    const std::int16_t event_index,
    const model::SceneEventField field,
    const std::int16_t value) noexcept {
    if (scene_id >= 0 && event_index >= 0) {
        static_cast<void>(snapshot_.set_event_value(
            static_cast<std::size_t>(scene_id), static_cast<std::size_t>(event_index), field, value));
    }
}

void SceneSession::modify_event(const std::span<const std::int16_t, 13> arguments) {
    const auto target_scene = arguments[0] == -2 ? scene_id_ : arguments[0];
    auto target_event = arguments[1];
    if (target_event == -2 || target_event == -1) {
        target_event = event_context_.event_index;
    }
    if (target_scene < 0 || target_event < 0 ||
        static_cast<std::size_t>(target_scene) >= model::kSceneCount ||
        static_cast<std::size_t>(target_event) >= model::kSceneEventCount) {
        return;
    }
    const auto old_x = event_field(
        target_scene, target_event, model::SceneEventField::x).value_or(-1);
    const auto old_y = event_field(
        target_scene, target_event, model::SceneEventField::y).value_or(-1);
    if (arguments[1] == -1 && event_context_.x >= 0 && event_context_.y >= 0) {
        set_scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::event_index),
                        event_context_.x, event_context_.y, -1);
    }
    for (std::size_t field = 0U; field < model::kSceneEventWordCount; ++field) {
        const auto value = arguments[field + 2U];
        if (value != -2) {
            set_event_field(target_scene, target_event, static_cast<model::SceneEventField>(field), value);
        }
    }
    if (arguments[11] != -2 || arguments[12] != -2) {
        set_scene_value(
            scene_id_, static_cast<std::int16_t>(model::SceneLayer::event_index),
            old_x, old_y, -1);
        const auto new_x = arguments[11] == -2 ? old_x : arguments[11];
        const auto new_y = arguments[12] == -2 ? old_y : arguments[12];
        set_scene_value(
            scene_id_, static_cast<std::int16_t>(model::SceneLayer::event_index),
            new_x, new_y, target_event);
    }
}

void SceneSession::set_scene_value(
    const std::int16_t scene_id,
    const std::int16_t layer,
    const std::int16_t x,
    const std::int16_t y,
    const std::int16_t value) noexcept {
    const auto target_scene = scene_id == -2 ? scene_id_ : scene_id;
    if (target_scene < 0 || layer < 0 || layer >= static_cast<std::int16_t>(model::kSceneLayerCount) ||
        x < 0 || x >= kSceneExtent || y < 0 || y >= kSceneExtent) {
        return;
    }
    static_cast<void>(snapshot_.set_scene_value(
        static_cast<std::size_t>(target_scene), static_cast<model::SceneLayer>(layer), tile_index(x, y), value));
}

std::int16_t SceneSession::scene_value(
    const std::int16_t scene_id,
    const std::int16_t layer,
    const std::int16_t x,
    const std::int16_t y) const noexcept {
    const auto target_scene = scene_id == -2 ? scene_id_ : scene_id;
    if (target_scene < 0 || layer < 0 || layer >= static_cast<std::int16_t>(model::kSceneLayerCount) ||
        x < 0 || x >= kSceneExtent || y < 0 || y >= kSceneExtent) {
        return 0;
    }
    return snapshot_.scene_value(
        static_cast<std::size_t>(target_scene), static_cast<model::SceneLayer>(layer), tile_index(x, y)).value_or(0);
}

bool SceneSession::party_contains(const std::int16_t role_id) const noexcept {
    for (std::size_t index = 0U; index < model::kTeamMemberCount; ++index) {
        if (snapshot_.ranger.header.team_member(index).value == role_id) {
            return true;
        }
    }
    return false;
}

bool SceneSession::inventory_contains_id(const std::int16_t item_id) const noexcept {
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        if (snapshot_.ranger.header.inventory_item(index).value == item_id) {
            return true;
        }
    }
    return false;
}

std::optional<std::int16_t> SceneSession::first_inventory_count(
    const std::int16_t item_id) const noexcept {
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        if (snapshot_.ranger.header.inventory_item(index).value == item_id) {
            return snapshot_.ranger.header.inventory_count(index);
        }
    }
    return std::nullopt;
}

int SceneSession::inventory_count(const std::int16_t item_id) const noexcept {
    int total = 0;
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        if (snapshot_.ranger.header.inventory_item(index).value == item_id) {
            total += snapshot_.ranger.header.inventory_count(index);
        }
    }
    return total;
}

void SceneSession::add_inventory(const std::int16_t item_id, const std::int16_t count) {
    bool found = false;
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        if (snapshot_.ranger.header.inventory_item(index).value == item_id) {
            snapshot_.ranger.header.set_inventory(
                index, model::ItemId{item_id},
                static_cast<std::int16_t>(
                    snapshot_.ranger.header.inventory_count(index) + count));
            found = true;
        }
    }
    if (found) {
        return;
    }
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        if (snapshot_.ranger.header.inventory_item(index).value == -1) {
            snapshot_.ranger.header.set_inventory(
                index, model::ItemId{item_id},
                static_cast<std::int16_t>(
                    snapshot_.ranger.header.inventory_count(index) + count));
            return;
        }
    }
}

void SceneSession::change_first_inventory(
    const std::int16_t item_id,
    const std::int16_t count) {
    for (std::size_t index = 0U; index < model::kInventoryCount; ++index) {
        if (snapshot_.ranger.header.inventory_item(index).value != item_id) {
            continue;
        }
        const auto changed = static_cast<std::int16_t>(
            snapshot_.ranger.header.inventory_count(index) + count);
        if (changed > 0) {
            snapshot_.ranger.header.set_inventory(index, model::ItemId{item_id}, changed);
            return;
        }
        for (std::size_t source = index + 1U; source < model::kInventoryCount; ++source) {
            snapshot_.ranger.header.set_inventory(
                source - 1U,
                snapshot_.ranger.header.inventory_item(source),
                snapshot_.ranger.header.inventory_count(source));
        }
        snapshot_.ranger.header.set_inventory(
            model::kInventoryCount - 1U, model::ItemId{-1}, 0);
        return;
    }
}

void SceneSession::update_book_event_if_ready() {
    if (snapshot_.ranger.roles.empty() ||
        snapshot_.ranger.roles[0].word(model::role_word::fame) < 200 ||
        inventory_contains_id(189)) {
        return;
    }
    for (std::int16_t item_id = 144; item_id <= 157; ++item_id) {
        if (!inventory_contains_id(item_id)) {
            return;
        }
    }
    constexpr std::array<std::int16_t, 13> event_change{
        70, 11, 1, 1, 932, -1, -1, 7968, 7968, 7968, -2, -2, -2};
    modify_event(event_change);
}

void SceneSession::close_shop_events() {
    std::span<const std::int16_t> events;
    constexpr std::array<std::int16_t, 2> scene_1{17, 18};
    constexpr std::array<std::int16_t, 2> scene_3{15, 16};
    constexpr std::array<std::int16_t, 2> scene_40{21, 22};
    constexpr std::array<std::int16_t, 2> scene_60{17, 18};
    constexpr std::array<std::int16_t, 3> scene_61{10, 11, 12};
    switch (scene_id_) {
    case 1: events = scene_1; break;
    case 3: events = scene_3; break;
    case 40: events = scene_40; break;
    case 60: events = scene_60; break;
    case 61: events = scene_61; break;
    default: break;
    }
    for (const auto event : events) {
        set_event_field(scene_id_, event, model::SceneEventField::event_3, 939);
    }
}

void SceneSession::remove_team_role(const std::int16_t role_id) {
    bool found = false;
    for (std::size_t index = 1U; index < model::kTeamMemberCount; ++index) {
        if (!found && snapshot_.ranger.header.team_member(index).value == role_id) {
            found = true;
        }
        if (found) {
            const auto next = index + 1U < model::kTeamMemberCount
                                  ? snapshot_.ranger.header.team_member(index + 1U)
                                  : model::CharacterId{-1};
            snapshot_.ranger.header.set_team_member(index, next);
        }
    }
    clear_role_personal_items(role_id);
}

void SceneSession::clear_role_personal_items(const std::int16_t role_id) {
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= snapshot_.ranger.roles.size()) {
        return;
    }
    auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
    constexpr std::array<std::size_t, 3> fields{
        model::role_word::equipment_begin,
        model::role_word::equipment_begin + 1U,
        model::role_word::practice_item};
    for (const auto field : fields) {
        const auto item_id = role.word(field);
        if (item_id >= 0 && static_cast<std::size_t>(item_id) < snapshot_.ranger.items.size()) {
            snapshot_.ranger.items[static_cast<std::size_t>(item_id)].set_word(
                model::item_word::user, -1);
        }
        role.set_word(field, -1);
    }
    role.set_word(model::role_word::item_experience, 0);
}

void SceneSession::add_role_item(
    const std::int16_t role_id,
    const std::int16_t item_id,
    const std::int16_t count) {
    if (role_id < 0 || static_cast<std::size_t>(role_id) >= snapshot_.ranger.roles.size()) {
        return;
    }
    auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
    int slot = -1;
    for (std::size_t index = 0U; index < model::role_word::taking_item_count; ++index) {
        if (role.word(model::role_word::taking_item_begin + index) == item_id) {
            slot = static_cast<int>(index);
            break;
        }
    }
    if (slot < 0) {
        for (std::size_t index = 0U; index < model::role_word::taking_item_count; ++index) {
            if (role.word(model::role_word::taking_item_begin + index) == -1) {
                slot = static_cast<int>(index);
                break;
            }
        }
    }
    if (slot < 0) {
        return;
    }
    const auto index = static_cast<std::size_t>(slot);
    const auto existing = role.word(model::role_word::taking_item_begin + index) == item_id;
    role.set_word(model::role_word::taking_item_begin + index, item_id);
    role.set_word(
        model::role_word::taking_item_count_begin + index,
        existing
            ? static_cast<std::int16_t>(
                  role.word(model::role_word::taking_item_count_begin + index) + count)
            : count);
}

void SceneSession::queue_dialogue(
    const std::int16_t talk_id,
    const std::int16_t head_id,
    const std::int16_t style) {
    auto result = current_result(SceneStepKind::dialogue);
    result.talk_id = talk_id;
    result.head_id = head_id;
    result.style = style;
    const auto text = talk_id >= 0 && static_cast<std::size_t>(talk_id) < assets_.talk_count()
                          ? assets_.talk(static_cast<std::size_t>(talk_id))
                          : std::vector<std::uint8_t>{0U};
    for (auto& page : paginate_dialogue(text)) {
        queued_outputs_.push_back(QueuedOutput{result, std::move(page)});
    }
}

void SceneSession::queue_notice(
    std::vector<std::uint8_t> text,
    const std::int16_t style) {
    auto result = current_result(SceneStepKind::notice);
    result.style = style;
    queued_outputs_.push_back(QueuedOutput{result, std::move(text)});
}

SceneStepResult SceneSession::emit_queued() {
    if (queued_outputs_.empty()) {
        pending_ = current_result(SceneStepKind::stay);
        pending_text_.clear();
        return pending_;
    }
    auto output = std::move(queued_outputs_.front());
    queued_outputs_.pop_front();
    pending_ = output.result;
    pending_text_ = std::move(output.text);
    if (pending_.kind == SceneStepKind::shop) {
        continuation_ = PendingContinuation::shop;
    }
    return pending_;
}

std::optional<SceneStepResult> SceneSession::advance_pan_frame() {
    if (!pan_state_.has_value()) {
        return std::nullopt;
    }
    if (pan_state_->x != pan_state_->target_x) {
        view_origin_x_ = std::clamp(pan_state_->x - 11, 0, kSceneMaximumViewOrigin);
        pan_state_->x += pan_state_->step_x;
    } else if (pan_state_->y != pan_state_->target_y) {
        view_origin_y_ = std::clamp(pan_state_->y - 11, 0, kSceneMaximumViewOrigin);
        pan_state_->y += pan_state_->step_y;
    } else {
        pan_state_.reset();
        return std::nullopt;
    }
    pending_ = current_result(SceneStepKind::present);
    pending_.wait_ticks = 2U;
    return pending_;
}

std::optional<SceneStepResult> SceneSession::advance_picture_animation_frame() {
    if (!picture_animation_state_.has_value()) {
        return std::nullopt;
    }
    if (picture_animation_state_->frame > picture_animation_state_->end_frame) {
        picture_animation_state_.reset();
        return std::nullopt;
    }
    const auto frame = static_cast<std::int16_t>(picture_animation_state_->frame);
    set_animated_picture(picture_animation_state_->event_index, frame);
    picture_animation_state_->frame += 2;
    pending_ = current_result(SceneStepKind::present);
    pending_.wait_ticks = 2U;
    return pending_;
}

std::optional<SceneStepResult> SceneSession::advance_scripted_walk_frame() {
    if (!scripted_walk_state_.has_value()) {
        return std::nullopt;
    }
    if (scripted_walk_state_->x != scripted_walk_state_->target_x) {
        apply_scripted_walk_step(true, scripted_walk_state_->step_x);
        scripted_walk_state_->x += scripted_walk_state_->step_x;
    } else if (scripted_walk_state_->y != scripted_walk_state_->target_y) {
        apply_scripted_walk_step(false, scripted_walk_state_->step_y);
        scripted_walk_state_->y += scripted_walk_state_->step_y;
    } else {
        walk_frame_offset_ = 0;
        player_frame_override_.reset();
        commit_header();
        scripted_walk_state_.reset();
        return std::nullopt;
    }
    pending_ = current_result(SceneStepKind::present);
    pending_.wait_ticks = 3U;
    return pending_;
}

std::optional<SceneStepResult> SceneSession::advance_dual_picture_animation_frame() {
    if (!dual_picture_animation_state_.has_value()) {
        return std::nullopt;
    }
    if (dual_picture_animation_state_->first_picture >
        dual_picture_animation_state_->first_end_picture) {
        const auto quit_after = dual_picture_animation_state_->quit_after;
        dual_picture_animation_state_.reset();
        if (quit_after) {
            return start_ending();
        }
        return std::nullopt;
    }
    if (!dual_picture_animation_state_->skip_negative_events ||
        dual_picture_animation_state_->first_event != -1) {
        set_animated_picture(
            dual_picture_animation_state_->first_event,
            static_cast<std::int16_t>(dual_picture_animation_state_->first_picture));
    }
    if (!dual_picture_animation_state_->skip_negative_events ||
        dual_picture_animation_state_->second_event != -1) {
        set_animated_picture(
            dual_picture_animation_state_->second_event,
            static_cast<std::int16_t>(dual_picture_animation_state_->second_picture));
    }
    dual_picture_animation_state_->first_picture += 2;
    dual_picture_animation_state_->second_picture += 2;
    pending_ = current_result(SceneStepKind::present);
    pending_.wait_ticks = 2U;
    return pending_;
}

std::optional<SceneStepResult> SceneSession::advance_three_statue_animation_frame() {
    if (!three_statue_animation_state_.has_value()) {
        return std::nullopt;
    }
    if (three_statue_animation_state_->phase == 0) {
        if (three_statue_animation_state_->value <= 7674) {
            player_frame_override_ = static_cast<std::int16_t>(three_statue_animation_state_->value);
            three_statue_animation_state_->value += 2;
            pending_ = current_result(SceneStepKind::present);
            pending_.wait_ticks = 2U;
            return pending_;
        }
        three_statue_animation_state_->phase = 1;
        three_statue_animation_state_->value = 0;
    }
    if (three_statue_animation_state_->value > 56) {
        three_statue_animation_state_.reset();
        return std::nullopt;
    }
    const auto value = three_statue_animation_state_->value;
    if (player_frame() < 7688) {
        player_frame_override_ = static_cast<std::int16_t>(value + 7676);
    }
    set_animated_picture(2, static_cast<std::int16_t>(value + 7690));
    set_animated_picture(3, static_cast<std::int16_t>(value + 7748));
    set_animated_picture(4, static_cast<std::int16_t>(value + 7806));
    three_statue_animation_state_->value += 2;
    pending_ = current_result(SceneStepKind::present);
    pending_.wait_ticks = 2U;
    return pending_;
}

SceneStepResult SceneSession::start_load_menu() {
    load_menu_framebuffer_.clear(0U);
    load_menu_framebuffer_.set_palette(palette_);
    load_menu_state_ = LoadMenuState{};
    pending_ = current_result(SceneStepKind::fade_from_black);
    return pending_;
}

SceneStepResult SceneSession::advance_load_menu(const int translated_key) {
    if (!load_menu_state_.has_value()) {
        return current_result(SceneStepKind::stay);
    }
    auto& state = *load_menu_state_;
    if (state.phase == LoadMenuState::Phase::fade_in) {
        state.phase = LoadMenuState::Phase::menu;
    } else if (state.phase == LoadMenuState::Phase::load_slot_fade) {
        const auto selected_slot = state.selected_slot;
        event_active_ = false;
        pending_ = current_result(SceneStepKind::load_slot);
        pending_.save_slot = selected_slot;
        return pending_;
    } else if (state.phase == LoadMenuState::Phase::confirm) {
        if (translated_key == static_cast<int>('Y')) {
            event_active_ = false;
            pending_ = current_result(SceneStepKind::quit);
            return pending_;
        }
        state.phase = LoadMenuState::Phase::menu;
    } else if (translated_key == 0x98) {
        state.selection = state.selection == 3 ? 0 : static_cast<std::int16_t>(state.selection + 1);
    } else if (translated_key == 0x9E) {
        state.selection = state.selection == 0 ? 2 : static_cast<std::int16_t>(state.selection - 1);
    } else if (translated_key == 0x0D || translated_key == 0x20 || translated_key == 0x96) {
        if (state.selection < 3) {
            state.selected_slot = state.selection;
            state.phase = LoadMenuState::Phase::load_slot_fade;
            pending_ = current_result(SceneStepKind::fade_to_black);
            return pending_;
        }
        state.phase = LoadMenuState::Phase::confirm;
    }
    if (!render_load_menu()) {
        load_menu_state_.reset();
        event_active_ = false;
        pending_ = current_result(SceneStepKind::stay);
        return pending_;
    }
    pending_ = current_result(SceneStepKind::load_menu);
    pending_.menu_index = state.selection;
    return pending_;
}

bool SceneSession::render_load_menu() {
    if (!load_menu_state_.has_value()) {
        return false;
    }
    render::Big5GlyphCache cache{big5_font_};
    const auto draw_text = [this, &cache](
                               const int x,
                               const int y,
                               const std::span<const std::uint8_t> text,
                               const std::uint16_t colors) {
        return render::draw_legacy_text(
            load_menu_framebuffer_, x, y, text, ascii_font_, cache,
            static_cast<std::uint8_t>(colors & 0xFFU),
            static_cast<std::uint8_t>(colors >> 8U));
    };
    if (load_menu_state_->phase == LoadMenuState::Phase::confirm) {
        return draw_panel(load_menu_framebuffer_, 71, 140, 177, 31) &&
               draw_text(75, 145, kExitPrompt, 0x0705U);
    }
    if (!draw_panel(load_menu_framebuffer_, 109, 40, 101, 90)) {
        return false;
    }
    for (std::size_t index = 0U; index < kProgressMenuItems.size(); ++index) {
        if (!draw_text(
                119, 45 + static_cast<int>(index) * 20,
                kProgressMenuItems[index], 0x2321U)) {
            return false;
        }
    }
    if (!draw_text(
            119, 45 + static_cast<int>(load_menu_state_->selection) * 20,
            kProgressMenuItems[static_cast<std::size_t>(load_menu_state_->selection)],
            0x6663U)) {
        return false;
    }
    return true;
}

SceneStepResult SceneSession::start_death_menu() {
    if (!load_death_image()) {
        event_active_ = false;
        pending_ = current_result(SceneStepKind::stay);
        return pending_;
    }
    const auto date = death_date_override_.has_value()
                          ? *death_date_override_
                          : current_local_date();
    death_menu_state_ = DeathMenuState{
        DeathMenuState::Phase::fade_in, 0, -1, date.year, date.month, date.day};
    if (!render_death_menu()) {
        event_active_ = false;
        pending_ = current_result(SceneStepKind::stay);
        return pending_;
    }
    pending_ = current_result(SceneStepKind::fade_from_black);
    return pending_;
}

SceneStepResult SceneSession::advance_death_menu(const int translated_key) {
    if (!death_menu_state_.has_value()) {
        return current_result(SceneStepKind::stay);
    }
    auto& state = *death_menu_state_;
    if (state.phase == DeathMenuState::Phase::fade_in) {
        state.phase = DeathMenuState::Phase::menu;
    } else if (state.phase == DeathMenuState::Phase::load_slot_clear) {
        event_active_ = false;
        state.phase = DeathMenuState::Phase::menu;
        pending_ = current_result(SceneStepKind::load_slot);
        pending_.save_slot = state.selected_slot;
        return pending_;
    } else if (state.phase == DeathMenuState::Phase::confirm) {
        if (translated_key == static_cast<int>('Y')) {
            event_active_ = false;
            pending_ = current_result(SceneStepKind::quit);
            return pending_;
        }
        state.phase = DeathMenuState::Phase::menu;
    } else if (translated_key == 0x98) {
        state.selection = state.selection == 3 ? 0 : static_cast<std::int16_t>(state.selection + 1);
    } else if (translated_key == 0x9E) {
        state.selection = state.selection == 0 ? 3 : static_cast<std::int16_t>(state.selection - 1);
    } else if (translated_key == 0x0D || translated_key == 0x20 || translated_key == 0x96) {
        if (state.selection < 3) {
            state.selected_slot = state.selection;
            state.phase = DeathMenuState::Phase::load_slot_clear;
            death_framebuffer_.clear(0U);
            death_framebuffer_.set_palette(palette_);
            pending_ = current_result(SceneStepKind::present);
            return pending_;
        }
        state.phase = DeathMenuState::Phase::confirm;
    }
    if (!render_death_menu()) {
        event_active_ = false;
        pending_ = current_result(SceneStepKind::stay);
        return pending_;
    }
    pending_ = current_result(SceneStepKind::death_menu);
    pending_.menu_index = state.selection;
    pending_.death_confirm = state.phase == DeathMenuState::Phase::confirm;
    return pending_;
}

bool SceneSession::load_death_image() {
    if (death_image_.size() == compat::kLegacyPixelCount) {
        return true;
    }
    const auto file = data_root_.read("DEAD.BIG");
    if (!file) {
        error_ = file.error;
        return false;
    }
    if (file.bytes.size() != compat::kLegacyPixelCount) {
        error_ = "DEAD.BIG does not contain exactly 64000 pixels";
        return false;
    }
    death_image_ = file.bytes;
    return true;
}

void SceneSession::blend_panel_rectangle(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const int width,
    const int height) const {
    const auto source = palette_[0U];
    const auto begin_x = std::max(x, 0);
    const auto end_x = std::min(x + width, render::IndexedFramebuffer::width);
    const auto begin_y = std::max(y, 0);
    const auto end_y = std::min(y + height, render::IndexedFramebuffer::height);
    for (int destination_y = begin_y; destination_y < end_y; ++destination_y) {
        for (int destination_x = begin_x; destination_x < end_x; ++destination_x) {
            auto& destination_index = framebuffer.row(destination_y)[destination_x];
            const auto destination = palette_[destination_index];
            const auto red = static_cast<int>(source.red) / 8 +
                             static_cast<int>(destination.red) / 8;
            const auto green = static_cast<int>(source.green) / 8 +
                               static_cast<int>(destination.green) / 8;
            const auto blue = static_cast<int>(source.blue) / 8 +
                              static_cast<int>(destination.blue) / 8;
            destination_index = rgb4_lookup_[static_cast<std::size_t>(
                red * 256 + green * 16 + blue)];
        }
    }
}

void SceneSession::blend_panel(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const int width,
    const int height) const {
    blend_panel_rectangle(framebuffer, x + 5, y, width - 10, 1);
    blend_panel_rectangle(framebuffer, x + 4, y + 1, width - 8, 1);
    blend_panel_rectangle(framebuffer, x + 3, y + 2, width - 6, 1);
    blend_panel_rectangle(framebuffer, x + 2, y + 3, width - 4, 1);
    blend_panel_rectangle(framebuffer, x + 1, y + 4, width - 2, 1);
    blend_panel_rectangle(framebuffer, x, y + 5, width, height - 10);
    blend_panel_rectangle(framebuffer, x + 1, y + height - 5, width - 2, 1);
    blend_panel_rectangle(framebuffer, x + 2, y + height - 4, width - 4, 1);
    blend_panel_rectangle(framebuffer, x + 3, y + height - 3, width - 6, 1);
    blend_panel_rectangle(framebuffer, x + 4, y + height - 2, width - 8, 1);
    blend_panel_rectangle(framebuffer, x + 5, y + height - 1, width - 10, 1);
}

bool SceneSession::draw_panel_border(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const int width,
    const int height) const {
    const auto fill = [&framebuffer](const int left, const int top, const int w, const int h) {
        return framebuffer.fill_rectangle(
            left, top, static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h), 0xFFU);
    };
    return fill(x + 5, y + 1, width - 10, 1) &&
           fill(x + 4, y + 2, 1, 2) && fill(x + width - 5, y + 2, 1, 2) &&
           fill(x + 2, y + 4, 2, 1) && fill(x + width - 4, y + 4, 2, 1) &&
           fill(x + 1, y + 5, 1, height - 10) &&
           fill(x + width - 2, y + 5, 1, height - 10) &&
           fill(x + 2, y + height - 5, 2, 1) &&
           fill(x + width - 4, y + height - 5, 2, 1) &&
           fill(x + 4, y + height - 4, 1, 2) &&
           fill(x + width - 5, y + height - 4, 1, 2) &&
           fill(x + 5, y + height - 2, width - 10, 1);
}

bool SceneSession::draw_panel(
    render::IndexedFramebuffer& framebuffer,
    const int x,
    const int y,
    const int width,
    const int height) const {
    if (width <= 10 || height <= 10) {
        return false;
    }
    blend_panel(framebuffer, x, y, width, height);
    return draw_panel_border(framebuffer, x, y, width, height);
}

bool SceneSession::draw_death_panel(
    const int x,
    const int y,
    const int width,
    const int height) {
    return draw_panel(death_framebuffer_, x, y, width, height);
}

bool SceneSession::render_death_menu() {
    if (!death_menu_state_.has_value() || death_image_.size() != compat::kLegacyPixelCount) {
        return false;
    }
    std::copy(death_image_.begin(), death_image_.end(), death_framebuffer_.pixels().begin());
    death_framebuffer_.set_palette(palette_);
    render::Big5GlyphCache cache{big5_font_};
    const auto draw_text = [this, &cache](
                               const int x,
                               const int y,
                               const std::span<const std::uint8_t> text,
                               const std::uint16_t colors) {
        return render::draw_legacy_text(
            death_framebuffer_, x, y, text, ascii_font_, cache,
            static_cast<std::uint8_t>(colors & 0xFFU),
            static_cast<std::uint8_t>(colors >> 8U));
    };
    const auto& protagonist = snapshot_.ranger.roles[0].bytes;
    std::vector<std::uint8_t> name(
        protagonist.begin() + static_cast<std::ptrdiff_t>(model::role_word::name_byte),
        protagonist.end());
    name.push_back(0U);
    std::array<std::uint8_t, 32> date{};
    std::snprintf(
        reinterpret_cast<char*>(date.data()), date.size(), "  %4d/%2d/%2d  ",
        death_menu_state_->year, death_menu_state_->month, death_menu_state_->day);
    if (!draw_text(97, 46, name, 0x6C6EU) ||
        !draw_text(190, 8, date, 0x1517U) ||
        !draw_text(190, 28, kDeathLocationText, 0x1517U) ||
        !draw_text(190, 48, kDeathMissingText, 0x1517U) ||
        !draw_text(190, 68, kDeathAnotherText, 0x1517U) ||
        !draw_death_panel(205, 90, 101, 90)) {
        return false;
    }
    for (std::size_t index = 0U; index < kProgressMenuItems.size(); ++index) {
        if (!draw_text(215, 95 + static_cast<int>(index) * 20,
                       kProgressMenuItems[index], 0x2321U)) {
            return false;
        }
    }
    if (!draw_text(
            215, 95 + static_cast<int>(death_menu_state_->selection) * 20,
            kProgressMenuItems[static_cast<std::size_t>(death_menu_state_->selection)],
            0x6663U)) {
        return false;
    }
    if (death_menu_state_->phase == DeathMenuState::Phase::confirm) {
        return draw_death_panel(71, 180, 177, 20) &&
               draw_text(75, 182, kExitPrompt, 0x0705U);
    }
    return true;
}

SceneStepResult SceneSession::start_ending() {
    if (!load_ending_assets()) {
        event_active_ = false;
        pending_ = current_result(SceneStepKind::stay);
        return pending_;
    }
    if (!render_map(ending_framebuffer_)) {
        event_active_ = false;
        pending_ = current_result(SceneStepKind::stay);
        return pending_;
    }
    ending_framebuffer_.set_palette(ending_palette_);
    EndingState state;
    state.credit_y = kEndingCreditInitialY;
    ending_state_ = state;
    pending_ = current_result(SceneStepKind::fade_to_black);
    return pending_;
}

bool SceneSession::load_ending_assets() {
    if (ending_words_.entry_count() >= 23U) {
        return true;
    }
    const auto palette_file = data_root_.read("ENDCOL.COL");
    if (!palette_file) {
        error_ = palette_file.error;
        return false;
    }
    const auto parsed_palette = resource::parse_vga_palette(palette_file.bytes);
    if (!parsed_palette) {
        error_ = parsed_palette.error;
        return false;
    }
    ending_words_ = resource::PackedArchive::open(
        data_root_.path() / "ENDWORD.IDX", data_root_.path() / "ENDWORD.GRP");
    if (!ending_words_.valid() || ending_words_.entry_count() < 23U) {
        error_ = ending_words_.valid() ? "ENDWORD archive has fewer than 23 frames"
                                      : ending_words_.error();
        return false;
    }
    ending_palette_ = parsed_palette.palette;
    ending_framebuffer_.clear(0U);
    ending_framebuffer_.set_palette(ending_palette_);
    return true;
}

bool SceneSession::load_ending_frames() {
    if (ending_frames_.entry_count() >= 221U) {
        return true;
    }
    ending_frames_ = resource::PackedArchive::open(
        data_root_.path() / "KEND.IDX", data_root_.path() / "KEND.GRP");
    if (!ending_frames_.valid() || ending_frames_.entry_count() < 221U) {
        error_ = ending_frames_.valid() ? "KEND archive has fewer than 221 frames"
                                       : ending_frames_.error();
        return false;
    }
    for (std::size_t frame = 0U; frame < 221U; ++frame) {
        if (ending_frames_.entry(frame).size() != compat::kLegacyPixelCount) {
            error_ = "KEND frame does not contain exactly 64000 pixels";
            return false;
        }
    }
    return true;
}

bool SceneSession::draw_ending_word(
    const std::int16_t legacy_id,
    const int x,
    const int y) {
    const auto index = render::legacy_sprite_index(static_cast<std::uint16_t>(legacy_id));
    if (!index.has_value() || *index >= ending_words_.entry_count()) {
        error_ = "ENDWORD sprite index is out of range";
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(ending_words_.entry(*index));
    if (!frame.valid()) {
        error_ = frame.error();
        return false;
    }
    render::draw_rle_sprite(ending_framebuffer_, frame, x, y);
    return true;
}

bool SceneSession::set_ending_frame(const std::size_t frame) {
    if (frame >= ending_frames_.entry_count()) {
        error_ = "KEND frame index is out of range";
        return false;
    }
    const auto bytes = ending_frames_.entry(frame);
    if (bytes.size() != ending_framebuffer_.pixels().size()) {
        error_ = "KEND frame does not contain exactly 64000 pixels";
        return false;
    }
    std::copy(bytes.begin(), bytes.end(), ending_framebuffer_.pixels().begin());
    ending_framebuffer_.set_palette(ending_palette_);
    return true;
}

bool SceneSession::draw_ending_credits() {
    if (!ending_state_.has_value()) {
        return false;
    }
    ending_framebuffer_.clear(0U);
    ending_framebuffer_.set_palette(ending_palette_);
    for (std::size_t index = 0U; index < kEndingCreditIds.size(); ++index) {
        if (!draw_ending_word(
                kEndingCreditIds[index], kEndingCreditX[index],
                ending_state_->credit_y[index])) {
            return false;
        }
    }
    return true;
}

SceneStepResult SceneSession::advance_ending() {
    if (!ending_state_.has_value()) {
        return current_result(SceneStepKind::stay);
    }
    auto& state = *ending_state_;
    const auto fail = [this]() {
        event_active_ = false;
        pending_ = current_result(SceneStepKind::stay);
        return pending_;
    };
    switch (state.phase) {
    case EndingState::Phase::title_draw:
        ending_framebuffer_.clear(0U);
        ending_framebuffer_.set_palette(ending_palette_);
        if (!draw_ending_word(0, 94, 90)) {
            return fail();
        }
        state.phase = EndingState::Phase::title_fade_out;
        pending_ = current_result(SceneStepKind::fade_from_black);
        pending_.wait_ticks = legacy_delay_ticks(2000);
        return pending_;
    case EndingState::Phase::title_fade_out:
        state.phase = EndingState::Phase::word_scroll_setup;
        pending_ = current_result(SceneStepKind::fade_to_black);
        return pending_;
    case EndingState::Phase::word_scroll_setup:
        ending_framebuffer_.clear(0U);
        ending_framebuffer_.set_palette(ending_palette_);
        if (!draw_ending_word(2, 44, state.word_first_y) ||
            !draw_ending_word(4, 44, state.word_second_y)) {
            return fail();
        }
        state.phase = EndingState::Phase::word_scroll;
        pending_ = current_result(SceneStepKind::fade_from_black);
        return pending_;
    case EndingState::Phase::word_scroll:
        if (state.word_second_y > -130) {
            ending_framebuffer_.clear(0U);
            ending_framebuffer_.set_palette(ending_palette_);
            if (!draw_ending_word(2, 44, state.word_first_y) ||
                !draw_ending_word(4, 44, state.word_second_y)) {
                return fail();
            }
            --state.word_first_y;
            --state.word_second_y;
            pending_ = current_result(SceneStepKind::present);
            pending_.wait_ticks = legacy_delay_ticks(100);
            return pending_;
        }
        ending_framebuffer_.clear(0U);
        ending_framebuffer_.set_palette(ending_palette_);
        state.phase = EndingState::Phase::kend_setup;
        pending_ = current_result(SceneStepKind::fade_to_black);
        return pending_;
    case EndingState::Phase::kend_setup:
        if (!load_ending_frames() || !set_ending_frame(0U)) {
            return fail();
        }
        state.kend_frame = 1U;
        state.phase = EndingState::Phase::kend_frames;
        pending_ = current_result(SceneStepKind::fade_from_black);
        return pending_;
    case EndingState::Phase::kend_frames:
        if (state.kend_frame < 221U) {
            if (!set_ending_frame(state.kend_frame++)) {
                return fail();
            }
            pending_ = current_result(SceneStepKind::present);
            return pending_;
        }
        state.phase = EndingState::Phase::kend_fade_out;
        pending_ = current_result(SceneStepKind::wait_key);
        return pending_;
    case EndingState::Phase::kend_fade_out:
        state.phase = EndingState::Phase::credits_setup;
        pending_ = current_result(SceneStepKind::fade_to_black);
        return pending_;
    case EndingState::Phase::credits_setup:
        ending_framebuffer_.clear(0U);
        ending_framebuffer_.set_palette(ending_palette_);
        for (std::size_t index = 0U; index < 4U; ++index) {
            if (!draw_ending_word(
                    kEndingCreditIds[index], kEndingCreditX[index], state.credit_y[index])) {
                return fail();
            }
        }
        state.phase = EndingState::Phase::credits_scroll;
        pending_ = current_result(SceneStepKind::fade_from_black);
        return pending_;
    case EndingState::Phase::credits_scroll:
        if (state.credit_y.back() > 57) {
            if (!draw_ending_credits()) {
                return fail();
            }
            for (auto& y : state.credit_y) {
                --y;
            }
            pending_ = current_result(SceneStepKind::present);
            pending_.wait_ticks = legacy_delay_ticks(100);
            return pending_;
        }
        state.phase = EndingState::Phase::credits_fade_out;
        pending_ = current_result(SceneStepKind::wait_key);
        return pending_;
    case EndingState::Phase::credits_fade_out:
        state.phase = EndingState::Phase::finish;
        pending_ = current_result(SceneStepKind::fade_to_black);
        return pending_;
    case EndingState::Phase::finish:
        event_active_ = false;
        pending_ = current_result(SceneStepKind::quit);
        pending_.ending_complete = true;
        return pending_;
    }
    return fail();
}

std::optional<SceneStepResult> SceneSession::advance_tournament_trial(
    const SceneStepKind previous_kind, const SceneResponse response) {
    const auto queue_step = [this](const SceneStepKind kind, const std::uint16_t wait_ticks = 1U) {
        auto step = current_result(kind);
        step.wait_ticks = wait_ticks;
        queued_outputs_.push_back(QueuedOutput{step, {}});
    };
    while (tournament_trial_state_.has_value()) {
        switch (tournament_trial_state_->phase) {
        case TournamentTrialState::Phase::choose_opponent: {
            int opponent = 0;
            const auto base = tournament_trial_state_->group * 6;
            do {
                opponent = random_.bounded(6);
            } while (tournament_trial_state_->chosen[static_cast<std::size_t>(base + opponent)]);
            const auto index = base + opponent;
            tournament_trial_state_->chosen[static_cast<std::size_t>(index)] = true;
            queue_dialogue(
                static_cast<std::int16_t>(2854 + index),
                kTournamentHeadIds[static_cast<std::size_t>(index)], 0);
            queue_step(SceneStepKind::present);
            auto battle = current_result(SceneStepKind::battle);
            battle.battle_id = static_cast<std::int16_t>(102 + index);
            queued_outputs_.push_back(QueuedOutput{battle, {}});
            tournament_trial_state_->phase = TournamentTrialState::Phase::awaiting_battle;
            return emit_queued();
        }
        case TournamentTrialState::Phase::awaiting_battle:
            if (previous_kind != SceneStepKind::battle ||
                response != SceneResponse::battle_victory) {
                tournament_trial_state_.reset();
                return start_death_menu();
            }
            queue_step(SceneStepKind::present);
            queue_step(SceneStepKind::fade_from_black);
            queue_dialogue(2890, 0, 1);
            queue_step(SceneStepKind::present);
            tournament_trial_state_->phase = TournamentTrialState::Phase::after_victory;
            return emit_queued();
        case TournamentTrialState::Phase::after_victory:
            ++tournament_trial_state_->victories;
            if (tournament_trial_state_->victories < 3) {
                tournament_trial_state_->phase = TournamentTrialState::Phase::choose_opponent;
                continue;
            }
            if (tournament_trial_state_->group < 4) {
                queue_dialogue(2891, 70, 0);
                queue_step(SceneStepKind::present);
                queue_step(SceneStepKind::fade_to_black, 9U);
                tournament_trial_state_->phase = TournamentTrialState::Phase::interround_fade;
                return emit_queued();
            }
            for (const auto [talk_id, head_id, style] :
                 std::array<std::array<std::int16_t, 3>, 5>{
                     std::array<std::int16_t, 3>{2884, 0, 1},
                     {2885, 70, 0}, {2886, 12, 0}, {2887, 64, 4}, {2888, 19, 0}}) {
                queue_dialogue(talk_id, head_id, style);
                queue_step(SceneStepKind::present);
            }
            queue_step(SceneStepKind::fade_to_black);
            tournament_trial_state_->phase = TournamentTrialState::Phase::finale_fade;
            return emit_queued();
        case TournamentTrialState::Phase::interround_fade: {
            if (!snapshot_.ranger.roles.empty()) {
                auto& role = snapshot_.ranger.roles[0];
                if (role.word(model::role_word::hurt) < 50 &&
                    role.word(model::role_word::poison) == 0) {
                    role.set_word(model::role_word::hurt, 0);
                    role.set_word(model::role_word::physical_power, 100);
                    role.set_word(model::role_word::mp, role.word(model::role_word::maximum_mp));
                    role.set_word(model::role_word::hp, role.word(model::role_word::maximum_hp));
                }
            }
            queue_step(SceneStepKind::fade_from_black);
            queue_dialogue(2892, 0, 1);
            queue_step(SceneStepKind::present);
            tournament_trial_state_->phase = TournamentTrialState::Phase::interround_finish;
            return emit_queued();
        }
        case TournamentTrialState::Phase::interround_finish:
            ++tournament_trial_state_->group;
            tournament_trial_state_->victories = 0;
            tournament_trial_state_->phase = TournamentTrialState::Phase::choose_opponent;
            continue;
        case TournamentTrialState::Phase::finale_fade:
            for (std::int16_t event = 24; event < 73; ++event) {
                const std::array<std::int16_t, 13> arguments{
                    -2, event, 0, 0, -1, -1, -1, -1, -1, -1, -2, -2, -2};
                modify_event(arguments);
            }
            queue_step(SceneStepKind::present);
            queue_step(SceneStepKind::fade_from_black);
            queue_dialogue(2889, 0, 1);
            queue_step(SceneStepKind::present);
            tournament_trial_state_->phase = TournamentTrialState::Phase::finale_finish;
            return emit_queued();
        case TournamentTrialState::Phase::finale_finish:
            add_inventory(143, 1);
            queue_notice(ascii_message("item 143 1"));
            tournament_trial_state_->phase = TournamentTrialState::Phase::reward_notice;
            return emit_queued();
        case TournamentTrialState::Phase::reward_notice:
            tournament_trial_state_.reset();
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void SceneSession::apply_scripted_walk_step(const bool horizontal, const int step) {
    player_frame_override_.reset();
    walk_frame_offset_ = static_cast<std::int16_t>(walk_frame_offset_ + 2);
    if (walk_frame_offset_ > 12) {
        walk_frame_offset_ = 2;
    }
    direction_ = horizontal
                     ? (step < 0 ? SceneDirection::left : SceneDirection::right)
                     : (step < 0 ? SceneDirection::down : SceneDirection::up);
    const auto target_x = std::clamp(scene_x_ + (horizontal ? step : 0), 0, kSceneExtent - 1);
    const auto target_y = std::clamp(scene_y_ + (horizontal ? 0 : step), 0, kSceneExtent - 1);
    if (target_is_walkable(target_x, target_y)) {
        scene_x_ = target_x;
        scene_y_ = target_y;
        update_view_origin();
    }
    commit_header();
}

void SceneSession::set_animated_picture(
    const std::int16_t event_index, const std::int16_t picture) {
    if (event_index == -1) {
        player_frame_override_ = picture;
        return;
    }
    std::array<std::int16_t, 13> arguments{
        -2, event_index,
        -2, -2, -2, -2, -2, picture, picture, picture, -2, -2, -2,
    };
    modify_event(arguments);
}

void SceneSession::commit_header() noexcept {
    snapshot_.ranger.header.set_word(model::header_word::in_sub_map, 1);
    snapshot_.ranger.header.set_word(model::header_word::sub_map_x, static_cast<std::int16_t>(scene_x_));
    snapshot_.ranger.header.set_word(model::header_word::sub_map_y, static_cast<std::int16_t>(scene_y_));
    snapshot_.ranger.header.set_word(model::header_word::face_towards, static_cast<std::int16_t>(direction_));
}

void SceneSession::update_view_origin() noexcept {
    view_origin_x_ = std::clamp(scene_x_ - 11, 0, kSceneMaximumViewOrigin);
    view_origin_y_ = std::clamp(scene_y_ - 11, 0, kSceneMaximumViewOrigin);
}

void SceneSession::clear_event() noexcept {
    event_active_ = false;
    script_.clear();
    program_counter_ = 0;
    continuation_ = PendingContinuation::none;
    pan_state_.reset();
    picture_animation_state_.reset();
    scripted_walk_state_.reset();
    dual_picture_animation_state_.reset();
    three_statue_animation_state_.reset();
    tournament_trial_state_.reset();
    event_context_ = {};
    pending_ = current_result(SceneStepKind::stay);
    pending_text_.clear();
}

std::int16_t SceneSession::player_frame() const noexcept {
    if (player_frame_override_.has_value()) {
        return *player_frame_override_;
    }
    return static_cast<std::int16_t>(
        kPlayerFrameBase[static_cast<std::size_t>(direction_)] + walk_frame_offset_);
}

bool SceneSession::render_map(render::IndexedFramebuffer& framebuffer) const {
    if (!valid()) {
        return false;
    }
    framebuffer.clear(0U);
    framebuffer.set_palette(palette_);
    const auto player_sprite = player_frame();
    bool player_drawn = false;
    for (int local_x = 0; local_x < kSceneViewExtent; ++local_x) {
        for (int local_y = 0; local_y < kSceneViewExtent; ++local_y) {
            const auto x = local_x + view_origin_x_;
            const auto y = local_y + view_origin_y_;
            if (scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::building_height),
                            static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)) == 0) {
                const auto anchor_x = 18 * (local_x - local_y) + 145;
                const auto anchor_y = 9 * (local_x + local_y) - 81;
                if (!draw_sprite(framebuffer,
                                 scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::earth),
                                             static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)),
                                 anchor_x, anchor_y)) {
                    return false;
                }
            }
        }
    }
    for (int local_x = 0; local_x < kSceneViewExtent; ++local_x) {
        for (int local_y = 0; local_y < kSceneViewExtent; ++local_y) {
            const auto x = local_x + view_origin_x_;
            const auto y = local_y + view_origin_y_;
            const auto anchor_x = 18 * (local_x - local_y) + 145;
            const auto anchor_y = 9 * (local_x + local_y) - 81;
            const auto building_height = scene_value(
                scene_id_, static_cast<std::int16_t>(model::SceneLayer::building_height),
                static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
            if (building_height != 0 &&
                !draw_sprite(framebuffer,
                             scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::earth),
                                         static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)),
                             anchor_x, anchor_y)) {
                return false;
            }
            const auto building = scene_value(
                scene_id_, static_cast<std::int16_t>(model::SceneLayer::building),
                static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
            if (building != 0 && building != 15000 &&
                !draw_sprite(framebuffer, building, anchor_x, anchor_y - building_height)) {
                return false;
            }
            const auto event = event_at(x, y);
            if (event.has_value()) {
                const auto picture = event_field(scene_id_, *event, model::SceneEventField::begin_picture).value_or(0);
                if (picture > 0 && !draw_sprite(framebuffer, picture, anchor_x, anchor_y - building_height)) {
                    return false;
                }
            }
            if (x == scene_x_ && y == scene_y_ && player_sprite != 0 && player_sprite != -86) {
                if (!draw_sprite(
                        framebuffer, player_sprite, anchor_x, anchor_y - building_height)) {
                    diagnostics::log_error(
                        "scene player sprite draw failed scene=" + std::to_string(scene_id_) +
                        " x=" + std::to_string(scene_x_) +
                        " y=" + std::to_string(scene_y_) +
                        " frame=" + std::to_string(player_sprite));
                    return false;
                }
                player_drawn = true;
            }
            const auto decoration = scene_value(
                scene_id_, static_cast<std::int16_t>(model::SceneLayer::decoration),
                static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
            if (decoration != 0) {
                const auto height = scene_value(
                    scene_id_, static_cast<std::int16_t>(model::SceneLayer::decoration_height),
                    static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
                if (!draw_sprite(framebuffer, decoration, anchor_x, anchor_y - height)) {
                    return false;
                }
            }
        }
    }
    if (weather_active_) {
        for (const auto& particle : weather_) {
            if (particle.y > -1000 && !draw_weather_particle(framebuffer, particle)) {
                return false;
            }
        }
    }
    if (player_sprite != 0 && player_sprite != -86 && !player_drawn) {
        diagnostics::log_warning(
            "scene player not visited by render grid scene=" + std::to_string(scene_id_) +
            " x=" + std::to_string(scene_x_) +
            " y=" + std::to_string(scene_y_) +
            " frame=" + std::to_string(player_sprite) +
            " view_origin=" + std::to_string(view_origin_x_) + "," +
            std::to_string(view_origin_y_));
    } else {
        diagnostics::log_trace(
            "scene player render scene=" + std::to_string(scene_id_) +
            " x=" + std::to_string(scene_x_) +
            " y=" + std::to_string(scene_y_) +
            " frame=" + std::to_string(player_sprite) +
            " drawn=" + (player_drawn ? std::string{"true"} : std::string{"false"}));
    }
    return true;
}

bool SceneSession::render(render::IndexedFramebuffer& framebuffer) const {
    if (load_menu_state_.has_value()) {
        framebuffer = load_menu_framebuffer_;
        return true;
    }
    if (death_menu_state_.has_value()) {
        framebuffer = death_framebuffer_;
        return true;
    }
    if (ending_state_.has_value()) {
        framebuffer = ending_framebuffer_;
        return true;
    }
    return render_map(framebuffer) && draw_overlay(framebuffer);
}

bool SceneSession::draw_sprite(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t legacy_id,
    const int anchor_x,
    const int anchor_y) const {
    if (legacy_id < 0) {
        return false;
    }
    const auto index = render::legacy_sprite_index(static_cast<std::uint16_t>(legacy_id));
    if (!index.has_value() || *index >= sprites_.entry_count()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(sprites_.entry(*index));
    if (!frame.valid()) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, frame, anchor_x, anchor_y);
    return true;
}

bool SceneSession::draw_portrait(
    render::IndexedFramebuffer& framebuffer,
    const std::int16_t head_id,
    const int x,
    const int y) const {
    if (head_id < 0 || static_cast<std::size_t>(head_id) >= portraits_.entry_count()) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(
        portraits_.entry(static_cast<std::size_t>(head_id)));
    if (!frame.valid()) {
        return false;
    }
    render::draw_rle_sprite(framebuffer, frame, x, y);
    return true;
}

bool SceneSession::draw_weather_particle(
    render::IndexedFramebuffer& framebuffer, const WeatherParticle& particle) const {
    if (particle.kind < 0 ||
        static_cast<std::size_t>(particle.kind) >= weather_sprites_.entry_count() ||
        particle.speed < 0 || particle.speed > 8) {
        return false;
    }
    const auto frame = resource::SpriteFrameView::parse(
        weather_sprites_.entry(static_cast<std::size_t>(particle.kind)));
    if (!frame.valid()) {
        return false;
    }
    const auto source_weight = static_cast<int>(particle.speed);
    const auto destination_weight = 8 - source_weight;
    const auto left = static_cast<int>(particle.x) - static_cast<int>(frame.x_offset());
    const auto top = static_cast<int>(particle.y) - static_cast<int>(frame.y_offset());
    for (std::size_t row_index = 0U; row_index < frame.rows().size(); ++row_index) {
        const auto destination_y = top + static_cast<int>(row_index);
        auto destination_x = left;
        for (const auto& run : frame.rows()[row_index].runs) {
            destination_x += static_cast<int>(run.skip);
            for (const auto source_index : run.pixels) {
                if (destination_y >= 0 && destination_y < render::IndexedFramebuffer::height &&
                    destination_x >= 0 && destination_x < render::IndexedFramebuffer::width) {
                    auto& destination_index = framebuffer.row(destination_y)[destination_x];
                    const auto source = palette_[source_index];
                    const auto destination = palette_[destination_index];
                    const auto red = (static_cast<int>(source.red) * source_weight) / 32 +
                                     (static_cast<int>(destination.red) * destination_weight) / 32;
                    const auto green = (static_cast<int>(source.green) * source_weight) / 32 +
                                       (static_cast<int>(destination.green) * destination_weight) / 32;
                    const auto blue = (static_cast<int>(source.blue) * source_weight) / 32 +
                                      (static_cast<int>(destination.blue) * destination_weight) / 32;
                    const auto lookup_index = static_cast<std::size_t>(red * 256 + green * 16 + blue);
                    destination_index = rgb4_lookup_[lookup_index];
                }
                ++destination_x;
            }
        }
    }
    return true;
}

bool SceneSession::draw_overlay(render::IndexedFramebuffer& framebuffer) const {
    if (pending_.kind == SceneStepKind::stay || pending_.kind == SceneStepKind::moved ||
        pending_.kind == SceneStepKind::present || pending_.kind == SceneStepKind::wait_key ||
        pending_.kind == SceneStepKind::load_menu ||
        pending_.kind == SceneStepKind::death_menu || pending_.kind == SceneStepKind::load_slot ||
        pending_.kind == SceneStepKind::fade_from_black ||
        pending_.kind == SceneStepKind::fade_to_black ||
        pending_.kind == SceneStepKind::return_world || pending_.kind == SceneStepKind::quit ||
        pending_.kind == SceneStepKind::open_ui || pending_.kind == SceneStepKind::battle) {
        return true;
    }
    if (pending_.kind == SceneStepKind::question) {
        constexpr int x = 61;
        constexpr int y = 40;
        if (!draw_panel(framebuffer, x, y, 187, 27)) {
            return false;
        }
        render::Big5GlyphCache cache{big5_font_};
        return render::draw_legacy_text(
            framebuffer, 71, 45, pending_text_, ascii_font_, cache, 0x05U, 0x07U);
    }
    if (pending_.kind == SceneStepKind::notice &&
        (pending_.style == 52 || pending_.style == 53)) {
        const auto x = pending_.style == 52 ? 54 : 50;
        constexpr int y = 40;
        const auto width = pending_.style == 52 ? 212 : 220;
        if (!draw_panel(framebuffer, x, y, width, 27)) {
            return false;
        }
        if (pending_text_.empty()) {
            return true;
        }
        render::Big5GlyphCache cache{big5_font_};
        return render::draw_legacy_text(
            framebuffer, x + 10, y + 5, pending_text_, ascii_font_, cache, 0x05U, 0x07U);
    }
    if (pending_.kind == SceneStepKind::scene_title) {
        const auto terminator = std::find(pending_text_.begin(), pending_text_.end(), 0U);
        const auto length = static_cast<int>(
            std::distance(pending_text_.begin(), terminator));
        const auto x = 150 - 4 * length;
        constexpr int y = 10;
        const auto width = 8 * length + 20;
        if (!draw_panel(framebuffer, x, y, width, 27)) {
            return false;
        }
        if (pending_text_.empty()) {
            return true;
        }
        render::Big5GlyphCache cache{big5_font_};
        return render::draw_legacy_text(
            framebuffer, x + 10, y + 5, pending_text_, ascii_font_, cache, 0x05U, 0x07U);
    }
    int x = 54;
    int y = 40;
    int width = 212;
    int height = 27;
    auto text_x = x + 4;
    if (pending_.kind == SceneStepKind::dialogue) {
        width = 218;
        height = 57;
        std::optional<std::pair<int, int>> portrait_position;
        switch (pending_.style) {
        case 1:
            x = 8;
            y = 130;
            portrait_position = std::pair{237, 125};
            break;
        case 2:
            x = 94;
            y = 17;
            break;
        case 3:
            x = 8;
            y = 130;
            break;
        case 4:
            x = 8;
            y = 17;
            portrait_position = std::pair{237, 12};
            break;
        case 5:
            x = 94;
            y = 130;
            portrait_position = std::pair{23, 125};
            break;
        default:
            x = 94;
            y = 17;
            portrait_position = std::pair{23, 12};
            break;
        }
        if (!draw_panel(framebuffer, x, y, width, height)) {
            return false;
        }
        if (portrait_position.has_value()) {
            const auto [portrait_x, portrait_y] = *portrait_position;
            blend_panel(framebuffer, portrait_x, portrait_y, 60, 62);
            if (!draw_portrait(framebuffer, pending_.head_id, portrait_x + 2, portrait_y + 59) ||
                !draw_panel_border(framebuffer, portrait_x, portrait_y, 60, 62)) {
                return false;
            }
        }
        text_x = x + 13;
    } else if (!framebuffer.fill_rectangle(
                   x, y, static_cast<std::uint16_t>(width),
                   static_cast<std::uint16_t>(height), 0U)) {
        return false;
    }
    if (pending_text_.empty()) {
        return true;
    }
    render::Big5GlyphCache cache{big5_font_};
    std::vector<std::uint8_t> line;
    line.reserve(pending_text_.size());
    int line_index = 0;
    for (const auto value : pending_text_) {
        if (value != static_cast<std::uint8_t>('*') && value != 0U) {
            line.push_back(value);
            continue;
        }
        line.push_back(0U);
        const auto rendered = pending_.kind == SceneStepKind::dialogue
                                  ? draw_dialogue_text(
                                        framebuffer,
                                        text_x,
                                        y + 3 + line_index * 17,
                                        line,
                                        ascii_font_,
                                        cache,
                                        0x17U,
                                        0x15U)
                                  : render::draw_legacy_text(
                                        framebuffer,
                                        text_x,
                                        y + 3 + line_index * 17,
                                        line,
                                        ascii_font_,
                                        cache,
                                        0x17U,
                                        0x15U);
        if (!rendered) {
            return false;
        }
        line.clear();
        ++line_index;
        if (value == 0U) {
            return true;
        }
    }
    return false;
}

std::vector<SceneAudioCommand> SceneSession::take_audio_commands() {
    auto result = std::move(audio_commands_);
    audio_commands_.clear();
    return result;
}

}  // namespace openlegend::scene
