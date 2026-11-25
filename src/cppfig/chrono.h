#pragma once

#include <chrono>
#include <cstddef>
#include <limits>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "cppfig/traits.h"

/// @file
/// ConfigTraits for std::chrono::duration. Kept out of cppfig.h because the
/// full `<chrono>` header roughly doubles cppfig's parse time; include it in the
/// translation units that declare duration settings.

namespace cppfig {

namespace detail {

    template <typename Period>
    struct DurationUnit;

    template <>
    struct DurationUnit<std::nano> {
        static constexpr std::string_view name = "ns";
    };

    template <>
    struct DurationUnit<std::micro> {
        static constexpr std::string_view name = "us";
    };

    template <>
    struct DurationUnit<std::milli> {
        static constexpr std::string_view name = "ms";
    };

    template <>
    struct DurationUnit<std::ratio<1>> {
        static constexpr std::string_view name = "s";
    };

    template <>
    struct DurationUnit<std::chrono::minutes::period> {
        static constexpr std::string_view name = "min";
    };

    template <>
    struct DurationUnit<std::chrono::hours::period> {
        static constexpr std::string_view name = "h";
    };

    template <typename Period>
    concept NamedDurationUnit = requires { DurationUnit<Period>::name; };

    /// Converts a count of FromPeriod units, given as a sign and a magnitude,
    /// to Duration, only when the result is exact and representable. Taking
    /// the magnitude unsigned covers every Rep up to 64 bits, including the
    /// most negative signed value and unsigned values above LLONG_MAX.
    template <typename Duration, typename FromPeriod>
    [[nodiscard]] auto ConvertDurationExactly(bool negative, unsigned long long magnitude) -> std::optional<Duration>
    {
        using Rep = Duration::rep;
        using Scale = std::ratio_divide<FromPeriod, typename Duration::period>;
        constexpr auto numerator = static_cast<unsigned long long>(Scale::num);
        constexpr auto denominator = static_cast<unsigned long long>(Scale::den);
        if (magnitude > std::numeric_limits<unsigned long long>::max() / numerator) {
            return std::nullopt;
        }
        const unsigned long long scaled = magnitude * numerator;
        if (scaled % denominator != 0) {
            return std::nullopt;
        }
        const unsigned long long count = scaled / denominator;

        if (!negative || count == 0) {
            if (!std::in_range<Rep>(count)) {
                return std::nullopt;
            }
            return Duration(static_cast<Rep>(count));
        }
        if constexpr (std::is_unsigned_v<Rep>) {
            return std::nullopt;
        }
        else {
            // The most negative value's magnitude is one more than the
            // largest positive value, so negate count - 1 and step down once.
            if (!std::in_range<Rep>(count - 1)) {
                return std::nullopt;
            }
            return -Duration(static_cast<Rep>(count - 1)) - Duration(1);
        }
    }

}  // namespace detail

/// Durations are an integer followed directly by a unit: ns, us, ms, s, min
/// or h, as in `250ms` or `2h`. The unit is required. A value that the target
/// duration cannot hold exactly, such as `1500us` for milliseconds, is rejected.
/// Durations are written in their own unit.
template <typename Rep, typename Period>
    requires detail::ConfigurableInteger<Rep> && detail::NamedDurationUnit<Period>
struct ConfigTraits<std::chrono::duration<Rep, Period>> {
    using value_type = std::chrono::duration<Rep, Period>;

    static auto FromString(std::string_view text) -> std::optional<value_type>
    {
        const bool negative = !text.empty() && text.front() == '-';
        const std::size_t digits_start = negative ? 1 : 0;
        std::size_t unit_start = digits_start;
        while (unit_start < text.size() && text[unit_start] >= '0' && text[unit_start] <= '9') {
            ++unit_start;
        }
        const auto magnitude = ConfigTraits<unsigned long long>::FromString(text.substr(digits_start, unit_start - digits_start));
        if (!magnitude) {
            return std::nullopt;
        }

        const auto unit = text.substr(unit_start);
        if (unit == "ns") {
            return detail::ConvertDurationExactly<value_type, std::nano>(negative, *magnitude);
        }
        if (unit == "us") {
            return detail::ConvertDurationExactly<value_type, std::micro>(negative, *magnitude);
        }
        if (unit == "ms") {
            return detail::ConvertDurationExactly<value_type, std::milli>(negative, *magnitude);
        }
        if (unit == "s") {
            return detail::ConvertDurationExactly<value_type, std::ratio<1>>(negative, *magnitude);
        }
        if (unit == "min") {
            return detail::ConvertDurationExactly<value_type, std::chrono::minutes::period>(negative, *magnitude);
        }
        if (unit == "h") {
            return detail::ConvertDurationExactly<value_type, std::chrono::hours::period>(negative, *magnitude);
        }
        return std::nullopt;
    }

    static auto ToString(const value_type& value) -> std::string
    {
        return ConfigTraits<Rep>::ToString(value.count()) + std::string(detail::DurationUnit<Period>::name);
    }
};

}  // namespace cppfig
