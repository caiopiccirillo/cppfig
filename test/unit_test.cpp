/// @file unit_test.cpp
/// @brief Unit tests for cppfig configuration library.

#include <gtest/gtest.h>

#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>

namespace cppfig::test {

TEST(ConfigTraitsTest, BoolToJson) {
    EXPECT_EQ(ConfigTraits<bool>::ToJson(true), true);
    EXPECT_EQ(ConfigTraits<bool>::ToJson(false), false);
}

TEST(ConfigTraitsTest, BoolFromJson) {
    EXPECT_EQ(ConfigTraits<bool>::FromJson(nlohmann::json(true)), true);
    EXPECT_EQ(ConfigTraits<bool>::FromJson(nlohmann::json(false)), false);
    EXPECT_EQ(ConfigTraits<bool>::FromJson(nlohmann::json("invalid")), std::nullopt);
}

TEST(ConfigTraitsTest, BoolFromString) {
    EXPECT_EQ(ConfigTraits<bool>::FromString("true"), true);
    EXPECT_EQ(ConfigTraits<bool>::FromString("false"), false);
    EXPECT_EQ(ConfigTraits<bool>::FromString("1"), true);
    EXPECT_EQ(ConfigTraits<bool>::FromString("0"), false);
    EXPECT_EQ(ConfigTraits<bool>::FromString("yes"), true);
    EXPECT_EQ(ConfigTraits<bool>::FromString("no"), false);
    EXPECT_EQ(ConfigTraits<bool>::FromString("invalid"), std::nullopt);
}

TEST(ConfigTraitsTest, IntToJson) {
    EXPECT_EQ(ConfigTraits<int>::ToJson(42), 42);
    EXPECT_EQ(ConfigTraits<int>::ToJson(-1), -1);
}

TEST(ConfigTraitsTest, IntFromJson) {
    EXPECT_EQ(ConfigTraits<int>::FromJson(nlohmann::json(42)), 42);
    EXPECT_EQ(ConfigTraits<int>::FromJson(nlohmann::json("invalid")), std::nullopt);
}

TEST(ConfigTraitsTest, IntFromString) {
    EXPECT_EQ(ConfigTraits<int>::FromString("42"), 42);
    EXPECT_EQ(ConfigTraits<int>::FromString("-1"), -1);
    EXPECT_EQ(ConfigTraits<int>::FromString("abc"), std::nullopt);
    EXPECT_EQ(ConfigTraits<int>::FromString("42abc"), std::nullopt);
}

TEST(ConfigTraitsTest, DoubleToJson) {
    EXPECT_DOUBLE_EQ(ConfigTraits<double>::ToJson(3.14).get<double>(), 3.14);
}

TEST(ConfigTraitsTest, DoubleFromJson) {
    auto result = ConfigTraits<double>::FromJson(nlohmann::json(3.14));
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 3.14);
}

TEST(ConfigTraitsTest, StringToJson) {
    EXPECT_EQ(ConfigTraits<std::string>::ToJson("hello"), "hello");
}

TEST(ConfigTraitsTest, StringFromJson) {
    EXPECT_EQ(ConfigTraits<std::string>::FromJson(nlohmann::json("hello")), "hello");
    EXPECT_EQ(ConfigTraits<std::string>::FromJson(nlohmann::json(42)), std::nullopt);
}

TEST(ValidatorTest, MinValidator) {
    auto validator = Min(5);
    EXPECT_TRUE(validator(5));
    EXPECT_TRUE(validator(10));
    EXPECT_FALSE(validator(4));
}

TEST(ValidatorTest, MaxValidator) {
    auto validator = Max(10);
    EXPECT_TRUE(validator(10));
    EXPECT_TRUE(validator(5));
    EXPECT_FALSE(validator(11));
}

TEST(ValidatorTest, RangeValidator) {
    auto validator = Range(1, 100);
    EXPECT_TRUE(validator(1));
    EXPECT_TRUE(validator(50));
    EXPECT_TRUE(validator(100));
    EXPECT_FALSE(validator(0));
    EXPECT_FALSE(validator(101));
}

TEST(ValidatorTest, NotEmptyValidator) {
    auto validator = NotEmpty();
    EXPECT_TRUE(validator("hello"));
    EXPECT_FALSE(validator(""));
}

