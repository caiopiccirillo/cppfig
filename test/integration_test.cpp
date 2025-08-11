#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

// This test verifies the single-header experience that external users would have
#include "cppfig.h"

namespace integration_test {

// Define a simple configuration enum
enum class UserConfig : uint8_t {
    AppName,
    Version,
    MaxUsers,
    DebugEnabled,
    Timeout
};

} // namespace integration_test

// Declare type mappings
namespace config {
DECLARE_CONFIG_TYPE(integration_test::UserConfig, integration_test::UserConfig::AppName, std::string);
DECLARE_CONFIG_TYPE(integration_test::UserConfig, integration_test::UserConfig::Version, std::string);
DECLARE_CONFIG_TYPE(integration_test::UserConfig, integration_test::UserConfig::MaxUsers, int);
DECLARE_CONFIG_TYPE(integration_test::UserConfig, integration_test::UserConfig::DebugEnabled, bool);
DECLARE_CONFIG_TYPE(integration_test::UserConfig, integration_test::UserConfig::Timeout, double);
} // namespace config

namespace integration_test {

// Implement enum string conversion for JSON serialization
std::string ToString(UserConfig config)
{
    switch (config) {
    case UserConfig::AppName:
        return "app_name";
    case UserConfig::Version:
        return "version";
    case UserConfig::MaxUsers:
        return "max_users";
    case UserConfig::DebugEnabled:
        return "debug_enabled";
    case UserConfig::Timeout:
        return "timeout";
    default:
        return "unknown";
    }
}

UserConfig FromString(const std::string& str)
{
    if (str == "app_name") return UserConfig::AppName;
    if (str == "version") return UserConfig::Version;
    if (str == "max_users") return UserConfig::MaxUsers;
    if (str == "debug_enabled") return UserConfig::DebugEnabled;
    if (str == "timeout") return UserConfig::Timeout;
    throw std::runtime_error("Invalid config name: " + str);
}

} // namespace integration_test

// Specialize JsonSerializer for the enum
namespace config {
template <>
inline std::string JsonSerializer<integration_test::UserConfig, BasicSettingVariant<integration_test::UserConfig>>::ToString(integration_test::UserConfig enumValue)
{
    return integration_test::ToString(enumValue);
}

template <>
inline integration_test::UserConfig JsonSerializer<integration_test::UserConfig, BasicSettingVariant<integration_test::UserConfig>>::FromString(const std::string& str)
{
    return integration_test::FromString(str);
}
} // namespace config

namespace integration_test {

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_config_path_ = std::filesystem::temp_directory_path() / "integration_test_config.json";
        
        // Clean up any existing test file
        if (std::filesystem::exists(test_config_path_)) {
            std::filesystem::remove(test_config_path_);
        }
    }

    void TearDown() override
    {
        // Clean up test file
        if (std::filesystem::exists(test_config_path_)) {
            std::filesystem::remove(test_config_path_);
        }
    }

    std::filesystem::path test_config_path_;
};

// Test the complete single-header user experience
TEST_F(IntegrationTest, SingleHeaderExperience)
{
    // Use the convenience configuration type
    using Config = ::config::BasicJsonConfiguration<UserConfig>;
    
    // Create default configuration using the helper functions
    const Config::DefaultConfigMap defaults = {
        { UserConfig::AppName,
          ::config::ConfigHelpers<UserConfig>::CreateStringSetting<UserConfig::AppName>(
              "TestApp", "Application name") },
        { UserConfig::Version,
          ::config::ConfigHelpers<UserConfig>::CreateStringSetting<UserConfig::Version>(
              "1.0.0", "Application version") },
        { UserConfig::MaxUsers,
          ::config::ConfigHelpers<UserConfig>::CreateIntSetting<UserConfig::MaxUsers>(
              100, 1, 10000, "Maximum number of concurrent users", "users") },
        { UserConfig::DebugEnabled,
          ::config::ConfigHelpers<UserConfig>::CreateBoolSetting<UserConfig::DebugEnabled>(
              false, "Enable debug mode") },
        { UserConfig::Timeout,
          ::config::ConfigHelpers<UserConfig>::CreateDoubleSetting<UserConfig::Timeout>(
              30.0, 1.0, 300.0, "Request timeout", "seconds") }
    };

    // Create configuration instance
    Config config(test_config_path_, defaults);

    // Test type-safe access
    auto app_name_setting = config.GetSetting<UserConfig::AppName>();
    auto version_setting = config.GetSetting<UserConfig::Version>();
    auto max_users_setting = config.GetSetting<UserConfig::MaxUsers>();
    auto debug_setting = config.GetSetting<UserConfig::DebugEnabled>();
    auto timeout_setting = config.GetSetting<UserConfig::Timeout>();

    // Verify automatic type deduction
    auto app_name = app_name_setting.Value(); // std::string
    auto version = version_setting.Value(); // std::string
    auto max_users = max_users_setting.Value(); // int
    auto debug_enabled = debug_setting.Value(); // bool
    auto timeout = timeout_setting.Value(); // double

    // Verify default values
    EXPECT_EQ(app_name, "TestApp");
    EXPECT_EQ(version, "1.0.0");
    EXPECT_EQ(max_users, 100);
    EXPECT_EQ(debug_enabled, false);
    EXPECT_EQ(timeout, 30.0);

    // Test type-safe modification
    app_name_setting.SetValue(std::string("ModifiedApp"));
    max_users_setting.SetValue(500);
    debug_setting.SetValue(true);
    timeout_setting.SetValue(45.5);

    // Verify modifications
    EXPECT_EQ(config.GetSetting<UserConfig::AppName>().Value(), "ModifiedApp");
    EXPECT_EQ(config.GetSetting<UserConfig::MaxUsers>().Value(), 500);
    EXPECT_EQ(config.GetSetting<UserConfig::DebugEnabled>().Value(), true);
    EXPECT_EQ(config.GetSetting<UserConfig::Timeout>().Value(), 45.5);

    // Test validation
    EXPECT_TRUE(config.ValidateAll());

    // Test save/load cycle
    EXPECT_TRUE(config.Save());
    EXPECT_TRUE(std::filesystem::exists(test_config_path_));

    // Create a new config instance and verify it loads the saved values
    Config loaded_config(test_config_path_, defaults);
    EXPECT_EQ(loaded_config.GetSetting<UserConfig::AppName>().Value(), "ModifiedApp");
    EXPECT_EQ(loaded_config.GetSetting<UserConfig::MaxUsers>().Value(), 500);
    EXPECT_EQ(loaded_config.GetSetting<UserConfig::DebugEnabled>().Value(), true);
    EXPECT_EQ(loaded_config.GetSetting<UserConfig::Timeout>().Value(), 45.5);
}

