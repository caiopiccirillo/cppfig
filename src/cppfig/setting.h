#pragma once

#include <string_view>

#include "cppfig/traits.h"
#include "cppfig/validator.h"

namespace cppfig {

/// @brief Concept for valid setting types.
///
/// A type satisfies IsSetting if it provides all required static members
/// and functions for configuration management.
///
/// Required:
/// - path: std::string_view with the hierarchical key path
/// - value_type: The type of the setting value
/// - default_value(): Static function returning the default value
///
/// Optional:
/// - env_override: std::string_view with environment variable name
/// - validator(): Static function returning a Validator<value_type>
///
/// Example:
/// @code
/// struct ServerHost {
///     static constexpr std::string_view path = "server.host";
///     static constexpr std::string_view env_override = "SERVER_HOST";
///     using value_type = std::string;
///     static auto default_value() -> value_type { return "localhost"; }
///     static auto validator() -> Validator<value_type> { return NotEmpty(); }
/// };
/// @endcode
template <typename S>
concept IsSetting = requires {
    { S::path } -> std::convertible_to<std::string_view>;
    typename S::value_type;
    { S::default_value() } -> std::same_as<typename S::value_type>;
} && Configurable<typename S::value_type>;

/// @brief Concept for settings with environment variable override.
template <typename S>
concept HasEnvOverride = IsSetting<S> && requires {
    { S::env_override } -> std::convertible_to<std::string_view>;
};

/// @brief Concept for settings with custom validator.
template <typename S>
concept HasValidator = IsSetting<S> && requires {
    { S::validator() } -> std::same_as<Validator<typename S::value_type>>;
};

/// @brief Helper to get environment override for a setting (empty if not defined).
template <IsSetting S>
constexpr auto GetEnvOverride() -> std::string_view
{
    if constexpr (HasEnvOverride<S>) {
        return S::env_override;
    }
    else {
        return "";
    }
}

/// @brief Helper to get validator for a setting (always-valid if not defined).
template <IsSetting S>
auto GetSettingValidator() -> Validator<typename S::value_type>
{
    if constexpr (HasValidator<S>) {
        return S::validator();
    }
    else {
        return AlwaysValid<typename S::value_type>();
    }
}

}  // namespace cppfig
