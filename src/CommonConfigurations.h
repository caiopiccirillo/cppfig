#pragma once

#include <variant>
#include "Setting.h"
#include "JsonSerializer.h"
#include "GenericConfiguration.h"

namespace config {

/**
 * @brief Common setting variant for typical applications
 * 
 * This variant includes the most commonly used types in configuration files:
 * - int: Integer values (counters, limits, etc.)
 * - float: Floating point values (rates, percentages, etc.)  
 * - double: High-precision floating point values
 * - std::string: Text values (paths, names, URLs, etc.)
 * - bool: Boolean flags (enable/disable options)
 * 
 * External developers can define their own variants for custom types.
 */
template <typename E>
using BasicSettingVariant = std::variant<
    Setting<E, int>,
    Setting<E, float>,
    Setting<E, double>,
    Setting<E, std::string>,
    Setting<E, bool>
>;

/**
 * @brief Extended setting variant with additional numeric types
 * 
 * This variant includes additional commonly requested types:
 * - All BasicSettingVariant types
 * - long: Large integer values
 * - uint32_t: Unsigned 32-bit integers (IDs, counts)
 * - int64_t: 64-bit integers (timestamps, large counters)
 */
template <typename E>
using ExtendedSettingVariant = std::variant<
    Setting<E, int>,
    Setting<E, float>,
    Setting<E, double>,
    Setting<E, std::string>,
    Setting<E, bool>,
    Setting<E, long>,
    Setting<E, uint32_t>,
    Setting<E, int64_t>
>;

/**
 * @brief Basic JSON configuration using common types
 * 
 * This is the most common configuration setup that most applications will use.
 * It provides JSON serialization with the 5 fundamental types.
 * 
 * Usage:
 * @code
 * enum class MyConfig { Setting1, Setting2 };
 * using MyAppConfig = BasicJsonConfiguration<MyConfig>;
 * @endcode
 */
template <typename E>
using BasicJsonConfiguration = GenericConfiguration<E, BasicSettingVariant<E>, JsonSerializer<E, BasicSettingVariant<E>>>;

/**
 * @brief Extended JSON configuration with additional numeric types
 * 
 * For applications that need more numeric type variety.
 * 
 * Usage:
 * @code
 * enum class MyConfig { LargeNumber, Timestamp };
 * using MyAppConfig = ExtendedJsonConfiguration<MyConfig>;
 * @endcode
 */
template <typename E>
using ExtendedJsonConfiguration = GenericConfiguration<E, ExtendedSettingVariant<E>, JsonSerializer<E, ExtendedSettingVariant<E>>>;

/**
 * @brief Helper to create a custom configuration with your own SettingVariant
 * 
 * For maximum flexibility when you need custom types.
 * 
 * Usage:
 * @code
 * enum class MyConfig { CustomType1, CustomType2 };
 * using MyVariant = std::variant<
 *     Setting<MyConfig, int>,
 *     Setting<MyConfig, MyCustomType>
 * >;
 * using MyAppConfig = CustomJsonConfiguration<MyConfig, MyVariant>;
 * @endcode
 */
template <typename E, typename SettingVariant>
using CustomJsonConfiguration = GenericConfiguration<E, SettingVariant, JsonSerializer<E, SettingVariant>>;

/**
 * @brief Type aliases for default configurations (backwards compatibility)
 * 
 * These provide smooth migration from the old API.
 */
template <typename E>
using Configuration = BasicJsonConfiguration<E>;

template <typename E>
using JsonConfiguration = BasicJsonConfiguration<E>;

/**
 * @brief Macro to easily define a complete configuration setup
 * 
 * This macro creates all the necessary boilerplate for a basic configuration.
 * 
 * Usage:
 * @code
 * DEFINE_BASIC_CONFIGURATION(MyAppConfig, MyConfigEnum);
 * @endcode
 * 
 * This creates:
 * - Type alias MyAppConfig for BasicJsonConfiguration<MyConfigEnum>
 * - Default map type alias MyAppConfigDefaults
 */
#define DEFINE_BASIC_CONFIGURATION(ConfigName, EnumType) \
    using ConfigName = ::config::BasicJsonConfiguration<EnumType>; \
    using ConfigName##Defaults = typename ConfigName::DefaultConfigMap

/**
 * @brief Macro to define an extended configuration setup
 * 
 * Similar to DEFINE_BASIC_CONFIGURATION but with extended numeric types.
 */
#define DEFINE_EXTENDED_CONFIGURATION(ConfigName, EnumType) \
    using ConfigName = ::config::ExtendedJsonConfiguration<EnumType>; \
    using ConfigName##Defaults = typename ConfigName::DefaultConfigMap

/**
 * @brief Macro to define a custom configuration setup
 * 
 * For when you need complete control over the SettingVariant.
 * 
 * Usage:
 * @code
 * using MyVariant = std::variant<Setting<MyEnum, int>, Setting<MyEnum, CustomType>>;
 * DEFINE_CUSTOM_CONFIGURATION(MyAppConfig, MyEnum, MyVariant);
 * @endcode
 */
#define DEFINE_CUSTOM_CONFIGURATION(ConfigName, EnumType, VariantType) \
    using ConfigName = ::config::CustomJsonConfiguration<EnumType, VariantType>; \
    using ConfigName##Defaults = typename ConfigName::DefaultConfigMap

} // namespace config