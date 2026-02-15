#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>
#include <gtest/gtest.h>

namespace cppfig::test {

TEST(ConfigTraitsTest, BoolToJson)
{
    EXPECT_EQ(ConfigTraits<bool>::ToJson(true), true);
    EXPECT_EQ(ConfigTraits<bool>::ToJson(false), false);
}

TEST(ConfigTraitsTest, BoolFromJson)
{
    EXPECT_EQ(ConfigTraits<bool>::FromJson(nlohmann::json(true)), true);
    EXPECT_EQ(ConfigTraits<bool>::FromJson(nlohmann::json(false)), false);
    EXPECT_EQ(ConfigTraits<bool>::FromJson(nlohmann::json("invalid")), std::nullopt);
}

TEST(ConfigTraitsTest, BoolFromString)
{
    EXPECT_EQ(ConfigTraits<bool>::FromString("true"), true);
    EXPECT_EQ(ConfigTraits<bool>::FromString("false"), false);
    EXPECT_EQ(ConfigTraits<bool>::FromString("1"), true);
    EXPECT_EQ(ConfigTraits<bool>::FromString("0"), false);
    EXPECT_EQ(ConfigTraits<bool>::FromString("yes"), true);
    EXPECT_EQ(ConfigTraits<bool>::FromString("no"), false);
    EXPECT_EQ(ConfigTraits<bool>::FromString("invalid"), std::nullopt);
}

TEST(ConfigTraitsTest, IntToJson)
{
    EXPECT_EQ(ConfigTraits<int>::ToJson(42), 42);
    EXPECT_EQ(ConfigTraits<int>::ToJson(-1), -1);
}

TEST(ConfigTraitsTest, IntFromJson)
{
    EXPECT_EQ(ConfigTraits<int>::FromJson(nlohmann::json(42)), 42);
    EXPECT_EQ(ConfigTraits<int>::FromJson(nlohmann::json("invalid")), std::nullopt);
}

TEST(ConfigTraitsTest, IntFromString)
{
    EXPECT_EQ(ConfigTraits<int>::FromString("42"), 42);
    EXPECT_EQ(ConfigTraits<int>::FromString("-1"), -1);
    EXPECT_EQ(ConfigTraits<int>::FromString("abc"), std::nullopt);
    EXPECT_EQ(ConfigTraits<int>::FromString("42abc"), std::nullopt);
}

TEST(ConfigTraitsTest, DoubleToJson)
{
    EXPECT_DOUBLE_EQ(ConfigTraits<double>::ToJson(3.14).get<double>(), 3.14);
}

TEST(ConfigTraitsTest, DoubleFromJson)
{
    auto result = ConfigTraits<double>::FromJson(nlohmann::json(3.14));
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 3.14);

    // Test invalid type returns nullopt
    EXPECT_EQ(ConfigTraits<double>::FromJson(nlohmann::json("invalid")), std::nullopt);
}

TEST(ConfigTraitsTest, StringToJson)
{
    EXPECT_EQ(ConfigTraits<std::string>::ToJson("hello"), "hello");
}

TEST(ConfigTraitsTest, StringFromJson)
{
    EXPECT_EQ(ConfigTraits<std::string>::FromJson(nlohmann::json("hello")), "hello");
    EXPECT_EQ(ConfigTraits<std::string>::FromJson(nlohmann::json(42)), std::nullopt);
}

// Additional traits tests for coverage
TEST(ConfigTraitsTest, DoubleFromString)
{
    EXPECT_DOUBLE_EQ(*ConfigTraits<double>::FromString("3.14"), 3.14);
    EXPECT_DOUBLE_EQ(*ConfigTraits<double>::FromString("-2.5"), -2.5);
    EXPECT_EQ(ConfigTraits<double>::FromString("abc"), std::nullopt);
    EXPECT_EQ(ConfigTraits<double>::FromString("3.14abc"), std::nullopt);
}

TEST(ConfigTraitsTest, DoubleToString)
{
    auto str = ConfigTraits<double>::ToString(3.14);
    EXPECT_FALSE(str.empty());
}

TEST(ConfigTraitsTest, FloatToJson)
{
    EXPECT_FLOAT_EQ(ConfigTraits<float>::ToJson(3.14f).get<float>(), 3.14f);
}

TEST(ConfigTraitsTest, FloatFromJson)
{
    auto result = ConfigTraits<float>::FromJson(nlohmann::json(3.14f));
    ASSERT_TRUE(result.has_value());
    EXPECT_FLOAT_EQ(*result, 3.14f);
    EXPECT_EQ(ConfigTraits<float>::FromJson(nlohmann::json("invalid")), std::nullopt);
}

