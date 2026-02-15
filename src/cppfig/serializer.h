#pragma once

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include <concepts>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>

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
    { S::Parse(is) } -> std::same_as<absl::StatusOr<Value>>;
    { S::Stringify(data) } -> std::convertible_to<std::string>;
};

/// @brief Helper to read a file into a Value tree via a serializer.
template <Serializer S>
auto ReadFile(const std::string& path) -> absl::StatusOr<Value>
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return absl::NotFoundError("Could not open file: " + path);
    }
    return S::Parse(file);
}

/// @brief Helper to write a Value tree to a file via a serializer.
template <Serializer S>
auto WriteFile(const std::string& path, const Value& data) -> absl::Status
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
