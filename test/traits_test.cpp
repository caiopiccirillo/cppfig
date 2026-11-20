#include <cppfig/chrono.h>
#include <cppfig/cppfig.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>

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