TEST(ConfigTraitsTest, FloatFromString)
{
    EXPECT_FLOAT_EQ(*ConfigTraits<float>::FromString("3.14"), 3.14f);
    EXPECT_EQ(ConfigTraits<float>::FromString("abc"), std::nullopt);
    EXPECT_EQ(ConfigTraits<float>::FromString("3.14abc"), std::nullopt);
}

TEST(ConfigTraitsTest, FloatToString)
{
    auto str = ConfigTraits<float>::ToString(3.14f);
    EXPECT_FALSE(str.empty());
}

TEST(ConfigTraitsTest, StringToString)
{
    EXPECT_EQ(ConfigTraits<std::string>::ToString("hello"), "hello");
}

TEST(ConfigTraitsTest, StringFromString)
{
    EXPECT_EQ(ConfigTraits<std::string>::FromString("hello"), "hello");
}

TEST(ValidatorTest, MinValidator)
{
    auto validator = Min(5);
    EXPECT_TRUE(validator(5));
    EXPECT_TRUE(validator(10));
    EXPECT_FALSE(validator(4));
}

TEST(ValidatorTest, MaxValidator)
{
    auto validator = Max(10);
    EXPECT_TRUE(validator(10));
    EXPECT_TRUE(validator(5));
    EXPECT_FALSE(validator(11));
}

TEST(ValidatorTest, RangeValidator)
{
    auto validator = Range(1, 100);
    EXPECT_TRUE(validator(1));
    EXPECT_TRUE(validator(50));
    EXPECT_TRUE(validator(100));
    EXPECT_FALSE(validator(0));
    EXPECT_FALSE(validator(101));
}

TEST(ValidatorTest, NotEmptyValidator)
{
    auto validator = NotEmpty();
    EXPECT_TRUE(validator("hello"));
    EXPECT_FALSE(validator(""));
}

TEST(ValidatorTest, MaxLengthValidator)
{
    auto validator = MaxLength(5);
    EXPECT_TRUE(validator("hello"));
    EXPECT_TRUE(validator("hi"));
    EXPECT_FALSE(validator("hello world"));
}

TEST(ValidatorTest, OneOfValidator)
{
    auto validator = OneOf<std::string>({ "debug", "info", "warn", "error" });
    EXPECT_TRUE(validator("debug"));
    EXPECT_TRUE(validator("info"));
    EXPECT_FALSE(validator("trace"));
}

TEST(ValidatorTest, AndCombinator)
{
    auto validator = Min(1).And(Max(10));
    EXPECT_TRUE(validator(5));
    EXPECT_FALSE(validator(0));
    EXPECT_FALSE(validator(11));
}

TEST(ValidatorTest, OrCombinator)
{
    auto validator = Predicate<int>([](int v) { return v == 0; }, "Must be 0")
                         .Or(Predicate<int>([](int v) { return v == 100; }, "Must be 100"));
    EXPECT_TRUE(validator(0));
    EXPECT_TRUE(validator(100));
    EXPECT_FALSE(validator(50));
}

TEST(ValidatorTest, PredicateValidatorSuccess)
{
    // Test predicate returning true (covers the success path at line 197)
    auto validator = Predicate<int>([](int v) { return v > 0; }, "Must be positive");
    auto result = validator(42);
    EXPECT_TRUE(result);
    EXPECT_TRUE(result.error_message.empty());
}

TEST(ValidatorTest, PredicateValidatorFailure)
{
    auto validator = Predicate<int>([](int v) { return v > 0; }, "Must be positive");
    auto result = validator(-1);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error_message, "Must be positive");
}

TEST(ValidatorTest, AlwaysValidValidator)
{
    auto validator = AlwaysValid<int>();
    EXPECT_TRUE(validator(0));
    EXPECT_TRUE(validator(-1000));
    EXPECT_TRUE(validator(1000));
}

TEST(ValidatorTest, MinLengthValidator)
{
    auto validator = MinLength(3);
    EXPECT_TRUE(validator("hello"));
    EXPECT_TRUE(validator("abc"));
    EXPECT_FALSE(validator("ab"));
    EXPECT_FALSE(validator(""));
}

TEST(ValidatorTest, PositiveValidator)
{
    auto validator = Positive<int>();
    EXPECT_TRUE(validator(1));
    EXPECT_TRUE(validator(100));
    EXPECT_FALSE(validator(0));
    EXPECT_FALSE(validator(-1));
}

