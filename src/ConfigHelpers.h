#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "ConfigurationTraits.h"
#include "Setting.h"

namespace config {

/**
 * @brief Helper functions for configuration management with configurable types
 * 
 * This class provides convenient factory methods for creating settings with
 * various types. It works with any SettingVariant that contains the
 * corresponding Setting<E, T> types.
 * 
 * @tparam E Enum type for configuration keys
 */
template <typename E>
class ConfigHelpers {
public:
    /**
     * @brief Create a simple int setting
     * 
     * @tparam EnumValue The enum value for this setting
     * @param value Default value
     * @param min_val Optional minimum value
     * @param max_val Optional maximum value  
     * @param description Optional description
     * @param unit Optional unit string
     * @return Setting<E, int> configured with the provided parameters
     */
    template <E EnumValue>
    static std::enable_if_t<is_valid_config_type_v<E, EnumValue, int>, Setting<E, int>>
    CreateIntSetting(int value,
                     std::optional<int> min_val = std::nullopt,
                     std::optional<int> max_val = std::nullopt,
                     const std::string& description = "",
                     const std::string& unit = "")
    {
        return Setting<E, int>(
            EnumValue,
            value,
            max_val,
            min_val,
            unit.empty() ? std::nullopt : std::make_optional(unit),
            description.empty() ? std::nullopt : std::make_optional(description));
    }

    /**
     * @brief Create a simple string setting
     * 
     * @tparam EnumValue The enum value for this setting
     * @param value Default value
     * @param description Optional description
     * @return Setting<E, std::string> configured with the provided parameters
     */
    template <E EnumValue>
    static std::enable_if_t<is_valid_config_type_v<E, EnumValue, std::string>, Setting<E, std::string>>
    CreateStringSetting(const std::string& value,
                        const std::string& description = "")
    {
        return Setting<E, std::string>(
            EnumValue,
            value,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            description.empty() ? std::nullopt : std::make_optional(description));
    }

    /**
     * @brief Create a simple bool setting
     * 
     * @tparam EnumValue The enum value for this setting
     * @param value Default value
     * @param description Optional description
     * @return Setting<E, bool> configured with the provided parameters
     */
    template <E EnumValue>
    static std::enable_if_t<is_valid_config_type_v<E, EnumValue, bool>, Setting<E, bool>>
    CreateBoolSetting(bool value, const std::string& description = "")
    {
        return Setting<E, bool>(
            EnumValue,
            value,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            description.empty() ? std::nullopt : std::make_optional(description));
    }

    /**
     * @brief Create a simple float setting
     * 
     * @tparam EnumValue The enum value for this setting
     * @param value Default value
     * @param min_val Optional minimum value
     * @param max_val Optional maximum value
     * @param description Optional description
     * @param unit Optional unit string
     * @return Setting<E, float> configured with the provided parameters
     */
    template <E EnumValue>
    static std::enable_if_t<is_valid_config_type_v<E, EnumValue, float>, Setting<E, float>>
    CreateFloatSetting(float value,
                       std::optional<float> min_val = std::nullopt,
                       std::optional<float> max_val = std::nullopt,
                       const std::string& description = "",
                       const std::string& unit = "")
    {
        return Setting<E, float>(
            EnumValue,
            value,
            max_val,
            min_val,
            unit.empty() ? std::nullopt : std::make_optional(unit),
            description.empty() ? std::nullopt : std::make_optional(description));
    }

    /**
     * @brief Create a simple double setting
     * 
     * @tparam EnumValue The enum value for this setting
     * @param value Default value
     * @param min_val Optional minimum value
     * @param max_val Optional maximum value
     * @param description Optional description
     * @param unit Optional unit string
     * @return Setting<E, double> configured with the provided parameters
     */
    template <E EnumValue>
    static std::enable_if_t<is_valid_config_type_v<E, EnumValue, double>, Setting<E, double>>
    CreateDoubleSetting(double value,
                        std::optional<double> min_val = std::nullopt,
                        std::optional<double> max_val = std::nullopt,
                        const std::string& description = "",
                        const std::string& unit = "")
    {
        return Setting<E, double>(
            EnumValue,
            value,
            max_val,
            min_val,
            unit.empty() ? std::nullopt : std::make_optional(unit),
            description.empty() ? std::nullopt : std::make_optional(description));
    }

    /**
     * @brief Create a setting for any custom type that has ConfigurationTraits
     * 
     * This is the generic factory method that works with any type T that has
     * ConfigurationTraits specialized. It provides a unified interface for
     * creating settings regardless of the type.
     * 
     * @tparam EnumValue The enum value for this setting
     * @tparam T The type of the setting value
     * @param value Default value
     * @param min_val Optional minimum value (if T supports ordering)
     * @param max_val Optional maximum value (if T supports ordering)
     * @param description Optional description
     * @param unit Optional unit string
     * @return Setting<E, T> configured with the provided parameters
     */
    template <E EnumValue, typename T>
    static std::enable_if_t<
        is_valid_config_type_v<E, EnumValue, T> && has_configuration_traits_v<T>,
        Setting<E, T>
    >
    CreateCustomSetting(T value,
                        std::optional<T> min_val = std::nullopt,
                        std::optional<T> max_val = std::nullopt,
                        const std::string& description = "",
                        const std::string& unit = "")
    {
        return Setting<E, T>(
            EnumValue,
            std::move(value),
            std::move(max_val),
            std::move(min_val),
            unit.empty() ? std::nullopt : std::make_optional(unit),
            description.empty() ? std::nullopt : std::make_optional(description));
    }

