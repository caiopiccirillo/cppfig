#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include "cppfig/status.h"

namespace cppfig {

struct ConfEntry {
    std::string_view path;
    std::string_view text;
    std::size_t line;
    std::size_t column;
};

/// @brief Allocation-conscious parser for flat `path = value` documents.
///
/// Unquoted values are views into the retained source document. Quoted values
/// use one temporary decoding string per entry and support \\, \", \n, \r and \t.
class ConfParser {
public:
    template <typename Fn>
    [[nodiscard]] static auto ForEachEntry(std::string_view document, Fn&& fn) -> Status
    {
        std::size_t offset = 0;
        std::size_t line_number = 1;

        while (offset < document.size()) {
            const std::size_t newline = document.find('\n', offset);
            const std::size_t end = newline == std::string_view::npos ? document.size() : newline;
            const auto line = document.substr(offset, end - offset);
            const auto trimmed = Trim(line);

            if (!trimmed.empty() && trimmed.front() != '#') {
                const std::size_t equals = trimmed.find('=');
                if (equals == std::string_view::npos) {
                    return ParseError(line_number, 1, "missing '='");
                }

                const auto path = Trim(trimmed.substr(0, equals));
                if (!IsPathWellFormed(path)) {
                    return ParseError(line_number, 1, "ill-formed setting path");
                }

                const auto raw_value = Trim(trimmed.substr(equals + 1));
                std::string decoded;
                auto value = DecodeValue(raw_value, decoded, line_number);
                if (!value.ok()) {
                    return value.status();
                }

                const std::size_t column = static_cast<std::size_t>(path.data() - document.data()) - offset + 1;
                auto status = fn(ConfEntry { path, *value, line_number, column });
                if (!status.ok()) {
                    return status;
                }
            }

            if (newline == std::string_view::npos) {
                break;
            }
            offset = newline + 1;
            ++line_number;
        }

        return OkStatus();
    }

private:
    [[nodiscard]] static constexpr auto Trim(std::string_view text) -> std::string_view
    {
        const std::size_t first = text.find_first_not_of(" \t\r");
        if (first == std::string_view::npos) {
            return {};
        }
        const std::size_t last = text.find_last_not_of(" \t\r");
        return text.substr(first, last - first + 1);
    }

    [[nodiscard]] static constexpr auto IsPathWellFormed(std::string_view path) -> bool
    {
        if (path.empty() || path.front() == '.' || path.back() == '.') {
            return false;
        }
        for (std::size_t i = 0; i < path.size(); ++i) {
            const char c = path[i];
            const unsigned char value = static_cast<unsigned char>(c);
            if (value <= 0x20U || value == 0x7fU || c == '#' || c == '"' || c == '\\') {
                return false;
            }
            if (c == '.' && i > 0 && path[i - 1] == '.') {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static auto ParseError(std::size_t line, std::size_t column, std::string message) -> Status
    {
        return InvalidArgumentError("conf parse error on line " + std::to_string(line) + ", column " + std::to_string(column) +
                                    ": " + std::move(message));
    }

    [[nodiscard]] static auto DecodeValue(std::string_view raw, std::string& decoded, std::size_t line)
        -> StatusOr<std::string_view>
    {
        if (raw.empty() || raw.front() != '"') {
            if (raw.find('"') != std::string_view::npos) {
                return ParseError(line, 1, "unexpected quote in an unquoted value");
            }
            return raw;
        }

        decoded.clear();
        decoded.reserve(raw.size());
        for (std::size_t i = 1; i < raw.size(); ++i) {
            const char c = raw[i];
            if (c == '"') {
                if (i + 1 != raw.size()) {
                    return ParseError(line, i + 2, "characters after the closing quote");
                }
                return std::string_view(decoded);
            }
            if (c != '\\') {
                decoded.push_back(c);
                continue;
            }
            if (++i == raw.size()) {
                return ParseError(line, i + 1, "unterminated escape sequence");
            }
            switch (raw[i]) {
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
                return ParseError(line, i + 1, "unknown escape sequence");
            }
        }

        return ParseError(line, raw.size(), "unterminated quoted value");
    }
};

/// @brief Escapes a serialized value only when the flat grammar requires quotes.
[[nodiscard]] inline auto EscapeConfValue(std::string_view value) -> std::string
{
    const bool needs_quotes = value.empty() || value.front() == ' ' || value.back() == ' ' || value.front() == '\t' ||
                              value.back() == '\t' || value.find_first_of("=#\"\\\n\r\t") != std::string_view::npos;
    if (!needs_quotes) {
        return std::string(value);
    }

    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char c : value) {
        switch (c) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped.push_back(c);
        }
    }
    escaped.push_back('"');
    return escaped;
}

}  // namespace cppfig