TEST(ValidatorTest, NonNegativeValidator)
{
    auto validator = NonNegative<int>();
    EXPECT_TRUE(validator(0));
    EXPECT_TRUE(validator(100));
    EXPECT_FALSE(validator(-1));
}

struct TestStringSetting {
    static constexpr std::string_view path = "test.string";
    using value_type = std::string;
    static auto default_value() -> std::string { return "default"; }
};

struct TestIntSetting {
    static constexpr std::string_view path = "test.int";
    using value_type = int;
    static auto default_value() -> int { return 42; }
};

struct TestSettingWithEnv {
    static constexpr std::string_view path = "test.env";
    static constexpr std::string_view env_override = "TEST_ENV_SETTING";
    using value_type = std::string;
    static auto default_value() -> std::string { return "from_default"; }
};

struct TestSettingWithValidator {
    static constexpr std::string_view path = "test.validated";
    using value_type = int;
    static auto default_value() -> int { return 50; }
    static auto validator() -> Validator<int> { return Range(1, 100); }
};

TEST(SettingTest, SettingConcept)
{
    static_assert(IsSetting<TestStringSetting>);
    static_assert(IsSetting<TestIntSetting>);
    static_assert(IsSetting<TestSettingWithEnv>);
    static_assert(IsSetting<TestSettingWithValidator>);
}

TEST(SettingTest, HasEnvOverrideConcept)
{
    static_assert(!HasEnvOverride<TestStringSetting>);
    static_assert(HasEnvOverride<TestSettingWithEnv>);
}

TEST(SettingTest, HasValidatorConcept)
{
    static_assert(!HasValidator<TestStringSetting>);
    static_assert(HasValidator<TestSettingWithValidator>);
}

TEST(SettingTest, GetEnvOverrideHelper)
{
    EXPECT_EQ(GetEnvOverride<TestStringSetting>(), "");
    EXPECT_EQ(GetEnvOverride<TestSettingWithEnv>(), "TEST_ENV_SETTING");
}

TEST(SettingTest, GetValidatorHelper)
{
    auto validator1 = GetSettingValidator<TestStringSetting>();
    EXPECT_TRUE(validator1("any value"));  // AlwaysValid

    auto validator2 = GetSettingValidator<TestSettingWithValidator>();
    EXPECT_TRUE(validator2(50));
    EXPECT_FALSE(validator2(0));
}

using TestSchema = ConfigSchema<TestStringSetting, TestIntSetting, TestSettingWithValidator>;

TEST(ConfigSchemaTest, SchemaSize)
{
    EXPECT_EQ(TestSchema::Size(), 3);
}

TEST(ConfigSchemaTest, HasSettingCheck)
{
    static_assert(TestSchema::has_setting<TestStringSetting>);
    static_assert(TestSchema::has_setting<TestIntSetting>);
    static_assert(!TestSchema::has_setting<TestSettingWithEnv>);
}

TEST(ConfigSchemaTest, GetPaths)
{
    auto paths = TestSchema::GetPaths();
    EXPECT_EQ(paths.size(), 3);
    EXPECT_EQ(paths[0], "test.string");
    EXPECT_EQ(paths[1], "test.int");
    EXPECT_EQ(paths[2], "test.validated");
}

TEST(ConfigSchemaTest, ForEachSetting)
{
    int count = 0;
    TestSchema::ForEachSetting([&count]<typename S>() { ++count; });
    EXPECT_EQ(count, 3);
}

TEST(JsonSerializerTest, ParseAndStringify)
{
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

TEST(JsonSerializerTest, Merge)
{
    nlohmann::json base = { { "a", 1 }, { "b", { { "c", 2 } } } };
    nlohmann::json overlay = { { "b", { { "d", 3 } } }, { "e", 4 } };

    auto merged = JsonSerializer::Merge(base, overlay);

    EXPECT_EQ(merged["a"], 1);
    EXPECT_EQ(merged["b"]["c"], 2);
    EXPECT_EQ(merged["b"]["d"], 3);
    EXPECT_EQ(merged["e"], 4);
}

TEST(JsonSerializerTest, GetAtPath)
{
    nlohmann::json data = { { "a", { { "b", { { "c", 42 } } } } } };

    auto result = JsonSerializer::GetAtPath(data, "a.b.c");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(*result, 42);

    auto not_found = JsonSerializer::GetAtPath(data, "a.b.d");
    EXPECT_FALSE(not_found.ok());
}

TEST(JsonSerializerTest, SetAtPath)
{
    nlohmann::json data = nlohmann::json::object();

    JsonSerializer::SetAtPath(data, "a.b.c", 42);

    EXPECT_EQ(data["a"]["b"]["c"], 42);
}

TEST(JsonSerializerTest, HasPath)
{
    nlohmann::json data = { { "a", { { "b", 1 } } } };

    EXPECT_TRUE(JsonSerializer::HasPath(data, "a.b"));
    EXPECT_FALSE(JsonSerializer::HasPath(data, "a.c"));
}

TEST(JsonSerializerTest, ParseStringSuccess)
{
    auto result = JsonSerializer::ParseString(R"({"key": "value"})");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ((*result)["key"], "value");
}

TEST(JsonSerializerTest, ParseStringError)
{
    auto result = JsonSerializer::ParseString("not valid json {{{");
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsInvalidArgument(result.status()));
}

