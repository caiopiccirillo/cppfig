#pragma once

#include <concepts>
#include <type_traits>

#include "cppfig/schema.h"

namespace cppfig {

/// @brief A type that exposes one frozen setting with reference semantics.
template <typename Provider, typename S>
concept SettingProvider = IsSetting<S> && requires(const Provider& provider) {
    { provider.template Get<S>() } -> std::same_as<const typename S::value_type&>;
};

namespace detail {

    template <typename Provider, typename RegistryType>
    struct IsConfigurationProvider : std::false_type { };

    template <typename Provider, IsSetting... Settings>
    struct IsConfigurationProvider<Provider, Registry<Settings...>> : std::bool_constant<(SettingProvider<Provider, Settings> && ...)> { };

}  // namespace detail

/// @brief Compile-time dependency-injection contract for all registry settings.
template <typename Provider, typename RegistryType>
concept ConfigurationProvider = detail::IsConfigurationProvider<Provider, RegistryType>::value;

}  // namespace cppfig
