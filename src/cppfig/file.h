#pragma once

/// @file
/// Reading and atomically writing .conf files. Kept out of the core headers so
/// that code which only reads a configuration does not include the file
/// system, stream and POSIX headers this needs.

#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "cppfig/configuration.h"
#include "cppfig/document.h"
#include "cppfig/status.h"

#if __has_include(<unistd.h>)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#define CPPFIG_HAS_POSIX_IO 1
#else
#define CPPFIG_HAS_POSIX_IO 0
#endif

namespace cppfig {

namespace detail {

#if CPPFIG_HAS_POSIX_IO
    [[nodiscard]] inline auto ErrorText(int code) -> std::string { return std::generic_category().message(code); }

    /// Writes all of data to fd, retrying short writes and interruptions.
    /// Returns 0 or the failing errno value. A write that makes no progress is
    /// reported as EIO rather than retried forever.
    [[nodiscard]] inline auto WriteAll(int fd, std::string_view data) -> int
    {
        while (!data.empty()) {
            const ::ssize_t written = ::write(fd, data.data(), data.size());
            if (written < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return errno;
            }
            if (written == 0) {
                return EIO;
            }
            data.remove_prefix(static_cast<std::size_t>(written));
        }
        return 0;
    }

    [[nodiscard]] inline auto ReplaceFileAtomically(const std::filesystem::path& destination, std::string_view text) -> Status
    {
        // Unique per process and call, so concurrent saves never share a temporary.
        static std::atomic<unsigned long> counter = 0;
        constexpr int kMaxAttempts = 16;
        // Lets the process umask decide the mode of a newly created file.
        constexpr ::mode_t kNewFileMode = 0666;
        std::filesystem::path temporary;
        int fd = -1;
        for (int attempt = 0; attempt < kMaxAttempts && fd < 0; ++attempt) {
            temporary = destination;
            temporary += ".cppfig-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1)) + ".tmp";
            fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, kNewFileMode);
            if (fd < 0 && errno != EEXIST) {
                return InternalError("Failed to create temporary configuration file '" + temporary.string() + "': " + ErrorText(errno));
            }
        }
        if (fd < 0) {
            return InternalError("Failed to create a unique temporary file for '" + destination.string() + "'");
        }

        const auto fail = [&](std::string_view action, int code) -> Status {
            if (fd >= 0) {
                ::close(fd);
            }
            ::unlink(temporary.c_str());
            return InternalError("Failed to " + std::string(action) + " '" + destination.string() + "': " + ErrorText(code));
        };

        struct ::stat existing {};
        if (::stat(destination.c_str(), &existing) == 0) {
            constexpr ::mode_t kPermissionBits = S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
            if (::fchmod(fd, existing.st_mode & kPermissionBits) != 0) {
                return fail("copy the permissions of", errno);
            }
            // Changing the owner needs privileges; without them the new file
            // belongs to the saving user, as any rewrite by that user would.
            if (::fchown(fd, existing.st_uid, existing.st_gid) != 0) {
            }
        }

        if (const int code = WriteAll(fd, text); code != 0) {
            return fail("write the temporary file for", code);
        }
        if (::fsync(fd) != 0) {
            return fail("flush the temporary file for", errno);
        }
        const int closed = ::close(fd);
        fd = -1;
        if (closed != 0) {
            return fail("close the temporary file for", errno);
        }
        if (::rename(temporary.c_str(), destination.c_str()) != 0) {
            return fail("replace", errno);
        }

        // Make the rename itself durable. The new file is already in place, so
        // a failure here is not reported as a failed save.
        const auto directory = destination.has_parent_path() ? destination.parent_path() : std::filesystem::path(".");
        const int directory_fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory_fd >= 0) {
            ::fsync(directory_fd);
            ::close(directory_fd);
        }
        return OkStatus();
    }
#else
    [[nodiscard]] inline auto ReplaceFileAtomically(const std::filesystem::path& destination, std::string_view text) -> Status
    {
        static std::atomic<unsigned long> counter = 0;
        auto temporary = destination;
        temporary += ".cppfig-" + std::to_string(counter.fetch_add(1)) + ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            output.flush();
            if (!output) {
                output.close();
                std::error_code error;
                std::filesystem::remove(temporary, error);
                return InternalError("Failed to write temporary configuration file '" + temporary.string() + "'");
            }
        }
        std::error_code error;
        std::filesystem::rename(temporary, destination, error);
        if (error) {
            std::filesystem::remove(temporary, error);
            return InternalError("Failed to replace configuration file '" + destination.string() + "': " + error.message());
        }
        return OkStatus();
    }
#endif

}  // namespace detail

/// @brief Reads a whole file. A missing file reads as empty text.
///
/// The text is read into a single allocation of the file's size.
[[nodiscard]] inline auto ReadConfFile(const std::filesystem::path& path) -> StatusOr<std::string>
{
    if (path.empty()) {
        return InvalidArgumentError("Configuration file path must not be empty");
    }

    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        return std::string {};
    }
    if (error) {
        return InternalError("Failed to inspect configuration file '" + path.string() + "': " + error.message());
    }

    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return InternalError("Failed to open configuration file '" + path.string() + "'");
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(input.gcount()));
    // A file that grew since its size was taken is read to its end.
    if (!input.eof()) {
        text.append(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    if (input.bad()) {
        return InternalError("Failed to read configuration file '" + path.string() + "'");
    }
    return text;
}

/// @brief Loads a flat .conf file and captures environment overrides once.
///
/// A missing file yields defaults and a document holding them; save the
/// document to create the file. Errors name the file as given.
template <typename RegistryType>
[[nodiscard]] auto LoadFile(const std::filesystem::path& path, LoadOptions options = {}) -> StatusOr<LoadResult<RegistryType>>
{
    auto text = ReadConfFile(path);
    if (!text) {
        return std::move(text).error();
    }
    const std::string source = path.string();
    return Configuration<RegistryType>::FromText(std::move(*text), source, options);
}

/// @brief Atomically writes a document's text to path.
///
/// The text is written to a temporary file unique to this call, flushed to
/// disk and renamed over the destination, so readers and concurrent savers see
/// either the old or the new file. The directory is then flushed so that the
/// rename survives a crash; that step is best effort, and its failure is not
/// reported because the file has already been replaced. Without POSIX I/O the
/// replacement is atomic but nothing is flushed.
///
/// An existing file keeps its permission bits (and owner, where the process may
/// set it); other metadata, such as ACLs, extended attributes and hard links to
/// the old file, is not carried over. A symlink keeps pointing at its target,
/// which is what gets replaced. Missing parent directories are created.
[[nodiscard]] inline auto SaveFile(const std::filesystem::path& path, const ConfDocument& document) -> Status
{
    namespace fs = std::filesystem;
    if (path.empty()) {
        return InvalidArgumentError("Configuration file path must not be empty");
    }
    fs::path destination(path);
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(destination, error))) {
        auto target = fs::canonical(destination, error);
        if (error) {
            return InternalError("Failed to resolve configuration symlink '" + path.string() + "': " + error.message());
        }
        destination = std::move(target);
    }
    if (destination.has_parent_path()) {
        fs::create_directories(destination.parent_path(), error);
        if (error) {
            return InternalError("Failed to create configuration directory: " + error.message());
        }
    }
    return detail::ReplaceFileAtomically(destination, document.GetText());
}

}  // namespace cppfig