TEST(JsonSerializerTest, ParseStreamError)
{
    std::istringstream stream("invalid json {{{");
    auto result = JsonSerializer::Parse(stream);
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsInvalidArgument(result.status()));
}

TEST(JsonSerializerTest, MergeNonObject)
{
    // When base is not an object, overlay replaces it entirely
    nlohmann::json base = 42;
    nlohmann::json overlay = { { "key", "value" } };

    auto merged = JsonSerializer::Merge(base, overlay);
    EXPECT_EQ(merged["key"], "value");
}

TEST(JsonSerializerTest, MergeNonObjectOverlay)
{
    // When overlay is not an object, it replaces base
    nlohmann::json base = { { "key", "value" } };
    nlohmann::json overlay = 42;

    auto merged = JsonSerializer::Merge(base, overlay);
    EXPECT_EQ(merged, 42);
}

TEST(JsonSerializerTest, GetAtPathNotAnObject)
{
    nlohmann::json data = { { "a", 42 } };
    auto result = JsonSerializer::GetAtPath(data, "a.b");
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsNotFound(result.status()));
}

TEST(ConfigDiffTest, NoDifferences)
{
    nlohmann::json a = { { "key", "value" } };
    nlohmann::json b = { { "key", "value" } };

    auto diff = DiffJson(a, b);
    EXPECT_FALSE(diff.HasDifferences());
}

TEST(ConfigDiffTest, AddedEntry)
{
    nlohmann::json base = { { "a", 1 } };
    nlohmann::json target = { { "a", 1 }, { "b", 2 } };

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Added().size(), 1);
    EXPECT_EQ(diff.Added()[0].path, "b");
}

TEST(ConfigDiffTest, RemovedEntry)
{
    nlohmann::json base = { { "a", 1 }, { "b", 2 } };
    nlohmann::json target = { { "a", 1 } };

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Removed().size(), 1);
    EXPECT_EQ(diff.Removed()[0].path, "b");
}

TEST(ConfigDiffTest, ModifiedEntry)
{
    nlohmann::json base = { { "a", 1 } };
    nlohmann::json target = { { "a", 2 } };

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Modified().size(), 1);
    EXPECT_EQ(diff.Modified()[0].path, "a");
}

TEST(ConfigDiffTest, DiffEntryTypeString)
{
    DiffEntry added { .type = DiffType::Added, .path = "path", .old_value = "", .new_value = "value" };
    EXPECT_EQ(added.TypeString(), "ADDED");

    DiffEntry removed { .type = DiffType::Removed, .path = "path", .old_value = "value", .new_value = "" };
    EXPECT_EQ(removed.TypeString(), "REMOVED");

    DiffEntry modified { .type = DiffType::Modified, .path = "path", .old_value = "old", .new_value = "new" };
    EXPECT_EQ(modified.TypeString(), "MODIFIED");
}

TEST(ConfigDiffTest, DiffSize)
{
    nlohmann::json base = { { "a", 1 } };
    nlohmann::json target = { { "a", 2 }, { "b", 3 } };

    auto diff = DiffJson(base, target);
    EXPECT_EQ(diff.Size(), 2);  // modified + added
}

TEST(ConfigDiffTest, ToStringNoDifferences)
{
    ConfigDiff diff;
    auto str = diff.ToString();
    EXPECT_EQ(str, "No differences found.\n");
}

TEST(ConfigDiffTest, ToStringWithAdded)
{
    ConfigDiff diff;
    diff.entries.push_back({ DiffType::Added, "new.setting", "", "42" });

    auto str = diff.ToString();
    EXPECT_NE(str.find("ADDED"), std::string::npos);
    EXPECT_NE(str.find("new.setting"), std::string::npos);
    EXPECT_NE(str.find("= 42"), std::string::npos);
}

