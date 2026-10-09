#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "openlegend/attributes.hpp"
#include "openlegend/persistence/save_file.hpp"
#include "save_file_operations.hpp"
#include "test_support.hpp"

namespace {

using namespace openlegend;
using namespace openlegend::persistence;
namespace io = openlegend::persistence::detail;

constexpr std::size_t kLimit = 64U * 1024U * 1024U;
const std::vector<std::uint8_t> kOld{0U, 1U, 255U, 13U, 10U, 2U};
const std::vector<std::uint8_t> kNew(131'077U, 73U);

struct Faults {
    std::string operation;
    int occurrence{1};
    std::map<std::string, int> calls;
    std::size_t transfer_limit{65536U};
    int initial_size_delta{};
    bool early_eof{};
    bool nonregular{};
    bool remove_before_publish_failure{};
    bool protect_temporary_before_publish_failure{};
    bool fail_restore{};
    bool fail_cleanup{};
    std::function<void()> before_commit_flush;

    void visit(const std::string& name) {
        const auto count = ++calls[name];
        if ((name == operation && count == occurrence) || (name == "restore" && fail_restore) ||
            (name == "remove" && fail_cleanup)) {
            throw std::system_error(std::make_error_code(std::errc::io_error), "injected " + name);
        }
    }
};

class ObservedFile final : public io::SaveFileHandle {
public:
    ObservedFile(std::unique_ptr<io::SaveFileHandle> file, Faults& faults, std::string kind)
        : file_(std::move(file)), faults_(faults), kind_(std::move(kind)) {}

    NODISCARD io::SaveFileInformation information() override {
        faults_.visit(kind_ + ".information");
        auto result = file_->information();
        if (kind_ == "source" && faults_.nonregular) {
            result.regular = false;
        }
        if (kind_ == "source" && faults_.calls[kind_ + ".information"] == 1) {
            result.size = static_cast<std::uint64_t>(static_cast<std::int64_t>(result.size) + faults_.initial_size_delta);
        }
        return result;
    }

    NODISCARD std::size_t read(const std::span<std::uint8_t> bytes) override {
        faults_.visit(kind_ + ".read");
        if (faults_.early_eof) {
            return 0U;
        }
        return file_->read(bytes.first(std::min(bytes.size(), faults_.transfer_limit)));
    }

    NODISCARD std::size_t write(const std::span<const std::uint8_t> bytes) override {
        faults_.visit(kind_ + ".write");
        return file_->write(bytes.first(std::min(bytes.size(), faults_.transfer_limit)));
    }

    void flush() override {
        faults_.visit(kind_ + ".flush");
        file_->flush();
    }

    void close() override {
        file_->close();
        faults_.visit(kind_ + ".close");
    }

private:
    std::unique_ptr<io::SaveFileHandle> file_;
    Faults& faults_;
    std::string kind_;
};

class ObservedOperations final : public io::SaveFileOperations {
public:
    Faults faults;

    NODISCARD std::unique_ptr<io::SaveFileHandle> open(
        const std::filesystem::path& path, const io::SaveFileAccess access) override {
        const std::string kind = access == io::SaveFileAccess::read ? "source"
            : path.stem().extension() == ".rollback" ? "backup" : "temporary";
        faults.visit(kind + ".open");
        return std::make_unique<ObservedFile>(native_.open(path, access), faults, kind);
    }

    void publish(const std::filesystem::path& temporary,
        const std::filesystem::path& target, const bool replacing) override {
        if (faults.remove_before_publish_failure &&
            faults.calls["publish"] + 1 == faults.occurrence) {
            native_.remove(target);
        }
        if (faults.protect_temporary_before_publish_failure) {
            std::filesystem::permissions(temporary, std::filesystem::perms::owner_read);
        }
        faults.visit("publish");
        native_.publish(temporary, target, replacing);
    }

    void restore(const std::filesystem::path& backup, const std::filesystem::path& target) override {
        faults.visit("restore");
        native_.restore(backup, target);
    }

    void remove(const std::filesystem::path& path) override {
        faults.visit("remove");
        native_.remove(path);
    }

    void sync_directory(const std::filesystem::path& path) override {
        if (faults.calls["sync_directory"] == 1 && faults.before_commit_flush) {
            faults.before_commit_flush();
        }
        faults.visit("sync_directory");
        native_.sync_directory(path);
    }

private:
    io::SaveFileOperations& native_{io::native_save_file_operations()};
};

NODISCARD std::filesystem::path suffixed(std::filesystem::path path, const char* suffix) {
    path += suffix;
    return path;
}

void put(const std::filesystem::path& path, const std::span<const std::uint8_t> bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!bytes.empty()) {
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    output.close();
    OL_CHECK(output.good());
}

void expect_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& expected) {
    const auto read = read_save_file(path, kLimit);
    OL_CHECK(read && read.bytes == expected);
}

void check_native_files(const std::filesystem::path& root) {
    const auto path = root / std::filesystem::path{u8"存檔.bin"};
    OL_CHECK(read_save_file(path, kLimit).status == SaveFileStatus::not_found);
    OL_CHECK(replace_save_file(path, kOld, kLimit));
    expect_bytes(path, kOld);
    OL_CHECK(replace_save_file(path, kNew, kLimit));
    expect_bytes(path, kNew);
    OL_CHECK(!std::filesystem::exists(suffixed(path, ".tmp")));
    OL_CHECK(!std::filesystem::exists(suffixed(path, ".rollback.tmp")));
    OL_CHECK(replace_save_file(path, {}, 0U).status == SaveFileStatus::too_large);
    std::filesystem::remove(path);
    OL_CHECK(replace_save_file(path, {}, 0U));
    OL_CHECK(read_save_file(path, 0U) && read_save_file(path, 0U).bytes->empty());
    OL_CHECK(replace_save_file(path, kOld, kOld.size() - 1U).status == SaveFileStatus::too_large);
    OL_CHECK(read_save_file(path, 0U).bytes->empty());
    put(path, kOld);
    OL_CHECK(read_save_file(path, kOld.size() - 1U).status == SaveFileStatus::too_large);
    OL_CHECK(read_save_file(root, kLimit).status == SaveFileStatus::not_regular);
    OL_CHECK(!replace_save_file(root, kOld, kLimit));
    OL_CHECK(std::filesystem::is_directory(root));
    OL_CHECK(!replace_save_file(root / "missing" / "001.toml", kOld, kLimit));

    for (const auto suffix : {".tmp", ".rollback.tmp"}) {
        const auto collision = suffixed(path, suffix);
        put(collision, kNew);
        OL_CHECK(!replace_save_file(path, kNew, kLimit));
        expect_bytes(path, kOld);
        expect_bytes(collision, kNew);
        std::filesystem::remove(collision);
        std::filesystem::create_directory(collision);
        OL_CHECK(!replace_save_file(path, kNew, kLimit));
        OL_CHECK(std::filesystem::is_directory(collision));
        std::filesystem::remove(collision);
    }

#if defined(_WIN32)
    std::filesystem::permissions(path, std::filesystem::perms::owner_read);
    const auto protected_target = replace_save_file(path, kNew, kLimit);
    OL_CHECK(!protected_target && protected_target.status != SaveFileStatus::rollback_failed);
    expect_bytes(path, kOld);
    OL_CHECK((std::filesystem::status(path).permissions() & std::filesystem::perms::owner_write) == std::filesystem::perms::none);
    std::filesystem::permissions(path, std::filesystem::perms::owner_all);
    OL_CHECK(!std::filesystem::exists(suffixed(path, ".tmp")));
    OL_CHECK(!std::filesystem::exists(suffixed(path, ".rollback.tmp")));
#endif
    const auto link = root / "link.toml";
    std::error_code link_error;
    std::filesystem::create_symlink(path, link, link_error);
    OL_CHECK(!link_error);
    if (!link_error) {
        OL_CHECK(read_save_file(link, kLimit).status == SaveFileStatus::not_regular);
        OL_CHECK(replace_save_file(link, kNew, kLimit).status == SaveFileStatus::not_regular);
        expect_bytes(path, kOld);
        OL_CHECK(std::filesystem::is_symlink(link));
        std::filesystem::remove(path);
        OL_CHECK(read_save_file(link, kLimit).status == SaveFileStatus::not_regular);
        OL_CHECK(!replace_save_file(link, kNew, kLimit));
        OL_CHECK(!std::filesystem::exists(path));
        std::filesystem::remove(link);
    }
    const auto large = root / "large.toml";
    put(large, {});
    std::filesystem::resize_file(large, kLimit + 1U);
    const auto oversized = read_save_file(large, kLimit);
    OL_CHECK(!oversized && !oversized.bytes && oversized.status == SaveFileStatus::too_large);
    std::filesystem::resize_file(large, kLimit);
    const auto exact = read_save_file(large, kLimit);
    OL_CHECK(exact && exact.bytes->size() == kLimit);
    if (exact) {
        OL_CHECK(std::ranges::all_of(*exact.bytes, [](const auto value) { return value == 0U; }));
    }
    std::filesystem::remove(large);
}

void check_read_failures(const std::filesystem::path& root) {
    const auto path = root / "read.toml";
    put(path, kOld);
    for (const auto operation : {"source.open", "source.information", "source.read", "source.close"}) {
        ObservedOperations operations;
        operations.faults.operation = operation;
        const auto result = io::read_save_file(operations, path, kLimit);
        OL_CHECK(!result && !result.bytes && !result.detail.empty());
        expect_bytes(path, kOld);
    }
    for (const auto delta : {-1, 1}) {
        ObservedOperations operations;
        operations.faults.initial_size_delta = delta;
        const auto result = io::read_save_file(operations, path, kLimit);
        OL_CHECK(!result && !result.bytes);
    }
    ObservedOperations nonregular;
    nonregular.faults.nonregular = true;
    OL_CHECK(io::read_save_file(nonregular, path, kLimit).status == SaveFileStatus::not_regular);
    OL_CHECK(io::replace_save_file(nonregular, path, kNew, kLimit).status == SaveFileStatus::not_regular);
    OL_CHECK(nonregular.faults.calls["source.read"] == 0);
    expect_bytes(path, kOld);
    ObservedOperations early;
    early.faults.early_eof = true;
    OL_CHECK(!io::read_save_file(early, path, kLimit));
    ObservedOperations partial;
    partial.faults.transfer_limit = 1U;
    OL_CHECK(io::read_save_file(partial, path, kLimit).bytes == kOld);
}

void check_write_failures(const std::filesystem::path& root) {
    const auto path = root / "write.toml";
    std::size_t failure_cases{};
    for (const bool existing : {false, true}) {
        for (const auto& [operation, occurrence] : std::to_array<std::pair<const char*, int>>({
                {"temporary.open", 1}, {"source.open", 1}, {"source.information", 1},
                {"source.read", 1}, {"source.close", 1}, {"backup.open", 1},
                {"backup.information", 1}, {"backup.write", 1}, {"backup.flush", 1}, {"backup.close", 1},
                {"temporary.information", 1}, {"temporary.write", 1}, {"temporary.write", 2},
                {"temporary.flush", 1}, {"temporary.close", 1}, {"sync_directory", 1},
                {"publish", 1}, {"sync_directory", 2}, {"remove", 1}})) {
            const std::string name{operation};
            if (!existing && ((name.starts_with("source.") && name != "source.open") ||
                    name == "backup.write")) {
                continue;
            }
            ++failure_cases;
            std::filesystem::remove(path);
            if (existing) {
                put(path, kOld);
            }
            ObservedOperations operations;
            operations.faults.operation = name;
            operations.faults.occurrence = occurrence;
            const auto result = io::replace_save_file(operations, path, kNew, kLimit);
            if (result) {
                std::cerr << "fault did not reject: " << name << ' ' << occurrence << '\n';
            }
            OL_CHECK(!result && result.status != SaveFileStatus::rollback_failed && !result.detail.empty());
            OL_CHECK(operations.faults.calls[name] >= occurrence);
            if (name == "publish" && existing) {
                OL_CHECK(operations.faults.calls["restore"] == 0);
            }
            if (existing) {
                expect_bytes(path, kOld);
            } else {
                OL_CHECK(!std::filesystem::exists(path));
            }
            OL_CHECK(!std::filesystem::exists(suffixed(path, ".tmp")));
            OL_CHECK(!std::filesystem::exists(suffixed(path, ".rollback.tmp")));
        }
    }
    put(path, kOld);
    ObservedOperations removed_target;
    removed_target.faults.operation = "publish";
    removed_target.faults.remove_before_publish_failure = true;
    OL_CHECK(!io::replace_save_file(removed_target, path, kNew, kLimit));
    expect_bytes(path, kOld);
    OL_CHECK(removed_target.faults.calls["restore"] == 1);

    ObservedOperations protected_temporary;
    protected_temporary.faults.operation = "publish";
    protected_temporary.faults.protect_temporary_before_publish_failure = true;
    OL_CHECK(!io::replace_save_file(protected_temporary, path, kNew, kLimit));
    expect_bytes(path, kOld);
    OL_CHECK(!std::filesystem::exists(suffixed(path, ".tmp")));
    OL_CHECK(!std::filesystem::exists(suffixed(path, ".rollback.tmp")));

    ObservedOperations partial;
    partial.faults.transfer_limit = 17U;
    OL_CHECK(io::replace_save_file(partial, path, kNew, kLimit));
    expect_bytes(path, kNew);
    ObservedOperations stalled;
    stalled.faults.transfer_limit = 0U;
    OL_CHECK(!io::replace_save_file(stalled, root / "stalled.toml", kNew, kLimit));
    OL_CHECK(!std::filesystem::exists(root / "stalled.toml.tmp"));

    put(path, kOld);
    ObservedOperations rollback;
    rollback.faults.operation = "sync_directory";
    rollback.faults.occurrence = 2;
    rollback.faults.fail_restore = true;
    const auto failed = io::replace_save_file(rollback, path, kNew, kLimit);
    OL_CHECK(!failed && failed.status == SaveFileStatus::rollback_failed);
    OL_CHECK(failed.recovery_path == suffixed(path, ".rollback.tmp"));
    expect_bytes(failed.recovery_path, kOld);
    io::native_save_file_operations().restore(failed.recovery_path, path);
    expect_bytes(path, kOld);

    ObservedOperations cleanup;
    cleanup.faults.operation = "temporary.flush";
    cleanup.faults.fail_cleanup = true;
    const auto unclean = io::replace_save_file(cleanup, path, kNew, kLimit);
    OL_CHECK(!unclean && !unclean.recovery_path.empty());
    OL_CHECK(unclean.detail.find("cleanup failed") != std::string::npos);
    expect_bytes(path, kOld);
    std::filesystem::remove(suffixed(path, ".tmp"));
    std::filesystem::remove(suffixed(path, ".rollback.tmp"));
    std::cout << "Recoverable write failure cases: " << failure_cases + 2U << '\n';
}

void check_overlapping_writes(const std::filesystem::path& root) {
    const auto path = root / "overlapping.toml";
    for (const bool existing : {false, true}) {
        for (const bool fail_commit : {false, true}) {
            std::filesystem::remove(path);
            if (existing) {
                put(path, kOld);
            }
            ObservedOperations operations;
            bool entered{};
            operations.faults.before_commit_flush = [&] {
                entered = true;
                const auto second = replace_save_file(path, kOld, kLimit);
                OL_CHECK(!second);
                expect_bytes(path, kNew);
            };
            if (fail_commit) {
                operations.faults.operation = "sync_directory";
                operations.faults.occurrence = 2;
            }
            const auto first = io::replace_save_file(operations, path, kNew, kLimit);
            OL_CHECK(entered && static_cast<bool>(first) == !fail_commit);
            if (!fail_commit) {
                expect_bytes(path, kNew);
            } else if (existing) {
                expect_bytes(path, kOld);
            } else {
                OL_CHECK(!std::filesystem::exists(path));
            }
            OL_CHECK(!std::filesystem::exists(suffixed(path, ".tmp")));
            OL_CHECK(!std::filesystem::exists(suffixed(path, ".rollback.tmp")));
        }
    }
}

void check_file_groups(const std::filesystem::path& root) {
    const std::array<std::vector<std::uint8_t>, 3> originals{{{1U, 2U}, {3U, 4U, 5U}, {6U}}};
    const std::array<std::vector<std::uint8_t>, 3> replacements{{{11U}, {12U, 13U}, {14U, 15U, 16U}}};
    const std::array<SaveFileReplacement, 3> files{{
        {root / "S9.GRP", replacements[0], kLimit},
        {root / "D9.GRP", replacements[1], kLimit},
        {root / "R9.GRP", replacements[2], kLimit},
    }};
    const auto prepare = [&](const unsigned int mask) {
        for (std::size_t index = 0U; index < files.size(); ++index) {
            std::filesystem::remove(files[index].path);
            std::filesystem::remove(suffixed(files[index].path, ".tmp"));
            std::filesystem::remove(suffixed(files[index].path, ".rollback.tmp"));
            if ((mask & (1U << index)) != 0U) {
                put(files[index].path, originals[index]);
            }
        }
    };
    const auto expect_originals = [&](const unsigned int mask) {
        for (std::size_t index = 0U; index < files.size(); ++index) {
            if ((mask & (1U << index)) != 0U) {
                expect_bytes(files[index].path, originals[index]);
            } else {
                OL_CHECK(!std::filesystem::exists(files[index].path));
            }
        }
    };
    const auto expect_clean = [&] {
        for (const auto& file : files) {
            OL_CHECK(!std::filesystem::exists(suffixed(file.path, ".tmp")));
            OL_CHECK(!std::filesystem::exists(suffixed(file.path, ".rollback.tmp")));
        }
    };
    std::size_t failure_cases{};
    for (unsigned int mask = 0U; mask < 8U; ++mask) {
        prepare(mask);
        ObservedOperations successful;
        const auto saved = io::replace_save_files(successful, files);
        OL_CHECK(saved && saved.committed && saved.recovery_paths.empty());
        for (std::size_t index = 0U; index < files.size(); ++index) {
            expect_bytes(files[index].path, replacements[index]);
        }
        expect_clean();
        for (const auto& [operation, count] : successful.faults.calls) {
            for (int occurrence = 1; occurrence <= count; ++occurrence) {
                prepare(mask);
                ObservedOperations failing;
                failing.faults.operation = operation;
                failing.faults.occurrence = occurrence;
                const auto result = io::replace_save_files(failing, files);
                OL_CHECK(!result && !result.detail.empty());
                OL_CHECK(failing.faults.calls[operation] >= occurrence);
                if (operation == "remove") {
                    OL_CHECK(result.committed && result.recovery_paths.size() == 1U);
                    for (std::size_t index = 0U; index < files.size(); ++index) {
                        expect_bytes(files[index].path, replacements[index]);
                    }
                    if (!result.recovery_paths.empty()) {
                        OL_CHECK(result.recovery_paths[0] == suffixed(files[occurrence - 1].path, ".rollback.tmp"));
                        std::filesystem::remove(result.recovery_paths[0]);
                    }
                } else {
                    OL_CHECK(!result.committed && result.status != SaveFileStatus::rollback_failed);
                    expect_originals(mask);
                    ++failure_cases;
                }
                expect_clean();
            }
        }
        for (const bool fail_commit : {false, true}) {
            prepare(mask);
            ObservedOperations overlapping;
            bool entered{};
            overlapping.faults.before_commit_flush = [&] {
                entered = true;
                const auto second = replace_save_files(files);
                OL_CHECK(!second && !second.committed);
            };
            if (fail_commit) {
                overlapping.faults.operation = "sync_directory";
                overlapping.faults.occurrence = 2;
            }
            const auto result = io::replace_save_files(overlapping, files);
            OL_CHECK(entered && static_cast<bool>(result) == !fail_commit);
            if (fail_commit) {
                expect_originals(mask);
            } else {
                for (std::size_t index = 0U; index < files.size(); ++index) {
                    expect_bytes(files[index].path, replacements[index]);
                }
            }
            expect_clean();
        }
    }
    for (int occurrence = 1; occurrence <= 3; ++occurrence) {
        prepare(7U);
        ObservedOperations partial_publish;
        partial_publish.faults.operation = "publish";
        partial_publish.faults.occurrence = occurrence;
        partial_publish.faults.remove_before_publish_failure = true;
        OL_CHECK(!io::replace_save_files(partial_publish, files));
        expect_originals(7U);
        expect_clean();
    }
    prepare(7U);
    ObservedOperations failed_restore;
    failed_restore.faults.operation = "sync_directory";
    failed_restore.faults.occurrence = 2;
    failed_restore.faults.fail_restore = true;
    const auto unrestored = io::replace_save_files(failed_restore, files);
    OL_CHECK(!unrestored && !unrestored.committed);
    OL_CHECK(unrestored.status == SaveFileStatus::rollback_failed && unrestored.recovery_paths.size() == 3U);
    for (std::size_t index = 0U; index < files.size(); ++index) {
        expect_bytes(suffixed(files[index].path, ".rollback.tmp"), originals[index]);
        expect_bytes(files[index].path, replacements[index]);
    }
    prepare(7U);
    ObservedOperations committed_cleanup;
    committed_cleanup.faults.fail_cleanup = true;
    const auto committed = io::replace_save_files(committed_cleanup, files);
    OL_CHECK(!committed && committed.committed && committed.recovery_paths.size() == 3U);
    for (std::size_t index = 0U; index < files.size(); ++index) {
        expect_bytes(files[index].path, replacements[index]);
        expect_bytes(suffixed(files[index].path, ".rollback.tmp"), originals[index]);
    }
    prepare(7U);
    ObservedOperations partial_transfer;
    partial_transfer.faults.transfer_limit = 1U;
    OL_CHECK(io::replace_save_files(partial_transfer, files));
    for (std::size_t index = 0U; index < files.size(); ++index) {
        expect_bytes(files[index].path, replacements[index]);
    }
    prepare(7U);
    ObservedOperations zero_transfer;
    zero_transfer.faults.transfer_limit = 0U;
    OL_CHECK(!io::replace_save_files(zero_transfer, files));
    expect_originals(7U);
    expect_clean();
    prepare(7U);
    ObservedOperations failed_cleanup;
    failed_cleanup.faults.operation = "temporary.flush";
    failed_cleanup.faults.fail_cleanup = true;
    const auto unclean = io::replace_save_files(failed_cleanup, files);
    OL_CHECK(!unclean && !unclean.committed && unclean.recovery_paths.size() == 6U);
    expect_originals(7U);
    prepare(0U);
    ObservedOperations failed_removal;
    failed_removal.faults.operation = "sync_directory";
    failed_removal.faults.occurrence = 2;
    failed_removal.faults.fail_cleanup = true;
    const auto unremoved = io::replace_save_files(failed_removal, files);
    OL_CHECK(!unremoved && !unremoved.committed && unremoved.status == SaveFileStatus::rollback_failed);
    OL_CHECK(unremoved.recovery_paths.size() == 6U);
    for (std::size_t index = 0U; index < files.size(); ++index) {
        expect_bytes(files[index].path, replacements[index]);
        expect_bytes(suffixed(files[index].path, ".rollback.tmp"), {});
    }
    prepare(7U);
    auto invalid = files;
    invalid[2].maximum_bytes = 0U;
    ObservedOperations preflight;
    OL_CHECK(!io::replace_save_files(preflight, invalid));
    OL_CHECK(preflight.faults.calls.empty());
    invalid = files;
    invalid[2].path += ".TMP";
    OL_CHECK(!io::replace_save_files(preflight, invalid));
    OL_CHECK(preflight.faults.calls.empty());
    invalid = files;
    invalid[2].path = files[0].path;
    OL_CHECK(!io::replace_save_files(preflight, invalid));
    expect_originals(7U);
    expect_clean();
    OL_CHECK(!replace_save_files({}));
    std::cout << "Recoverable group write failure cases: " << failure_cases + 3U << '\n';
}

}

int main() {
    try {
        const auto root = test::utf8_path(OPENLEGEND_TEST_OUTPUT_ROOT) / "save-files";
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        check_native_files(root);
        check_read_failures(root);
        check_write_failures(root);
        check_overlapping_writes(root);
        check_file_groups(root);
        std::filesystem::remove_all(root);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return test::failures == 0 ? 0 : 1;
}
