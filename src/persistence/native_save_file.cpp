#include "save_file_operations.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <limits>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "openlegend/attributes.hpp"

namespace openlegend::persistence::detail {
namespace {

#if defined(_WIN32)

void fail_windows(const char* operation) {
    const auto error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
        throw std::system_error(std::make_error_code(std::errc::no_such_file_or_directory), operation);
    }
    throw std::system_error(static_cast<int>(error), std::system_category(), operation);
}

class NativeSaveFile final : public SaveFileHandle {
public:
    ~NativeSaveFile() override {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    void open(const std::filesystem::path& path, const SaveFileAccess access) {
        handle_ = CreateFileW(path.c_str(), access == SaveFileAccess::read ? GENERIC_READ : GENERIC_WRITE,
            access == SaveFileAccess::read ? FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE : 0U,
            nullptr, access == SaveFileAccess::read ? OPEN_EXISTING : CREATE_NEW,
            FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            fail_windows("open save file");
        }
    }

    NODISCARD SaveFileInformation information() override {
        const auto type = GetFileType(handle_);
        if (type != FILE_TYPE_DISK) {
            return {};
        }
        BY_HANDLE_FILE_INFORMATION information{};
        if (!GetFileInformationByHandle(handle_, &information)) {
            fail_windows("inspect save file");
        }
        return {(information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0U,
            (static_cast<std::uint64_t>(information.nFileSizeHigh) << 32U) | information.nFileSizeLow};
    }

    NODISCARD std::size_t read(const std::span<std::uint8_t> bytes) override {
        DWORD count{};
        const auto size = static_cast<DWORD>(std::min(bytes.size(), static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        if (!ReadFile(handle_, bytes.data(), size, &count, nullptr)) {
            fail_windows("read save file");
        }
        return count;
    }

    NODISCARD std::size_t write(const std::span<const std::uint8_t> bytes) override {
        DWORD count{};
        const auto size = static_cast<DWORD>(std::min(bytes.size(), static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        if (!WriteFile(handle_, bytes.data(), size, &count, nullptr)) {
            fail_windows("write save file");
        }
        return count;
    }

    void flush() override {
        if (!FlushFileBuffers(handle_)) {
            fail_windows("flush save file");
        }
    }

    void close() override {
        const auto handle = std::exchange(handle_, INVALID_HANDLE_VALUE);
        if (handle != INVALID_HANDLE_VALUE && !CloseHandle(handle)) {
            fail_windows("close save file");
        }
    }

private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
};

#else

void fail_posix(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}

class NativeSaveFile final : public SaveFileHandle {
public:
    ~NativeSaveFile() override {
        if (descriptor_ >= 0) {
            static_cast<void>(::close(descriptor_));
        }
    }

    void open(const std::filesystem::path& path, const SaveFileAccess access) {
        const int flags = access == SaveFileAccess::read
            ? O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC
            : O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC;
        do {
            descriptor_ = ::open(path.c_str(), flags, 0600);
        } while (descriptor_ < 0 && errno == EINTR);
        if (descriptor_ < 0) {
            fail_posix("open save file");
        }
    }

    NODISCARD SaveFileInformation information() override {
        struct stat information{};
        if (::fstat(descriptor_, &information) < 0) {
            fail_posix("inspect save file");
        }
        return {S_ISREG(information.st_mode) && information.st_size >= 0,
            information.st_size >= 0 ? static_cast<std::uint64_t>(information.st_size) : 0U};
    }

    NODISCARD std::size_t read(const std::span<std::uint8_t> bytes) override {
        ssize_t count{};
        do {
            count = ::read(descriptor_, bytes.data(), bytes.size());
        } while (count < 0 && errno == EINTR);
        if (count < 0) {
            fail_posix("read save file");
        }
        return static_cast<std::size_t>(count);
    }

    NODISCARD std::size_t write(const std::span<const std::uint8_t> bytes) override {
        ssize_t count{};
        do {
            count = ::write(descriptor_, bytes.data(), bytes.size());
        } while (count < 0 && errno == EINTR);
        if (count < 0) {
            fail_posix("write save file");
        }
        return static_cast<std::size_t>(count);
    }

    void flush() override {
        int result{};
        do {
            result = ::fsync(descriptor_);
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            fail_posix("flush save file");
        }
    }

    void close() override {
        const int descriptor = std::exchange(descriptor_, -1);
        if (descriptor >= 0 && ::close(descriptor) < 0) {
            fail_posix("close save file");
        }
    }

private:
    int descriptor_{-1};
};

#endif

class NativeSaveFileOperations final : public SaveFileOperations {
public:
    NODISCARD std::unique_ptr<SaveFileHandle> open(
        const std::filesystem::path& path, const SaveFileAccess access) override {
        auto file = std::make_unique<NativeSaveFile>();
        file->open(path, access);
        return file;
    }

    void publish(const std::filesystem::path& temporary,
        const std::filesystem::path& target, const bool replacing) override {
#if defined(_WIN32)
        if (replacing) {
            if (!ReplaceFileW(target.c_str(), temporary.c_str(), nullptr, 0U, nullptr, nullptr)) {
                fail_windows("replace save file");
            }
        } else if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
            fail_windows("publish save file");
        }
#else
        static_cast<void>(replacing);
        if (::rename(temporary.c_str(), target.c_str()) < 0) {
            fail_posix("replace save file");
        }
#endif
    }

    void restore(const std::filesystem::path& backup, const std::filesystem::path& target) override {
#if defined(_WIN32)
        if (!MoveFileExW(backup.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            fail_windows("restore previous save file");
        }
#else
        if (::rename(backup.c_str(), target.c_str()) < 0) {
            fail_posix("restore previous save file");
        }
#endif
    }

    void remove(const std::filesystem::path& path) override {
#if defined(_WIN32)
        if (!DeleteFileW(path.c_str())) {
            auto error = GetLastError();
            if (error == ERROR_ACCESS_DENIED) {
                const auto attributes = GetFileAttributesW(path.c_str());
                if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0U &&
                    (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0U) {
                    if (!SetFileAttributesW(path.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY)) {
                        fail_windows("clear temporary file read-only attribute");
                    }
                    if (DeleteFileW(path.c_str())) {
                        return;
                    }
                    error = GetLastError();
                }
            }
            if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
                SetLastError(error);
                fail_windows("remove save temporary file");
            }
        }
#else
        if (::unlink(path.c_str()) < 0 && errno != ENOENT) {
            fail_posix("remove save temporary file");
        }
#endif
    }

    void sync_directory(const std::filesystem::path& path) override {
#if defined(_WIN32)
        static_cast<void>(path);
#else
        int descriptor{};
        do {
            descriptor = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        } while (descriptor < 0 && errno == EINTR);
        if (descriptor < 0) {
            fail_posix("open save directory");
        }
        int result{};
        do {
            result = ::fsync(descriptor);
        } while (result < 0 && errno == EINTR);
        const int sync_error = errno;
        const int close_result = ::close(descriptor);
        if (result < 0) {
            throw std::system_error(sync_error, std::generic_category(), "flush save directory");
        }
        if (close_result < 0) {
            fail_posix("close save directory");
        }
#endif
    }
};

}

SaveFileOperations& native_save_file_operations() {
    static NativeSaveFileOperations operations;
    return operations;
}

}
