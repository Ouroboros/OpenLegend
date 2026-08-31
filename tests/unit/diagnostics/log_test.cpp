#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "openlegend/diagnostics/log.hpp"
#include "test_support.hpp"

#ifndef OPENLEGEND_TEST_OUTPUT_ROOT
#error OPENLEGEND_TEST_OUTPUT_ROOT must name a build-tree directory
#endif

namespace {

using openlegend::diagnostics::LogLevel;
using openlegend::diagnostics::LoggingInitializationStatus;
using openlegend::diagnostics::initialize_logging;
using openlegend::diagnostics::log_debug;
using openlegend::diagnostics::log_info;
using openlegend::diagnostics::log_warning;
using openlegend::diagnostics::logging_to_file;
using openlegend::diagnostics::set_minimum_log_level;
using openlegend::diagnostics::shutdown_logging;

class TestTree {
public:
    TestTree() {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = openlegend::test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) /
                ("logging-" + std::to_string(unique));
        std::filesystem::create_directories(root_);
    }

    ~TestTree() {
        shutdown_logging();
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    [[nodiscard]] std::filesystem::path path(const char* name) const {
        return root_ / name;
    }

    [[nodiscard]] std::string read(const std::filesystem::path& path) const {
        std::ifstream input{path, std::ios::binary};
        return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    }

private:
    std::filesystem::path root_;
};

void check_metadata_and_flush() {
    const TestTree tree;
    const auto path = tree.path("metadata/openlegend.log");
    OL_CHECK(initialize_logging(path, LogLevel::trace) ==
             LoggingInitializationStatus::initialized);
    OL_CHECK(logging_to_file());

    const auto message_line = __LINE__ + 1U;
    log_info("first line\nsecond line");
    const auto contents = tree.read(path);
    OL_CHECK(contents.size() > 24U);
    OL_CHECK(contents[4] == '-' && contents[7] == '-' && contents[10] == 'T' &&
             contents[13] == ':' && contents[16] == ':' && contents[19] == '.' &&
             contents[23] == 'Z');
    OL_CHECK(contents.find(" [INFO] [thread=") != std::string::npos);
    OL_CHECK(contents.find(
                 "[log_test.cpp:" + std::to_string(message_line) +
                 "] first line\\nsecond line") != std::string::npos);
}

void check_level_filter() {
    const TestTree tree;
    const auto path = tree.path("levels.log");
    OL_CHECK(initialize_logging(path, LogLevel::warning) ==
             LoggingInitializationStatus::initialized);
    log_info("hidden-info-record");
    log_warning("visible-warning-record");
    const auto contents = tree.read(path);
    OL_CHECK(contents.find("hidden-info-record") == std::string::npos);
    OL_CHECK(contents.find("[WARNING]") != std::string::npos);
    OL_CHECK(contents.find("visible-warning-record") != std::string::npos);
}

void check_initialization_failure_and_recovery() {
    const TestTree tree;
    const auto blocker = tree.path("not-a-directory");
    {
        std::ofstream output{blocker, std::ios::binary};
        output << "blocker";
    }
    OL_CHECK(initialize_logging(blocker / "openlegend.log") ==
             LoggingInitializationStatus::directory_creation_failed);
    OL_CHECK(!logging_to_file());

    const auto recovered = tree.path("recovered.log");
    OL_CHECK(initialize_logging(recovered) == LoggingInitializationStatus::initialized);
    log_info("recovered-record");
    OL_CHECK(tree.read(recovered).find("recovered-record") != std::string::npos);
}

void check_concurrent_records_are_atomic() {
    const TestTree tree;
    const auto path = tree.path("concurrent.log");
    OL_CHECK(initialize_logging(path, LogLevel::debug) ==
             LoggingInitializationStatus::initialized);
    set_minimum_log_level(LogLevel::debug);

    constexpr int thread_count = 4;
    constexpr int records_per_thread = 50;
    std::vector<std::thread> workers;
    workers.reserve(thread_count);
    for (int thread = 0; thread < thread_count; ++thread) {
        workers.emplace_back([thread] {
            for (int record = 0; record < records_per_thread; ++record) {
                log_debug(
                    "concurrent worker=" + std::to_string(thread) +
                    " record=" + std::to_string(record));
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    shutdown_logging();
    std::istringstream input{tree.read(path)};
    std::string line;
    int record_count = 0;
    bool complete = true;
    while (std::getline(input, line)) {
        ++record_count;
        complete = complete && line.find(" [DEBUG] [thread=") != std::string::npos &&
                   line.find("] [log_test.cpp:") != std::string::npos &&
                   line.find("] concurrent worker=") != std::string::npos &&
                   line.find(" record=") != std::string::npos;
    }
    OL_CHECK(record_count == thread_count * records_per_thread);
    OL_CHECK(complete);
}

}  // namespace

int main() {
    check_metadata_and_flush();
    check_level_filter();
    check_initialization_failure_and_recovery();
    check_concurrent_records_are_atomic();
    return openlegend::test::failures == 0 ? 0 : 1;
}
