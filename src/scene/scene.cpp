#include "openlegend/scene/scene.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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
constexpr std::array<std::size_t, 68> kInstructionWidths{
    1, 4, 3, 14, 4, 3, 5, 1, 2, 3, 2, 3, 1, 1, 1, 2, 4,
    6, 4, 3, 3, 2, 1, 3, 1, 5, 6, 4, 6, 6, 5, 4, 3, 4,
    3, 5, 4, 2, 5, 2, 2, 4, 3, 4, 7, 3, 3, 3, 3, 3, 8,
    1, 1, 1, 1, 5, 2, 1, 1, 1, 6, 3, 7, 3, 1, 1, 2, 2};

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

[[nodiscard]] std::int16_t clamped_add(
    const std::int16_t value,
    const std::int16_t delta,
    const std::int16_t minimum,
    const std::int16_t maximum) noexcept {
    const auto result = static_cast<int>(value) + static_cast<int>(delta);
    return static_cast<std::int16_t>(std::clamp(result, static_cast<int>(minimum), static_cast<int>(maximum)));
}

}  // namespace

std::vector<std::vector<std::uint8_t>> paginate_dialogue(
    const std::span<const std::uint8_t> zero_terminated_text) {
    constexpr int maximum_line_width = 208;
    std::vector<std::vector<std::uint8_t>> pages;
    std::vector<std::uint8_t> page;
    std::vector<std::uint8_t> line;
    page.reserve(zero_terminated_text.size());
    line.reserve(zero_terminated_text.size());
    std::size_t page_line_count = 0U;
    const auto flush_page = [&pages, &page, &page_line_count]() {
        if (!page.empty() && page.back() == static_cast<std::uint8_t>('*')) {
            page.pop_back();
        }
        page.push_back(0U);
        pages.push_back(std::move(page));
        page.clear();
        page_line_count = 0U;
    };
    const auto flush_line = [&page, &line, &page_line_count, &flush_page]() {
        page.insert(page.end(), line.begin(), line.end());
        line.clear();
        ++page_line_count;
        page.push_back(static_cast<std::uint8_t>('*'));
        if (page_line_count == 3U) {
            flush_page();
        }
    };

    int line_width = 0;
    for (std::size_t index = 0U;
         index < zero_terminated_text.size() && zero_terminated_text[index] != 0U;) {
        const auto first = zero_terminated_text[index];
        if (first == static_cast<std::uint8_t>('*')) {
            flush_line();
            line_width = 0;
            ++index;
            continue;
        }
        const auto token_size = first > 0x7FU ? 2U : 1U;
        if (index + token_size > zero_terminated_text.size()) {
            break;
        }
        const auto token_width = first > 0x7FU
                                     ? 16
                                     : (first == static_cast<std::uint8_t>('_') ? 4 : 8);
        if (!line.empty() && line_width + token_width > maximum_line_width) {
            flush_line();
            line_width = 0;
        }
        line.insert(
            line.end(),
            zero_terminated_text.begin() + static_cast<std::ptrdiff_t>(index),
            zero_terminated_text.begin() + static_cast<std::ptrdiff_t>(index + token_size));
        line_width += token_width;
        index += token_size;
    }
    if (!line.empty()) {
        flush_line();
    }
    if (!page.empty()) {
        flush_page();
    }
    if (pages.empty()) {
        pages.push_back(std::vector<std::uint8_t>{0U});
    }
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
    const bool use_jump_entrance)
    : data_root_(data_root),
      snapshot_(snapshot),
      random_(random),
      assets_(data_root),
      weather_sprites_(resource::PackedArchive::open(
          data_root.path() / "CLOUD.IDX", data_root.path() / "CLOUD.GRP")),
      scene_id_(scene_id) {
    if (!snapshot_.valid()) {
        error_ = "scene session requires a valid game snapshot";
        return;
    }
    if (!assets_.valid()) {
        error_ = assets_.error();
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
    pending_ = current_result(SceneStepKind::scene_title);
    pending_text_.clear();
    if (static_cast<std::size_t>(scene_id_) < snapshot_.ranger.scenes.size()) {
        const auto& bytes = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)].bytes;
        pending_text_.assign(bytes.begin() + 2, bytes.begin() + 12);
        pending_text_.push_back(0U);
    }
}

