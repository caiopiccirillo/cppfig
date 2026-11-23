#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "cppfig/setting.h"
#include "cppfig/storage.h"

namespace cppfig {

namespace detail {

    // Duplicate setting types are rejected by the sorted path check below:
    // repeating a type necessarily repeats its static path as well.
    [[nodiscard]] constexpr auto IsPathPrefix(std::string_view prefix, std::string_view path) -> bool
    {
        return path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '.';
    }

    [[nodiscard]] constexpr auto IsPathWellFormed(std::string_view path) -> bool
    {
        constexpr unsigned char kDelete = 0x7f;
        if (path.empty() || path.front() == '.' || path.back() == '.') {
            return false;
        }
        for (std::size_t i = 0; i < path.size(); ++i) {
            const char c = path[i];
            const auto value = static_cast<unsigned char>(c);
            if (value <= ' ' || value == kDelete || c == '=' || c == '#' || c == '"' || c == '\\') {
                return false;
            }
            if (c == '.' && i > 0 && path[i - 1] == '.') {
                return false;
            }
        }
        return true;
    }

    /// Portable POSIX environment variable name: [A-Za-z_][A-Za-z0-9_]*.
    [[nodiscard]] constexpr auto IsEnvNameWellFormed(std::string_view name) -> bool
    {
        if (name.empty() || (name.front() >= '0' && name.front() <= '9')) {
            return false;
        }
        return std::ranges::all_of(name, [](const char c) {
            const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            const bool digit = c >= '0' && c <= '9';
            return alpha || digit || c == '_';
        });
    }

    /// A plain list of setting types, for matching a registry's settings
    /// without naming Registry itself (a registry may be a derived struct).
    template <typename... Settings>
    struct SettingList { };

    /// A path or environment override name and the index of its setting.
    struct KeyEntry {
        std::string_view key;
        std::size_t index;
    };

    /// Binary search of a table sorted by key. Returns not_found when absent.
    template <std::size_t N>
    [[nodiscard]] constexpr auto FindKey(const std::array<KeyEntry, N>& table, std::string_view key, std::size_t not_found)
        -> std::size_t
    {
        std::size_t first = 0;
        std::size_t last = N;
        while (first < last) {
            const std::size_t middle = first + ((last - first) / 2);
            const auto& entry = table[middle];
            if (entry.key < key) {
                first = middle + 1;
            }
            else if (key < entry.key) {
                last = middle;
            }
            else {
                return entry.index;
            }
        }
        return not_found;
    }

}  // namespace detail

/// @brief Compile-time list of unique setting types and their static metadata.
///
/// Use it through an alias or, for shorter names in diagnostics, clangd
/// hovers and symbols, as the base of a struct of your own:
/// @code
/// struct AppRegistry : cppfig::Registry<settings::Port, settings::Host> { };
/// @endcode
template <IsSetting... Settings>
class Registry {
public:
    static constexpr std::size_t size = sizeof...(Settings);

    using SettingList = detail::SettingList<Settings...>;

    template <typename S>
    static constexpr bool has_setting = (std::is_same_v<S, Settings> || ...);

    template <std::size_t I>
    using SettingAt = detail::TypeAt<I, Settings...>;

    template <template <typename> typename Holder>
    using held_values_type = detail::MakeHeldStorage<Holder, Settings...>;

    using values_type = detail::MakeHeldStorage<std::type_identity_t, Settings...>;

    static_assert((detail::CheckDefaultWhenPossible<Settings>() && ...),
                  "A constexpr-evaluable default value failed its setting validator");
    // Repeating a setting type also repeats its path, so the sorted path
    // validation enforces unique setting types without quadratic templates.

    template <typename S>
    static constexpr std::size_t index = [] {
        static_assert(has_setting<S>, "The requested setting is not present in this Registry");
        constexpr std::array<bool, size> matches { std::is_same_v<S, Settings>... };
        for (std::size_t i = 0; i < size; ++i) {
            if (matches[i]) {
                return i;
            }
        }
        return size;
    }();

