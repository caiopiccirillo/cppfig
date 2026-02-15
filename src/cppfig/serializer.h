#pragma once

#include <concepts>
#include <fstream>
#include <istream>
#include <string>

#include "cppfig/status.h"
#include "cppfig/value.h"

namespace cppfig {

/// @brief Concept for serializer types.
///
/// A serializer must provide:
/// - Parse: Read from a stream and produce a Value tree
/// - Stringify: Convert a Value tree to a string
template <typename S>
concept Serializer = requires(const Value& data, std::istream& is) {
    typename S::data_type;
    { S::Parse(is) } -> std::same_as<StatusOr<Value>>;
    { S::Stringify(data) } -> std::convertible_to<std::string>;
};

/// @brief Helper to read a file into a Value tree via a serializer.
template <Serializer S>
auto ReadFile(const std::string& path) -> StatusOr<Value>
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return NotFoundError("Could not open file: " + path);
    }
    return S::Parse(file);
}

/// @brief Helper to write a Value tree to a file via a serializer.
template <Serializer S>
auto WriteFile(const std::string& path, const Value& data) -> Status
{
    std::ofstream file(path);
    if (!file.is_open()) {
        return InternalError("Could not write to file: " + path);
    }
    file << S::Stringify(data);
    if (file.fail()) {
        return InternalError("Failed to write to file: " + path);
    }
    return OkStatus();
}

}  // namespace cppfig
