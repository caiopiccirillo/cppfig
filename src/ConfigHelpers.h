#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "Setting.h"

namespace config {

/**
 * @brief Simple helper functions for configuration management
 */
template <typename E>
class ConfigHelpers {
public:
    using SettingVariant = std::variant<
        Setting<E, int>,
        Setting<E, float>,
        Setting<E, double>,
        Setting<E, std::string>,
        Setting<E, bool>>;

    using ConfigMap = std::unordered_map<E, SettingVariant>;

    /**
     * @brief Create a simple int setting
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
};

/**
 * @brief Simple validation functions
 */
template <typename T>
class SimpleValidators {
public:
    using ValidatorFunc = std::function<bool(const T&)>;

    /**
     * @brief Create a range validator
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
};

} // namespace config
