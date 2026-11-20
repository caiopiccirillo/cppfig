#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include "test_settings.h"

namespace cppfig::test {
namespace {

    static_assert(IsSetting<settings::ServerPort>);
    static_assert(HasEnvOverride<settings::ServerPort>);
    static_assert(HasValidator<settings::ServerPort>);
    static_assert(!HasEnvOverride<settings::AppName>);
    static_assert(TestRegistry::has_setting<settings::ServerPort>);
    static_assert(TestRegistry::size == 7);
    static_assert(ConfigurationProvider<TestConfiguration, TestRegistry>);
    static_assert(!std::is_polymorphic_v<TestConfiguration>);

    struct Unregistered {
        static constexpr std::string_view path = "unregistered.value";
        using value_type = int;
        static auto default_value() -> int { return 0; }
    };

    static_assert(!TestRegistry::has_setting<Unregistered>);

    struct CountedDefault {
        static constexpr std::string_view path = "factory.value";
        using value_type = int;
        inline static int calls = 0;
        static auto default_value() -> int
        {
            ++calls;
            return 42;
        }
    };

    // Declared so that naive declaration-order storage would pad after each bool.
    struct LayoutFlagA {
        static constexpr std::string_view path = "layout.flag_a";
        using value_type = bool;
        static auto default_value() -> bool { return true; }
    };

    struct LayoutWideA {
        static constexpr std::string_view path = "layout.wide_a";
        using value_type = double;
        static auto default_value() -> double { return 1.5; }
    };

    struct LayoutFlagB {
        static constexpr std::string_view path = "layout.flag_b";
        using value_type = bool;
        static auto default_value() -> bool { return false; }
    };

    struct LayoutWideB {
        static constexpr std::string_view path = "layout.wide_b";
        using value_type = double;
        static auto default_value() -> double { return 2.5; }
    };

    using InterleavedRegistry = Registry<LayoutFlagA, LayoutWideA, LayoutFlagB, LayoutWideB>;

    struct PackedReference {
        double wide_a;
        double wide_b;
        bool flag_a;
        bool flag_b;
    };

    /// A type that can be neither default-constructed nor copied.
    struct MoveOnlyValue {
        explicit MoveOnlyValue(int initial)
            : value(std::make_unique<int>(initial))
        {
        }

        std::unique_ptr<int> value;
    };

}  // namespace
}  // namespace cppfig::test

template <>
struct cppfig::ConfigTraits<cppfig::test::MoveOnlyValue> {
    static auto FromString(std::string_view text) -> std::optional<cppfig::test::MoveOnlyValue>
    {
        auto parsed = ConfigTraits<int>::FromString(text);
        if (!parsed) {
            return std::nullopt;
        }
        return cppfig::test::MoveOnlyValue { *parsed };
    }

    static auto ToString(const cppfig::test::MoveOnlyValue& value) -> std::string { return ConfigTraits<int>::ToString(*value.value); }
};

namespace cppfig::test {
namespace {

    struct MoveOnlySetting {
        static constexpr std::string_view path = "move.only";
        using value_type = MoveOnlyValue;
        static auto default_value() -> MoveOnlyValue { return MoveOnlyValue { 5 }; }
    };

    static_assert(!std::is_default_constructible_v<MoveOnlyValue>);
    static_assert(!std::is_copy_constructible_v<MoveOnlyValue>);

    TEST(LayoutTest, StorageIsOrderedToAvoidInterleavedPadding)
    {
        EXPECT_EQ(sizeof(InterleavedRegistry::values_type), sizeof(PackedReference));
    }

    TEST(LayoutTest, ReorderedStorageKeepsDeclarationIndexing)
    {
        static_assert(InterleavedRegistry::index<LayoutFlagA> == 0);
        static_assert(InterleavedRegistry::index<LayoutWideB> == 3);

        auto loaded = Configuration<InterleavedRegistry>::FromDefaults();
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_TRUE(loaded->Get<LayoutFlagA>());
        EXPECT_DOUBLE_EQ(loaded->Get<LayoutWideA>(), 1.5);
        EXPECT_FALSE(loaded->Get<LayoutFlagB>());
        EXPECT_DOUBLE_EQ(loaded->Get<LayoutWideB>(), 2.5);

        InterleavedRegistry::ForEachSetting([&]<typename S>() {
            const auto address = reinterpret_cast<std::uintptr_t>(std::addressof(loaded->Get<S>()));
            EXPECT_EQ(address % alignof(typename S::value_type), 0U) << S::path;
        });
    }

