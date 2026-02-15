#pragma once

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "cppfig/value.h"

namespace cppfig {

/// @brief Type of change detected in a diff.
enum class DiffType : std::uint8_t { Added,
                                     Removed,
                                     Modified };

/// @brief Represents a single difference between two configurations.
struct DiffEntry {
    DiffType type;
    std::string path;
    std::string old_value;
    std::string new_value;

    [[nodiscard]] auto TypeString() const -> std::string
    {
        switch (type) {
        case DiffType::Added:
            return "ADDED";
        case DiffType::Removed:
            return "REMOVED";
        case DiffType::Modified:
            return "MODIFIED";
        }
        return "UNKNOWN";  // LCOV_EXCL_LINE
    }
};

/// @brief Result of comparing two configurations.
class ConfigDiff {
public:
    std::vector<DiffEntry> entries;

    /// @brief Checks if there are any differences.
    [[nodiscard]] auto HasDifferences() const -> bool { return !entries.empty(); }

    /// @brief Returns the number of differences.
    [[nodiscard]] auto Size() const -> std::size_t { return entries.size(); }

    /// @brief Filters entries by type.
    [[nodiscard]] auto Filter(DiffType type) const -> std::vector<DiffEntry>
    {
        std::vector<DiffEntry> result;
        for (const auto& entry : entries) {
            if (entry.type == type) {
                result.push_back(entry);
            }
        }
        return result;
    }

    /// @brief Returns entries that were added.
    [[nodiscard]] auto Added() const -> std::vector<DiffEntry> { return Filter(DiffType::Added); }

    /// @brief Returns entries that were removed.
    [[nodiscard]] auto Removed() const -> std::vector<DiffEntry> { return Filter(DiffType::Removed); }

    /// @brief Returns entries that were modified.
    [[nodiscard]] auto Modified() const -> std::vector<DiffEntry> { return Filter(DiffType::Modified); }

    /// @brief Converts the diff to a human-readable string.
    [[nodiscard]] auto ToString() const -> std::string
    {
        if (!HasDifferences()) {
            return "No differences found.\n";
        }

        std::ostringstream ss;
        ss << "Configuration differences:\n";

        for (const auto& entry : entries) {
            ss << "  [" << entry.TypeString() << "] " << entry.path;
            switch (entry.type) {
            case DiffType::Added:
                ss << " = " << entry.new_value;
                break;
            case DiffType::Removed:
                ss << " (was: " << entry.old_value << ")";
                break;
            case DiffType::Modified:
                ss << ": " << entry.old_value << " -> " << entry.new_value;
                break;
            }
            ss << "\n";
        }

        return ss.str();
    }
};

namespace detail {

    /// @brief Recursively compares two Value objects and collects differences.
    inline void CompareValueRecursive(const Value& base, const Value& target, const std::string& prefix,
                                      ConfigDiff& diff)
    {
        // Check for keys in target that are not in base (added)
        if (target.IsObject()) {
            for (const auto& [key, value] : target.Items()) {
                std::string path = prefix.empty() ? key : prefix + "." + key;

                if (!base.IsObject() || !base.Contains(key)) {
                    diff.entries.push_back({ DiffType::Added, path, "", value.Dump() });
                }
                else if (base[key] != value) {
                    if (base[key].IsObject() && value.IsObject()) {
                        CompareValueRecursive(base[key], value, path, diff);
                    }
                    else {
                        diff.entries.push_back({ DiffType::Modified, path, base[key].Dump(), value.Dump() });
                    }
                }
            }
        }

        // Check for keys in base that are not in target (removed)
        if (base.IsObject()) {
            for (const auto& [key, value] : base.Items()) {
                std::string path = prefix.empty() ? key : prefix + "." + key;

                if (!target.IsObject() || !target.Contains(key)) {
                    diff.entries.push_back({ DiffType::Removed, path, value.Dump(), "" });
                }
            }
        }
    }

}  // namespace detail

/// @brief Compares two Value configurations and returns the differences.
///
/// @param base The base configuration (e.g., defaults).
/// @param target The target configuration (e.g., file values).
/// @return A ConfigDiff containing all differences.
inline auto DiffValues(const Value& base, const Value& target) -> ConfigDiff
{
    ConfigDiff diff;
    detail::CompareValueRecursive(base, target, "", diff);
    return diff;
}

/// @brief Compares file configuration against defaults.
///
/// Shows what settings in the file differ from defaults:
/// - ADDED: Settings in file not in defaults (possibly deprecated)
/// - REMOVED: Settings in defaults not in file (will use default)
/// - MODIFIED: Settings that differ from defaults
inline auto DiffFileFromDefaults(const Value& defaults, const Value& file_values) -> ConfigDiff
{
    return DiffValues(defaults, file_values);
}

/// @brief Compares defaults against file configuration.
///
/// Shows what default settings are missing or different in file:
/// - ADDED: New settings in defaults not in file (schema migration)
/// - REMOVED: Settings in file not in defaults (deprecated)
/// - MODIFIED: Settings that will be overridden by file
inline auto DiffDefaultsFromFile(const Value& defaults, const Value& file_values) -> ConfigDiff
{
    return DiffValues(file_values, defaults);
}

}  // namespace cppfig
