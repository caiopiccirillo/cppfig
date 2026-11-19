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

    template <typename Provider, typename List>
    struct ProvidesEverySetting : std::false_type { };

    template <typename Provider, typename... Settings>
    struct ProvidesEverySetting<Provider, SettingList<Settings...>> : std::bool_constant<(SettingProvider<Provider, Settings> && ...)> { };

}  // namespace detail

/// @brief Compile-time dependency-injection contract for all registry settings.
/// RegistryType may be a Registry or a struct derived from one.
template <typename Provider, typename RegistryType>
concept ConfigurationProvider = requires { typename RegistryType::setting_list; }
                                && detail::ProvidesEverySetting<Provider, typename RegistryType::setting_list>::value;

}  // namespace cppfig