    TEST(ConfigurationTest, SupportsMoveOnlyNonDefaultConstructibleValues)
    {
        using MoveOnlyRegistry = Registry<MoveOnlySetting, LayoutFlagA>;
        auto loaded = Configuration<MoveOnlyRegistry>::FromDefaults();
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_EQ(*loaded->Get<MoveOnlySetting>().value, 5);
        EXPECT_TRUE(loaded->Get<LayoutFlagA>());

        testing::MockConfigurationBuilder<MoveOnlyRegistry> builder;
        builder.Set<MoveOnlySetting>(MoveOnlyValue { 9 });
        auto built = builder.Build();
        ASSERT_TRUE(built.ok()) << built.status().message();
        EXPECT_EQ(*built->Get<MoveOnlySetting>().value, 9);
    }

    /// Deriving gives the registry a short name of its own, which keeps
    /// diagnostics, clangd hovers and symbol names readable.
    struct DerivedRegistry : Registry<settings::ServerPort, settings::AppName> { };

    static_assert(ConfigurationProvider<Configuration<DerivedRegistry>, DerivedRegistry>);
    static_assert(ConfigurationProvider<testing::MockConfiguration<DerivedRegistry>, DerivedRegistry>);
    static_assert(!ConfigurationProvider<Configuration<DerivedRegistry>, TestRegistry>);

    TEST(RegistryTest, DerivedRegistryWorksLikeItsBase)
    {
        auto loaded = Configuration<DerivedRegistry>::FromDefaults();
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_EQ(loaded->Get<settings::ServerPort>(), 8080);
        EXPECT_EQ(loaded->Get<settings::AppName>(), "MyApplication");
    }

    TEST(RegistryTest, FindsEveryDeclaredPathAndRejectsUnknownPaths)
    {
        TestRegistry::ForEachSetting([]<typename S>() {
            EXPECT_EQ(TestRegistry::FindPath(S::path), TestRegistry::index<S>);
            EXPECT_EQ(TestRegistry::GetPaths()[TestRegistry::index<S>], S::path);
        });
        EXPECT_EQ(TestRegistry::FindPath("server.missing"), TestRegistry::size);
        EXPECT_EQ(TestRegistry::FindPath("SERVER.HOST"), TestRegistry::size);
    }

    TEST(ConfigurationTest, CreatesTypedValuesFromDefaults)
    {
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        const auto& config = loaded.value();

        EXPECT_EQ(config.Get<settings::AppName>(), "MyApplication");
        EXPECT_EQ(config.Get<settings::ServerHost>(), "localhost");
        EXPECT_EQ(config.Get<settings::ServerPort>(), 8080);
        EXPECT_TRUE(config.Get<settings::LoggingEnabled>());
        EXPECT_DOUBLE_EQ(config.Get<settings::Ratio>(), 1.5);
        EXPECT_EQ(config.Get<settings::Origin>(), (Point { 1, 2 }));
        EXPECT_EQ(config.Get<settings::Aligned>().value, 16);
    }

    TEST(ConfigurationTest, GetReturnsStableConstReferences)
    {
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.ok());
        const auto& config = loaded.value();

        static_assert(std::is_same_v<decltype(config.Get<settings::ServerPort>()), const int&>);
        static_assert(std::is_same_v<decltype(config.Get<settings::AppName>()), const std::string&>);

        const auto* name = std::addressof(config.Get<settings::AppName>());
        const auto* port = std::addressof(config.Get<settings::ServerPort>());
        EXPECT_EQ(name, std::addressof(config.Get<settings::AppName>()));
        EXPECT_EQ(port, std::addressof(config.Get<settings::ServerPort>()));
    }

    TEST(ConfigurationTest, PreservesNaturalAlignmentForEverySetting)
    {
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.ok());
        const auto& config = loaded.value();

