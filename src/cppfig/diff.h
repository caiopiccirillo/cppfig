/// @file diff.h
/// @brief Diff functionality for configuration files.
///
/// Provides utilities to compare configuration values between the file
/// and default values, detecting additions, removals, and modifications.

#ifndef CPPFIG_DIFF_H
#define CPPFIG_DIFF_H

#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace cppfig {

/// @brief Type of change detected in a diff.
enum class DiffType { kAdded, kRemoved, kModified };

/// @brief Represents a single difference between two configurations.
struct DiffEntry {
    DiffType type;
    std::string path;
    std::string old_value;
    std::string new_value;

    [[nodiscard]] auto TypeString() const -> std::string {
        switch (type) {
            case DiffType::kAdded:
                return "ADDED";
            case DiffType::kRemoved:
                return "REMOVED";
            case DiffType::kModified:
                return "MODIFIED";
        }
        return "UNKNOWN";
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
    [[nodiscard]] auto Filter(DiffType type) const -> std::vector<DiffEntry> {
        std::vector<DiffEntry> result;
        for (const auto& entry : entries) {
            if (entry.type == type) {
                result.push_back(entry);
            }
        }
        return result;
    }

    /// @brief Returns entries that were added.
    [[nodiscard]] auto Added() const -> std::vector<DiffEntry> { return Filter(DiffType::kAdded); }

    /// @brief Returns entries that were removed.
    [[nodiscard]] auto Removed() const -> std::vector<DiffEntry> { return Filter(DiffType::kRemoved); }

    /// @brief Returns entries that were modified.
    [[nodiscard]] auto Modified() const -> std::vector<DiffEntry> { return Filter(DiffType::kModified); }

    /// @brief Converts the diff to a human-readable string.
    [[nodiscard]] auto ToString() const -> std::string {
        if (!HasDifferences()) {
            return "No differences found.\n";
        }

        std::ostringstream ss;
        ss << "Configuration differences:\n";

        for (const auto& entry : entries) {
            ss << "  [" << entry.TypeString() << "] " << entry.path;
            switch (entry.type) {
                case DiffType::kAdded:
                    ss << " = " << entry.new_value;
                    break;
                case DiffType::kRemoved:
                    ss << " (was: " << entry.old_value << ")";
                    break;
                case DiffType::kModified:
                    ss << ": " << entry.old_value << " -> " << entry.new_value;
                    break;
            }
            ss << "\n";
        }

        return ss.str();
    }
};

namespace detail {

/// @brief Recursively compares two JSON objects and collects differences.
inline void CompareJsonRecursive(const nlohmann::json& base, const nlohmann::json& target, const std::string& prefix,
                                 ConfigDiff& diff) {
    // Check for keys in target that are not in base (added)
    if (target.is_object()) {
        for (auto& [key, value] : target.items()) {
            std::string path = prefix.empty() ? key : prefix + "." + key;

            if (!base.is_object() || !base.contains(key)) {
                diff.entries.push_back({DiffType::kAdded, path, "", value.dump()});
            } else if (base[key] != value) {
                if (base[key].is_object() && value.is_object()) {
                    CompareJsonRecursive(base[key], value, path, diff);
                } else {
                    diff.entries.push_back({DiffType::kModified, path, base[key].dump(), value.dump()});
                }
            }
        }
    }

    // Check for keys in base that are not in target (removed)
    if (base.is_object()) {
        for (auto& [key, value] : base.items()) {
            std::string path = prefix.empty() ? key : prefix + "." + key;

            if (!target.is_object() || !target.contains(key)) {
                diff.entries.push_back({DiffType::kRemoved, path, value.dump(), ""});
            }
        }
    }
}

}  // namespace detail

/// @brief Compares two JSON configurations and returns the differences.
///
/// @param base The base configuration (e.g., defaults).
/// @param target The target configuration (e.g., file values).
/// @return A ConfigDiff containing all differences.
inline auto DiffJson(const nlohmann::json& base, const nlohmann::json& target) -> ConfigDiff {
    ConfigDiff diff;
    detail::CompareJsonRecursive(base, target, "", diff);
    return diff;
}

/// @brief Compares file configuration against defaults.
///
/// Shows what settings in the file differ from defaults:
/// - ADDED: Settings in file not in defaults (possibly deprecated)
/// - REMOVED: Settings in defaults not in file (will use default)
/// - MODIFIED: Settings that differ from defaults
inline auto DiffFileFromDefaults(const nlohmann::json& defaults, const nlohmann::json& file_values) -> ConfigDiff {
    return DiffJson(defaults, file_values);
}

/// @brief Compares defaults against file configuration.
///
/// Shows what default settings are missing or different in file:
/// - ADDED: New settings in defaults not in file (schema migration)
/// - REMOVED: Settings in file not in defaults (deprecated)
/// - MODIFIED: Settings that will be overridden by file
inline auto DiffDefaultsFromFile(const nlohmann::json& defaults, const nlohmann::json& file_values) -> ConfigDiff {
    return DiffJson(file_values, defaults);
}

}  // namespace cppfig

#endif  // CPPFIG_DIFF_H
