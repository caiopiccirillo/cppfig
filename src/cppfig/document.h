#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "cppfig/status.h"

#if __has_include(<unistd.h>)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#define CPPFIG_HAS_POSIX_IO 1
#else
#define CPPFIG_HAS_POSIX_IO 0
#endif

namespace cppfig {

template <typename RegistryType>
class Configuration;

/// @brief How Load treats file entries whose path is not in the registry.
enum class UnknownKeyPolicy : std::uint8_t {
    /// Fail initialization. Catches typos, but an older binary then rejects a
    /// file written by a newer one, and removing a setting breaks existing files.
    kReject,
    /// Leave the entry in the document untouched and report it through
    /// ConfDocument::GetUnknownKeys. Duplicate known keys are still rejected.
    kPreserve,
};

/// @brief Options for Configuration::Load.
struct LoadOptions {
    UnknownKeyPolicy unknown_keys = UnknownKeyPolicy::kReject;
};

/// @brief A file entry kept under UnknownKeyPolicy::kPreserve.
struct UnknownKey {
    std::string path;
    std::size_t line = 0;
};

namespace detail {

#if CPPFIG_HAS_POSIX_IO
    [[nodiscard]] inline auto ErrorText(int code) -> std::string { return std::generic_category().message(code); }

    /// Writes all of data to fd, retrying short writes and interruptions.
    /// Returns 0 or the failing errno value.
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
            data.remove_prefix(static_cast<std::size_t>(written));
        }
        return 0;
    }

    [[nodiscard]] inline auto ReplaceFileAtomically(const std::filesystem::path& destination, std::string_view text) -> Status
    {
        // Unique per process and call, so concurrent saves never share a temporary.
        static std::atomic<unsigned long> counter = 0;
        std::filesystem::path temporary;
        int fd = -1;
        for (int attempt = 0; attempt < 16 && fd < 0; ++attempt) {
            temporary = destination;
            temporary += ".cppfig-" + std::to_string(::getpid()) + "-" + std::to_string(counter.fetch_add(1)) + ".tmp";
            // 0666 lets the process umask decide the mode of a newly created file.
            fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0666);
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
            if (::fchmod(fd, existing.st_mode & 07777) != 0) {
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

/// @brief The flat .conf source a configuration was loaded from, with any
/// migrations appended.
///
/// Kept apart from Configuration so that the frozen configuration holds only
/// typed values: the source text is cold data needed to save the file, and can
/// be discarded once it has been saved. The type is not a template, so its
/// saving code is compiled once for every registry.
class ConfDocument {
public:
    [[nodiscard]] auto GetFilePath() const noexcept -> std::string_view { return file_path_; }

    /// Returns the preserved source text, including not-yet-saved migrations.
    [[nodiscard]] auto GetSourceText() const noexcept -> std::string_view { return text_; }

    /// True when the file did not exist or settings were appended to it, so
    /// the text differs from what is on disk. Reflects the state at load time.
    [[nodiscard]] auto NeedsSave() const noexcept -> bool { return needs_save_; }

    /// Entries that were kept because their path is not in the registry, in
    /// file order. Always empty under UnknownKeyPolicy::kReject.
    [[nodiscard]] auto GetUnknownKeys() const noexcept -> std::span<const UnknownKey> { return unknown_keys_; }

    /// Atomically persists the source text and any migrations.
    /// Environment overrides are never part of the document.
    ///
    /// The text is written to a temporary file unique to this call, flushed to
    /// disk and renamed over the destination, so readers and concurrent savers
    /// see either the old or the new file. An existing file keeps its
    /// permissions (and owner, where the process may set it); a symlink keeps
    /// pointing at its target, which is what gets replaced.
    [[nodiscard]] auto Save() const -> Status
    {
        namespace fs = std::filesystem;
        fs::path destination(file_path_);
        std::error_code error;
        if (fs::is_symlink(fs::symlink_status(destination, error))) {
            auto target = fs::canonical(destination, error);
            if (error) {
                return InternalError("Failed to resolve configuration symlink '" + file_path_ + "': " + error.message());
            }
            destination = std::move(target);
        }
        if (destination.has_parent_path()) {
            fs::create_directories(destination.parent_path(), error);
            if (error) {
                return InternalError("Failed to create configuration directory: " + error.message());
            }
        }
        return detail::ReplaceFileAtomically(destination, text_);
    }

private:
    template <typename RegistryType>
    friend class Configuration;

    ConfDocument(std::string file_path, std::string text, std::vector<UnknownKey> unknown_keys, bool needs_save)
        : file_path_(std::move(file_path))
        , text_(std::move(text))
        , unknown_keys_(std::move(unknown_keys))
        , needs_save_(needs_save)
    {
    }

    /// Reads file_path into text and reports whether it exists. A missing file
    /// leaves text empty and succeeds.
    [[nodiscard]] static auto ReadFile(const std::string& file_path, std::string& text, bool& exists) -> Status
    {
        if (file_path.empty()) {
            return InvalidArgumentError("Configuration file path must not be empty");
        }

        std::error_code error;
        exists = std::filesystem::exists(file_path, error);
        if (error) {
            return InternalError("Failed to inspect configuration file '" + file_path + "': " + error.message());
        }
        if (!exists) {
            return OkStatus();
        }

        std::ifstream input(file_path, std::ios::binary);
        if (!input.is_open()) {
            return InternalError("Failed to open configuration file '" + file_path + "'");
        }
        text.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        if (input.bad()) {
            return InternalError("Failed to read configuration file '" + file_path + "'");
        }
        return OkStatus();
    }

    /// Appends migrated entries after the existing text, leaving it untouched.
    static void AppendMigrations(std::string& text, std::string_view migrations)
    {
        if (migrations.empty()) {
            return;
        }
        if (!text.empty()) {
            if (text.back() != '\n') {
                text.push_back('\n');
            }
            text += "\n# Added by cppfig\n";
        }
        text += migrations;
    }

    std::string file_path_;
    std::string text_;
    std::vector<UnknownKey> unknown_keys_;
    bool needs_save_ = false;
};

}  // namespace cppfig