SceneStepResult SceneSession::current_result(const SceneStepKind kind) const noexcept {
    SceneStepResult result;
    result.kind = kind;
    result.scene_id = scene_id_;
    result.scene_x = static_cast<std::int16_t>(scene_x_);
    result.scene_y = static_cast<std::int16_t>(scene_y_);
    return result;
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

    if (static_cast<std::size_t>(scene_id_) < snapshot_.ranger.scenes.size()) {
        const auto& metadata = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)];
        for (std::size_t index = 0U; index < model::scene_metadata_word::exit_count; ++index) {
            if (scene_x_ == metadata.word(model::scene_metadata_word::exit_x_begin + index) &&
                scene_y_ == metadata.word(model::scene_metadata_word::exit_y_begin + index)) {
                diagnostics::log_info(
                    "scene exit scene=" + std::to_string(scene_id_) +
                    " x=" + std::to_string(scene_x_) +
                    " y=" + std::to_string(scene_y_));
                snapshot_.ranger.header.set_word(model::header_word::in_sub_map, 0);
                pending_ = current_result(SceneStepKind::return_world);
                return pending_;
            }
        }
        const auto jump_scene = metadata.word(model::scene_metadata_word::jump_scene);
        if (jump_scene >= 0 &&
            scene_x_ == metadata.word(model::scene_metadata_word::jump_x) &&
            scene_y_ == metadata.word(model::scene_metadata_word::jump_y)) {
            const auto use_return = metadata.word(model::scene_metadata_word::jump_return_x) == 0 &&
                                    metadata.word(model::scene_metadata_word::jump_return_y) == 0;
            const auto previous_scene = scene_id_;
            scene_id_ = jump_scene;
            diagnostics::log_info(
                "scene jump from=" + std::to_string(previous_scene) +
                " to=" + std::to_string(scene_id_) +
                " trigger=" + std::to_string(scene_x_) + "," + std::to_string(scene_y_));
            weather_enabled_ = std::find(kWeatherSceneIds.begin(), kWeatherSceneIds.end(), scene_id_) !=
                               kWeatherSceneIds.end();
            if (!weather_enabled_) {
                weather_active_ = false;
            }
            if (!load_scene_sprites()) {
                pending_ = current_result(SceneStepKind::stay);
                return pending_;
            }
            if (static_cast<std::size_t>(scene_id_) < snapshot_.ranger.scenes.size()) {
                const auto& target = snapshot_.ranger.scenes[static_cast<std::size_t>(scene_id_)];
                scene_x_ = target.word(use_return ? model::scene_metadata_word::jump_return_x
                                                  : model::scene_metadata_word::entrance_x);
                scene_y_ = target.word(use_return ? model::scene_metadata_word::jump_return_y
                                                  : model::scene_metadata_word::entrance_y);
                scene_x_ = std::clamp(scene_x_, 0, kSceneExtent - 1);
                scene_y_ = std::clamp(scene_y_, 0, kSceneExtent - 1);
                update_view_origin();
                commit_header();
            }
        }
    }
    return run_auto_event(SceneStepKind::moved);
}

SceneStepResult SceneSession::interact() {
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
    const auto script = event_field(scene_id_, *event, model::SceneEventField::event_1);
    if (!script.has_value() || *script <= 0) {
        return current_result(SceneStepKind::stay);
    }
    return begin_event(*script, *event, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y));
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
    const auto script = event_field(scene_id_, *event, model::SceneEventField::event_2);
    if (!script.has_value() || *script <= 0) {
        return current_result(SceneStepKind::stay);
    }
    return begin_event(
        *script, *event, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), item_id);
}

SceneStepResult SceneSession::open_ui() const noexcept {
    return current_result(SceneStepKind::open_ui);
}

