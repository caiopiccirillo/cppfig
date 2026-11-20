#include <cppfig/chrono.h>
#include <cppfig/cppfig.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace cppfig::test {
namespace {

    enum class Level : std::uint8_t { kDebug,
                                      kInfo,
                                      kError };

    inline constexpr std::array<EnumName<Level>, 3> kLevelNames { {
        { Level::kDebug, "debug" },
        { Level::kInfo, "info" },
        { Level::kError, "error" },
    } };

}  // namespace
}  // namespace cppfig::test

template <>
struct cppfig::ConfigTraits<cppfig::test::Level> : cppfig::EnumTraits<cppfig::test::kLevelNames> { };

namespace cppfig::test {
namespace {

    TEST(TraitsTest, ParsesEveryStandardIntegerTypeExactly)
    {
        static_assert(Configurable<short> && Configurable<unsigned short> && Configurable<unsigned> && Configurable<long>
                      && Configurable<unsigned long> && Configurable<long long> && Configurable<unsigned long long>
                      && Configurable<std::uint16_t> && Configurable<std::int8_t>);

        EXPECT_EQ(ConfigTraits<std::uint16_t>::FromString("65535"), std::uint16_t { 65535 });
        EXPECT_EQ(ConfigTraits<std::uint16_t>::FromString("65536"), std::nullopt);
        EXPECT_EQ(ConfigTraits<unsigned>::FromString("-1"), std::nullopt);
        EXPECT_EQ(ConfigTraits<long long>::FromString("-9223372036854775808"), std::numeric_limits<long long>::min());
        EXPECT_EQ(ConfigTraits<unsigned long long>::ToString(18446744073709551615ULL), "18446744073709551615");
        EXPECT_EQ(ConfigTraits<std::int8_t>::FromString("-128"), std::int8_t { -128 });
    }

    TEST(TraitsTest, CharacterTypesAreNotTreatedAsIntegers)
    {
        static_assert(!Configurable<char>);
        static_assert(!Configurable<char8_t>);
        static_assert(!Configurable<wchar_t>);
    }

    TEST(TraitsTest, ParsesEveryFloatingPointType)
    {
        if constexpr (Configurable<long double>) {
            EXPECT_EQ(ConfigTraits<long double>::FromString("0.5"), 0.5L);
        }
        EXPECT_EQ(ConfigTraits<float>::FromString("1e"), std::nullopt);
    }

    // libstdc++'s long double from_chars reads past the end of the range for
    // "inf" and "nan". Values are views into larger text, so FromString must
    // stay inside the view; AddressSanitizer catches the overread here.
    TEST(TraitsTest, ParsesLongDoubleSpecialValuesWithinTheText)
    {
        if constexpr (Configurable<long double>) {
            for (const std::string_view text : { "inf", "-inf", "nan" }) {
                const auto buffer = std::make_unique<char[]>(text.size());
                std::memcpy(buffer.get(), text.data(), text.size());
                const auto value = ConfigTraits<long double>::FromString(std::string_view(buffer.get(), text.size()));
                ASSERT_TRUE(value.has_value()) << text;
                EXPECT_EQ(std::isnan(*value), text == "nan") << text;
            }
        }
    }

    TEST(TraitsTest, ParsesDurationsWithRequiredUnits)
    {
        using std::chrono::milliseconds;
        using std::chrono::seconds;
        EXPECT_EQ(ConfigTraits<milliseconds>::FromString("250ms"), milliseconds { 250 });
        EXPECT_EQ(ConfigTraits<milliseconds>::FromString("2s"), milliseconds { 2000 });
        EXPECT_EQ(ConfigTraits<milliseconds>::FromString("1min"), milliseconds { 60000 });
        EXPECT_EQ(ConfigTraits<seconds>::FromString("2h"), seconds { 7200 });
        EXPECT_EQ(ConfigTraits<seconds>::FromString("-5s"), seconds { -5 });
        EXPECT_EQ(ConfigTraits<std::chrono::nanoseconds>::FromString("3us"), std::chrono::nanoseconds { 3000 });
    }

