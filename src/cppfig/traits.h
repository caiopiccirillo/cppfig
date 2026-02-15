#pragma once

#include <concepts>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

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
    /// @brief Serializes a value to JSON.
    /// @param value The value to serialize.
    /// @return The JSON representation of the value.
    static auto ToJson(const T& value) -> nlohmann::json = delete;

    /// @brief Deserializes a value from JSON.
    /// @param json The JSON to deserialize.
    /// @return The deserialized value, or std::nullopt on failure.
    static auto FromJson(const nlohmann::json& json) -> std::optional<T> = delete;

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
concept Configurable = requires(const T& value, const nlohmann::json& json, std::string_view str) {
    { ConfigTraits<T>::ToJson(value) } -> std::convertible_to<nlohmann::json>;
    { ConfigTraits<T>::FromJson(json) } -> std::same_as<std::optional<T>>;
    { ConfigTraits<T>::ToString(value) } -> std::convertible_to<std::string>;
    { ConfigTraits<T>::FromString(str) } -> std::same_as<std::optional<T>>;
};

template <>
struct ConfigTraits<bool> {
    static auto ToJson(bool value) -> nlohmann::json { return value; }

    static auto FromJson(const nlohmann::json& json) -> std::optional<bool>
    {
        if (!json.is_boolean()) {
            return std::nullopt;
        }
        return json.get<bool>();
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
    static auto ToJson(int value) -> nlohmann::json { return value; }

    static auto FromJson(const nlohmann::json& json) -> std::optional<int>
    {
        if (!json.is_number_integer()) {
            return std::nullopt;
        }
        return json.get<int>();
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
    static auto ToJson(std::int64_t value) -> nlohmann::json { return value; }

    static auto FromJson(const nlohmann::json& json) -> std::optional<std::int64_t>
    {
        if (!json.is_number_integer()) {
            return std::nullopt;
        }
        return json.get<std::int64_t>();
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
    static auto ToJson(double value) -> nlohmann::json { return value; }

    static auto FromJson(const nlohmann::json& json) -> std::optional<double>
    {
        if (!json.is_number()) {
            return std::nullopt;
        }
        return json.get<double>();
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
    static auto ToJson(float value) -> nlohmann::json { return value; }

    static auto FromJson(const nlohmann::json& json) -> std::optional<float>
    {
        if (!json.is_number()) {
            return std::nullopt;
        }
        return json.get<float>();
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
    static auto ToJson(const std::string& value) -> nlohmann::json { return value; }

    static auto FromJson(const nlohmann::json& json) -> std::optional<std::string>
    {
        if (!json.is_string()) {
            return std::nullopt;
        }
        return json.get<std::string>();
    }

    static auto ToString(const std::string& value) -> std::string { return value; }

    static auto FromString(std::string_view str) -> std::optional<std::string> { return std::string(str); }
};

/// @brief Concept for types that have nlohmann::json ADL serialization.
template <typename T>
concept HasJsonAdl = requires(const T& value, nlohmann::json& json) {
    { to_json(json, value) };
    { from_json(json, std::declval<T&>()) };
};

/// @brief Helper to create ConfigTraits for types with nlohmann::json ADL.
///
/// Users can inherit from this to get automatic trait implementation:
/// @code
/// template<>
/// struct ConfigTraits<MyType> : ConfigTraitsFromJsonAdl<MyType> {};
/// @endcode
template <typename T>
    requires HasJsonAdl<T>
struct ConfigTraitsFromJsonAdl {
    static auto ToJson(const T& value) -> nlohmann::json
    {
        nlohmann::json json;
        to_json(json, value);
        return json;
    }

    static auto FromJson(const nlohmann::json& json) -> std::optional<T>
    {
        try {
            T value;
            from_json(json, value);
            return value;
        }
        catch (...) {
            return std::nullopt;
        }
    }

    static auto ToString(const T& value) -> std::string { return ToJson(value).dump(); }

    static auto FromString(std::string_view str) -> std::optional<T>
    {
        try {
            auto json = nlohmann::json::parse(str);
            return FromJson(json);
        }
        catch (...) {
            return std::nullopt;
        }
    }
};

}  // namespace cppfig
