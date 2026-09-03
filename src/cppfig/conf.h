#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cppfig/status.h"
#include "cppfig/value.h"

namespace cppfig {

/// @brief Flat key-value `.conf` serializer — the default, zero-dependency serializer.
///
/// File format:
/// @code
/// # Comment lines start with #
///
/// server.host = localhost
/// server.port = 8080
/// logging.enabled = true
/// logging.level = info
/// @endcode
///
/// Keys are the full dot-separated setting paths. There are no sections
/// or grouping — each line is simply `path = value`.
///
/// Type inference during parsing:
/// - `true`/`false`/`yes`/`no`/`on`/`off` → bool
/// - All-digit strings (optional leading `-`) → int64
/// - Numeric with decimal point or exponent → double
/// - Quoted strings (`"..."`) → string (quotes stripped)
/// - Everything else → string
struct ConfSerializer {
    using data_type = Value;

    /// @brief Parses a `.conf` stream into a Value tree.
    static auto Parse(std::istream& is) -> StatusOr<Value>
    {
        Value result = Value::Object();
        std::string line;
        int line_number = 0;

        while (std::getline(is, line)) {
            ++line_number;

            auto trimmed = Trim(line);

            // Skip empty lines and comments
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            // key = value
            auto eq_pos = trimmed.find('=');
            if (eq_pos == std::string::npos) {
                return InvalidArgumentError("conf parse error: missing '=' on line " + std::to_string(line_number));
            }

            std::string key = Trim(trimmed.substr(0, eq_pos));
            std::string value_str = Trim(trimmed.substr(eq_pos + 1));

            // Reject keys SetAtPath would have to ignore, so a malformed file
            // is reported with its line number rather than silently dropped.
            if (Value::SplitPath(key).empty()) {
                return InvalidArgumentError("conf parse error: ill-formed key '" + key + "' on line " + std::to_string(line_number));
            }

            result.SetAtPath(key, InferValue(value_str));
        }

        return result;
    }

    /// @brief Parses a `.conf` string into a Value tree.
    static auto ParseString(std::string_view str) -> StatusOr<Value>
    {
        std::istringstream stream { std::string(str) };
        return Parse(stream);
    }

    /// @brief Converts a Value tree to flat `key = value` lines.
    static auto Stringify(const Value& data) -> std::string
    {
        std::vector<std::pair<std::string, const Value*>> leaves;
        CollectLeaves(data, "", leaves);

        std::ostringstream stream;
        for (const auto& [path, val] : leaves) {
            stream << path << " = " << ValueToString(*val) << '\n';
        }

        return stream.str();
    }

    /// @brief Rewrites an existing `.conf` document, preserving its layout.
    ///
    /// Regenerating the file from the Value tree sorts the keys and drops
    /// every comment the user wrote, which is user-hostile for a format whose
    /// whole point is being hand-editable. Merge instead walks the existing
    /// lines, updates the value of each key it recognises, leaves comments,
    /// blank lines and ordering alone, and appends the keys the file does not
    /// have yet.
    ///
    /// Lines whose value is unchanged are emitted verbatim, so a save that
    /// changes nothing leaves the file byte-identical.
    ///
    /// @param existing The current file contents.
    /// @param data The Value tree to write.
    static auto Merge(std::string_view existing, const Value& data) -> std::string
    {
        std::vector<std::pair<std::string, const Value*>> leaves;
        CollectLeaves(data, "", leaves);

        std::map<std::string, const Value*, std::less<>> by_path;
        for (const auto& [path, val] : leaves) {
            by_path.emplace(path, val);
        }

        std::set<std::string, std::less<>> updated;
        std::ostringstream out;
        std::istringstream in { std::string(existing) };
        std::string line;

        while (std::getline(in, line)) {
            auto trimmed = Trim(line);
            auto eq_pos = line.find('=');

            // Comments, blank lines and anything without a separator are the
            // user's; pass them through untouched.
            if (trimmed.empty() || trimmed[0] == '#' || eq_pos == std::string::npos) {
                out << line << '\n';
                continue;
            }

            const std::string key = Trim(line.substr(0, eq_pos));
            auto entry = by_path.find(key);
            if (entry == by_path.end()) {
                // A key the schema no longer has. Removing it is the user's
                // call, not ours.
                out << line << '\n';
                continue;
            }

            updated.insert(key);

            const std::string rendered = ValueToString(*entry->second);
            if (InferValue(Trim(line.substr(eq_pos + 1))) == *entry->second) {
                out << line << '\n';  // unchanged — keep the user's spacing
            }
            else {
                out << line.substr(0, eq_pos + 1) << ' ' << rendered << '\n';
            }
        }

        AppendMissing(out, leaves, updated);
        return out.str();
    }

private:
    /// @brief Trims leading/trailing whitespace.
    static auto Trim(std::string_view sv) -> std::string
    {
        auto start = sv.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) {
            return "";
        }
        auto end = sv.find_last_not_of(" \t\r\n");
        return std::string(sv.substr(start, end - start + 1));
    }

