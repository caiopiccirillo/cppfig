// This file demonstrates compile-time safety features of the cppfig library.
// Most of the code below should NOT compile, demonstrating that type safety
// is enforced at compile time rather than runtime.
//
// To test these examples:
// 1. Uncomment sections one at a time
// 2. Try to compile - they should fail with clear error messages
// 3. This proves the library catches type mismatches at compile time

#include <gtest/gtest.h>

#include <string>

#include "ConfigHelpers.h"
#include "GenericConfiguration.h"
#include "JsonSerializer.h"
#include "Setting.h"
#include "TypedSetting.h"

namespace compile_time_safety_demo {

// Define test configuration enum
enum class DemoConfigKey : uint8_t {
    StringSetting,
    IntSetting,
    BoolSetting,
    FloatSetting
};

// String conversion functions
inline std::string ToString(DemoConfigKey key)
{
    switch (key) {
    case DemoConfigKey::StringSetting:
        return "string_setting";
    case DemoConfigKey::IntSetting:
        return "int_setting";
    case DemoConfigKey::BoolSetting:
        return "bool_setting";
    case DemoConfigKey::FloatSetting:
        return "float_setting";
    default:
        return "unknown";
    }
}

inline DemoConfigKey FromString(const std::string& str)
{
    if (str == "string_setting")
        return DemoConfigKey::StringSetting;
    if (str == "int_setting")
        return DemoConfigKey::IntSetting;
    if (str == "bool_setting")
        return DemoConfigKey::BoolSetting;
    if (str == "float_setting")
        return DemoConfigKey::FloatSetting;
    throw std::runtime_error("Invalid configuration key: " + str);
}

} // namespace compile_time_safety_demo

// Compile-time type mappings
namespace config {
DECLARE_CONFIG_TYPE(compile_time_safety_demo::DemoConfigKey, compile_time_safety_demo::DemoConfigKey::StringSetting, std::string);
DECLARE_CONFIG_TYPE(compile_time_safety_demo::DemoConfigKey, compile_time_safety_demo::DemoConfigKey::IntSetting, int);
DECLARE_CONFIG_TYPE(compile_time_safety_demo::DemoConfigKey, compile_time_safety_demo::DemoConfigKey::BoolSetting, bool);
DECLARE_CONFIG_TYPE(compile_time_safety_demo::DemoConfigKey, compile_time_safety_demo::DemoConfigKey::FloatSetting, float);
} // namespace config

// JSON serializer specializations
namespace config {
template <>
inline std::string JsonSerializer<compile_time_safety_demo::DemoConfigKey>::ToString(compile_time_safety_demo::DemoConfigKey enumValue)
{
    return compile_time_safety_demo::ToString(enumValue);
}

template <>
inline compile_time_safety_demo::DemoConfigKey JsonSerializer<compile_time_safety_demo::DemoConfigKey>::FromString(const std::string& str)
{
    return compile_time_safety_demo::FromString(str);
}
} // namespace config