TEST(ConfigDiffTest, ToStringWithRemoved)
{
    ConfigDiff diff;
    diff.entries.push_back({ DiffType::Removed, "old.setting", "\"value\"", "" });

    auto str = diff.ToString();
    EXPECT_NE(str.find("REMOVED"), std::string::npos);
    EXPECT_NE(str.find("old.setting"), std::string::npos);
    EXPECT_NE(str.find("was:"), std::string::npos);
}

TEST(ConfigDiffTest, ToStringWithModified)
{
    ConfigDiff diff;
    diff.entries.push_back({ DiffType::Modified, "changed.setting", "1", "2" });

    auto str = diff.ToString();
    EXPECT_NE(str.find("MODIFIED"), std::string::npos);
    EXPECT_NE(str.find("changed.setting"), std::string::npos);
    EXPECT_NE(str.find("->"), std::string::npos);
}

TEST(ConfigDiffTest, FilterByType)
{
    ConfigDiff diff;
    diff.entries.push_back({ DiffType::Added, "a", "", "1" });
    diff.entries.push_back({ DiffType::Removed, "b", "2", "" });
    diff.entries.push_back({ DiffType::Added, "c", "", "3" });

    auto added = diff.Filter(DiffType::Added);
    EXPECT_EQ(added.size(), 2);

    auto removed = diff.Filter(DiffType::Removed);
    EXPECT_EQ(removed.size(), 1);
}

TEST(ConfigDiffTest, DiffDefaultsFromFile)
{
    nlohmann::json defaults = { { "a", 1 }, { "b", 2 } };
    nlohmann::json file_values = { { "a", 1 } };

    auto diff = DiffDefaultsFromFile(defaults, file_values);
    // "b" is in defaults but not in file - shows as Added from perspective of file->defaults
    EXPECT_TRUE(diff.HasDifferences());
}

TEST(ConfigDiffTest, DiffFileFromDefaults)
{
    nlohmann::json defaults = { { "a", 1 } };
    nlohmann::json file_values = { { "a", 1 }, { "b", 2 } };

    auto diff = DiffFileFromDefaults(defaults, file_values);
    // "b" is in file but not in defaults
    EXPECT_TRUE(diff.HasDifferences());
}

struct MockAppName {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "MyApp"; }
};

struct MockAppPort {
    static constexpr std::string_view path = "app.port";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
};

using MockSchema = ConfigSchema<MockAppName, MockAppPort>;

TEST(MockConfigurationTest, GetDefault)
{
    testing::MockConfiguration<MockSchema> config;

    // These should return defaults since nothing was set
    EXPECT_EQ(config.Get<MockAppName>(), "MyApp");
    EXPECT_EQ(config.Get<MockAppPort>(), 8080);
}

TEST(MockConfigurationTest, GetAfterSetReturnsSetValue)
{
    testing::MockConfiguration<MockSchema> config;

    // First get default
    EXPECT_EQ(config.Get<MockAppPort>(), 8080);

    // Then set and get
    config.SetValue<MockAppPort>(9000);
    EXPECT_EQ(config.Get<MockAppPort>(), 9000);
}

TEST(MockConfigurationTest, SetValue)
{
    testing::MockConfiguration<MockSchema> config;
    config.SetValue<MockAppPort>(9000);

    EXPECT_EQ(config.Get<MockAppPort>(), 9000);
}

TEST(MockConfigurationTest, Reset)
{
    testing::MockConfiguration<MockSchema> config;
    config.SetValue<MockAppPort>(9000);
    config.Reset();

    EXPECT_EQ(config.Get<MockAppPort>(), 8080);
}

struct MockValidatedPort {
    static constexpr std::string_view path = "app.validated_port";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
    static auto validator() -> Validator<int> { return Range(1, 65535); }
};

using MockSchemaWithValidation = ConfigSchema<MockAppName, MockValidatedPort>;

TEST(MockConfigurationTest, SetWithValidationSuccess)
{
    testing::MockConfiguration<MockSchemaWithValidation> config;

    auto status = config.Set<MockValidatedPort>(9000);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(config.Get<MockValidatedPort>(), 9000);
}

// Test for mock Get returning default when value exists but fails to parse
// This covers line 70 in mock.h
struct MockPointSetting {
    static constexpr std::string_view path = "mock.point";
    using value_type = int;  // We'll store a non-int JSON to trigger parse failure
    static auto default_value() -> int { return 42; }
};

