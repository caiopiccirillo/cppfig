#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppfig {

template <typename RegistryType>
class Configuration;

/// @brief A file entry kept under UnknownKeyPolicy::kPreserve.
struct UnknownKey {
    std::string path;
    std::size_t line = 0;
};

/// @brief The flat .conf text a configuration was loaded from, with any
/// migrations appended.
///
/// Kept apart from Configuration so that the frozen configuration holds only
/// typed values: the text is cold data needed to save the file, and can be
/// discarded once it has been saved. The type is not a template, so its code is
/// compiled once for every registry. Writing it to disk is in cppfig/file.h.
class ConfDocument {
public:
    /// Returns the preserved text, including not-yet-saved migrations.
    [[nodiscard]] auto GetText() const noexcept -> std::string_view { return text_; }

    /// True when settings missing from the text were appended to it, so the
    /// text differs from what was read. A missing file reads as empty text, so
    /// it always needs saving.
    [[nodiscard]] auto NeedsSave() const noexcept -> bool { return needs_save_; }

    /// Entries that were kept because their path is not in the registry, in
    /// text order. Always empty under UnknownKeyPolicy::kReject.
    [[nodiscard]] auto GetUnknownKeys() const noexcept -> std::span<const UnknownKey> { return unknown_keys_; }

private:
    template <typename RegistryType>
    friend class Configuration;

    ConfDocument(std::string text, std::vector<UnknownKey> unknown_keys, bool needs_save)
        : text_(std::move(text))
        , unknown_keys_(std::move(unknown_keys))
        , needs_save_(needs_save)
    {
    }

    /// Returns text with migrated entries appended after it, leaving the
    /// existing text untouched. The result is allocated once at its exact size:
    /// the document keeps it for as long as the caller does, so growth slack
    /// would be memory held for nothing.
    [[nodiscard]] static auto AppendMigrations(std::string text, std::string_view migrations) -> std::string
    {
        if (migrations.empty()) {
            return text;
        }
        constexpr std::string_view header = "\n# Added by cppfig\n";
        const bool add_newline = !text.empty() && text.back() != '\n';
        const std::size_t header_size = text.empty() ? 0 : header.size();

        std::string combined;
        combined.reserve(text.size() + (add_newline ? 1 : 0) + header_size + migrations.size());
        combined.append(text);
        if (add_newline) {
            combined.push_back('\n');
        }
        combined.append(header.substr(0, header_size));
        combined.append(migrations);
        return combined;
    }

    std::string text_;
    std::vector<UnknownKey> unknown_keys_;
    bool needs_save_ = false;
};

}  // namespace cppfig
