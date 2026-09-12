#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "openlegend/app/runtime_configuration.hpp"
#include "test_support.hpp"

namespace {

class TemporaryTree {
public:
    TemporaryTree() {
        static std::atomic_uint64_t sequence{};
        const auto stamp = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        root_ = std::filesystem::temp_directory_path() /
            ("openlegend-config-test-" + std::to_string(stamp) + "-" +
             std::to_string(sequence.fetch_add(1U)));
        executable_directory_ = root_ / "bin";
        launch_directory_ = root_ / "launch";
        configured_directory_ = root_ / path_from_literal(u8"配置数据");
        command_directory_ = root_ / path_from_literal(u8"命令行数据");
        save_directory_ = root_ / path_from_literal(u8"独立存档");
        std::filesystem::create_directories(executable_directory_);
        std::filesystem::create_directories(launch_directory_);
        std::filesystem::create_directories(configured_directory_);
        std::filesystem::create_directories(command_directory_);
        std::filesystem::create_directories(save_directory_);
    }

    ~TemporaryTree() {
        std::error_code error;
        std::filesystem::remove_all(root_, error);
    }

    TemporaryTree(const TemporaryTree&) = delete;
    TemporaryTree& operator=(const TemporaryTree&) = delete;

    [[nodiscard]] const std::filesystem::path& executable_directory() const noexcept {
        return executable_directory_;
    }

    [[nodiscard]] const std::filesystem::path& launch_directory() const noexcept {
        return launch_directory_;
    }

    [[nodiscard]] const std::filesystem::path& configured_directory() const noexcept {
        return configured_directory_;
    }

    [[nodiscard]] const std::filesystem::path& command_directory() const noexcept {
        return command_directory_;
    }

    [[nodiscard]] const std::filesystem::path& save_directory() const noexcept {
        return save_directory_;
    }

    [[nodiscard]] std::filesystem::path configuration_path() const {
        return executable_directory_ / openlegend::app::kConfigurationFilename;
    }