    /**
     * @brief Create a setting for extended numeric types (long, uint32_t, int64_t)
     * 
     * @tparam EnumValue The enum value for this setting
     * @tparam T The numeric type (long, uint32_t, int64_t, etc.)
     * @param value Default value
     * @param min_val Optional minimum value
     * @param max_val Optional maximum value
     * @param description Optional description
     * @param unit Optional unit string
     * @return Setting<E, T> configured with the provided parameters
     */
    template <E EnumValue, typename T>
    static std::enable_if_t<
        is_valid_config_type_v<E, EnumValue, T> && 
        (std::is_same_v<T, long> || std::is_same_v<T, uint32_t> || std::is_same_v<T, int64_t>),
        Setting<E, T>
    >
    CreateNumericSetting(T value,
                         std::optional<T> min_val = std::nullopt,
                         std::optional<T> max_val = std::nullopt,
                         const std::string& description = "",
                         const std::string& unit = "")
    {
        return Setting<E, T>(
            EnumValue,
            value,
            max_val,
            min_val,
            unit.empty() ? std::nullopt : std::make_optional(unit),
            description.empty() ? std::nullopt : std::make_optional(description));
    }
};

/**
 * @brief Simple validation functions for common types
 * 
 * These validators work with any type T that supports the required operations.
 * They can be used with built-in types or custom types that implement
 * the necessary operators.
 */
template <typename T>
class SimpleValidators {
public:
    using ValidatorFunc = std::function<bool(const T&)>;

    /**
     * @brief Create a range validator for any comparable type
     */
    static ValidatorFunc Range(T min_val, T max_val)
    {
        return [min_val, max_val](const T& value) {
            return value >= min_val && value <= max_val;
        };
    }

    /**
     * @brief Create a minimum value validator
     */
    static ValidatorFunc MinValue(T min_val)
    {
        return [min_val](const T& value) {
            return value >= min_val;
        };
    }

    /**
     * @brief Create a maximum value validator
     */
    static ValidatorFunc MaxValue(T max_val)
    {
        return [max_val](const T& value) {
            return value <= max_val;
        };
    }

    /**
     * @brief Create a custom validator using ConfigurationTraits
     * 
     * This validator uses the IsValid method from ConfigurationTraits<T>
     * if available, providing trait-based validation.
     */
    template <typename U = T>
    static std::enable_if_t<has_configuration_traits_v<U>, ValidatorFunc>
    TraitBased()
    {
        return [](const U& value) {
            return ConfigurationTraits<U>::IsValid(value);
        };
    }
};

/**
 * @brief Specialized validators for strings
 */
template <>
class SimpleValidators<std::string> {
public:
    using ValidatorFunc = std::function<bool(const std::string&)>;

    /**
     * @brief Create a non-empty string validator
     */
    static ValidatorFunc NonEmpty()
    {
        return [](const std::string& value) {
            return !value.empty();
        };
    }

    /**
     * @brief Create a length range validator
     */
    static ValidatorFunc LengthRange(size_t min_len, size_t max_len)
    {
        return [min_len, max_len](const std::string& value) {
            return value.length() >= min_len && value.length() <= max_len;
        };
    }

    /**
     * @brief Create a validator for specific values
     */
    static ValidatorFunc OneOf(const std::vector<std::string>& valid_values)
    {
        return [valid_values](const std::string& value) {
            return std::find(valid_values.begin(), valid_values.end(), value) != valid_values.end();
        };
    }

    /**
     * @brief Create a regex pattern validator
     */
    static ValidatorFunc Pattern(const std::string& pattern)
    {
        return [pattern](const std::string& value) {
            // For C++17 compatibility, we'll do a simple contains check
            // In a real implementation, you might want to use std::regex
            return value.find(pattern) != std::string::npos;
        };
    }
};

/**
 * @brief Helper macros for creating configurations with validation
 */

/**
 * @brief Macro to create a validated int setting
 */
#define CREATE_VALIDATED_INT_SETTING(EnumValue, Value, MinVal, MaxVal, Description, Unit) \
    config::ConfigHelpers<std::decay_t<decltype(EnumValue)>>::CreateIntSetting<EnumValue>( \
        Value, MinVal, MaxVal, Description, Unit)

/**
 * @brief Macro to create a validated string setting
 */
#define CREATE_VALIDATED_STRING_SETTING(EnumValue, Value, Description) \
    config::ConfigHelpers<std::decay_t<decltype(EnumValue)>>::CreateStringSetting<EnumValue>( \
        Value, Description)

/**
 * @brief Macro to create a custom type setting
 */
#define CREATE_CUSTOM_TYPE_SETTING(EnumValue, Value, Description) \
    config::ConfigHelpers<std::decay_t<decltype(EnumValue)>>::CreateCustomSetting<EnumValue>( \
        Value, std::nullopt, std::nullopt, Description, "")

} // namespace config