TEST(MockConfigurationTest, SetWithValidationFailure)
{
    testing::MockConfiguration<MockSchemaWithValidation> config;

    auto status = config.Set<MockValidatedPort>(99999);
    EXPECT_FALSE(status.ok());
    EXPECT_TRUE(absl::IsInvalidArgument(status));
    // Value should not be changed
    EXPECT_EQ(config.Get<MockValidatedPort>(), 8080);
}

TEST(MockConfigurationTest, GetReturnsDefaultWhenKeyNotFound)
{
    // Test that Get returns default when the key is not in the values map
    // This covers the fallback path at mock.h line 70
    testing::MockConfiguration<MockSchema> config;

    // Clear the value to simulate key not found
    config.ClearValue(MockAppPort::path);

    // Get should return the default value
    EXPECT_EQ(config.Get<MockAppPort>(), 8080);
}

TEST(MockConfigurationTest, GetReturnsDefaultWhenParseFailsWithInvalidType)
{
    // Test that Get returns default when FromJson fails
    // This covers the parse failure branch at mock.h line 70
    testing::MockConfiguration<MockSchema> config;

    // Inject a string where an int is expected - FromJson will fail
    config.SetRawJson(std::string(MockAppPort::path), "not_an_integer");

    // Get should return the default value since parsing fails
    EXPECT_EQ(config.Get<MockAppPort>(), 8080);
}

TEST(LoggerTest, InfoLog)
{
    // Just verify it doesn't crash - output goes to stdout
    ::testing::internal::CaptureStdout();
    Logger::Info("test info message");
    auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("INFO"), std::string::npos);
    EXPECT_NE(output.find("test info message"), std::string::npos);
}

TEST(LoggerTest, WarnLog)
{
    ::testing::internal::CaptureStderr();
    Logger::Warn("test warn message");
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("WARN"), std::string::npos);
    EXPECT_NE(output.find("test warn message"), std::string::npos);
}

TEST(LoggerTest, ErrorLog)
{
    ::testing::internal::CaptureStderr();
    Logger::Error("test error message");
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("test error message"), std::string::npos);
}

TEST(LoggerTest, LogWithLevel)
{
    ::testing::internal::CaptureStdout();
    Logger::Log(LogLevel::Info, "info via Log");
    auto stdout_output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(stdout_output.find("INFO"), std::string::npos);

    ::testing::internal::CaptureStderr();
    Logger::Log(LogLevel::Warning, "warn via Log");
    auto stderr_warn = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(stderr_warn.find("WARN"), std::string::npos);

    ::testing::internal::CaptureStderr();
    Logger::Log(LogLevel::Error, "error via Log");
    auto stderr_error = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(stderr_error.find("ERROR"), std::string::npos);
}

TEST(LoggerTest, InfoFFormatted)
{
    ::testing::internal::CaptureStdout();
    Logger::InfoF("Value is %d", 42);
    auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("INFO"), std::string::npos);
    EXPECT_NE(output.find("Value is 42"), std::string::npos);
}

TEST(LoggerTest, WarnFFormatted)
{
    ::testing::internal::CaptureStderr();
    Logger::WarnF("Warning: %s", "test");
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("WARN"), std::string::npos);
    EXPECT_NE(output.find("Warning: test"), std::string::npos);
}

TEST(LoggerTest, ErrorFFormatted)
{
    ::testing::internal::CaptureStderr();
    Logger::ErrorF("Error code: %d", 500);
    auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("ERROR"), std::string::npos);
    EXPECT_NE(output.find("Error code: 500"), std::string::npos);
}

TEST(ValidatorTest, MinValidatorDouble)
{
    auto validator = Min(5.0);
    EXPECT_TRUE(validator(5.0));
    EXPECT_TRUE(validator(10.0));
    EXPECT_FALSE(validator(4.9));
}

TEST(ValidatorTest, MaxValidatorDouble)
{
    auto validator = Max(10.0);
    EXPECT_TRUE(validator(10.0));
    EXPECT_TRUE(validator(5.0));
    EXPECT_FALSE(validator(10.1));
}

TEST(ValidatorTest, RangeValidatorDouble)
{
    auto validator = Range(0.0, 1.0);
    EXPECT_TRUE(validator(0.0));
    EXPECT_TRUE(validator(0.5));
    EXPECT_TRUE(validator(1.0));
    EXPECT_FALSE(validator(-0.1));
    EXPECT_FALSE(validator(1.1));
}

