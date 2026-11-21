#pragma once

#include <cstddef>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppfig {

template <typename RegistryType>
class Configuration;

/// @brief A file entry kept under UnknownKeyPolicy::kPreserve. The path is a
/// view into the document's text.
struct UnknownKey {
    std::string_view path;
    std::size_t line = 0;
};

namespace detail {

    /// Where an unknown key's path sits in the document text. Positions rather
    /// than views or copies: they stay valid when the document is moved, and
    /// cost no allocation per key.
    struct KeyPosition {
        std::size_t offset;
        std::size_t length;
        std::size_t line;
    };

}  // namespace detail

/// @brief The unknown keys of a document, in text order. A view: it is valid
/// while the document it came from is neither moved nor destroyed.
class UnknownKeys {
public:
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = UnknownKey;
        using reference = UnknownKey;
        using pointer = void;

        Iterator() = default;

        [[nodiscard]] auto operator*() const -> UnknownKey { return MakeKey(text_, *position_); }

        auto operator++() -> Iterator&
        {
            ++position_;
            return *this;
        }

        auto operator++(int) -> Iterator
        {
            auto previous = *this;
            ++position_;
            return previous;
        }

        [[nodiscard]] auto operator==(const Iterator& other) const -> bool { return position_ == other.position_; }

    private:
        friend class UnknownKeys;

        Iterator(std::string_view text, const detail::KeyPosition* position)
            : text_(text)
            , position_(position)
        {
        }

        std::string_view text_;
        const detail::KeyPosition* position_ = nullptr;
    };

    [[nodiscard]] auto size() const noexcept -> std::size_t { return positions_.size(); }
    [[nodiscard]] auto empty() const noexcept -> bool { return positions_.empty(); }
    [[nodiscard]] auto operator[](std::size_t index) const -> UnknownKey { return MakeKey(text_, positions_[index]); }
    [[nodiscard]] auto begin() const -> Iterator { return { text_, positions_.data() }; }
    [[nodiscard]] auto end() const -> Iterator { return { text_, positions_.data() + positions_.size() }; }

private:
    friend class ConfDocument;

    UnknownKeys(std::string_view text, std::span<const detail::KeyPosition> positions)
        : text_(text)
        , positions_(positions)
    {
    }

    [[nodiscard]] static auto MakeKey(std::string_view text, const detail::KeyPosition& position) -> UnknownKey
    {
        return { text.substr(position.offset, position.length), position.line };
    }

    std::string_view text_;
    std::span<const detail::KeyPosition> positions_;
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
    [[nodiscard]] auto GetUnknownKeys() const noexcept -> UnknownKeys { return { text_, unknown_keys_ }; }

private:
    template <typename RegistryType>
    friend class Configuration;

    ConfDocument(std::string text, std::vector<detail::KeyPosition> unknown_keys, bool needs_save)
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
    std::vector<detail::KeyPosition> unknown_keys_;
    bool needs_save_ = false;
};

}  // namespace cppfig
