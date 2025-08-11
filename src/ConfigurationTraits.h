#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <type_traits>

namespace config {

/**
 * @brief Primary template for configuration type traits
 * 
 * Users must specialize this template for their custom types to define:
 * 1. How to serialize to JSON
 * 2. How to deserialize from JSON
 * 3. How to convert to string representation
 * 4. How to validate the type
 * 
 * @tparam T The type to define traits for
 */
template <typename T>
struct ConfigurationTraits {
    // These methods must be specialized by users for custom types
    // Default implementations are provided for fundamental types below

    /**
     * @brief Convert value to JSON
     * @param value The value to convert
     * @return JSON representation
     */
    static nlohmann::json ToJson(const T& value);

    /**
     * @brief Convert JSON to value
     * @param json The JSON to convert
     * @return The converted value
     */
    static T FromJson(const nlohmann::json& json);

    /**
     * @brief Convert value to string representation
     * @param value The value to convert
     * @return String representation
     */
    static std::string ToString(const T& value);

    /**
     * @brief Check if a value is valid (optional custom validation)
     * @param value The value to validate
     * @return true if valid, false otherwise
     */
    static bool IsValid(const T& value) {
        // Default: all values are valid
        return true;
    }

    /**
     * @brief Get validation error message for invalid values
     * @param value The invalid value
     * @return Error message describing why the value is invalid
     */
    static std::string GetValidationError(const T& value) {
        return "Invalid value";
    }
};

// Specializations for fundamental types that our library supports by default

/**
 * @brief Traits specialization for int
 */
template <>
struct ConfigurationTraits<int> {
    static nlohmann::json ToJson(const int& value) {
        return nlohmann::json(value);
    }

    static int FromJson(const nlohmann::json& json) {
        return json.get<int>();
    }

    static std::string ToString(const int& value) {
        return std::to_string(value);
    }

    static bool IsValid(const int& value) {
        return true; // All int values are valid by default
    }

    static std::string GetValidationError(const int& value) {
        return "Invalid integer value: " + std::to_string(value);
    }
};

/**
 * @brief Traits specialization for float
 */
template <>
struct ConfigurationTraits<float> {
    static nlohmann::json ToJson(const float& value) {
        return nlohmann::json(value);
    }

    static float FromJson(const nlohmann::json& json) {
        return json.get<float>();
    }

    static std::string ToString(const float& value) {
        return std::to_string(value);
    }

    static bool IsValid(const float& value) {
        return !std::isnan(value) && !std::isinf(value);
    }

    static std::string GetValidationError(const float& value) {
        return "Invalid float value: " + std::to_string(value);
    }
};

/**
 * @brief Traits specialization for double
 */
template <>
struct ConfigurationTraits<double> {
    static nlohmann::json ToJson(const double& value) {
        return nlohmann::json(value);
    }

    static double FromJson(const nlohmann::json& json) {
        return json.get<double>();
    }

    static std::string ToString(const double& value) {
        return std::to_string(value);
    }

    static bool IsValid(const double& value) {
        return !std::isnan(value) && !std::isinf(value);
    }

    static std::string GetValidationError(const double& value) {
        return "Invalid double value: " + std::to_string(value);
    }
};

/**
 * @brief Traits specialization for std::string
 */
template <>
struct ConfigurationTraits<std::string> {
    static nlohmann::json ToJson(const std::string& value) {
        return nlohmann::json(value);
    }

    static std::string FromJson(const nlohmann::json& json) {
        return json.get<std::string>();
    }

    static std::string ToString(const std::string& value) {
        return value;
    }

    static bool IsValid(const std::string& value) {
        return true; // All string values are valid by default
    }

    static std::string GetValidationError(const std::string& value) {
        return "Invalid string value: " + value;
    }
};

/**
 * @brief Traits specialization for bool
 */
template <>
struct ConfigurationTraits<bool> {
    static nlohmann::json ToJson(const bool& value) {
        return nlohmann::json(value);
    }

    static bool FromJson(const nlohmann::json& json) {
        return json.get<bool>();
    }

    static std::string ToString(const bool& value) {
        return value ? "true" : "false";
    }

    static bool IsValid(const bool& value) {
        return true; // All bool values are valid
    }

    static std::string GetValidationError(const bool& value) {
        return "Invalid boolean value";
    }
};

/**
 * @brief SFINAE helper to check if a type has ConfigurationTraits specialized
 * 
 * This uses SFINAE to detect if ConfigurationTraits<T> has been properly specialized
 * by checking if the required methods exist and are callable.
 */
template <typename T, typename = void>
struct has_configuration_traits : std::false_type {};

template <typename T>
struct has_configuration_traits<T, std::void_t<
    decltype(ConfigurationTraits<T>::ToJson(std::declval<const T&>())),
    decltype(ConfigurationTraits<T>::FromJson(std::declval<const nlohmann::json&>())),
    decltype(ConfigurationTraits<T>::ToString(std::declval<const T&>())),
    decltype(ConfigurationTraits<T>::IsValid(std::declval<const T&>())),
    decltype(ConfigurationTraits<T>::GetValidationError(std::declval<const T&>()))
>> : std::true_type {};

/**
 * @brief Helper variable template for has_configuration_traits
 */
template <typename T>
constexpr bool has_configuration_traits_v = has_configuration_traits<T>::value;

/**
 * @brief Concept-like helper for C++17 to ensure a type is configuration-compatible
 * 
 * This can be used in enable_if to restrict templates to types that have proper traits.
 */
template <typename T>
using enable_if_configurable_t = std::enable_if_t<has_configuration_traits_v<T>>;

} // namespace config