    void write_configuration(const std::string_view text) const {
        std::ofstream output{configuration_path(), std::ios::binary | std::ios::trunc};
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

private:
    [[nodiscard]] static std::filesystem::path path_from_literal(const std::u8string_view text) {
        return std::filesystem::path{std::u8string{text}};
    }

    std::filesystem::path root_;
    std::filesystem::path executable_directory_;
    std::filesystem::path launch_directory_;
    std::filesystem::path configured_directory_;
    std::filesystem::path command_directory_;
    std::filesystem::path save_directory_;
};

[[nodiscard]] std::string utf8_bytes(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return std::string{
        reinterpret_cast<const char*>(text.data()),
        reinterpret_cast<const char*>(text.data() + text.size())};
}

void test_missing_configuration_uses_launch_directory() {
    using namespace openlegend::app;
    const TemporaryTree tree;
    const auto resolution = resolve_data_directory(
        {}, tree.executable_directory(), tree.launch_directory());
    OL_CHECK(resolution.status == DataDirectoryStatus::ready);
    OL_CHECK(resolution.source == DataDirectorySource::launch_directory);
    OL_CHECK(resolution.directory == std::filesystem::absolute(tree.launch_directory()));

    constexpr WindowSize fallback{960, 600};
    const auto window = load_window_configuration(tree.configuration_path(), fallback);
    OL_CHECK(window.status == WindowConfigurationStatus::ready);
    OL_CHECK(window.size == fallback);
    OL_CHECK(!window.maximized);
    OL_CHECK(!window.loaded_from_file);
}

void test_configuration_paths_and_window() {
    using namespace openlegend::app;
    const TemporaryTree tree;
    const auto relative_data = std::filesystem::relative(
        tree.configured_directory(), tree.executable_directory());
    tree.write_configuration(
        "[paths]\n"
        "data_dir = '" + utf8_bytes(relative_data) + "'\n"
        "\n[window]\n"
        "width = 1280\n"
        "height = 800\n"
        "maximized = true\n");

    const std::array<std::string_view, 1> arguments{"--smoke-test"};
    const auto resolution = resolve_data_directory(
        arguments, tree.executable_directory(), tree.launch_directory());
    OL_CHECK(resolution.status == DataDirectoryStatus::ready);
    OL_CHECK(resolution.source == DataDirectorySource::configuration_file);
    OL_CHECK(resolution.directory == std::filesystem::absolute(tree.configured_directory()));

    const auto window = load_window_configuration(tree.configuration_path(), {960, 600});
    OL_CHECK(window.status == WindowConfigurationStatus::ready);
    OL_CHECK((window.size == WindowSize{1280, 800}));
    OL_CHECK(window.maximized);
    OL_CHECK(window.loaded_from_file);
}

void test_runtime_configuration_root() {
    using namespace openlegend::app;
    using openlegend::diagnostics::LogLevel;
    const TemporaryTree tree;
    const auto relative_data = std::filesystem::relative(
        tree.configured_directory(), tree.executable_directory());
    const auto relative_save = std::filesystem::relative(
        tree.save_directory(), tree.executable_directory());
    tree.write_configuration(
        "[paths]\n"
        "data_dir = '" + utf8_bytes(relative_data) + "'\n"
        "save_dir = '" + utf8_bytes(relative_save) + "'\n"
        "\n[logging]\n"
        "path = 'diagnostics/runtime.log'\n"
        "level = 'debug'\n"
        "\n[input]\n"
        "movement_repeat_delay_ms = 25\n"
        "menu_repeat_delay_ms = 30\n"
        "menu_repeat_interval_ms = 35\n"
        "\n[timing]\n"
        "fade_frame_delay_ms = 5.5\n"
        "\n[window]\n"
        "width = 1280\n"
        "height = 720\n"
        "maximized = true\n"
        "\n[display]\n"
        "width = 640\n"
        "height = 360\n");

    const auto configuration = load_runtime_configuration(
        {},
        tree.configuration_path(),
        tree.executable_directory(),
        tree.launch_directory(),
        RuntimeConfigurationDefaults{
            tree.executable_directory() / "logs" / "openlegend.log",
            LogLevel::info,
            WindowSize{960, 600},
            std::chrono::milliseconds{500},
            std::chrono::milliseconds{500},
            std::chrono::milliseconds{55},
            std::chrono::milliseconds{14},
            GameResolution{320, 200}});

    OL_CHECK(configuration.paths.data_directory.status == DataDirectoryStatus::ready);
    OL_CHECK(configuration.paths.data_directory.source ==
        DataDirectorySource::configuration_file);
    OL_CHECK(configuration.paths.data_directory.directory ==
        std::filesystem::absolute(tree.configured_directory()));
    OL_CHECK(configuration.paths.save_directory.status ==
        SaveDirectoryConfigurationStatus::ready);
    OL_CHECK(configuration.paths.save_directory.configured);
    OL_CHECK(configuration.paths.save_directory.directory ==
        std::filesystem::absolute(tree.save_directory()));
    OL_CHECK(configuration.logging.status == LoggingConfigurationStatus::ready);
    OL_CHECK(configuration.logging.path ==
        (tree.executable_directory() / "diagnostics" / "runtime.log").lexically_normal());
    OL_CHECK(configuration.logging.minimum_level == LogLevel::debug);
    OL_CHECK(configuration.input.status == InputConfigurationStatus::ready);
    OL_CHECK(configuration.input.movement_repeat_delay == std::chrono::milliseconds{25});
    OL_CHECK(configuration.input.menu_repeat_delay == std::chrono::milliseconds{30});
    OL_CHECK(configuration.input.menu_repeat_interval == std::chrono::milliseconds{35});
    OL_CHECK(configuration.timing.status == TimingConfigurationStatus::ready);
    OL_CHECK(configuration.timing.fade_frame_delay == std::chrono::microseconds{5500});
    OL_CHECK(configuration.window.status == WindowConfigurationStatus::ready);
    OL_CHECK((configuration.window.size == WindowSize{1280, 720}));
    OL_CHECK(configuration.window.maximized);
    OL_CHECK(configuration.display.status == DisplayConfigurationStatus::ready);
    OL_CHECK((configuration.display.resolution == GameResolution{640, 360}));
    OL_CHECK(configuration.display.loaded_from_file);
}

void test_display_configuration() {
    using namespace openlegend::app;
    const TemporaryTree tree;
    constexpr GameResolution fallback{320, 200};
    const auto load = [&tree]() {
        return load_display_configuration(
            tree.configuration_path(), GameResolution{320, 200});
    };

    const auto missing = load();
    OL_CHECK(missing.status == DisplayConfigurationStatus::read_failed);
    OL_CHECK(missing.resolution == fallback);
    OL_CHECK(!missing.loaded_from_file);

    tree.write_configuration("[window]\nwidth = 960\nheight = 600\n");
    const auto absent = load();
    OL_CHECK(absent.status == DisplayConfigurationStatus::ready);
    OL_CHECK(absent.resolution == fallback);
    OL_CHECK(!absent.loaded_from_file);

    tree.write_configuration("[display]\n");
    const auto empty = load();
    OL_CHECK(empty.status == DisplayConfigurationStatus::ready);
    OL_CHECK(empty.resolution == fallback);
    OL_CHECK(!empty.loaded_from_file);
    tree.write_configuration(
        "[display]\n"
        "# width = 1280\n"
        "# height = 720\n");
    const auto commented = load();
    OL_CHECK(commented.status == DisplayConfigurationStatus::ready);
    OL_CHECK(commented.resolution == fallback);
    OL_CHECK(!commented.loaded_from_file);

    tree.write_configuration("[display]\nwidth = 320\nheight = 200\n");
    OL_CHECK(load().status == DisplayConfigurationStatus::ready);
    tree.write_configuration("[display]\nwidth = 1280\nheight = 800\n");
    OL_CHECK(load().status == DisplayConfigurationStatus::ready);
    tree.write_configuration("[display]\nwidth = 640\nheight = 360\n");
    const auto configured = load();
    OL_CHECK(configured.status == DisplayConfigurationStatus::ready);
    OL_CHECK((configured.resolution == GameResolution{640, 360}));
    OL_CHECK(configured.loaded_from_file);

    tree.write_configuration("[display\n");
    OL_CHECK(load().status == DisplayConfigurationStatus::parse_failed);
    tree.write_configuration("display = 7\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_display_table);
    tree.write_configuration("[display]\nwidth = 640\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_game_resolution);
    tree.write_configuration("[display]\nheight = 360\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_game_resolution);
    tree.write_configuration("[display]\nwidth = 319\nheight = 200\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_game_resolution);
    tree.write_configuration("[display]\nwidth = 320\nheight = 199\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_game_resolution);
    tree.write_configuration("[display]\nwidth = 1281\nheight = 800\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_game_resolution);
    tree.write_configuration("[display]\nwidth = 1280\nheight = 801\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_game_resolution);
    tree.write_configuration("[display]\nwidth = '640'\nheight = 360\n");
    OL_CHECK(load().status ==
        DisplayConfigurationStatus::invalid_game_resolution);
}

void test_input_configuration() {
    using namespace openlegend::app;
    using namespace std::chrono_literals;
    const TemporaryTree tree;

    const auto load = [&tree]() {
        return load_input_configuration(
            tree.configuration_path(), 400ms, 450ms, 60ms);
    };

    const auto missing = load();
    OL_CHECK(missing.status == InputConfigurationStatus::read_failed);
    OL_CHECK(missing.movement_repeat_delay == 400ms);
    OL_CHECK(missing.menu_repeat_delay == 450ms);
    OL_CHECK(missing.menu_repeat_interval == 60ms);

    tree.write_configuration(
        "[input]\n"
        "movement_repeat_delay_ms = 25\n"
        "menu_repeat_delay_ms = 30\n"
        "menu_repeat_interval_ms = 35\n");
    const auto configured = load();
    OL_CHECK(configured.status == InputConfigurationStatus::ready);
    OL_CHECK(configured.loaded_from_file);
    OL_CHECK(configured.movement_repeat_delay == 25ms);
    OL_CHECK(configured.menu_repeat_delay == 30ms);
    OL_CHECK(configured.menu_repeat_interval == 35ms);

    tree.write_configuration(
        "[input]\n"
        "movement_repeat_delay_ms = 0\n"
        "menu_repeat_interval_ms = 1\n");
    const auto partial = load();
    OL_CHECK(partial.status == InputConfigurationStatus::ready);
    OL_CHECK(partial.loaded_from_file);
    OL_CHECK(partial.movement_repeat_delay == 0ms);
    OL_CHECK(partial.menu_repeat_delay == 450ms);
    OL_CHECK(partial.menu_repeat_interval == 1ms);

    tree.write_configuration("input = 7\n");
    OL_CHECK(load().status == InputConfigurationStatus::invalid_input_table);
    tree.write_configuration("[input]\nmovement_repeat_delay_ms = -1\n");
    OL_CHECK(load().status ==
        InputConfigurationStatus::invalid_movement_repeat_delay);
    tree.write_configuration("[input]\nmenu_repeat_delay_ms = -1\n");
    OL_CHECK(load().status == InputConfigurationStatus::invalid_menu_repeat_delay);
    tree.write_configuration("[input]\nmenu_repeat_interval_ms = 0\n");
    OL_CHECK(load().status ==
        InputConfigurationStatus::invalid_menu_repeat_interval);
}

void test_save_directory_configuration() {
    using namespace openlegend::app;
    using openlegend::diagnostics::LogLevel;
    const TemporaryTree tree;
    const auto load = [&tree]() {
        return load_runtime_configuration(
            {},
            tree.configuration_path(),
            tree.executable_directory(),
            tree.launch_directory(),
            RuntimeConfigurationDefaults{
                tree.executable_directory() / "logs" / "openlegend.log",
                LogLevel::info,
                WindowSize{960, 600},
                std::chrono::milliseconds{500},
                std::chrono::milliseconds{500},
                std::chrono::milliseconds{55},
                std::chrono::milliseconds{14},
                GameResolution{320, 200}});
    };

    const auto missing = load();
    OL_CHECK(missing.paths.save_directory.status ==
        SaveDirectoryConfigurationStatus::ready);
    OL_CHECK(!missing.paths.save_directory.configured);
    OL_CHECK(missing.paths.save_directory.directory.empty());

    const auto relative_save = std::filesystem::relative(
        tree.save_directory(), tree.executable_directory());
    tree.write_configuration(
        "[paths]\n"
        "save_dir = '" + utf8_bytes(relative_save) + "'\n");
    const auto configured = load();
    OL_CHECK(configured.paths.save_directory.status ==
        SaveDirectoryConfigurationStatus::ready);
    OL_CHECK(configured.paths.save_directory.configured);
    OL_CHECK(configured.paths.save_directory.directory ==
        std::filesystem::absolute(tree.save_directory()));

    tree.write_configuration("[paths]\nsave_dir = 7\n");
    OL_CHECK(load().paths.save_directory.status ==
        SaveDirectoryConfigurationStatus::invalid_save_directory);

    tree.write_configuration("[paths]\nsave_dir = ''\n");
    OL_CHECK(load().paths.save_directory.status ==
        SaveDirectoryConfigurationStatus::invalid_save_directory);

    tree.write_configuration("paths = 7\n");
    OL_CHECK(load().paths.save_directory.status ==
        SaveDirectoryConfigurationStatus::invalid_paths_table);
}

void test_logging_configuration() {
    const openlegend::test::ScopedTimeZone time_zone{"PST8"};
    using namespace openlegend::app;
    using openlegend::diagnostics::LogLevel;
    const TemporaryTree tree;
    const auto fallback = tree.executable_directory() / "logs" / "openlegend.log";

    const auto missing = load_logging_configuration(
        tree.configuration_path(), tree.executable_directory(), fallback, LogLevel::info);
    OL_CHECK(missing.status == LoggingConfigurationStatus::ready);
    OL_CHECK(missing.path == fallback);
    OL_CHECK(missing.minimum_level == LogLevel::info);
    OL_CHECK(!missing.loaded_from_file);
    const auto first_session = make_session_log_path(
        missing.path, std::chrono::system_clock::time_point{}, 42U);
    const auto second_session = make_session_log_path(
        missing.path, std::chrono::system_clock::time_point{}, 43U);
    OL_CHECK(first_session ==
             tree.executable_directory() / "logs" /
                 "openlegend-1969-12-31_16-00-00-42.log");
    OL_CHECK(second_session != first_session);

    tree.write_configuration(
        "[logging]\n"
        "path = 'diagnostics/session.log'\n"
        "level = 'trace'\n");
    const auto loaded = load_logging_configuration(
        tree.configuration_path(), tree.executable_directory(), fallback, LogLevel::info);
    OL_CHECK(loaded.status == LoggingConfigurationStatus::ready);
    OL_CHECK(loaded.path ==
             (tree.executable_directory() / "diagnostics" / "session.log").lexically_normal());
    OL_CHECK(loaded.minimum_level == LogLevel::trace);
    OL_CHECK(loaded.loaded_from_file);
    OL_CHECK(make_session_log_path(
                 loaded.path, std::chrono::system_clock::time_point{}, 7U) ==
             tree.executable_directory() / "diagnostics" /
                 "session-1969-12-31_16-00-00-7.log");

    tree.write_configuration("logging = 7\n");
    OL_CHECK(load_logging_configuration(
                 tree.configuration_path(), tree.executable_directory(), fallback, LogLevel::info)
                 .status == LoggingConfigurationStatus::invalid_logging_table);

    tree.write_configuration("[logging]\npath = ''\n");
    OL_CHECK(load_logging_configuration(
                 tree.configuration_path(), tree.executable_directory(), fallback, LogLevel::info)
                 .status == LoggingConfigurationStatus::invalid_log_path);

    tree.write_configuration("[logging]\nlevel = 'verbose'\n");
    const auto invalid_level = load_logging_configuration(
        tree.configuration_path(), tree.executable_directory(), fallback, LogLevel::info);
    OL_CHECK(invalid_level.status == LoggingConfigurationStatus::invalid_log_level);
    OL_CHECK(invalid_level.detail == "verbose");
}

void test_command_line_overrides_configuration() {
    using namespace openlegend::app;
    const TemporaryTree tree;
    tree.write_configuration("[paths]\ndata_dir = '../missing-configured-data'\n");
    const std::string command_path = utf8_bytes(tree.command_directory());
    const std::array<std::string_view, 3> arguments{
        "--smoke-test", "--data-dir", command_path};
    const auto resolution = resolve_data_directory(
        arguments, tree.executable_directory(), tree.launch_directory());
    OL_CHECK(resolution.status == DataDirectoryStatus::ready);
    OL_CHECK(resolution.source == DataDirectorySource::command_line);
    OL_CHECK(resolution.directory == std::filesystem::absolute(tree.command_directory()));

    const std::string equals_argument = "--data-dir=" + command_path;
    const std::array<std::string_view, 2> equals_arguments{"--smoke-test", equals_argument};
    const auto equals_resolution = resolve_data_directory(
        equals_arguments, tree.executable_directory(), tree.launch_directory());
    OL_CHECK(equals_resolution.status == DataDirectoryStatus::ready);
    OL_CHECK(equals_resolution.source == DataDirectorySource::command_line);
    OL_CHECK(equals_resolution.directory == std::filesystem::absolute(tree.command_directory()));
}

void test_configuration_errors() {
    using namespace openlegend::app;
    const TemporaryTree tree;

    tree.write_configuration("paths = 7\n");
    OL_CHECK(resolve_data_directory({}, tree.executable_directory(), tree.launch_directory()).status ==
        DataDirectoryStatus::configuration_paths_not_table);

    tree.write_configuration("[paths]\ndata_dir = 7\n");
    OL_CHECK(resolve_data_directory({}, tree.executable_directory(), tree.launch_directory()).status ==
        DataDirectoryStatus::configuration_value_not_string);

    tree.write_configuration("[paths]\ndata_dir = ''\n");
    OL_CHECK(resolve_data_directory({}, tree.executable_directory(), tree.launch_directory()).status ==
        DataDirectoryStatus::empty_configuration_value);

    tree.write_configuration("[paths\n");
    OL_CHECK(resolve_data_directory({}, tree.executable_directory(), tree.launch_directory()).status ==
        DataDirectoryStatus::configuration_parse_failed);

    const std::array<std::string_view, 1> missing_value{"--data-dir"};
    OL_CHECK(resolve_data_directory(
                 missing_value, tree.executable_directory(), tree.launch_directory()).status ==
        DataDirectoryStatus::missing_command_line_value);

    const std::array<std::string_view, 1> empty_value{"--data-dir="};
    OL_CHECK(resolve_data_directory(
                 empty_value, tree.executable_directory(), tree.launch_directory()).status ==
        DataDirectoryStatus::empty_command_line_value);

    tree.write_configuration("[paths]\ndata_dir = '../does-not-exist'\n");
    OL_CHECK(resolve_data_directory({}, tree.executable_directory(), tree.launch_directory()).status ==
        DataDirectoryStatus::directory_not_found_or_not_directory);
}

void test_data_directory_activation() {
    using namespace openlegend::app;
    const TemporaryTree tree;
    std::error_code error;
    const auto original = std::filesystem::current_path(error);
    OL_CHECK(!error);
    OL_CHECK(activate_data_directory(tree.configured_directory(), error));
    OL_CHECK(!error);
    OL_CHECK(std::filesystem::current_path() == tree.configured_directory());
    std::filesystem::current_path(original, error);
    OL_CHECK(!error);
}

void test_window_errors_and_schema_writeback() {
    using namespace openlegend::app;
    const TemporaryTree tree;
    constexpr WindowSize fallback{960, 600};

    tree.write_configuration("window = 7\n");
    OL_CHECK(load_window_configuration(tree.configuration_path(), fallback).status ==
        WindowConfigurationStatus::invalid_window_table);

    tree.write_configuration("[window]\nwidth = 0\nheight = 600\n");
    OL_CHECK(load_window_configuration(tree.configuration_path(), fallback).status ==
        WindowConfigurationStatus::invalid_window_size);

    tree.write_configuration(
        "[window]\nwidth = 960\nheight = 600\nmaximized = 'yes'\n");
    OL_CHECK(load_window_configuration(tree.configuration_path(), fallback).status ==
        WindowConfigurationStatus::invalid_window_state);

    const auto relative_data = std::filesystem::relative(
        tree.configured_directory(), tree.executable_directory());
    const auto relative_save = std::filesystem::relative(
        tree.save_directory(), tree.executable_directory());
    tree.write_configuration(
        "[future]\nkept = 42\n"
        "\n[window]\ncustom = 'preserved'\n"
        "\n[display]\nheight = 360\n"
        "custom = 'discarded'\n"
        "width = 640\n"
        "\n[input]\nmenu_repeat_interval_ms = 55\n"
        "custom = 'discarded'\n"
        "movement_repeat_delay_ms = 500\n"
        "menu_repeat_delay_ms = 500\n"
        "\n[paths]\ndata_dir = '" + utf8_bytes(relative_data) + "'\n"
        "save_dir = '" + utf8_bytes(relative_save) + "'\n");
    std::string detail;
    OL_CHECK(save_window_configuration(
                 tree.configuration_path(), WindowSize{1024, 640}, true, detail) ==
        WindowConfigurationStatus::ready);
    OL_CHECK(detail.empty());
    const auto window = load_window_configuration(tree.configuration_path(), fallback);
    OL_CHECK((window.size == WindowSize{1024, 640}));
    OL_CHECK(window.maximized);
    const auto paths = resolve_data_directory(
        {}, tree.executable_directory(), tree.launch_directory());
    OL_CHECK(paths.directory == std::filesystem::absolute(tree.configured_directory()));

    std::ifstream input{tree.configuration_path(), std::ios::binary};
    const std::string saved{
        std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const auto paths_position = saved.find("[paths]");
    const auto input_position = saved.find("[input]");
    const auto window_position = saved.find("[window]");
    const auto display_position = saved.find("[display]");
    OL_CHECK(paths_position != std::string::npos);
    OL_CHECK(input_position != std::string::npos);
    OL_CHECK(window_position != std::string::npos);
    OL_CHECK(display_position != std::string::npos);
    OL_CHECK(paths_position < input_position);
    OL_CHECK(input_position < window_position);
    OL_CHECK(window_position < display_position);
    const auto data_directory_position = saved.find("data_dir = ", paths_position);
    const auto save_directory_position = saved.find("save_dir = ", paths_position);
    OL_CHECK(data_directory_position < save_directory_position);
    OL_CHECK(save_directory_position < input_position);
    const auto movement_delay_position =
        saved.find("movement_repeat_delay_ms = 500", input_position);
    const auto menu_delay_position =
        saved.find("menu_repeat_delay_ms = 500", input_position);
    const auto menu_interval_position =
        saved.find("menu_repeat_interval_ms = 55", input_position);
    OL_CHECK(movement_delay_position < menu_delay_position);
    OL_CHECK(menu_delay_position < menu_interval_position);
    OL_CHECK(menu_interval_position < window_position);
    const auto width_position = saved.find("width = 1024", window_position);
    const auto height_position = saved.find("height = 640", window_position);
    const auto maximized_position = saved.find("maximized = true", window_position);
    OL_CHECK(width_position < height_position);
    OL_CHECK(height_position < maximized_position);
    const auto display_width_position =
        saved.find("width = 640", display_position);
    const auto display_height_position =
        saved.find("height = 360", display_position);
    OL_CHECK(display_width_position < display_height_position);
    OL_CHECK(saved.find("custom = ") == std::string::npos);
    OL_CHECK(saved.find("[future]") == std::string::npos);
    OL_CHECK(saved.find("kept = 42") == std::string::npos);

    OL_CHECK(save_window_configuration(
                 tree.configuration_path(), WindowSize{0, 600}, false, detail) ==
        WindowConfigurationStatus::invalid_window_size);
}

}  // namespace

void run_runtime_configuration_tests() {
    test_missing_configuration_uses_launch_directory();
    test_configuration_paths_and_window();
    test_runtime_configuration_root();
    test_display_configuration();
    test_input_configuration();
    test_save_directory_configuration();
    test_logging_configuration();
    test_command_line_overrides_configuration();
    test_configuration_errors();
    test_data_directory_activation();
    test_window_errors_and_schema_writeback();
}
