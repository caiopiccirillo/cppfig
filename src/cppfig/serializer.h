#pragma once

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <nlohmann/json.hpp>

#include <concepts>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>

namespace cppfig {

/// @brief Concept for serializer types.
///
/// A serializer must provide:
/// - Parse: Read from a stream and produce a JSON-like structure
/// - Stringify: Convert a JSON-like structure to a string
/// - Merge: Merge two JSON-like structures (for schema migration)
template <typename S>
concept Serializer = requires(const typename S::DataType& data, std::istream& is) {
    typename S::DataType;
    { S::Parse(is) } -> std::same_as<absl::StatusOr<typename S::DataType>>;
    { S::Stringify(data) } -> std::convertible_to<std::string>;
    { S::Merge(data, data) } -> std::same_as<typename S::DataType>;
};

/// @brief JSON serializer using nlohmann::json.
struct JsonSerializer {
    using DataType = nlohmann::json;

    /// @brief Parses JSON from an input stream.
    static auto Parse(std::istream& is) -> absl::StatusOr<DataType>
    {
        try {
            DataType data;
            is >> data;
            return data;
        }
        catch (const nlohmann::json::parse_error& e) {
            return absl::InvalidArgumentError(std::string("JSON parse error: ") + e.what());
        }
    }

    /// @brief Parses JSON from a string.
    static auto ParseString(std::string_view str) -> absl::StatusOr<DataType>
    {
        try {
            return DataType::parse(str);
        }
        catch (const nlohmann::json::parse_error& e) {
            return absl::InvalidArgumentError(std::string("JSON parse error: ") + e.what());
        }
    }

    /// @brief Converts JSON to a formatted string.
    static auto Stringify(const DataType& data, int indent = 4) -> std::string { return data.dump(indent); }

    /// @brief Merges two JSON objects, with overlay taking precedence.
    ///
    /// This performs a deep merge where:
    /// - Objects are merged recursively
    /// - Arrays and primitives from overlay replace base
    static auto Merge(const DataType& base, const DataType& overlay) -> DataType
    {
        if (!base.is_object() || !overlay.is_object()) {
            return overlay;
        }

        DataType result = base;
        for (auto& [key, value] : overlay.items()) {
            if (result.contains(key) && result[key].is_object() && value.is_object()) {
                result[key] = Merge(result[key], value);
            }
            else {
                result[key] = value;
            }
        }
        return result;
    }

    /// @brief Gets a value at a dot-separated path.
    static auto GetAtPath(const DataType& data, std::string_view path) -> absl::StatusOr<DataType>
    {
        const DataType* current = &data;
        std::string path_str(path);
        std::istringstream ss(path_str);
        std::string segment;

        while (std::getline(ss, segment, '.')) {
            if (!current->is_object()) {
                return absl::NotFoundError("Path segment '" + segment + "' not found: parent is not an object");
            }
            if (!current->contains(segment)) {
                return absl::NotFoundError("Path segment '" + segment + "' not found");
            }
            current = &(*current)[segment];
        }
        return *current;
    }

    /// @brief Sets a value at a dot-separated path, creating intermediate objects.
    static void SetAtPath(DataType& data, std::string_view path, const DataType& value)
    {
        DataType* current = &data;
        std::string path_str(path);
        std::istringstream ss(path_str);
        std::string segment;
        std::vector<std::string> segments;

        while (std::getline(ss, segment, '.')) {
            segments.push_back(segment);
        }

        for (std::size_t i = 0; i < segments.size() - 1; ++i) {
            if (!current->contains(segments[i]) || !(*current)[segments[i]].is_object()) {
                (*current)[segments[i]] = DataType::object();
            }
            current = &(*current)[segments[i]];
        }

        if (!segments.empty()) {
            (*current)[segments.back()] = value;
        }
    }

    /// @brief Checks if a path exists in the data.
    static auto HasPath(const DataType& data, std::string_view path) -> bool
    {
        return GetAtPath(data, path).ok();
    }
};

/// @brief Helper to read a file into a serializer's data type.
template <Serializer S>
auto ReadFile(const std::string& path) -> absl::StatusOr<typename S::DataType>
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return absl::NotFoundError("Could not open file: " + path);
    }
    return S::Parse(file);
}

/// @brief Helper to write a serializer's data type to a file.
template <Serializer S>
auto WriteFile(const std::string& path, const typename S::DataType& data) -> absl::Status
{
    std::ofstream file(path);
    if (!file.is_open()) {
        return absl::InternalError("Could not write to file: " + path);
    }
    file << S::Stringify(data);
    if (file.fail()) {
        return absl::InternalError("Failed to write to file: " + path);
    }
    return absl::OkStatus();
}

}  // namespace cppfig
