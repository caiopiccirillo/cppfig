#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "cppfig/traits.h"

/// @file
/// ConfigTraits for std::filesystem::path. Kept out of the core headers so
/// that code which only reads a configuration does not include `<filesystem>`.
/// `<cppfig/file.h>`, and so `<cppfig/cppfig.h>`, include it; include it directly
/// to declare path settings with only the core headers.

namespace cppfig {

/// Paths are stored as text in the native narrow encoding.
template <>
struct ConfigTraits<std::filesystem::path> {
    static auto FromString(std::string_view text) -> std::optional<std::filesystem::path> { return std::filesystem::path(text); }
    static auto ToString(const std::filesystem::path& value) -> std::string { return value.string(); }
};

}  // namespace cppfig
