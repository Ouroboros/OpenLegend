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
        std::filesystem::create_directories(executable_directory_);
        std::filesystem::create_directories(launch_directory_);
        std::filesystem::create_directories(configured_directory_);
        std::filesystem::create_directories(command_directory_);
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

void test_window_errors_and_lossless_other_tables() {
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
    tree.write_configuration(
        "[paths]\ndata_dir = '" + utf8_bytes(relative_data) +
        "'\n\n[future]\nkept = 42\n");
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
    OL_CHECK(saved.find("[future]") != std::string::npos);
    OL_CHECK(saved.find("kept = 42") != std::string::npos);

    OL_CHECK(save_window_configuration(
                 tree.configuration_path(), WindowSize{0, 600}, false, detail) ==
        WindowConfigurationStatus::invalid_window_size);
}

}  // namespace

void run_runtime_configuration_tests() {
    test_missing_configuration_uses_launch_directory();
    test_configuration_paths_and_window();
    test_command_line_overrides_configuration();
    test_configuration_errors();
    test_data_directory_activation();
    test_window_errors_and_lossless_other_tables();
}
