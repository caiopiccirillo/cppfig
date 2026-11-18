#pragma once

#include <charconv>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace cppfig {

/// @brief Converts configuration values to and from their flat text representation.
///
/// Specialize this template for custom types. Both operations are used only
/// during initialization or migration, never by Configuration::Get.
template <typename T>
struct ConfigTraits {
    static auto FromString(std::string_view text) -> std::optional<T> = delete;
    static auto ToString(const T& value) -> std::string = delete;
};

/// @brief Types with complete non-throwing flat-text conversion traits.
template <typename T>
concept Configurable = requires(const T& value, std::string_view text) {
    { ConfigTraits<T>::FromString(text) } -> std::same_as<std::optional<T>>;
    { ConfigTraits<T>::ToString(value) } -> std::same_as<std::string>;
};

namespace detail {

    template <typename T>
    struct IntegerTraits {
        static auto FromString(std::string_view text) -> std::optional<T>
        {
            if (text.empty()) {
                return std::nullopt;
            }
            T value {};
            const auto* first = text.data();
            const auto* last = first + text.size();
            const auto result = std::from_chars(first, last, value);
            if (result.ec != std::errc {} || result.ptr != last) {
                return std::nullopt;
            }
            return value;
        }

        static auto ToString(T value) -> std::string
        {
            char buffer[32] {};
            const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
            return std::string(buffer, result.ptr);
        }
    };

    template <typename T>
    struct FloatingTraits {
        static auto FromString(std::string_view text) -> std::optional<T>
        {
            if (text.empty()) {
                return std::nullopt;
            }
            T value {};
            const auto* first = text.data();
            const auto* last = first + text.size();
            const auto result = std::from_chars(first, last, value, std::chars_format::general);
            if (result.ec != std::errc {} || result.ptr != last) {
                return std::nullopt;
            }
            return value;
        }

        static auto ToString(T value) -> std::string
        {
            char buffer[128] {};
            const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value, std::chars_format::general);
            return std::string(buffer, result.ptr);
        }
    };

}  // namespace detail

template <>
struct ConfigTraits<bool> {
    static auto FromString(std::string_view text) -> std::optional<bool>
    {
        if (text == "true" || text == "1" || text == "yes" || text == "on") {
            return true;
        }
        if (text == "false" || text == "0" || text == "no" || text == "off") {
            return false;
        }
        return std::nullopt;
    }

    static auto ToString(bool value) -> std::string { return value ? "true" : "false"; }
};

template <>
struct ConfigTraits<int> : detail::IntegerTraits<int> { };

template <>
struct ConfigTraits<std::int64_t> : detail::IntegerTraits<std::int64_t> { };

template <>
struct ConfigTraits<float> : detail::FloatingTraits<float> { };

template <>
struct ConfigTraits<double> : detail::FloatingTraits<double> { };

template <>
struct ConfigTraits<std::string> {
    static auto FromString(std::string_view text) -> std::optional<std::string> { return std::string(text); }
    static auto ToString(const std::string& value) -> std::string { return value; }
};

}  // namespace cppfig