SceneStepResult SceneSession::begin_event(
    const std::int16_t script_id,
    const std::int16_t event_index,
    const std::int16_t event_x,
    const std::int16_t event_y,
    const std::int16_t item_id) {
    if (!valid() || script_id <= 0 || static_cast<std::size_t>(script_id) >= assets_.script_count()) {
        return current_result(SceneStepKind::stay);
    }
    event_context_ = EventContext{event_index, event_x, event_y, item_id};
    script_ = assets_.script(static_cast<std::size_t>(script_id));
    if (script_.empty()) {
        error_ = "KDEF script is empty or has odd byte length";
        return current_result(SceneStepKind::stay);
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
    tournament_trial_state_.reset();
    queued_outputs_.clear();
    return run_event();
}

SceneStepResult SceneSession::resume(const SceneResponse response, const int value) {
    if (!valid()) {
        return current_result(SceneStepKind::stay);
    }
    const auto previous_kind = pending_.kind;
    const auto previous_shop_id = pending_.shop_id;
    pending_ = current_result(SceneStepKind::stay);
    pending_text_.clear();

    if (previous_kind == SceneStepKind::scene_title) {
        return run_auto_event(SceneStepKind::stay);
    }
    if (previous_kind == SceneStepKind::return_world || previous_kind == SceneStepKind::quit ||
        previous_kind == SceneStepKind::open_ui) {
        pending_.kind = previous_kind;
        return pending_;
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

    if (continuation_ == PendingContinuation::shop_feedback && queued_outputs_.empty()) {
        close_shop_events();
        continuation_ = PendingContinuation::none;
    }

    if (continuation_ == PendingContinuation::conditional) {
        const auto accepted = response == SceneResponse::yes;
        program_counter_ += accepted ? true_offset_ : false_offset_;
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
        return run_event();
    }
    clear_event();
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
            ++program_counter_;
            continue;
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
        case 11:
            true_offset_ = argument(1);
            false_offset_ = argument(2);
            program_counter_ += 3;
            continuation_ = PendingContinuation::conditional;
            pending_ = current_result(SceneStepKind::question);
            pending_.question = opcode == 5 ? SceneQuestion::battle
                                            : (opcode == 9 ? SceneQuestion::join : SceneQuestion::rest);
            return pending_;
        case 6:
            true_offset_ = argument(2);
            false_offset_ = argument(3);
            battle_get_exp_ = argument(4);
            program_counter_ += 5;
            continuation_ = PendingContinuation::battle;
            pending_ = current_result(SceneStepKind::battle);
            pending_.battle_id = argument(1);
            return pending_;
        case 7:
            clear_event();
            return current_result(SceneStepKind::stay);
        case 8:
            audio_commands_.push_back(SceneAudioCommand{SceneAudioCommand::Kind::music, argument(1)});
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
        case 24:
            program_counter_ += static_cast<std::ptrdiff_t>(width);
            event_active_ = false;
            pending_ = current_result(SceneStepKind::quit);
            return pending_;
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
        case 26:
            for (std::size_t field = 0U; field < 3U; ++field) {
                const auto event_field_id = static_cast<model::SceneEventField>(
                    static_cast<std::size_t>(model::SceneEventField::event_1) + field);
                const auto value = event_field(argument(1) == -2 ? scene_id_ : argument(1), argument(2), event_field_id).value_or(0);
                set_event_field(argument(1) == -2 ? scene_id_ : argument(1), argument(2), event_field_id,
                                static_cast<std::int16_t>(value + argument(3U + field)));
            }
            program_counter_ += 6;
            break;
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
        case 46:
        case 47:
        case 48: {
            const auto role_id = argument(1);
            if (role_id >= 0 && static_cast<std::size_t>(role_id) < snapshot_.ranger.roles.size()) {
                auto& role = snapshot_.ranger.roles[static_cast<std::size_t>(role_id)];
                const auto field = opcode == 34 ? model::role_word::iq
                                  : opcode == 45 ? model::role_word::speed
                                  : opcode == 46 ? model::role_word::maximum_mp
                                  : opcode == 47 ? model::role_word::attack
                                                 : model::role_word::maximum_hp;
                const auto maximum = (opcode == 46 || opcode == 48) ? std::int16_t{999} : std::int16_t{100};
                const auto before = role.word(field);
                const auto after = clamped_add(before, argument(2), 0, maximum);
                role.set_word(field, after);
                if (opcode == 46) {
                    role.set_word(model::role_word::mp, clamped_add(role.word(model::role_word::mp), argument(2), 0, after));
                } else if (opcode == 48) {
                    role.set_word(model::role_word::hp, clamped_add(role.word(model::role_word::hp), argument(2), 0, after));
                }
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
            const auto field = opcode == 52 ? model::role_word::morality : model::role_word::fame;
            const auto value = snapshot_.ranger.roles.empty() ? 0 : snapshot_.ranger.roles[0].word(field);
            program_counter_ += 1;
            queue_notice(ascii_message(std::string{opcode == 52 ? "morality " : "fame "} + std::to_string(value)));
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
                    static_cast<std::int16_t>(role.word(model::role_word::fame) + argument(1)));
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
            audio_commands_.push_back(SceneAudioCommand{SceneAudioCommand::Kind::music, argument(1)});
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
    if (event.has_value()) {
        const auto script = event_field(scene_id_, *event, model::SceneEventField::event_3);
        if (script.has_value() && *script > 0) {
            return begin_event(
                *script,
                *event,
                static_cast<std::int16_t>(scene_x_),
                static_cast<std::int16_t>(scene_y_));
        }
    }
    pending_ = current_result(SceneStepKind::stay);
    return current_result(fallback);
}

void SceneSession::periodic_tick() {
    if (valid() && weather_enabled_ && pending_.kind == SceneStepKind::stay) {
        update_weather();
    }
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
    for (std::size_t event = 0U; event < model::kSceneEventCount; ++event) {
        const auto begin = event_field(scene_id_, static_cast<std::int16_t>(event), model::SceneEventField::begin_picture).value_or(0);
        const auto end = event_field(scene_id_, static_cast<std::int16_t>(event), model::SceneEventField::end_picture).value_or(0);
        auto current = event_field(scene_id_, static_cast<std::int16_t>(event), model::SceneEventField::current_picture).value_or(0);
        const auto delay = event_field(scene_id_, static_cast<std::int16_t>(event), model::SceneEventField::picture_delay).value_or(0);
        if (begin <= 0) {
            continue;
        }
        if (current >= end) {
            current = begin;
        }
        if (current > begin && animation_counter_ % 4 == 0 && current < end) {
            current = static_cast<std::int16_t>(current + 2);
        }
        if (delay <= animation_counter_ % 100 && current == begin && current < end) {
            current = static_cast<std::int16_t>(current + 2);
        }
        set_event_field(scene_id_, static_cast<std::int16_t>(event), model::SceneEventField::current_picture, current);
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
    if (target_scene == scene_id_ && (arguments[11] != -2 || arguments[12] != -2)) {
        const auto old_x = event_field(target_scene, target_event, model::SceneEventField::x).value_or(-1);
        const auto old_y = event_field(target_scene, target_event, model::SceneEventField::y).value_or(-1);
        if (old_x >= 0 && old_y >= 0) {
            set_scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::event_index), old_x, old_y, -1);
        }
        const auto new_x = arguments[11] == -2 ? old_x : arguments[11];
        const auto new_y = arguments[12] == -2 ? old_y : arguments[12];
        set_scene_value(scene_id_, static_cast<std::int16_t>(model::SceneLayer::event_index), new_x, new_y, target_event);
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
    if (role_id < 0 || item_id < 0 || count == 0 ||
        static_cast<std::size_t>(role_id) >= snapshot_.ranger.roles.size()) {
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
            if (role.word(model::role_word::taking_item_begin + index) < 0) {
                slot = static_cast<int>(index);
                break;
            }
        }
    }
    if (slot < 0) {
        return;
    }
    const auto index = static_cast<std::size_t>(slot);
    const auto before = role.word(model::role_word::taking_item_count_begin + index);
    role.set_word(model::role_word::taking_item_begin + index, item_id);
    role.set_word(model::role_word::taking_item_count_begin + index,
                  static_cast<std::int16_t>(before + count));
    if (role.word(model::role_word::taking_item_count_begin + index) <= 0) {
        role.set_word(model::role_word::taking_item_begin + index, -1);
        role.set_word(model::role_word::taking_item_count_begin + index, 0);
    }
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

void SceneSession::queue_notice(std::vector<std::uint8_t> text) {
    queued_outputs_.push_back(QueuedOutput{current_result(SceneStepKind::notice), std::move(text)});
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
            event_active_ = false;
            pending_ = current_result(SceneStepKind::quit);
            return pending_;
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
                event_active_ = false;
                pending_ = current_result(SceneStepKind::quit);
                return pending_;
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
                     : (step < 0 ? SceneDirection::up : SceneDirection::down);
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
                const auto picture = event_field(scene_id_, *event, model::SceneEventField::current_picture).value_or(0);
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
        pending_.kind == SceneStepKind::present ||
        pending_.kind == SceneStepKind::fade_from_black ||
        pending_.kind == SceneStepKind::fade_to_black ||
        pending_.kind == SceneStepKind::return_world || pending_.kind == SceneStepKind::quit ||
        pending_.kind == SceneStepKind::open_ui || pending_.kind == SceneStepKind::battle) {
        return true;
    }
    int x = 12;
    int y = 12;
    int width = 218;
    int height = 57;
    if (pending_.kind == SceneStepKind::dialogue) {
        switch (pending_.style) {
        case 1:
        case 3: x = 8; y = 130; break;
        case 4: x = 8; y = 17; break;
        case 5: x = 94; y = 130; break;
        default: x = 94; y = 17; break;
        }
    } else if (pending_.kind == SceneStepKind::scene_title) {
        width = 140;
        height = 27;
        x = 90;
        y = 10;
    } else {
        width = 212;
        height = 27;
        x = 54;
        y = 40;
    }
    if (!framebuffer.fill_rectangle(x, y, static_cast<std::uint16_t>(width),
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
        if (!render::draw_legacy_text(
                framebuffer,
                x + 4,
                y + 3 + line_index * 17,
                line,
                ascii_font_,
                cache,
                0x17U,
                0x15U)) {
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
