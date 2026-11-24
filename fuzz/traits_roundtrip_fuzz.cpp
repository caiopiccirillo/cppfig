// Fuzzes the built-in value conversions.
//
// The first byte picks a type; the rest is used two ways:
// - as text: whatever FromString accepts, ToString must write as text that
//   FromString reads back to the same value, and writing that again must give
//   the same text;
// - as the bytes of a value, for arithmetic types and durations: every value,
//   including ones no text produces, such as a default, must survive ToString
//   and FromString. Migrated defaults are written this way.

#include <cppfig/chrono.h>
#include <cppfig/cppfig.h>
#include <cppfig/path.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <ratio>
#include <string>
#include <string_view>
#include <type_traits>

#include "fuzz_check.h"

namespace fuzz {
namespace {

    enum class Color : std::uint8_t { kRed,
                                      kGreen,
                                      kBlue };

    constexpr std::array<cppfig::EnumName<Color>, 3> kColorNames { {
        { .value = Color::kRed, .name = "red" },
        { .value = Color::kGreen, .name = "green" },
        { .value = Color::kBlue, .name = "blue" },
    } };

}  // namespace
}  // namespace fuzz

template <>
struct cppfig::ConfigTraits<fuzz::Color> : cppfig::EnumTraits<fuzz::kColorNames> { };

namespace {

template <typename T>
auto Same(const T& left, const T& right) -> bool
{
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(left) || std::isnan(right)) {
            return std::isnan(left) && std::isnan(right) && std::signbit(left) == std::signbit(right);
        }
        // Equal and with the same sign, so 0 and -0 differ; avoids -Wfloat-equal.
        return left <= right && left >= right && std::signbit(left) == std::signbit(right);
    }
    else {
        return left == right;
    }
}

template <typename T>
void CheckValue(const T& value)
{
    using Traits = cppfig::ConfigTraits<T>;
    const std::string text = Traits::ToString(value);
    const auto reparsed = Traits::FromString(text);
    if (!reparsed) {
        std::fprintf(stderr, "ToString wrote '%s', which FromString rejects\n", text.c_str());
    }
    CPPFIG_FUZZ_CHECK(reparsed.has_value());
    CPPFIG_FUZZ_CHECK(Same(*reparsed, value));
    CPPFIG_FUZZ_CHECK(Traits::ToString(*reparsed) == text);
}

template <typename T>
void CheckText(std::string_view text)
{
    if (const auto parsed = cppfig::ConfigTraits<T>::FromString(text)) {
        CheckValue(*parsed);
    }
}

/// Reinterprets the leading bytes of text as a T, padding with zeros.
template <typename T>
auto ValueFromBytes(std::string_view text) -> T
{
    std::array<char, sizeof(T)> bytes {};
    std::memcpy(bytes.data(), text.data(), std::min(text.size(), bytes.size()));
    T value;
    std::memcpy(&value, bytes.data(), sizeof(T));
    return value;
}

template <typename T>
void CheckArithmetic(std::string_view text)
{
    CheckText<T>(text);
    CheckValue(ValueFromBytes<T>(text));
}

template <typename Duration>
void CheckDuration(std::string_view text)
{
    CheckText<Duration>(text);
    CheckValue(Duration(ValueFromBytes<typename Duration::rep>(text)));
}

}  // namespace

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) -> int
{
    if (size == 0) {
        return 0;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): libFuzzer's interface takes bytes.
    const std::string_view text(reinterpret_cast<const char*>(data) + 1, size - 1);

    switch (data[0] % 24) {
    case 0:
        CheckText<bool>(text);
        break;
    case 1:
        CheckArithmetic<signed char>(text);
        break;
    case 2:
        CheckArithmetic<unsigned char>(text);
        break;
    case 3:
        CheckArithmetic<short>(text);
        break;
    case 4:
        CheckArithmetic<unsigned short>(text);
        break;
    case 5:
        CheckArithmetic<int>(text);
        break;
    case 6:
        CheckArithmetic<unsigned int>(text);
        break;
    case 7:
        CheckArithmetic<long long>(text);
        break;
    case 8:
        CheckArithmetic<unsigned long long>(text);
        break;
    case 9:
        CheckArithmetic<float>(text);
        break;
    case 10:
        CheckArithmetic<double>(text);
        break;
    case 11:
        if constexpr (cppfig::Configurable<long double>) {
            CheckText<long double>(text);
        }
        break;
    case 12:
        CheckText<std::string>(text);
        break;
    case 13:
        CheckText<std::filesystem::path>(text);
        break;
    case 14:
        CheckText<fuzz::Color>(text);
        break;
    case 15:
        CheckDuration<std::chrono::nanoseconds>(text);
        break;
    case 16:
        CheckDuration<std::chrono::microseconds>(text);
        break;
    case 17:
        CheckDuration<std::chrono::milliseconds>(text);
        break;
    case 18:
        CheckDuration<std::chrono::seconds>(text);
        break;
    case 19:
        CheckDuration<std::chrono::minutes>(text);
        break;
    case 20:
        CheckDuration<std::chrono::hours>(text);
        break;
    case 21:
        CheckDuration<std::chrono::duration<std::int8_t, std::milli>>(text);
        break;
    case 22:
        CheckDuration<std::chrono::duration<std::uint64_t, std::micro>>(text);
        break;
    case 23:
        CheckDuration<std::chrono::duration<std::uint16_t, std::ratio<3600>>>(text);
        break;
    default:
        break;
    }
    return 0;
}