// Test that the API prevents compile-time errors
TEST_F(IntegrationTest, CompileTimeTypeSafety)
{
    using Config = ::config::BasicJsonConfiguration<UserConfig>;
    
    const Config::DefaultConfigMap defaults = {
        { UserConfig::MaxUsers,
          ::config::ConfigHelpers<UserConfig>::CreateIntSetting<UserConfig::MaxUsers>(
              100, 1, 10000, "Maximum users", "users") }
    };

    Config config(test_config_path_, defaults);
    
    // This should compile fine
    auto setting = config.GetSetting<UserConfig::MaxUsers>();
    auto value = setting.Value(); // int
    setting.SetValue(200); // int
    
    // These would cause compile-time errors if uncommented:
    // setting.SetValue("invalid"); // Error: cannot convert string to int
    // setting.SetValue(3.14f);     // Error: cannot convert float to int
    
    EXPECT_EQ(value, 100);
    EXPECT_EQ(setting.Value(), 200);
}

// Test metadata access
TEST_F(IntegrationTest, MetadataAccess)
{
    using Config = ::config::BasicJsonConfiguration<UserConfig>;
    
    const Config::DefaultConfigMap defaults = {
        { UserConfig::MaxUsers,
          ::config::ConfigHelpers<UserConfig>::CreateIntSetting<UserConfig::MaxUsers>(
              100, 1, 10000, "Maximum concurrent users", "users") }
    };

    Config config(test_config_path_, defaults);
    auto setting = config.GetSetting<UserConfig::MaxUsers>();
    
    // Test metadata access
    EXPECT_TRUE(setting.Description().has_value());
    EXPECT_EQ(*setting.Description(), "Maximum concurrent users");
    
    EXPECT_TRUE(setting.Unit().has_value());
    EXPECT_EQ(*setting.Unit(), "users");
    
    EXPECT_TRUE(setting.MinValue().has_value());
    EXPECT_EQ(*setting.MinValue(), 1);
    
    EXPECT_TRUE(setting.MaxValue().has_value());
    EXPECT_EQ(*setting.MaxValue(), 10000);
}

// Test validation
TEST_F(IntegrationTest, ValidationSystem)
{
    using Config = ::config::BasicJsonConfiguration<UserConfig>;
    
    const Config::DefaultConfigMap defaults = {
        { UserConfig::MaxUsers,
          ::config::ConfigHelpers<UserConfig>::CreateIntSetting<UserConfig::MaxUsers>(
              100, 1, 1000, "Maximum users", "users") }
    };

    Config config(test_config_path_, defaults);
    auto setting = config.GetSetting<UserConfig::MaxUsers>();
    
    // Valid values should pass
    setting.SetValue(500);
    EXPECT_TRUE(setting.IsValid());
    EXPECT_TRUE(config.ValidateAll());
    
    // Values outside range should fail
    setting.SetValue(2000); // Above max
    EXPECT_FALSE(setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());
    
    std::string error = setting.GetValidationError();
    EXPECT_FALSE(error.empty());
    EXPECT_TRUE(error.find("above maximum") != std::string::npos);
}

} // namespace integration_test