#pragma once

#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "cppfig/status.h"

namespace cppfig {

template <typename RegistryType>
class Configuration;

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

    /// Atomically persists the source text and any migrations.
    /// Environment overrides are never part of the document.
    [[nodiscard]] auto Save() const -> Status
    {
        namespace fs = std::filesystem;
        const fs::path destination(file_path_);
        if (destination.has_parent_path()) {
            std::error_code error;
            fs::create_directories(destination.parent_path(), error);
            if (error) {
                return InternalError("Failed to create configuration directory: " + error.message());
            }
        }

        fs::path temporary = destination;
        temporary += ".cppfig.tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                return InternalError("Failed to open temporary configuration file '" + temporary.string() + "'");
            }
            output.write(text_.data(), static_cast<std::streamsize>(text_.size()));
            output.flush();
            if (!output) {
                output.close();
                std::error_code error;
                fs::remove(temporary, error);
                return InternalError("Failed to write temporary configuration file '" + temporary.string() + "'");
            }
        }

        std::error_code error;
        fs::rename(temporary, destination, error);
        if (error) {
            fs::remove(temporary, error);
            return InternalError("Failed to replace configuration file '" + file_path_ + "'");
        }
        return OkStatus();
    }

private:
    template <typename RegistryType>
    friend class Configuration;

    ConfDocument(std::string file_path, std::string text, bool needs_save)
        : file_path_(std::move(file_path))
        , text_(std::move(text))
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
    bool needs_save_ = false;
};

}  // namespace cppfig
