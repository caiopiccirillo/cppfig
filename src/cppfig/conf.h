#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace cppfig {

/// @brief One `path = value` entry. Columns are 1-based byte offsets in the line.
struct ConfEntry {
    std::string_view path;
    std::string_view text;
    std::size_t line;
    std::size_t column;
    std::size_t value_column;
};

/// @brief A syntax error on one line. The column is a 1-based byte offset.
struct ConfError {
    std::size_t line;
    std::size_t column;
    std::string_view message;
};

/// @brief Allocation-conscious parser for flat `path = value` documents.
///
/// The grammar is documented in docs/conf-format.md. In short: blank lines
/// and lines starting with `#` are ignored; `#` outside quotes starts a
/// comment; unquoted values are trimmed views into the source; quoted values
/// support \\, \", \n, \r and \t and use one temporary decoding string per entry.
class ConfParser {
public:
    /// Calls on_entry for each entry and on_error for each malformed line, in
    /// line order. A malformed line is skipped and parsing continues with the
    /// next one, so every error in a document is found in one pass.
    template <typename EntryFn, typename ErrorFn>
    static void ForEachEntry(std::string_view document, EntryFn&& on_entry, ErrorFn&& on_error)
    {
        std::size_t offset = 0;
        std::size_t line_number = 1;
        std::string decoded;

        while (offset < document.size()) {
            const std::size_t newline = document.find('\n', offset);
            const std::size_t end = newline == std::string_view::npos ? document.size() : newline;
            const auto line = document.substr(offset, end - offset);
            ParseLine(line, line_number, decoded, on_entry, on_error);

            if (newline == std::string_view::npos) {
                break;
            }
            offset = newline + 1;
            ++line_number;
        }
    }

private:
    static constexpr std::string_view kWhitespace = " \t\r";

    template <typename EntryFn, typename ErrorFn>
    static void ParseLine(std::string_view line, std::size_t line_number, std::string& decoded, EntryFn& on_entry, ErrorFn& on_error)
    {
        const std::size_t first = line.find_first_not_of(kWhitespace);
        if (first == std::string_view::npos || line[first] == '#') {
            return;
        }

        const std::size_t equals = line.find('=', first);
        if (equals == std::string_view::npos) {
            on_error(ConfError { line_number, first + 1, "missing '=' after the setting path" });
            return;
        }

        const auto path = TrimRight(line.substr(first, equals - first));
        if (!IsPathWellFormed(path)) {
            const std::string message = "ill-formed setting path '" + std::string(path) + "'";
            on_error(ConfError { line_number, first + 1, message });
            return;
        }

        std::size_t value_start = line.find_first_not_of(kWhitespace, equals + 1);
        if (value_start == std::string_view::npos) {
            value_start = line.size();
        }
        const auto value = DecodeValue(line, value_start, decoded);
        if (!value.ok()) {
            on_error(ConfError { line_number, value.column, value.message });
            return;
        }
        on_entry(ConfEntry { path, value.text, line_number, first + 1, value_start + 1 });
    }

    [[nodiscard]] static constexpr auto TrimRight(std::string_view text) -> std::string_view
    {
        const std::size_t last = text.find_last_not_of(kWhitespace);
        return last == std::string_view::npos ? std::string_view {} : text.substr(0, last + 1);
    }

    [[nodiscard]] static constexpr auto IsPathWellFormed(std::string_view path) -> bool
    {
        if (path.empty() || path.front() == '.' || path.back() == '.') {
            return false;
        }
        for (std::size_t i = 0; i < path.size(); ++i) {
            const char c = path[i];
            const auto value = static_cast<unsigned char>(c);
            if (value <= 0x20U || value == 0x7fU || c == '#' || c == '"' || c == '\\') {
                return false;
            }
            if (c == '.' && i > 0 && path[i - 1] == '.') {
                return false;
            }
        }
        return true;
    }

    /// A decoded value, or an error message with its 1-based column.
    struct DecodedValue {
        std::string_view text;
        std::size_t column = 0;
        std::string message;

        [[nodiscard]] auto ok() const -> bool { return message.empty(); }
    };

    [[nodiscard]] static auto Error(std::size_t index, std::string message) -> DecodedValue
    {
        return DecodedValue { {}, index + 1, std::move(message) };
    }

    /// Decodes the value starting at index start of line. A `#` outside quotes
    /// ends the value and starts a comment.
    [[nodiscard]] static auto DecodeValue(std::string_view line, std::size_t start, std::string& decoded) -> DecodedValue
    {
        if (start == line.size() || line[start] != '"') {
            const std::size_t comment = line.find('#', start);
            const auto raw = TrimRight(line.substr(start, comment == std::string_view::npos ? std::string_view::npos : comment - start));
            if (const std::size_t quote = raw.find('"'); quote != std::string_view::npos) {
                return Error(start + quote, "unexpected quote in an unquoted value");
            }
            return DecodedValue { raw, 0, {} };
        }

        decoded.clear();
        for (std::size_t i = start + 1; i < line.size(); ++i) {
            const char c = line[i];
            if (c == '"') {
                const std::size_t next = line.find_first_not_of(kWhitespace, i + 1);
                if (next != std::string_view::npos && line[next] != '#') {
                    return Error(next, "characters after the closing quote");
                }
                return DecodedValue { decoded, 0, {} };
            }
            if (c != '\\') {
                decoded.push_back(c);
                continue;
            }
            if (i + 1 == line.size()) {
                return Error(i, "unterminated escape sequence");
            }
            switch (line[++i]) {
            case '\\':
                decoded.push_back('\\');
                break;
            case '"':
                decoded.push_back('"');
                break;
            case 'n':
                decoded.push_back('\n');
                break;
            case 'r':
                decoded.push_back('\r');
                break;
            case 't':
                decoded.push_back('\t');
                break;
            default:
                return Error(i - 1, "unknown escape sequence");
            }
        }

        return Error(start, "unterminated quoted value");
    }
};

/// @brief Appends a serialized value to out, quoted and escaped only when the
/// flat grammar requires it.
inline void AppendConfValue(std::string& out, std::string_view value)
{
    const bool needs_quotes = value.empty() || value.front() == ' ' || value.back() == ' ' || value.front() == '\t'
        || value.back() == '\t' || value.find_first_of("=#\"\\\n\r\t") != std::string_view::npos;
    if (!needs_quotes) {
        out.append(value);
        return;
    }

    out.push_back('"');
    for (const char c : value) {
        switch (c) {
        case '\\':
            out.append("\\\\");
            break;
        case '"':
            out.append("\\\"");
            break;
        case '\n':
            out.append("\\n");
            break;
        case '\r':
            out.append("\\r");
            break;
        case '\t':
            out.append("\\t");
            break;
        default:
            out.push_back(c);
        }
    }
    out.push_back('"');
}

/// @brief Escapes a serialized value only when the flat grammar requires quotes.
[[nodiscard]] inline auto EscapeConfValue(std::string_view value) -> std::string
{
    std::string escaped;
    AppendConfValue(escaped, value);
    return escaped;
}

}  // namespace cppfig