        TestRegistry::ForEachSetting([&]<typename S>() {
            const auto address = reinterpret_cast<std::uintptr_t>(std::addressof(config.Get<S>()));
            EXPECT_EQ(address % alignof(typename S::value_type), 0U) << S::path;
        });
        EXPECT_EQ(alignof(settings::Aligned::value_type), 16U);
    }

    TEST(ConfigurationTest, CallsEachDefaultFactoryOnce)
    {
        using CountedRegistry = Registry<CountedDefault>;
        CountedDefault::calls = 0;
        auto loaded = Configuration<CountedRegistry>::FromDefaults();
        ASSERT_TRUE(loaded.ok());
        EXPECT_EQ(CountedDefault::calls, 1);
        EXPECT_EQ(loaded->Get<CountedDefault>(), 42);
    }

    TEST(TraitsTest, RoundTripsBuiltInTypes)
    {
        EXPECT_EQ(ConfigTraits<bool>::FromString("yes"), true);
        EXPECT_EQ(ConfigTraits<bool>::FromString("off"), false);
        EXPECT_EQ(ConfigTraits<bool>::FromString("TRUE"), std::nullopt);
        EXPECT_EQ(ConfigTraits<int>::FromString("-42"), -42);
        EXPECT_EQ(ConfigTraits<int>::FromString("42x"), std::nullopt);
        EXPECT_EQ(ConfigTraits<int>::FromString(""), std::nullopt);
        EXPECT_EQ(ConfigTraits<std::int64_t>::FromString("9223372036854775807"), INT64_MAX);
        EXPECT_EQ(ConfigTraits<std::int64_t>::FromString("9223372036854775808"), std::nullopt);
        EXPECT_EQ(ConfigTraits<double>::FromString("1.25"), 1.25);
        EXPECT_EQ(ConfigTraits<float>::FromString("2.5"), 2.5F);
        EXPECT_EQ(ConfigTraits<std::string>::FromString("001"), "001");

        EXPECT_EQ(ConfigTraits<int>::ToString(-42), "-42");
        EXPECT_EQ(ConfigTraits<double>::ToString(1.25), "1.25");
        EXPECT_EQ(ConfigTraits<bool>::ToString(true), "true");
    }

    TEST(ValidatorTest, UsesTypedCompositionWithoutTypeErasure)
    {
        constexpr auto validator = Min(1).And(Max(10));
        static_assert(ValidatorFor<std::remove_cvref_t<decltype(validator)>, int>);
        EXPECT_TRUE(validator(5));
        EXPECT_FALSE(validator(0));
        EXPECT_FALSE(validator(11));

        constexpr auto either = Positive<int>().Or(Min(-5));
        EXPECT_TRUE(either(-3));
        EXPECT_FALSE(either(-6));
        EXPECT_TRUE(Range(1, 5)(5));
        EXPECT_TRUE(NotEmpty()("x"));
        EXPECT_FALSE(NotEmpty()(""));
        EXPECT_TRUE(MinLength(1)("x"));
        EXPECT_FALSE(MaxLength(1)("xy"));
        EXPECT_TRUE(OneOf<int>({ 1, 2 })(2));
        EXPECT_TRUE(Predicate<int>([](int value) { return value % 2 == 0; }, "must be even")(4));
        EXPECT_TRUE(AlwaysValid<std::string>()("anything"));
    }

    TEST(MockTest, BuilderFreezesValuesAndSatisfiesProviderConcept)
    {
        using Mock = testing::MockConfiguration<TestRegistry>;
        static_assert(ConfigurationProvider<Mock, TestRegistry>);

        testing::MockConfigurationBuilder<TestRegistry> builder;
        builder.Set<settings::ServerPort>(9000).Set<settings::AppName>("TestApp");
        auto built = builder.Build();
        ASSERT_TRUE(built.ok()) << built.status().message();

        const auto& mock = built.value();
        EXPECT_EQ(mock.Get<settings::ServerPort>(), 9000);
        EXPECT_EQ(mock.Get<settings::AppName>(), "TestApp");
        EXPECT_EQ(mock.Get<settings::ServerHost>(), "localhost");
    }

    TEST(MockTest, RejectsInvalidTestValuesAtBuild)
    {
        testing::MockConfigurationBuilder<TestRegistry> builder;
        builder.Set<settings::ServerPort>(0);
        auto built = builder.Build();
        ASSERT_FALSE(built.ok());
        EXPECT_NE(built.status().message().find("server.port"), std::string_view::npos);
    }

}  // namespace
}  // namespace cppfig::test