TEST(ValidatorTest, MaxLengthValidator) {
    auto validator = MaxLength(5);
    EXPECT_TRUE(validator("hello"));
    EXPECT_TRUE(validator("hi"));
    EXPECT_FALSE(validator("hello world"));
}

TEST(ValidatorTest, OneOfValidator) {
    auto validator = OneOf<std::string>({"debug", "info", "warn", "error"});
    EXPECT_TRUE(validator("debug"));
    EXPECT_TRUE(validator("info"));
    EXPECT_FALSE(validator("trace"));
}

TEST(ValidatorTest, AndCombinator) {
    auto validator = Min(1).And(Max(10));
    EXPECT_TRUE(validator(5));
    EXPECT_FALSE(validator(0));
    EXPECT_FALSE(validator(11));
}

TEST(ValidatorTest, OrCombinator) {
    auto validator = Predicate<int>([](int v) { return v == 0; }, "Must be 0")
                         .Or(Predicate<int>([](int v) { return v == 100; }, "Must be 100"));
    EXPECT_TRUE(validator(0));
    EXPECT_TRUE(validator(100));
    EXPECT_FALSE(validator(50));
}

struct TestStringSetting {
    static constexpr std::string_view kPath = "test.string";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "default"; }
};

struct TestIntSetting {
    static constexpr std::string_view kPath = "test.int";
    using ValueType = int;
    static auto DefaultValue() -> int { return 42; }
};

struct TestSettingWithEnv {
    static constexpr std::string_view kPath = "test.env";
    static constexpr std::string_view kEnvOverride = "TEST_ENV_SETTING";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "from_default"; }
};

struct TestSettingWithValidator {
    static constexpr std::string_view kPath = "test.validated";
    using ValueType = int;
    static auto DefaultValue() -> int { return 50; }
    static auto GetValidator() -> Validator<int> { return Range(1, 100); }
};

TEST(SettingTest, SettingConcept) {
    static_assert(IsSetting<TestStringSetting>);
    static_assert(IsSetting<TestIntSetting>);
    static_assert(IsSetting<TestSettingWithEnv>);
    static_assert(IsSetting<TestSettingWithValidator>);
}

TEST(SettingTest, HasEnvOverrideConcept) {
    static_assert(!HasEnvOverride<TestStringSetting>);
    static_assert(HasEnvOverride<TestSettingWithEnv>);
}

TEST(SettingTest, HasValidatorConcept) {
    static_assert(!HasValidator<TestStringSetting>);
    static_assert(HasValidator<TestSettingWithValidator>);
}

TEST(SettingTest, GetEnvOverrideHelper) {
    EXPECT_EQ(GetEnvOverride<TestStringSetting>(), "");
    EXPECT_EQ(GetEnvOverride<TestSettingWithEnv>(), "TEST_ENV_SETTING");
}

TEST(SettingTest, GetValidatorHelper) {
    auto validator1 = GetSettingValidator<TestStringSetting>();
    EXPECT_TRUE(validator1("any value"));  // AlwaysValid

    auto validator2 = GetSettingValidator<TestSettingWithValidator>();
    EXPECT_TRUE(validator2(50));
    EXPECT_FALSE(validator2(0));
}

using TestSchema = ConfigSchema<TestStringSetting, TestIntSetting, TestSettingWithValidator>;

TEST(ConfigSchemaTest, SchemaSize) {
    EXPECT_EQ(TestSchema::Size(), 3);
}

TEST(ConfigSchemaTest, HasSettingCheck) {
    static_assert(TestSchema::HasSetting<TestStringSetting>);
    static_assert(TestSchema::HasSetting<TestIntSetting>);
    static_assert(!TestSchema::HasSetting<TestSettingWithEnv>);
}

TEST(ConfigSchemaTest, GetPaths) {
    auto paths = TestSchema::GetPaths();
    EXPECT_EQ(paths.size(), 3);
    EXPECT_EQ(paths[0], "test.string");
    EXPECT_EQ(paths[1], "test.int");
    EXPECT_EQ(paths[2], "test.validated");
}

