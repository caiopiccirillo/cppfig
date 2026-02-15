#pragma once

#if !defined(CPPFIG_HAS_JSON)
// NOLINTNEXTLINE(readability-identifier-naming)
#error "cppfig: JSON support is not enabled. Set CPPFIG_ENABLE_JSON=ON in CMake or add the 'json' vcpkg feature."
#endif

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

#include "cppfig/value.h"

namespace cppfig {

// Conversion utilities

/// @brief Converts a nlohmann::json value to a cppfig::Value.
inline auto JsonToValue(const nlohmann::json& json) -> Value
{
    if (json.is_null()) {
        return {};
    }
    if (json.is_boolean()) {
        return { json.get<bool>() };
    }
    if (json.is_number_integer()) {
        return { json.get<std::int64_t>() };
    }
    if (json.is_number_float()) {
        return { json.get<double>() };
    }
    if (json.is_string()) {
        return { json.get<std::string>() };
    }
    if (json.is_object()) {
        auto obj = Value::Object();
        for (const auto& [key, val] : json.items()) {
            obj[key] = JsonToValue(val);
        }
        return obj;
    }
    if (json.is_array()) {
        auto arr = Value::Array();
        // Array elements are not used in config settings but supported
        // for completeness.
        return arr;
    }
    return {};
}

/// @brief Converts a cppfig::Value to a nlohmann::json value.
inline auto ValueToJson(const Value& value) -> nlohmann::json
{
    if (value.IsNull()) {
        return nullptr;
    }
    if (value.IsBoolean()) {
        return value.Get<bool>();
    }
    if (value.IsInteger()) {
        return value.Get<std::int64_t>();
    }
    if (value.IsDouble()) {
        return value.Get<double>();
    }
    if (value.IsString()) {
        return value.Get<std::string>();
    }
    if (value.IsObject()) {
        nlohmann::json json = nlohmann::json::object();
        for (const auto& [key, val] : value.Items()) {
            json[key] = ValueToJson(val);
        }
        return json;
    }
    if (value.IsArray()) {
        return nlohmann::json::array();
    }
    return nullptr;
}

/// @brief JSON serializer using nlohmann::json.
///
/// Converts between cppfig::Value trees and JSON file format.
struct JsonSerializer {
    using data_type = Value;

    /// @brief Parses JSON from an input stream.
    static auto Parse(std::istream& is) -> StatusOr<Value>
    {
        try {
            nlohmann::json data;
            is >> data;
            return JsonToValue(data);
        }
        catch (const nlohmann::json::parse_error& e) {
            return InvalidArgumentError(std::string("JSON parse error: ") + e.what());
        }
    }

    /// @brief Parses JSON from a string.
    static auto ParseString(std::string_view str) -> StatusOr<Value>
    {
        try {
            return JsonToValue(nlohmann::json::parse(str));
        }
        catch (const nlohmann::json::parse_error& e) {
            return InvalidArgumentError(std::string("JSON parse error: ") + e.what());
        }
    }

    /// @brief Converts a Value tree to a formatted JSON string.
    static auto Stringify(const Value& data, int indent = 4) -> std::string { return ValueToJson(data).dump(indent); }
};

// ADL-based ConfigTraits helper

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
/// #include <cppfig/json.h>
///
/// template<>
/// struct cppfig::ConfigTraits<MyType> : cppfig::ConfigTraitsFromJsonAdl<MyType> {};
/// @endcode
template <typename T>
    requires HasJsonAdl<T>
struct ConfigTraitsFromJsonAdl {
    static auto Serialize(const T& value) -> Value
    {
        nlohmann::json json;
        to_json(json, value);
        return JsonToValue(json);
    }

    static auto Deserialize(const Value& value) -> std::optional<T>
    {
        try {
            nlohmann::json json = ValueToJson(value);
            T result;
            from_json(json, result);
            return result;
        }
        catch (...) {
            return std::nullopt;
        }
    }

    static auto ToString(const T& value) -> std::string { return Serialize(value).Dump(); }

    static auto FromString(std::string_view str) -> std::optional<T>
    {
        try {
            auto json = nlohmann::json::parse(str);
            return Deserialize(JsonToValue(json));
        }
        catch (...) {
            return std::nullopt;
        }
    }
};

}  // namespace cppfig