    TEST(TraitsTest, RejectsInexactOrMalformedDurations)
    {
        using std::chrono::milliseconds;
        using std::chrono::seconds;
        EXPECT_EQ(ConfigTraits<milliseconds>::FromString("250"), std::nullopt) << "the unit is required";
        EXPECT_EQ(ConfigTraits<milliseconds>::FromString("1500us"), std::nullopt) << "would truncate";
        EXPECT_EQ(ConfigTraits<seconds>::FromString("10 s"), std::nullopt);
        EXPECT_EQ(ConfigTraits<seconds>::FromString("10sec"), std::nullopt);
        EXPECT_EQ(ConfigTraits<seconds>::FromString("ms"), std::nullopt);
        EXPECT_EQ(ConfigTraits<std::chrono::duration<std::int32_t>>::FromString("9999999999h"), std::nullopt) << "overflows";
    }

    TEST(TraitsTest, WritesDurationsInTheirOwnUnit)
    {
        EXPECT_EQ(ConfigTraits<std::chrono::milliseconds>::ToString(std::chrono::milliseconds { 1500 }), "1500ms");
        EXPECT_EQ(ConfigTraits<std::chrono::minutes>::ToString(std::chrono::minutes { 3 }), "3min");
        EXPECT_EQ(ConfigTraits<std::chrono::hours>::ToString(std::chrono::hours { 1 }), "1h");
        EXPECT_EQ(ConfigTraits<std::chrono::microseconds>::ToString(std::chrono::microseconds { 7 }), "7us");
    }

    TEST(TraitsTest, ReadsBackTheExtremeDurationsItWrites)
    {
        using Nanoseconds = std::chrono::nanoseconds;
        using UnsignedMicroseconds = std::chrono::duration<std::uint64_t, std::micro>;
        using TinyMilliseconds = std::chrono::duration<std::int8_t, std::milli>;

        for (const auto value : { Nanoseconds::min(), Nanoseconds::max() }) {
            EXPECT_EQ(ConfigTraits<Nanoseconds>::FromString(ConfigTraits<Nanoseconds>::ToString(value)), value);
        }
        const auto largest = UnsignedMicroseconds::max();
        EXPECT_EQ(ConfigTraits<UnsignedMicroseconds>::FromString(ConfigTraits<UnsignedMicroseconds>::ToString(largest)), largest);
        EXPECT_EQ(ConfigTraits<TinyMilliseconds>::FromString("-128ms"), TinyMilliseconds::min());

        EXPECT_EQ(ConfigTraits<Nanoseconds>::FromString("-9223372036854775809ns"), std::nullopt) << "one below the minimum";
        EXPECT_EQ(ConfigTraits<TinyMilliseconds>::FromString("-129ms"), std::nullopt);
        EXPECT_EQ(ConfigTraits<UnsignedMicroseconds>::FromString("-1us"), std::nullopt);
        EXPECT_EQ(ConfigTraits<UnsignedMicroseconds>::FromString("18446744073709551616us"), std::nullopt);
        EXPECT_EQ(ConfigTraits<Nanoseconds>::FromString("-ns"), std::nullopt);
    }

    TEST(TraitsTest, RoundTripsFilesystemPaths)
    {
        EXPECT_EQ(ConfigTraits<std::filesystem::path>::FromString("/var/lib/app data"), std::filesystem::path("/var/lib/app data"));
        EXPECT_EQ(ConfigTraits<std::filesystem::path>::ToString(std::filesystem::path("relative/dir")), "relative/dir");
    }

    TEST(TraitsTest, MapsEnumeratorsByName)
    {
        static_assert(Configurable<Level>);
        EXPECT_EQ(ConfigTraits<Level>::FromString("info"), Level::kInfo);
        EXPECT_EQ(ConfigTraits<Level>::FromString("INFO"), std::nullopt);
        EXPECT_EQ(ConfigTraits<Level>::FromString("1"), std::nullopt);
        EXPECT_EQ(ConfigTraits<Level>::ToString(Level::kError), "error");
    }

}  // namespace
}  // namespace cppfig::test