    [[nodiscard]] static constexpr auto GetPaths() -> std::array<std::string_view, size>
    {
        return { Settings::path... };
    }

    /// Returns the declaration-order setting index, or size when not found.
    [[nodiscard]] static constexpr auto FindPath(std::string_view path) -> std::size_t
    {
        return detail::FindKey(sorted_paths, path, size);
    }

    /// Number of settings that declare an environment override.
    static constexpr std::size_t env_override_count = (std::size_t { HasEnvOverride<Settings> } + ... + 0);

    /// Returns the index of the setting whose environment override is name,
    /// or size when no setting declares it.
    [[nodiscard]] static constexpr auto FindEnvOverride(std::string_view name) -> std::size_t
    {
        return detail::FindKey(sorted_env_overrides, name, size);
    }

    template <typename Fn>
    static constexpr void ForEachSetting(Fn&& fn)
    {
        (fn.template operator()<Settings>(), ...);
    }

private:
    [[nodiscard]] static consteval auto AllPathsWellFormed() -> bool
    {
        constexpr std::array<std::string_view, size> paths { Settings::path... };
        return std::ranges::all_of(paths, detail::IsPathWellFormed);
    }

    [[nodiscard]] static consteval auto MakeSortedPaths() -> std::array<detail::KeyEntry, size>
    {
        std::array<detail::KeyEntry, size> entries {};
        std::size_t index = 0;
        ((entries[index] = detail::KeyEntry { .key = Settings::path, .index = index }, ++index), ...);
        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.key < right.key; });
        return entries;
    }

    static constexpr std::array<detail::KeyEntry, size> sorted_paths = MakeSortedPaths();

    [[nodiscard]] static consteval auto SortedPathsAreUnique() -> bool
    {
        for (std::size_t i = 1; i < sorted_paths.size(); ++i) {
            const auto& previous = sorted_paths[i - 1].key;
            const auto& current = sorted_paths[i].key;
            if (previous == current || detail::IsPathPrefix(previous, current)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static consteval auto MakeSortedEnvOverrides() -> std::array<detail::KeyEntry, env_override_count>
    {
        std::array<detail::KeyEntry, env_override_count> entries {};
        if constexpr (env_override_count > 0) {
            std::size_t count = 0;
            std::size_t index = 0;
            ([&] {
                if constexpr (HasEnvOverride<Settings>) {
                    entries[count++] = detail::KeyEntry { .key = GetEnvOverride<Settings>(), .index = index };
                }
                ++index;
            }(),
             ...);
            std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.key < right.key; });
        }
        return entries;
    }

    static constexpr std::array<detail::KeyEntry, env_override_count> sorted_env_overrides = MakeSortedEnvOverrides();

    [[nodiscard]] static consteval auto AllEnvNamesWellFormed() -> bool
    {
        return std::ranges::all_of(sorted_env_overrides, [](const detail::KeyEntry& entry) { return detail::IsEnvNameWellFormed(entry.key); });
    }

    [[nodiscard]] static consteval auto SortedEnvNamesAreUnique() -> bool
    {
        for (std::size_t i = 1; i < sorted_env_overrides.size(); ++i) {
            if (sorted_env_overrides[i - 1].key == sorted_env_overrides[i].key) {
                return false;
            }
        }
        return true;
    }

    static_assert(AllPathsWellFormed(), "Registry paths must be non-empty and contain only valid non-empty segments");
    static_assert(AllEnvNamesWellFormed(),
                  "Environment override names must be non-empty and match [A-Za-z_][A-Za-z0-9_]*");
    static_assert(SortedEnvNamesAreUnique(), "Environment override names must be unique within a Registry");
    static_assert(SortedPathsAreUnique(), "Registry paths must be unique and neither path may be a prefix of another");
};

}  // namespace cppfig
