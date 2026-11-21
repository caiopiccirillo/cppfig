#pragma once

#include <concepts>
#include <string_view>
#include <type_traits>

#include "cppfig/traits.h"
#include "cppfig/validator.h"

namespace cppfig {

template <typename S>
concept SettingDeclaration = requires {
    typename S::value_type;
    { S::path } -> std::convertible_to<std::string_view>;
    { S::default_value() } -> std::same_as<typename S::value_type>;
} && Configurable<typename S::value_type>;

template <typename S>
concept HasEnvDeclaration = requires {
    S::env_override;
};

template <typename S>
concept HasValidatorDeclaration = requires {
    S::validator();
};

template <typename S>
concept ValidEnvDeclaration = requires {
    { S::env_override } -> std::convertible_to<std::string_view>;
};

template <typename S>
concept ValidValidatorDeclaration = requires(const typename S::value_type& value) {
    { S::validator()(value) } -> std::same_as<ValidationResult>;
};

/// @brief A complete setting declaration accepted by Registry.
template <typename S>
concept IsSetting = SettingDeclaration<S> && (!HasEnvDeclaration<S> || ValidEnvDeclaration<S>)
    && (!HasValidatorDeclaration<S> || ValidValidatorDeclaration<S>);

template <typename S>
concept HasEnvOverride = IsSetting<S> && HasEnvDeclaration<S>;

template <typename S>
concept HasValidator = IsSetting<S> && HasValidatorDeclaration<S>;

template <IsSetting S>
[[nodiscard]] constexpr auto GetEnvOverride() -> std::string_view
{
    if constexpr (HasEnvOverride<S>) {
        return S::env_override;
    }
    return {};
}

template <IsSetting S>
[[nodiscard]] constexpr auto ValidateSetting(const typename S::value_type& value) -> ValidationResult
{
    if constexpr (HasValidator<S>) {
        return S::validator()(value);
    }
    return ValidationResult::Ok();
}

namespace detail {

    template <typename S>
    consteval auto DefaultIsValid() -> bool
    {
        auto value = S::default_value();
        return static_cast<bool>(ValidateSetting<S>(value));
    }

    template <typename S>
    concept ConstantDefaultCheck = requires {
        typename std::bool_constant<DefaultIsValid<S>()>;
    };

    /// Non-constexpr factories and validators are checked during Load instead.
    template <typename S>
    consteval auto CheckDefaultWhenPossible() -> bool
    {
        if constexpr (ConstantDefaultCheck<S>) {
            return DefaultIsValid<S>();
        }
        return true;
    }

}  // namespace detail

}  // namespace cppfig
