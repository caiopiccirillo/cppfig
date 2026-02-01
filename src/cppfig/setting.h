/// @file setting.h
/// @brief Setting definition utilities for configuration entries.
///
/// Provides the base infrastructure for defining type-safe configuration
/// settings using structs. Users define settings as structs with static members.

#ifndef CPPFIG_SETTING_H
#define CPPFIG_SETTING_H

#include <optional>
#include <string>
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
/// - kPath: std::string_view with the hierarchical key path
/// - ValueType: The type of the setting value
/// - DefaultValue(): Static function returning the default value
///
/// Optional:
/// - kEnvOverride: std::string_view with environment variable name
/// - GetValidator(): Static function returning a Validator<ValueType>
///
/// Example:
/// @code
/// struct ServerHost {
///     static constexpr std::string_view kPath = "server.host";
///     static constexpr std::string_view kEnvOverride = "SERVER_HOST";
///     using ValueType = std::string;
///     static auto DefaultValue() -> ValueType { return "localhost"; }
///     static auto GetValidator() -> Validator<ValueType> { return NotEmpty(); }
/// };
/// @endcode
template <typename S>
concept IsSetting = requires {
    { S::kPath } -> std::convertible_to<std::string_view>;
    typename S::ValueType;
    { S::DefaultValue() } -> std::same_as<typename S::ValueType>;
} && Configurable<typename S::ValueType>;

/// @brief Concept for settings with environment variable override.
template <typename S>
concept HasEnvOverride = IsSetting<S> && requires {
    { S::kEnvOverride } -> std::convertible_to<std::string_view>;
};

/// @brief Concept for settings with custom validator.
template <typename S>
concept HasValidator = IsSetting<S> && requires {
    { S::GetValidator() } -> std::same_as<Validator<typename S::ValueType>>;
};

/// @brief Helper to get environment override for a setting (empty if not defined).
template <IsSetting S>
constexpr auto GetEnvOverride() -> std::string_view {
    if constexpr (HasEnvOverride<S>) {
        return S::kEnvOverride;
    } else {
        return "";
    }
}

/// @brief Helper to get validator for a setting (always-valid if not defined).
template <IsSetting S>
auto GetSettingValidator() -> Validator<typename S::ValueType> {
    if constexpr (HasValidator<S>) {
        return S::GetValidator();
    } else {
        return AlwaysValid<typename S::ValueType>();
    }
}

}  // namespace cppfig

#endif  // CPPFIG_SETTING_H
