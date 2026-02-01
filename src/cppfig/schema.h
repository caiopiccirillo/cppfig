/// @file schema.h
/// @brief Configuration schema registry.
///
/// Provides ConfigSchema which holds all setting types and enables
/// compile-time access with full type safety.

#ifndef CPPFIG_SCHEMA_H
#define CPPFIG_SCHEMA_H

#include <array>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "cppfig/setting.h"

namespace cppfig {

namespace detail {

/// @brief Helper to check if a type is in a parameter pack.
template <typename T, typename... Types>
struct IsOneOf : std::disjunction<std::is_same<T, Types>...> {};

/// @brief Helper to check if all paths are unique at compile time.
template <typename... Settings>
consteval auto AllPathsUnique() -> bool {
    constexpr std::array<std::string_view, sizeof...(Settings)> paths = {Settings::kPath...};
    for (std::size_t i = 0; i < paths.size(); ++i) {
        for (std::size_t j = i + 1; j < paths.size(); ++j) {
            if (paths[i] == paths[j]) {
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
///     static constexpr std::string_view kPath = "app.name";
///     using ValueType = std::string;
///     static auto DefaultValue() -> std::string { return "MyApp"; }
/// };
///
/// struct AppPort {
///     static constexpr std::string_view kPath = "app.port";
///     using ValueType = int;
///     static auto DefaultValue() -> int { return 8080; }
/// };
///
/// using MySchema = ConfigSchema<AppName, AppPort>;
/// @endcode
///
/// @tparam Settings The setting types to include in the schema.
template <IsSetting... Settings>
class ConfigSchema {
public:
    static constexpr std::size_t kSize = sizeof...(Settings);

    static_assert(detail::AllPathsUnique<Settings...>(), "All paths in ConfigSchema must be unique");

    /// @brief Checks if a setting type is in this schema.
    template <typename S>
    static constexpr bool HasSetting = detail::IsOneOf<S, Settings...>::value;

    /// @brief Returns all paths as a compile-time array.
    [[nodiscard]] static constexpr auto GetPaths() -> std::array<std::string_view, kSize> {
        return {Settings::kPath...};
    }

    /// @brief Returns the number of settings in the schema.
    [[nodiscard]] static constexpr auto Size() -> std::size_t { return kSize; }

    /// @brief Iterates over all setting types with a callable.
    ///
    /// The callable receives a type wrapper that can be used to access
    /// the setting type information.
    template <typename Fn>
    static void ForEachSetting(Fn&& fn) {
        (fn.template operator()<Settings>(), ...);
    }
};

/// @brief Helper alias to get the value type for a setting.
template <IsSetting S>
using SettingValueType = typename S::ValueType;

}  // namespace cppfig

#endif  // CPPFIG_SCHEMA_H