TEST(ValidatorTest, PositiveValidatorDouble)
{
    auto validator = Positive<double>();
    EXPECT_TRUE(validator(0.1));
    EXPECT_TRUE(validator(100.0));
    EXPECT_FALSE(validator(0.0));
    EXPECT_FALSE(validator(-0.1));
}

TEST(ValidatorTest, NonNegativeValidatorDouble)
{
    auto validator = NonNegative<double>();
    EXPECT_TRUE(validator(0.0));
    EXPECT_TRUE(validator(100.0));
    EXPECT_FALSE(validator(-0.1));
}

TEST(ValidatorTest, OneOfIntValidator)
{
    auto validator = OneOf<int>({ 1, 2, 3, 5, 8, 13 });
    EXPECT_TRUE(validator(1));
    EXPECT_TRUE(validator(13));
    EXPECT_FALSE(validator(4));
}

TEST(ValidatorTest, PredicateStringValidator)
{
    auto validator = Predicate<std::string>(
        [](const std::string& s) { return !s.empty(); }, "must not be empty");
    auto ok = validator("hello");
    EXPECT_TRUE(ok);
    auto fail = validator("");
    EXPECT_FALSE(fail);
    EXPECT_EQ(fail.error_message, "must not be empty");
}

TEST(ValidatorTest, MinValidatorFloat)
{
    auto validator = Min(1.0f);
    EXPECT_TRUE(validator(1.0f));
    EXPECT_TRUE(validator(2.0f));
    EXPECT_FALSE(validator(0.9f));
}

TEST(ValidatorTest, MaxValidatorFloat)
{
    auto validator = Max(100.0f);
    EXPECT_TRUE(validator(100.0f));
    EXPECT_TRUE(validator(50.0f));
    EXPECT_FALSE(validator(100.1f));
}

TEST(ValidatorTest, NotEmptyFailureMessage)
{
    auto validator = NotEmpty();
    auto result = validator("");
    EXPECT_FALSE(result);
    EXPECT_NE(result.error_message.find("not be empty"), std::string::npos);
}

TEST(ValidatorTest, MaxLengthFailureMessage)
{
    auto validator = MaxLength(3);
    auto result = validator("toolong");
    EXPECT_FALSE(result);
    EXPECT_NE(result.error_message.find("exceeds maximum"), std::string::npos);
}

TEST(ValidatorTest, MinLengthFailureMessage)
{
    auto validator = MinLength(5);
    auto result = validator("ab");
    EXPECT_FALSE(result);
    EXPECT_NE(result.error_message.find("less than minimum"), std::string::npos);
}

TEST(ValidatorTest, OneOfFailureMessage)
{
    auto validator = OneOf<std::string>({ "a", "b" });
    auto result = validator("c");
    EXPECT_FALSE(result);
    EXPECT_NE(result.error_message.find("not in the list"), std::string::npos);
}

TEST(ValidatorTest, AlwaysValidDouble)
{
    auto validator = AlwaysValid<double>();
    EXPECT_TRUE(validator(0.0));
    EXPECT_TRUE(validator(-1000.0));
}

TEST(ValidatorTest, AlwaysValidString)
{
    auto validator = AlwaysValid<std::string>();
    EXPECT_TRUE(validator(""));
    EXPECT_TRUE(validator("anything"));
}

TEST(ConfigTraitsTest, BoolFromStringOnOff)
{
    EXPECT_EQ(ConfigTraits<bool>::FromString("on"), true);
    EXPECT_EQ(ConfigTraits<bool>::FromString("off"), false);
}

TEST(ConfigTraitsTest, BoolToString)
{
    EXPECT_EQ(ConfigTraits<bool>::ToString(true), "true");
    EXPECT_EQ(ConfigTraits<bool>::ToString(false), "false");
}

TEST(ConfigTraitsTest, IntToString)
{
    EXPECT_EQ(ConfigTraits<int>::ToString(42), "42");
    EXPECT_EQ(ConfigTraits<int>::ToString(-1), "-1");
}

TEST(ConfigTraitsTest, Int64ToJson)
{
    EXPECT_EQ(ConfigTraits<std::int64_t>::ToJson(42LL), 42);
}

TEST(ConfigTraitsTest, Int64FromJson)
{
    EXPECT_EQ(ConfigTraits<std::int64_t>::FromJson(nlohmann::json(42)), 42LL);
    EXPECT_EQ(ConfigTraits<std::int64_t>::FromJson(nlohmann::json("invalid")), std::nullopt);
}

TEST(ConfigTraitsTest, Int64ToString)
{
    EXPECT_EQ(ConfigTraits<std::int64_t>::ToString(123456789LL), "123456789");
}

