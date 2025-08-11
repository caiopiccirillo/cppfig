#pragma once

#include "CMakeConfig.h"

/**
 * @file cppfig.h
 * @brief Main header file for the C++fig extensible configuration library
 * 
 * This is the primary header for the C++fig library. Simply include this file
 * to get access to all C++fig functionality:
 * 
 * #include "cppfig.h"
 * 
 * Features:
 * - Compile-time type safety for configuration settings
 * - Fully extensible type system via ConfigurationTraits
 * - Zero runtime overhead with sub-nanosecond value access
 * - Pluggable serializers (JSON included, XML/YAML/custom possible)
 * - Rich validation and metadata support
 * - C++17 compatible
 * 
 * @see examples/simple_config_example.cpp for basic usage
 * @see examples/custom_types_example.cpp for custom type usage
 */

// Core configuration traits and type system
#include "ConfigurationTraits.h"
#include "Setting.h"
#include "TypedSetting.h"

// Configuration interfaces and implementations
#include "IConfiguration.h"
#include "IConfigurationSerializer.h"
#include "GenericConfiguration.h"

// Serialization support
#include "JsonSerializer.h"

// Helper utilities
#include "ConfigHelpers.h"
#include "CommonConfigurations.h"

namespace config {

/**
 * @brief Backwards compatibility aliases for the old API
 * 
 * These aliases allow existing code to continue working while providing
 * access to the new configurable architecture.
 */

/**
 * @brief Legacy configuration type for basic applications
 * 
 * This provides the same interface as the old Configuration<E> but uses
 * the new configurable architecture underneath.
 * 
 * @deprecated Use BasicJsonConfiguration<E> for new code
 */
template <typename E>
using Configuration = BasicJsonConfiguration<E>;

/**
 * @brief Legacy JSON configuration alias
 * 
 * @deprecated Use BasicJsonConfiguration<E> for new code
 */
template <typename E>
using JsonConfiguration = BasicJsonConfiguration<E>;

/**
 * @brief Convenient aliases for common use cases
 */

/**
 * @brief Most common configuration setup - JSON with basic types
 * 
 * Supports: int, float, double, std::string, bool
 * Serialization: JSON format
 * 
 * Usage:
 * @code
 * enum class MyConfig { Setting1, Setting2 };
 * using MyAppConfig = SimpleConfig<MyConfig>;
 * @endcode
 */
template <typename E>
using SimpleConfig = BasicJsonConfiguration<E>;

/**
 * @brief Extended configuration with additional numeric types
 * 
 * Supports: int, float, double, std::string, bool, long, uint32_t, int64_t
 * Serialization: JSON format
 * 
 * Usage:
 * @code
 * enum class MyConfig { LargeNumber, Timestamp };
 * using MyAppConfig = ExtendedConfig<MyConfig>;
 * @endcode
 */
template <typename E>
using ExtendedConfig = ExtendedJsonConfiguration<E>;

/**
 * @brief Helper for creating completely custom configurations
 * 
 * For when you need custom types and full control over the SettingVariant.
 * 
 * Usage:
 * @code
 * enum class MyConfig { CustomType };
 * using MyVariant = std::variant<Setting<MyConfig, MyCustomType>>;
 * using MyAppConfig = CustomConfig<MyConfig, MyVariant>;
 * @endcode
 */
template <typename E, typename SettingVariant>
using CustomConfig = CustomJsonConfiguration<E, SettingVariant>;

} // namespace config

/**
 * @brief Convenience macros for quick setup
 */

/**
 * @brief Create a simple configuration with standard types
 * 
 * This macro sets up everything needed for a basic configuration:
 * - Configuration type alias
 * - Default configuration map type
 * - Enum string conversion functions (must be implemented separately)
 * 
 * Usage:
 * @code
 * CPPFIG_DECLARE_SIMPLE_CONFIG(MyAppConfig, MyConfigEnum);
 * 
 * // Then implement the enum conversion functions:
 * std::string ToString(MyConfigEnum e) { ... }
 * MyConfigEnum FromString(const std::string& s) { ... }
 * @endcode
 */
#define CPPFIG_DECLARE_SIMPLE_CONFIG(ConfigName, EnumType) \
    DEFINE_BASIC_CONFIGURATION(ConfigName, EnumType)

/**
 * @brief Create an extended configuration with additional numeric types
 * 
 * Similar to CPPFIG_DECLARE_SIMPLE_CONFIG but with support for more numeric types.
 */
#define CPPFIG_DECLARE_EXTENDED_CONFIG(ConfigName, EnumType) \
    DEFINE_EXTENDED_CONFIGURATION(ConfigName, EnumType)

/**
 * @brief Create a custom configuration with your own SettingVariant
 */
#define CPPFIG_DECLARE_CUSTOM_CONFIG(ConfigName, EnumType, VariantType) \
    DEFINE_CUSTOM_CONFIGURATION(ConfigName, EnumType, VariantType)

/**
 * @brief Library version information
 */
#define CPPFIG_VERSION_MAJOR PROJECT_VERSION_MAJOR
#define CPPFIG_VERSION_MINOR PROJECT_VERSION_MINOR
#define CPPFIG_VERSION_PATCH PROJECT_VERSION_PATCH
#define CPPFIG_VERSION_STRING PROJECT_VERSION

/**
 * @brief Getting Started Guide
 * 
 * Basic Usage:
 * @code
 * #include "cppfig.h"
 * 
 * // 1. Define configuration enum
 * enum class MyConfig { DatabaseUrl, MaxConnections, EnableLogging };
 * 
 * // 2. Declare type mappings  
 * DECLARE_CONFIG_TYPE(MyConfig, MyConfig::DatabaseUrl, std::string);
 * DECLARE_CONFIG_TYPE(MyConfig, MyConfig::MaxConnections, int);
 * DECLARE_CONFIG_TYPE(MyConfig, MyConfig::EnableLogging, bool);
 * 
 * // 3. Set up configuration type
 * using Config = config::BasicJsonConfiguration<MyConfig>;
 * 
 * // 4. Create and use
 * const Config::DefaultConfigMap defaults = { ... };
 * Config config("app.json", defaults);
 * 
 * auto setting = config.GetSetting<MyConfig::MaxConnections>();
 * auto value = setting.Value(); // Type-safe: int automatically deduced
 * @endcode
 * 
 * For complete examples with custom types, see:
 * - examples/simple_config_example.cpp (basic usage)
 * - examples/custom_types_example.cpp (custom enums/structs)
 */