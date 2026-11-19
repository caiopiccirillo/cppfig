#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cppfig/setting.h"
#include "cppfig/storage.h"

namespace cppfig {

namespace detail {

    // Duplicate setting types are rejected by the sorted path check below:
    // repeating a type necessarily repeats its static path as well.
    [[nodiscard]] consteval auto IsPathPrefix(std::string_view prefix, std::string_view path) -> bool
    {
        return path.size() > prefix.size() && path.starts_with(prefix) && path[prefix.size()] == '.';
    }

    [[nodiscard]] consteval auto IsPathWellFormed(std::string_view path) -> bool
    {
        if (path.empty() || path.front() == '.' || path.back() == '.') {
            return false;
        }
        for (std::size_t i = 0; i < path.size(); ++i) {
            const char c = path[i];
            const unsigned char value = static_cast<unsigned char>(c);
            if (value <= 0x20U || value == 0x7fU || c == '=' || c == '#' || c == '"' || c == '\\') {
                return false;
            }
            if (c == '.' && i > 0 && path[i - 1] == '.') {
                return false;
            }
        }
        return true;
    }

    /// Portable POSIX environment variable name: [A-Za-z_][A-Za-z0-9_]*.
    [[nodiscard]] consteval auto IsEnvNameWellFormed(std::string_view name) -> bool
    {
        if (name.empty() || (name.front() >= '0' && name.front() <= '9')) {
            return false;
        }
        for (const char c : name) {
            const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            const bool digit = c >= '0' && c <= '9';
            if (!alpha && !digit && c != '_') {
                return false;
            }
        }
        return true;
    }

    /// A plain list of setting types, for matching a registry's settings
    /// without naming Registry itself (a registry may be a derived struct).
    template <typename... Settings>
    struct SettingList { };

    struct PathEntry {
        std::string_view path;
        std::size_t index;
    };

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

    using setting_list = detail::SettingList<Settings...>;

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
        std::size_t first = 0;
        std::size_t last = size;
        while (first < last) {
            const std::size_t middle = first + (last - first) / 2;
            const auto& entry = sorted_paths[middle];
            if (entry.path < path) {
                first = middle + 1;
            }
            else if (path < entry.path) {
                last = middle;
            }
            else {
                return entry.index;
            }
        }
        return size;
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
        for (const auto path : paths) {
            if (!detail::IsPathWellFormed(path)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static consteval auto MakeSortedPaths() -> std::array<detail::PathEntry, size>
    {
        std::array<detail::PathEntry, size> entries {};
        std::size_t index = 0;
        ((entries[index++] = detail::PathEntry { Settings::path, index }), ...);
        std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.path < right.path; });
        return entries;
    }

    inline static constexpr std::array<detail::PathEntry, size> sorted_paths = MakeSortedPaths();

    [[nodiscard]] static consteval auto SortedPathsAreUnique() -> bool
    {
        for (std::size_t i = 1; i < sorted_paths.size(); ++i) {
            const auto& previous = sorted_paths[i - 1].path;
            const auto& current = sorted_paths[i].path;
            if (previous == current || detail::IsPathPrefix(previous, current)) {
                return false;
            }
        }
        return true;
    }

    static constexpr std::size_t env_override_count = (std::size_t { HasEnvOverride<Settings> } + ... + 0);

    [[nodiscard]] static consteval auto MakeSortedEnvNames() -> std::array<std::string_view, env_override_count>
    {
        std::array<std::string_view, env_override_count> names {};
        std::size_t count = 0;
        ([&] {
            if constexpr (HasEnvOverride<Settings>) {
                names[count++] = GetEnvOverride<Settings>();
            }
        }(), ...);
        std::sort(names.begin(), names.end());
        return names;
    }

    inline static constexpr std::array<std::string_view, env_override_count> sorted_env_names = MakeSortedEnvNames();

    [[nodiscard]] static consteval auto AllEnvNamesWellFormed() -> bool
    {
        for (const auto name : sorted_env_names) {
            if (!detail::IsEnvNameWellFormed(name)) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static consteval auto SortedEnvNamesAreUnique() -> bool
    {
        for (std::size_t i = 1; i < sorted_env_names.size(); ++i) {
            if (sorted_env_names[i - 1] == sorted_env_names[i]) {
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
