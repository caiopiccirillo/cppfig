#pragma once

#include <chrono>
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
/// full <chrono> header roughly doubles cppfig's parse time; include it in the
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
    struct DurationUnit<std::ratio<60>> {
        static constexpr std::string_view name = "min";
    };

    template <>
    struct DurationUnit<std::ratio<3600>> {
        static constexpr std::string_view name = "h";
    };

    template <typename Period>
    concept NamedDurationUnit = requires { DurationUnit<Period>::name; };

    /// Converts count units of FromPeriod to Duration, only when the result is
    /// exact and representable.
    template <typename Duration, typename FromPeriod>
    [[nodiscard]] auto ConvertDurationExactly(long long count) -> std::optional<Duration>
    {
        using Scale = std::ratio_divide<FromPeriod, typename Duration::period>;
        constexpr auto limit = std::numeric_limits<long long>::max();
        constexpr auto numerator = static_cast<long long>(Scale::num);
        constexpr auto denominator = static_cast<long long>(Scale::den);
        if (count > limit / numerator || count < -(limit / numerator)) {
            return std::nullopt;
        }
        const long long scaled = count * numerator;
        if (scaled % denominator != 0) {
            return std::nullopt;
        }
        const long long converted = scaled / denominator;
        if (!std::in_range<typename Duration::rep>(converted)) {
            return std::nullopt;
        }
        return Duration(static_cast<typename Duration::rep>(converted));
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
        std::size_t unit_start = 0;
        if (unit_start < text.size() && text[unit_start] == '-') {
            ++unit_start;
        }
        while (unit_start < text.size() && text[unit_start] >= '0' && text[unit_start] <= '9') {
            ++unit_start;
        }
        auto count = ConfigTraits<long long>::FromString(text.substr(0, unit_start));
        if (!count) {
            return std::nullopt;
        }

        const auto unit = text.substr(unit_start);
        if (unit == "ns") {
            return detail::ConvertDurationExactly<value_type, std::nano>(*count);
        }
        if (unit == "us") {
            return detail::ConvertDurationExactly<value_type, std::micro>(*count);
        }
        if (unit == "ms") {
            return detail::ConvertDurationExactly<value_type, std::milli>(*count);
        }
        if (unit == "s") {
            return detail::ConvertDurationExactly<value_type, std::ratio<1>>(*count);
        }
        if (unit == "min") {
            return detail::ConvertDurationExactly<value_type, std::ratio<60>>(*count);
        }
        if (unit == "h") {
            return detail::ConvertDurationExactly<value_type, std::ratio<3600>>(*count);
        }
        return std::nullopt;
    }

    static auto ToString(const value_type& value) -> std::string
    {
        return ConfigTraits<Rep>::ToString(value.count()) + std::string(detail::DurationUnit<Period>::name);
    }
};

}  // namespace cppfig
