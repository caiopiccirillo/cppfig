#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "cppfig/value.h"

namespace cppfig {

/// @brief Primary template for configuration type traits.
///
/// Users must specialize this template for custom types to enable
/// serialization, deserialization, and validation. Default specializations
/// are provided for primitive types (int, double, bool, std::string).
///
/// @tparam T The configuration value type.
template <typename T>
struct ConfigTraits {
    /// @brief Serializes a value to a Value node.
    /// @param value The value to serialize.
    /// @return The Value representation of the value.
    static auto Serialize(const T& value) -> Value = delete;

    /// @brief Deserializes a value from a Value node.
    /// @param value The Value node to deserialize.
    /// @return The deserialized value, or std::nullopt on failure.
    static auto Deserialize(const Value& value) -> std::optional<T> = delete;

    /// @brief Converts a value to a human-readable string.
    /// @param value The value to convert.
    /// @return String representation of the value.
    static auto ToString(const T& value) -> std::string = delete;

    /// @brief Parses a value from a string (e.g., from environment variables).
    /// @param str The string to parse.
    /// @return The parsed value, or std::nullopt on failure.
    static auto FromString(std::string_view str) -> std::optional<T> = delete;
};

/// @brief Concept constraining types that can be used as configuration values.
///
/// A type satisfies Configurable if ConfigTraits<T> provides the required
/// static member functions for serialization and conversion.
template <typename T>
concept Configurable = requires(const T& value, const Value& val, std::string_view str) {
    { ConfigTraits<T>::Serialize(value) } -> std::convertible_to<Value>;
    { ConfigTraits<T>::Deserialize(val) } -> std::same_as<std::optional<T>>;
    { ConfigTraits<T>::ToString(value) } -> std::convertible_to<std::string>;
    { ConfigTraits<T>::FromString(str) } -> std::same_as<std::optional<T>>;
};

template <>
struct ConfigTraits<bool> {
    static auto Serialize(bool value) -> Value { return value; }

    static auto Deserialize(const Value& value) -> std::optional<bool>
    {
        if (!value.IsBoolean()) {
            return std::nullopt;
        }
        return value.Get<bool>();
    }

    static auto ToString(bool value) -> std::string { return value ? "true" : "false"; }

    static auto FromString(std::string_view str) -> std::optional<bool>
    {
        if (str == "true" || str == "1" || str == "yes" || str == "on") {
            return true;
        }
        if (str == "false" || str == "0" || str == "no" || str == "off") {
            return false;
        }
        return std::nullopt;
    }
};

template <>
struct ConfigTraits<int> {
    static auto Serialize(int value) -> Value { return value; }

    static auto Deserialize(const Value& value) -> std::optional<int>
    {
        if (!value.IsInteger()) {
            return std::nullopt;
        }
        return value.Get<int>();
    }

    static auto ToString(int value) -> std::string { return std::to_string(value); }

    static auto FromString(std::string_view str) -> std::optional<int>
    {
        try {
            std::size_t pos = 0;
            int result = std::stoi(std::string(str), &pos);
            if (pos != str.size()) {
                return std::nullopt;
            }
            return result;
        }
        catch (...) {
            return std::nullopt;
        }
    }
};

template <>
struct ConfigTraits<std::int64_t> {
    static auto Serialize(std::int64_t value) -> Value { return value; }

    static auto Deserialize(const Value& value) -> std::optional<std::int64_t>
    {
        if (!value.IsInteger()) {
            return std::nullopt;
        }
        return value.Get<std::int64_t>();
    }

    static auto ToString(std::int64_t value) -> std::string { return std::to_string(value); }

    static auto FromString(std::string_view str) -> std::optional<std::int64_t>
    {
        try {
            std::size_t pos = 0;
            std::int64_t result = std::stoll(std::string(str), &pos);
            if (pos != str.size()) {
                return std::nullopt;
            }
            return result;
        }
        catch (...) {
            return std::nullopt;
        }
    }
};

template <>
struct ConfigTraits<double> {
    static auto Serialize(double value) -> Value { return value; }

    static auto Deserialize(const Value& value) -> std::optional<double>
    {
        if (!value.IsNumber()) {
            return std::nullopt;
        }
        return value.Get<double>();
    }

    static auto ToString(double value) -> std::string { return std::to_string(value); }

    static auto FromString(std::string_view str) -> std::optional<double>
    {
        try {
            std::size_t pos = 0;
            double result = std::stod(std::string(str), &pos);
            if (pos != str.size()) {
                return std::nullopt;
            }
            return result;
        }
        catch (...) {
            return std::nullopt;
        }
    }
};

template <>
struct ConfigTraits<float> {
    static auto Serialize(float value) -> Value { return value; }

    static auto Deserialize(const Value& value) -> std::optional<float>
    {
        if (!value.IsNumber()) {
            return std::nullopt;
        }
        return value.Get<float>();
    }

    static auto ToString(float value) -> std::string { return std::to_string(value); }

    static auto FromString(std::string_view str) -> std::optional<float>
    {
        try {
            std::size_t pos = 0;
            float result = std::stof(std::string(str), &pos);
            if (pos != str.size()) {
                return std::nullopt;
            }
            return result;
        }
        catch (...) {
            return std::nullopt;
        }
    }
};

template <>
struct ConfigTraits<std::string> {
    static auto Serialize(const std::string& value) -> Value { return value; }

    static auto Deserialize(const Value& value) -> std::optional<std::string>
    {
        if (!value.IsString()) {
            return std::nullopt;
        }
        return value.Get<std::string>();
    }

    static auto ToString(const std::string& value) -> std::string { return value; }

    static auto FromString(std::string_view str) -> std::optional<std::string> { return std::string(str); }
};

}  // namespace cppfig
