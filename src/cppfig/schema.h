#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "cppfig/setting.h"

namespace cppfig {

namespace detail {

    /// @brief Helper to check if a type is in a parameter pack.
    template <typename T, typename... Types>
    struct IsOneOf : std::disjunction<std::is_same<T, Types>...> { };

    /// @brief Returns true if @p prefix names an ancestor of @p path.
    ///
    /// "server" is an ancestor of "server.port"; "serve" is not, and neither
    /// is "server" of "serverport".
    consteval auto IsPathPrefix(std::string_view prefix, std::string_view path) -> bool
    {
        return path.size() > prefix.size() && path.substr(0, prefix.size()) == prefix && path[prefix.size()] == '.';
    }

    /// @brief Returns true if @p path is a non-empty dot-separated path with
    ///        no empty segments.
    ///
    /// Mirrors Value::SplitPath, which rejects the same shapes at runtime.
    consteval auto IsPathWellFormed(std::string_view path) -> bool
    {
        if (path.empty() || path.front() == '.' || path.back() == '.') {
            return false;
        }
        for (std::size_t i = 1; i < path.size(); ++i) {
            if (path[i] == '.' && path[i - 1] == '.') {
                return false;
            }
        }
        return true;
    }

    /// @brief Helper to check that every path is well-formed at compile time.
    template <typename... Settings>
    consteval auto AllPathsWellFormed() -> bool
    {
        constexpr std::array<std::string_view, sizeof...(Settings)> paths = { Settings::path... };
        for (auto path : paths) {
            if (!IsPathWellFormed(path)) {
                return false;
            }
        }
        return true;
    }

    /// @brief Helper to check if all paths are unique at compile time.
    ///
    /// Two paths collide not only when they are equal but also when one is an
    /// ancestor of the other: writing "server" and then "server.port" turns
    /// the first into an object, and writing them the other way round turns
    /// the second into a leaf. Which value survives depends on write order,
    /// so both shapes are rejected.
    template <typename... Settings>
    consteval auto AllPathsUnique() -> bool
    {
        constexpr std::array<std::string_view, sizeof...(Settings)> paths = { Settings::path... };
        for (std::size_t i = 0; i < paths.size(); ++i) {
            for (std::size_t j = i + 1; j < paths.size(); ++j) {
                if (paths[i] == paths[j] || IsPathPrefix(paths[i], paths[j]) || IsPathPrefix(paths[j], paths[i])) {
                    return false;
                }
            }
        }
        return true;
    }

}  // namespace detail

/// @brief Configuration schema holding all setting types.
///
/// This class acts as a compile-time registry for all configuration settings.
/// It ensures path uniqueness at compile time and provides type-safe access
/// to setting information.
///
/// Usage:
/// @code
/// struct AppName {
///     static constexpr std::string_view path = "app.name";
///     using value_type = std::string;
///     static auto default_value() -> std::string { return "MyApp"; }
/// };
///
/// struct AppPort {
///     static constexpr std::string_view path = "app.port";
///     using value_type = int;
///     static auto default_value() -> int { return 8080; }
/// };
///
/// using MySchema = ConfigSchema<AppName, AppPort>;
/// @endcode
///
/// @tparam Settings The setting types to include in the schema.
template <IsSetting... Settings>
class ConfigSchema {
public:
    static constexpr std::size_t size = sizeof...(Settings);

    static_assert(detail::AllPathsWellFormed<Settings...>(),
                  "Every path in ConfigSchema must be non-empty and free of empty dot-separated segments");

    static_assert(detail::AllPathsUnique<Settings...>(),
                  "All paths in ConfigSchema must be unique, and none may be a prefix of another");

    /// @brief Checks if a setting type is in this schema.
    template <typename S>
    static constexpr bool has_setting = detail::IsOneOf<S, Settings...>::value;

    /// @brief Returns all paths as a compile-time array.
    [[nodiscard]] static constexpr auto GetPaths() -> std::array<std::string_view, size>
    {
        return { Settings::path... };
    }

    /// @brief Returns the number of settings in the schema.
    [[nodiscard]] static constexpr auto Size() -> std::size_t { return size; }

    /// @brief Iterates over all setting types with a callable.
    ///
    /// The callable receives a type wrapper that can be used to access
    /// the setting type information.
    template <typename Fn>
    static void ForEachSetting(Fn&& fn)
    {
        (fn.template operator()<Settings>(), ...);
    }
};

/// @brief Helper alias to get the value type for a setting.
template <IsSetting S>
using setting_value_type = typename S::value_type;

}  // namespace cppfig