TEST(ConfigSchemaTest, ForEachSetting) {
    int count = 0;
    TestSchema::ForEachSetting([&count]<typename S>() { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(JsonSerializerTest, ParseAndStringify) {
    std::string json_str = R"({"key": "value", "number": 42})";
    std::istringstream stream(json_str);

    auto result = JsonSerializer::Parse(stream);
    ASSERT_TRUE(result.ok());

    auto data = *result;
    EXPECT_EQ(data["key"], "value");
    EXPECT_EQ(data["number"], 42);

    auto output = JsonSerializer::Stringify(data);
    EXPECT_FALSE(output.empty());
}

TEST(JsonSerializerTest, Merge) {
    nlohmann::json base = {{"a", 1}, {"b", {{"c", 2}}}};
    nlohmann::json overlay = {{"b", {{"d", 3}}}, {"e", 4}};

    auto merged = JsonSerializer::Merge(base, overlay);

    EXPECT_EQ(merged["a"], 1);
    EXPECT_EQ(merged["b"]["c"], 2);
    EXPECT_EQ(merged["b"]["d"], 3);
    EXPECT_EQ(merged["e"], 4);
}

TEST(JsonSerializerTest, GetAtPath) {
    nlohmann::json data = {{"a", {{"b", {{"c", 42}}}}}};

    auto result = JsonSerializer::GetAtPath(data, "a.b.c");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(*result, 42);

    auto not_found = JsonSerializer::GetAtPath(data, "a.b.d");
    EXPECT_FALSE(not_found.ok());
}

TEST(JsonSerializerTest, SetAtPath) {
    nlohmann::json data = nlohmann::json::object();

    JsonSerializer::SetAtPath(data, "a.b.c", 42);

    EXPECT_EQ(data["a"]["b"]["c"], 42);
}

TEST(JsonSerializerTest, HasPath) {
    nlohmann::json data = {{"a", {{"b", 1}}}};

    EXPECT_TRUE(JsonSerializer::HasPath(data, "a.b"));
    EXPECT_FALSE(JsonSerializer::HasPath(data, "a.c"));
}

TEST(ConfigDiffTest, NoDifferences) {
    nlohmann::json a = {{"key", "value"}};
    nlohmann::json b = {{"key", "value"}};

    auto diff = DiffJson(a, b);
    EXPECT_FALSE(diff.HasDifferences());
}

TEST(ConfigDiffTest, AddedEntry) {
    nlohmann::json base = {{"a", 1}};
    nlohmann::json target = {{"a", 1}, {"b", 2}};

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Added().size(), 1);
    EXPECT_EQ(diff.Added()[0].path, "b");
}

TEST(ConfigDiffTest, RemovedEntry) {
    nlohmann::json base = {{"a", 1}, {"b", 2}};
    nlohmann::json target = {{"a", 1}};

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Removed().size(), 1);
    EXPECT_EQ(diff.Removed()[0].path, "b");
}

TEST(ConfigDiffTest, ModifiedEntry) {
    nlohmann::json base = {{"a", 1}};
    nlohmann::json target = {{"a", 2}};

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Modified().size(), 1);
    EXPECT_EQ(diff.Modified()[0].path, "a");
}

struct MockAppName {
    static constexpr std::string_view kPath = "app.name";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "MyApp"; }
};

struct MockAppPort {
    static constexpr std::string_view kPath = "app.port";
    using ValueType = int;
    static auto DefaultValue() -> int { return 8080; }
};

using MockSchema = ConfigSchema<MockAppName, MockAppPort>;

TEST(MockConfigurationTest, GetDefault) {
    testing::MockConfiguration<MockSchema> config;

    EXPECT_EQ(config.Get<MockAppName>(), "MyApp");
    EXPECT_EQ(config.Get<MockAppPort>(), 8080);
}

TEST(MockConfigurationTest, SetValue) {
    testing::MockConfiguration<MockSchema> config;
    config.SetValue<MockAppPort>(9000);

    EXPECT_EQ(config.Get<MockAppPort>(), 9000);
}

TEST(MockConfigurationTest, Reset) {
    testing::MockConfiguration<MockSchema> config;
    config.SetValue<MockAppPort>(9000);
    config.Reset();

    EXPECT_EQ(config.Get<MockAppPort>(), 8080);
}

}  // namespace cppfig::test