TEST(ConfigTraitsTest, Int64FromString)
{
    EXPECT_EQ(ConfigTraits<std::int64_t>::FromString("123456789"), 123456789LL);
    EXPECT_EQ(ConfigTraits<std::int64_t>::FromString("abc"), std::nullopt);
    EXPECT_EQ(ConfigTraits<std::int64_t>::FromString("123abc"), std::nullopt);
}

TEST(JsonSerializerTest, SetAtPathOverwritesNonObject)
{
    nlohmann::json data = { { "a", 42 } };  // "a" is an int, not an object
    JsonSerializer::SetAtPath(data, "a.b.c", 100);
    EXPECT_EQ(data["a"]["b"]["c"], 100);
}

TEST(JsonSerializerTest, SetAtPathSingleSegment)
{
    nlohmann::json data = nlohmann::json::object();
    JsonSerializer::SetAtPath(data, "key", "value");
    EXPECT_EQ(data["key"], "value");
}

TEST(JsonSerializerTest, MergeDeepRecursive)
{
    nlohmann::json base = { { "a", { { "b", { { "c", 1 } } } } } };
    nlohmann::json overlay = { { "a", { { "b", { { "d", 2 } } } } } };
    auto merged = JsonSerializer::Merge(base, overlay);
    EXPECT_EQ(merged["a"]["b"]["c"], 1);
    EXPECT_EQ(merged["a"]["b"]["d"], 2);
}

TEST(ValidationResultTest, OkResult)
{
    auto result = ValidationResult::Ok();
    EXPECT_TRUE(result.is_valid);
    EXPECT_TRUE(result.error_message.empty());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(ValidationResultTest, ErrorResult)
{
    auto result = ValidationResult::Error("test error");
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.error_message, "test error");
    EXPECT_FALSE(static_cast<bool>(result));
}

TEST(ConfigDiffTest, DiffNestedObjects)
{
    nlohmann::json base = { { "a", { { "b", { { "c", 1 } } } } } };
    nlohmann::json target = { { "a", { { "b", { { "c", 2 } } } } } };

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Modified().size(), 1);
    EXPECT_EQ(diff.Modified()[0].path, "a.b.c");
}

TEST(ConfigDiffTest, DiffAddedToNonObject)
{
    nlohmann::json base = 42;
    nlohmann::json target = { { "a", 1 } };

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Added().size(), 1);
}

TEST(ConfigDiffTest, DiffRemovedFromNonObject)
{
    nlohmann::json base = { { "a", 1 } };
    nlohmann::json target = 42;

    auto diff = DiffJson(base, target);
    EXPECT_TRUE(diff.HasDifferences());
    EXPECT_EQ(diff.Removed().size(), 1);
}

TEST(MockConfigurationTest, LoadAndSaveAreNoOps)
{
    testing::MockConfiguration<MockSchema> config;
    EXPECT_TRUE(config.Load().ok());
    EXPECT_TRUE(config.Save().ok());
}

TEST(MockConfigurationTest, SetRawJsonAndGet)
{
    testing::MockConfiguration<MockSchema> config;
    config.SetRawJson(std::string(MockAppName::path), nlohmann::json("CustomName"));
    EXPECT_EQ(config.Get<MockAppName>(), "CustomName");
}

TEST(MockConfigurationTest, ClearValueAndGetDefault)
{
    testing::MockConfiguration<MockSchema> config;
    config.SetValue<MockAppPort>(9000);
    config.ClearValue(MockAppPort::path);
    EXPECT_EQ(config.Get<MockAppPort>(), 8080);  // default
}

TEST(ConfigurationTestFixtureTest, CreateTempFilePathDefault)
{
    auto path = testing::ConfigurationTestFixture::CreateTempFilePath();
    EXPECT_NE(path.find("/tmp/"), std::string::npos);
    EXPECT_NE(path.find("test_config"), std::string::npos);
    EXPECT_NE(path.find(".json"), std::string::npos);
}

TEST(ConfigurationTestFixtureTest, CreateTempFilePathWithPrefix)
{
    auto path = testing::ConfigurationTestFixture::CreateTempFilePath("my_prefix");
    EXPECT_NE(path.find("/tmp/"), std::string::npos);
    EXPECT_NE(path.find("my_prefix"), std::string::npos);
}

TEST(ConfigurationTestFixtureTest, RemoveFileNonexistent)
{
    // Should not crash for non-existent file
    testing::ConfigurationTestFixture::RemoveFile("/tmp/nonexistent_test_file_12345.json");
}

}  // namespace cppfig::test
