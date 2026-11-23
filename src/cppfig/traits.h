#pragma once

#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

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
            // Every digit of the largest value, and a sign.
            std::array<char, std::numeric_limits<T>::digits10 + 2> buffer {};
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
            return std::string(buffer.data(), result.ptr);
        }
    };

    /// Character types hold text, not numbers, so they get no integer traits.
    template <typename T>
    concept CharacterType = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t>
        || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

    template <typename T>
    concept ConfigurableInteger = std::is_integral_v<T> && !std::is_same_v<T, bool> && !CharacterType<T>;

    /// Floating-point types the standard library can convert with from_chars/to_chars.
    template <typename T>
    concept ConfigurableFloating = std::is_floating_point_v<T> && requires(const char* text, char* buffer, T value) {
        std::from_chars(text, text, value, std::chars_format::general);
        std::to_chars(buffer, buffer, value, std::chars_format::general);
    };

    /// Room for the shortest text that reads back as the same value, for every
    /// floating-point type including long double.
    inline constexpr std::size_t kMaxFloatingTextSize = 160;

    template <typename T>
    struct FloatingTraits {
        static auto FromString(std::string_view text) -> std::optional<T>
        {
            if (text.empty()) {
                return std::nullopt;
            }
            if constexpr (std::is_same_v<T, long double>) {
                // libstdc++ parses long double with code that, for "inf" and
                // "nan", reads past the end of the range. Values are views
                // into larger text, so parse a terminated copy instead. float
                // and double use a parser that stays within the range.
                const std::string terminated(text);
                return Parse(terminated);
            }
            else {
                return Parse(text);
            }
        }

        static auto ToString(T value) -> std::string
        {
            std::array<char, kMaxFloatingTextSize> buffer {};
            const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
            return std::string(buffer.data(), result.ptr);
        }

    private:
        static auto Parse(std::string_view text) -> std::optional<T>
        {
            T value {};
            const auto* first = text.data();
            const auto* last = first + text.size();
            const auto result = std::from_chars(first, last, value, std::chars_format::general);
            if (result.ec != std::errc {} || result.ptr != last) {
                return std::nullopt;
            }
            return value;
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

/// Every standard integer type except bool and the character types.
/// Parsing is exact: no sign for unsigned types, no prefixes, no trailing text,
/// and out-of-range values are rejected.
template <detail::ConfigurableInteger T>
struct ConfigTraits<T> : detail::IntegerTraits<T> { };

/// Every floating-point type the standard library can convert exactly.
template <detail::ConfigurableFloating T>
struct ConfigTraits<T> : detail::FloatingTraits<T> { };

template <>
struct ConfigTraits<std::string> {
    static auto FromString(std::string_view text) -> std::optional<std::string> { return std::string(text); }
    static auto ToString(const std::string& value) -> std::string { return value; }
};

/// Paths are stored as text in the native narrow encoding.
template <>
struct ConfigTraits<std::filesystem::path> {
    static auto FromString(std::string_view text) -> std::optional<std::filesystem::path> { return std::filesystem::path(text); }
    static auto ToString(const std::filesystem::path& value) -> std::string { return value.string(); }
};

/// @brief One enumerator and its name in a .conf file, for EnumTraits.
template <typename E>
    requires std::is_enum_v<E>
struct EnumName {
    E value;
    std::string_view name;
};

/// @brief ConfigTraits for an enum, mapping enumerators to names through a
/// constexpr table. Names are matched exactly; anything else fails to parse.
///
/// @code
/// enum class Level { kDebug, kInfo };
/// inline constexpr std::array<cppfig::EnumName<Level>, 2> kLevelNames { {
///     { .value = Level::kDebug, .name = "debug" },
///     { .value = Level::kInfo, .name = "info" },
/// } };
/// template <>
/// struct cppfig::ConfigTraits<Level> : cppfig::EnumTraits<kLevelNames> { };
/// @endcode
///
/// An enumerator missing from the table is written as its underlying number,
/// which the table then fails to parse back; list every value a setting can hold.
template <const auto& Names>
struct EnumTraits {
    using value_type = std::remove_cvref_t<decltype(Names[0].value)>;

    static_assert(std::size(Names) > 0, "EnumTraits needs at least one name");
    static_assert(
        [] {
            for (std::size_t i = 0; i < std::size(Names); ++i) {
                for (std::size_t j = i + 1; j < std::size(Names); ++j) {
                    if (Names[i].name == Names[j].name || Names[i].value == Names[j].value) {
                        return false;
                    }
                }
            }
            return true;
        }(),
        "EnumTraits names and values must each be unique");

    static auto FromString(std::string_view text) -> std::optional<value_type>
    {
        for (const auto& entry : Names) {
            if (entry.name == text) {
                return entry.value;
            }
        }
        return std::nullopt;
    }

    static auto ToString(const value_type& value) -> std::string
    {
        for (const auto& entry : Names) {
            if (entry.value == value) {
                return std::string(entry.name);
            }
        }
        return ConfigTraits<std::underlying_type_t<value_type>>::ToString(static_cast<std::underlying_type_t<value_type>>(value));
    }
};

}  // namespace cppfig
