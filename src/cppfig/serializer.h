#pragma once

#include <concepts>
#include <filesystem>
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
///
/// The write is atomic: the serialized data goes to a temporary file
/// alongside the target, which is then renamed over it.  A reader therefore
/// observes either the whole previous file or the whole new one, and a crash
/// or a full disk part-way through leaves the existing configuration intact
/// rather than truncated.
template <Serializer S>
auto WriteFile(const std::string& path, const Value& data) -> Status
{
    namespace fs = std::filesystem;

    const fs::path target(path);
    const fs::path temp_path = target.parent_path() / (target.filename().string() + ".tmp");

    {
        std::ofstream file(temp_path);
        if (!file.is_open()) {
            return InternalError("Could not write to file: " + path);
        }

        file << S::Stringify(data);
        file.close();

        if (file.fail()) {
            std::error_code remove_error;
            fs::remove(temp_path, remove_error);
            return InternalError("Failed to write to file: " + path);
        }
    }

    // Carry the existing file's permissions over to the replacement, so an
    // atomic save does not silently reset the mode of a deployed config.
    std::error_code status_error;
    const auto existing = fs::status(target, status_error);
    if (!status_error && fs::exists(existing)) {
        std::error_code permissions_error;
        fs::permissions(temp_path, existing.permissions(), fs::perm_options::replace, permissions_error);
    }

    std::error_code error_code;
    fs::rename(temp_path, target, error_code);
    if (error_code) {
        std::error_code remove_error;
        fs::remove(temp_path, remove_error);
        return InternalError("Failed to replace file: " + path + ": " + error_code.message());
    }

    return OkStatus();
}

}  // namespace cppfig