    /// @brief Infers the Value type from a raw string.
    static auto InferValue(const std::string& str) -> Value
    {
        // Quoted string
        if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
            return { str.substr(1, str.size() - 2) };
        }

        // Empty → empty string
        if (str.empty()) {
            return { std::string("") };
        }

        // Boolean
        if (str == "true" || str == "yes" || str == "on") {
            return { true };
        }
        if (str == "false" || str == "no" || str == "off") {
            return { false };
        }

        // Integer
        try {
            std::size_t pos = 0;
            auto int_val = std::stoll(str, &pos);
            if (pos == str.size()) {
                return { static_cast<std::int64_t>(int_val) };
            }
        }
        catch (...) {
        }

        // Double (only if it contains a decimal point or exponent)
        if (str.find('.') != std::string::npos || str.find('e') != std::string::npos || str.find('E') != std::string::npos) {
            try {
                std::size_t pos = 0;
                auto double_val = std::stod(str, &pos);
                if (pos == str.size()) {
                    return { double_val };
                }
            }
            catch (...) {
            }
        }

        // Unquoted string
        return { str };
    }

    /// @brief Converts a leaf Value to its string representation.
    static auto ValueToString(const Value& val) -> std::string
    {
        if (val.IsNull()) {
            return "";
        }
        if (val.IsBoolean()) {
            return val.Get<bool>() ? "true" : "false";
        }
        if (val.IsInteger()) {
            return std::to_string(val.Get<std::int64_t>());
        }
        if (val.IsDouble()) {
            std::ostringstream stream;
            stream << val.Get<double>();
            return stream.str();
        }
        if (val.IsString()) {
            const auto& str = val.Get<std::string>();
            // Quote if the string contains special characters or is empty
            bool needs_quoting = str.empty() || str.front() == ' ' || str.back() == ' ' || str.front() == '"' || str.find_first_of("=#\n\r") != std::string::npos;
            if (needs_quoting) {
                return "\"" + str + "\"";
            }
            return str;
        }
        return "";
    }

    /// @brief Appends the settings the existing document did not contain.
    static void AppendMissing(std::ostringstream& out,
                              const std::vector<std::pair<std::string, const Value*>>& leaves,
                              const std::set<std::string, std::less<>>& updated)
    {
        bool wrote_header = false;
        for (const auto& [path, val] : leaves) {
            if (updated.find(path) != updated.end()) {
                continue;
            }
            if (!wrote_header) {
                out << "\n# Added by cppfig\n";
                wrote_header = true;
            }
            out << path << " = " << ValueToString(*val) << '\n';
        }
    }

    /// @brief Recursively collects all leaf (path, Value*) pairs.
    static void CollectLeaves(const Value& node, const std::string& prefix, std::vector<std::pair<std::string, const Value*>>& leaves)
    {
        if (node.IsObject()) {
            for (const auto& [key, val] : node.Items()) {
                std::string path = prefix.empty() ? key : prefix + "." + key;
                CollectLeaves(val, path, leaves);
            }
        }
        else {
            leaves.emplace_back(prefix, &node);
        }
    }
};

}  // namespace cppfig