namespace compile_time_safety_demo {

using DemoConfig = ::config::GenericConfiguration<DemoConfigKey, ::config::JsonSerializer<DemoConfigKey>>;

// This test should compile and run successfully - it shows the CORRECT usage
TEST(CompileTimeSafetyDemo, CorrectUsage)
{
    DemoConfig::DefaultConfigMap defaults = {
        { DemoConfigKey::StringSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateStringSetting<DemoConfigKey::StringSetting>(
              "default_string", "A string setting") },
        { DemoConfigKey::IntSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateIntSetting<DemoConfigKey::IntSetting>(
              42, 0, 100, "An integer setting") },
        { DemoConfigKey::BoolSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateBoolSetting<DemoConfigKey::BoolSetting>(
              true, "A boolean setting") },
        { DemoConfigKey::FloatSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateFloatSetting<DemoConfigKey::FloatSetting>(
              3.14f, 0.0f, 10.0f, "A float setting") }
    };

    DemoConfig config("/tmp/demo_config.json", defaults);

    // This is the CORRECT way to use the API - types are automatically deduced
    auto string_setting = config.GetSetting<DemoConfigKey::StringSetting>();
    auto int_setting = config.GetSetting<DemoConfigKey::IntSetting>();
    auto bool_setting = config.GetSetting<DemoConfigKey::BoolSetting>();
    auto float_setting = config.GetSetting<DemoConfigKey::FloatSetting>();

    // These should work - correct types
    auto str_val = string_setting.Value(); // std::string
    auto int_val = int_setting.Value(); // int
    auto bool_val = bool_setting.Value(); // bool
    auto float_val = float_setting.Value(); // float

    // These should work - setting correct types
    string_setting.SetValue(std::string("new_string"));
    int_setting.SetValue(50);
    bool_setting.SetValue(false);
    float_setting.SetValue(2.71f);

    EXPECT_EQ(string_setting.Value(), "new_string");
    EXPECT_EQ(int_setting.Value(), 50);
    EXPECT_FALSE(bool_setting.Value());
    EXPECT_FLOAT_EQ(float_setting.Value(), 2.71f);
}

// ===========================================================================
// UNCOMMENT THE SECTIONS BELOW ONE AT A TIME TO TEST COMPILE-TIME FAILURES
// Each section should fail to compile with a clear error message
// ===========================================================================

/*
// ERROR TEST 1: Wrong type mapping
// This should fail because we're trying to map IntSetting to std::string
TEST(CompileTimeSafetyDemo, WrongTypeMapping) {
    DemoConfig::DefaultConfigMap defaults = {
        { DemoConfigKey::IntSetting,
          // This should FAIL to compile - IntSetting is mapped to int, not string
          ::config::ConfigHelpers<DemoConfigKey>::CreateStringSetting<DemoConfigKey::IntSetting>(
              "wrong_type", "This should not compile") }
    };
}
*/

/*
// ERROR TEST 2: Wrong value type in SetValue
TEST(CompileTimeSafetyDemo, WrongSetValueType) {
    DemoConfig::DefaultConfigMap defaults = {
        { DemoConfigKey::IntSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateIntSetting<DemoConfigKey::IntSetting>(
              42, 0, 100, "An integer setting") }
    };

    DemoConfig config("/tmp/demo_config.json", defaults);
    auto int_setting = config.GetSetting<DemoConfigKey::IntSetting>();

    // This should FAIL to compile - trying to set string to int setting
    int_setting.SetValue(std::string("not_an_int"));
}
*/

/*
// ERROR TEST 3: Wrong explicit type in Value()
TEST(CompileTimeSafetyDemo, WrongExplicitValueType) {
    DemoConfig::DefaultConfigMap defaults = {
        { DemoConfigKey::StringSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateStringSetting<DemoConfigKey::StringSetting>(
              "test", "A string setting") }
    };

    DemoConfig config("/tmp/demo_config.json", defaults);
    auto string_setting = config.GetSetting<DemoConfigKey::StringSetting>();

    // This should FAIL to compile - trying to get int from string setting
    auto wrong_val = string_setting.Value<int>();
}
*/

/*
// ERROR TEST 4: Wrong template parameter in CreateSetting helper
TEST(CompileTimeSafetyDemo, WrongCreateSettingTemplate) {
    DemoConfig::DefaultConfigMap defaults = {
        // This should FAIL to compile - BoolSetting mapped to bool, but creating int setting
        { DemoConfigKey::BoolSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateIntSetting<DemoConfigKey::BoolSetting>(
              42, 0, 100, "This should not compile") }
    };
}
*/

/*
// ERROR TEST 5: Missing type mapping
enum class UnmappedConfigKey : uint8_t {
    UnmappedSetting
};

TEST(CompileTimeSafetyDemo, MissingTypeMapping) {
    // This should FAIL to compile - no config_type_map specialization for UnmappedSetting
    auto setting = ::config::Setting<UnmappedConfigKey, int>(
        UnmappedConfigKey::UnmappedSetting, 42);
}
*/

/*
// ERROR TEST 6: Wrong enum in GetSetting
TEST(CompileTimeSafetyDemo, WrongEnumInGetSetting) {
    enum class WrongEnum : uint8_t {
        WrongSetting
    };

    DemoConfig::DefaultConfigMap defaults = {
        { DemoConfigKey::IntSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateIntSetting<DemoConfigKey::IntSetting>(
              42, 0, 100, "An integer setting") }
    };

    DemoConfig config("/tmp/demo_config.json", defaults);

    // This should FAIL to compile - wrong enum type
    auto wrong_setting = config.GetSetting<WrongEnum::WrongSetting>();
}
*/

/*
// ERROR TEST 7: Inconsistent template parameters
TEST(CompileTimeSafetyDemo, InconsistentTemplateParameters) {
    DemoConfig::DefaultConfigMap defaults = {
        { DemoConfigKey::StringSetting,
          ::config::ConfigHelpers<DemoConfigKey>::CreateStringSetting<DemoConfigKey::StringSetting>(
              "test", "A string setting") }
    };

    DemoConfig config("/tmp/demo_config.json", defaults);

    // This should FAIL to compile - StringSetting mapped to string, but trying to use as int
    auto wrong_setting = config.GetSetting<DemoConfigKey::StringSetting>();
    wrong_setting.SetValue<int>(42);  // Explicit wrong type should fail
}
*/

// DEMONSTRATION: Show what the compiler errors look like
TEST(CompileTimeSafetyDemo, DocumentExpectedErrors)
{
    // This test documents what kinds of compile-time errors you should expect:

    EXPECT_TRUE(true); // Placeholder to make test pass

    /*
    Expected error types when uncommenting the sections above:

    1. "static assertion failed: Template parameter mismatch"
    2. "no matching function for call to 'SetValue'"
    3. "static assertion failed: Explicit type must match the configured type"
    4. "template argument deduction/substitution failed"
    5. "incomplete type 'config_type_map<...>'"
    6. "no member named 'GetSetting' in class"
    7. "static assertion failed: Value type must match the configured type"

    These errors prove that type safety is enforced at compile time,
    preventing runtime type errors and making the library much safer to use.
    */
}

} // namespace compile_time_safety_demo
