#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>

#include "cppfig.h"

namespace cppfig_unit_test {

// Define comprehensive test configuration enum
enum class ConfigKey : uint8_t {
    DatabaseUrl,
    MaxConnections,
    EnableLogging,
    LogLevel,
    ApiTimeout,
    ServerPort,
    DebugMode,
    CacheSize,
    RetryCount,
    CompressionRatio
};

// Implement string conversion functions for JSON serialization
inline std::string ToString(ConfigKey key)
{
    switch (key) {
    case ConfigKey::DatabaseUrl:
        return "database_url";
    case ConfigKey::MaxConnections:
        return "max_connections";
    case ConfigKey::EnableLogging:
        return "enable_logging";
    case ConfigKey::LogLevel:
        return "log_level";
    case ConfigKey::ApiTimeout:
        return "api_timeout";
    case ConfigKey::ServerPort:
        return "server_port";
    case ConfigKey::DebugMode:
        return "debug_mode";
    case ConfigKey::CacheSize:
        return "cache_size";
    case ConfigKey::RetryCount:
        return "retry_count";
    case ConfigKey::CompressionRatio:
        return "compression_ratio";
    default:
        return "unknown";
    }
}

inline ConfigKey FromString(const std::string& str)
{
    if (str == "database_url")
        return ConfigKey::DatabaseUrl;
    if (str == "max_connections")
        return ConfigKey::MaxConnections;
    if (str == "enable_logging")
        return ConfigKey::EnableLogging;
    if (str == "log_level")
        return ConfigKey::LogLevel;
    if (str == "api_timeout")
        return ConfigKey::ApiTimeout;
    if (str == "server_port")
        return ConfigKey::ServerPort;
    if (str == "debug_mode")
        return ConfigKey::DebugMode;
    if (str == "cache_size")
        return ConfigKey::CacheSize;
    if (str == "retry_count")
        return ConfigKey::RetryCount;
    if (str == "compression_ratio")
        return ConfigKey::CompressionRatio;
    throw std::runtime_error("Invalid configuration key: " + str);
}

} // namespace cppfig_unit_test

// Declare compile-time type mappings for strong type safety
namespace config {
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::DatabaseUrl, std::string);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::MaxConnections, int);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::EnableLogging, bool);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::LogLevel, std::string);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::ApiTimeout, double);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::ServerPort, int);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::DebugMode, bool);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::CacheSize, int);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::RetryCount, int);
DECLARE_CONFIG_TYPE(cppfig_unit_test::ConfigKey, cppfig_unit_test::ConfigKey::CompressionRatio, float);
} // namespace config

// Template specializations for JSON serializer
namespace config {
template <>
inline std::string JsonSerializer<cppfig_unit_test::ConfigKey, BasicSettingVariant<cppfig_unit_test::ConfigKey>>::ToString(cppfig_unit_test::ConfigKey enumValue)
{
    return cppfig_unit_test::ToString(enumValue);
}

template <>
inline cppfig_unit_test::ConfigKey JsonSerializer<cppfig_unit_test::ConfigKey, BasicSettingVariant<cppfig_unit_test::ConfigKey>>::FromString(const std::string& str)
{
    return cppfig_unit_test::FromString(str);
}
} // namespace config

namespace cppfig_unit_test {

using TestConfig = config::BasicJsonConfiguration<ConfigKey>;

class CppfigUnitTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        test_config_path_ = std::filesystem::temp_directory_path() / "cppfig_unit_test.json";

        if (std::filesystem::exists(test_config_path_)) {
            std::filesystem::remove(test_config_path_);
        }
    }

    void TearDown() override
    {
        if (std::filesystem::exists(test_config_path_)) {
            std::filesystem::remove(test_config_path_);
        }
    }

    TestConfig::DefaultConfigMap CreateDefaultConfig()
    {
        return {
            { ConfigKey::DatabaseUrl,
              ::config::ConfigHelpers<ConfigKey>::CreateStringSetting<ConfigKey::DatabaseUrl>(
                  "postgresql://localhost:5432/app", "Database connection URL") },
            { ConfigKey::MaxConnections,
              ::config::ConfigHelpers<ConfigKey>::CreateIntSetting<ConfigKey::MaxConnections>(
                  100, 1, 1000, "Maximum database connections", "connections") },
            { ConfigKey::EnableLogging,
              ::config::ConfigHelpers<ConfigKey>::CreateBoolSetting<ConfigKey::EnableLogging>(
                  true, "Enable application logging") },
            { ConfigKey::LogLevel,
              ::config::ConfigHelpers<ConfigKey>::CreateStringSetting<ConfigKey::LogLevel>(
                  "info", "Logging level (debug, info, warn, error)") },
            { ConfigKey::ApiTimeout,
              ::config::ConfigHelpers<ConfigKey>::CreateDoubleSetting<ConfigKey::ApiTimeout>(
                  30.0, 0.1, 300.0, "API request timeout", "seconds") },
            { ConfigKey::ServerPort,
              ::config::ConfigHelpers<ConfigKey>::CreateIntSetting<ConfigKey::ServerPort>(
                  8080, 1024, 65535, "Server port number", "port") },
            { ConfigKey::DebugMode,
              ::config::ConfigHelpers<ConfigKey>::CreateBoolSetting<ConfigKey::DebugMode>(
                  false, "Enable debug mode") },
            { ConfigKey::CacheSize,
              ::config::ConfigHelpers<ConfigKey>::CreateIntSetting<ConfigKey::CacheSize>(
                  1024, 16, 65536, "Cache size", "MB") },
            { ConfigKey::RetryCount,
              ::config::ConfigHelpers<ConfigKey>::CreateIntSetting<ConfigKey::RetryCount>(
                  3, 0, 10, "Number of retry attempts", "attempts") },
            { ConfigKey::CompressionRatio,
              ::config::ConfigHelpers<ConfigKey>::CreateFloatSetting<ConfigKey::CompressionRatio>(
                  0.8f, 0.1f, 1.0f, "Compression ratio", "ratio") }
        };
    }

    std::filesystem::path test_config_path_;
};

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

TEST_F(CppfigUnitTest, BasicConfigurationCreation)
{
    auto defaults = CreateDefaultConfig();
    EXPECT_NO_THROW(TestConfig config(test_config_path_, defaults));
}

TEST_F(CppfigUnitTest, DefaultValueAccess)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // Test access to default values
    auto db_url_setting = config.template GetSetting<ConfigKey::DatabaseUrl>();
    auto max_conn_setting = config.template GetSetting<ConfigKey::MaxConnections>();
    auto logging_setting = config.template GetSetting<ConfigKey::EnableLogging>();
    auto timeout_setting = config.template GetSetting<ConfigKey::ApiTimeout>();

    auto db_url = db_url_setting.Value();
    auto max_conn = max_conn_setting.Value();
    auto logging = logging_setting.Value();
    auto timeout = timeout_setting.Value();
    auto compression_setting = config.template GetSetting<ConfigKey::CompressionRatio>();
    auto compression = compression_setting.Value();

    EXPECT_EQ(db_url, "postgresql://localhost:5432/app");
    EXPECT_EQ(max_conn, 100);
    EXPECT_TRUE(logging);
    EXPECT_DOUBLE_EQ(timeout, 30.0);
    EXPECT_FLOAT_EQ(compression, 0.8f);
}

// ============================================================================
// ERGONOMIC API TESTS
// ============================================================================

TEST_F(CppfigUnitTest, ErgonomicAPIBasics)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // Test ergonomic API - types automatically deduced
    auto db_setting = config.template GetSetting<ConfigKey::DatabaseUrl>();
    auto connections_setting = config.template GetSetting<ConfigKey::MaxConnections>();
    auto logging_setting = config.template GetSetting<ConfigKey::EnableLogging>();
    auto timeout_setting = config.template GetSetting<ConfigKey::ApiTimeout>();
    auto compression_setting = config.GetSetting<ConfigKey::CompressionRatio>();

    // Values should be correctly typed and accessible
    auto db_url = db_setting.Value(); // std::string
    auto max_conn = connections_setting.Value(); // int
    auto log_enabled = logging_setting.Value(); // bool
    auto timeout = timeout_setting.Value(); // double
    auto compression = compression_setting.Value(); // float

    EXPECT_EQ(db_url, "postgresql://localhost:5432/app");
    EXPECT_EQ(max_conn, 100);
    EXPECT_TRUE(log_enabled);
    EXPECT_DOUBLE_EQ(timeout, 30.0);
    EXPECT_FLOAT_EQ(compression, 0.8f);
}

TEST_F(CppfigUnitTest, ErgonomicAPITypeInformation)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    auto db_setting = config.GetSetting<ConfigKey::DatabaseUrl>();
    auto connections_setting = config.GetSetting<ConfigKey::MaxConnections>();
    auto logging_setting = config.GetSetting<ConfigKey::EnableLogging>();
    auto timeout_setting = config.GetSetting<ConfigKey::ApiTimeout>();
    auto compression_setting = config.GetSetting<ConfigKey::CompressionRatio>();

    // Test type information
    EXPECT_EQ(db_setting.GetTypeName(), "string");
    EXPECT_EQ(connections_setting.GetTypeName(), "int");
    EXPECT_EQ(logging_setting.GetTypeName(), "bool");
    EXPECT_EQ(timeout_setting.GetTypeName(), "double");
    EXPECT_EQ(compression_setting.GetTypeName(), "float");
}

TEST_F(CppfigUnitTest, ErgonomicAPIValueModification)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // Test setting values with ergonomic API
    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto debug_setting = config.GetSetting<ConfigKey::DebugMode>();
    auto timeout_setting = config.GetSetting<ConfigKey::ApiTimeout>();

    // Modify values using clean API
    port_setting.SetValue(9090);
    debug_setting.SetValue(true);
    timeout_setting.SetValue(45.5);

    // Verify changes
    EXPECT_EQ(port_setting.Value(), 9090);
    EXPECT_TRUE(debug_setting.Value());
    EXPECT_DOUBLE_EQ(timeout_setting.Value(), 45.5);

    // Test modification tracking
    EXPECT_TRUE(config.IsModified<ConfigKey::ServerPort>());
    EXPECT_TRUE(config.IsModified<ConfigKey::DebugMode>());
    EXPECT_TRUE(config.IsModified<ConfigKey::ApiTimeout>());
    EXPECT_FALSE(config.IsModified<ConfigKey::DatabaseUrl>());
}

// ============================================================================
// METADATA TESTS
// ============================================================================

TEST_F(CppfigUnitTest, MetadataAccess)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    auto cache_setting = config.GetSetting<ConfigKey::CacheSize>();
    auto retry_setting = config.GetSetting<ConfigKey::RetryCount>();
    auto timeout_setting = config.GetSetting<ConfigKey::ApiTimeout>();

    // Test metadata access
    EXPECT_TRUE(cache_setting.HasDescription());
    EXPECT_EQ(cache_setting.Description().value(), "Cache size");
    EXPECT_TRUE(cache_setting.HasUnit());
    EXPECT_EQ(cache_setting.Unit().value(), "MB");
    EXPECT_TRUE(cache_setting.HasMinValue());
    EXPECT_TRUE(cache_setting.HasMaxValue());
    EXPECT_EQ(cache_setting.MinValue().value(), 16);
    EXPECT_EQ(cache_setting.MaxValue().value(), 65536);

    EXPECT_TRUE(retry_setting.HasDescription());
    EXPECT_EQ(retry_setting.Description().value(), "Number of retry attempts");
    EXPECT_TRUE(retry_setting.HasUnit());
    EXPECT_EQ(retry_setting.Unit().value(), "attempts");

    EXPECT_TRUE(timeout_setting.HasDescription());
    EXPECT_EQ(timeout_setting.Description().value(), "API request timeout");
    EXPECT_TRUE(timeout_setting.HasUnit());
    EXPECT_EQ(timeout_setting.Unit().value(), "seconds");
}

TEST_F(CppfigUnitTest, ToStringFunctionality)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto debug_setting = config.GetSetting<ConfigKey::DebugMode>();
    auto timeout_setting = config.GetSetting<ConfigKey::ApiTimeout>();

    // Test ToString method
    auto port_str = port_setting.ToString();
    auto debug_str = debug_setting.ToString();
    auto timeout_str = timeout_setting.ToString();

    EXPECT_TRUE(port_str.find("int setting") != std::string::npos);
    EXPECT_TRUE(port_str.find("8080") != std::string::npos);
    EXPECT_TRUE(port_str.find("port") != std::string::npos);

    EXPECT_TRUE(debug_str.find("bool setting") != std::string::npos);
    EXPECT_TRUE(debug_str.find("false") != std::string::npos);

    EXPECT_TRUE(timeout_str.find("double setting") != std::string::npos);
    EXPECT_TRUE(timeout_str.find("30") != std::string::npos);
    EXPECT_TRUE(timeout_str.find("seconds") != std::string::npos);
}

// ============================================================================
// VALIDATION TESTS
// ============================================================================

TEST_F(CppfigUnitTest, ValidationSuccess)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // All default values should be valid
    EXPECT_TRUE(config.ValidateAll());

    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto connections_setting = config.GetSetting<ConfigKey::MaxConnections>();
    auto compression_setting = config.GetSetting<ConfigKey::CompressionRatio>();

    // Test valid values within constraints
    port_setting.SetValue(8443); // Valid port
    EXPECT_TRUE(port_setting.IsValid());

    connections_setting.SetValue(500); // Valid connection count
    EXPECT_TRUE(connections_setting.IsValid());

    compression_setting.SetValue(0.5f); // Valid compression ratio
    EXPECT_TRUE(compression_setting.IsValid());

    EXPECT_TRUE(config.ValidateAll());
}

TEST_F(CppfigUnitTest, ValidationFailures)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto connections_setting = config.GetSetting<ConfigKey::MaxConnections>();
    auto compression_setting = config.GetSetting<ConfigKey::CompressionRatio>();
    auto retry_setting = config.GetSetting<ConfigKey::RetryCount>();

    // Test values outside valid ranges
    port_setting.SetValue(100); // Below minimum (1024)
    EXPECT_FALSE(port_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    port_setting.SetValue(70000); // Above maximum (65535)
    EXPECT_FALSE(port_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    // Reset to valid value
    port_setting.SetValue(8080);
    EXPECT_TRUE(port_setting.IsValid());

    // Test other constraints
    connections_setting.SetValue(0); // Below minimum (1)
    EXPECT_FALSE(connections_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    connections_setting.SetValue(2000); // Above maximum (1000)
    EXPECT_FALSE(connections_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    // Reset to valid value
    connections_setting.SetValue(100);
    EXPECT_TRUE(connections_setting.IsValid());

    // Test float constraints
    compression_setting.SetValue(0.05f); // Below minimum (0.1)
    EXPECT_FALSE(compression_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    compression_setting.SetValue(1.5f); // Above maximum (1.0)
    EXPECT_FALSE(compression_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    // Reset to valid value
    compression_setting.SetValue(0.8f);
    EXPECT_TRUE(compression_setting.IsValid());

    // Test retry count
    retry_setting.SetValue(-1); // Below minimum (0)
    EXPECT_FALSE(retry_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    retry_setting.SetValue(15); // Above maximum (10)
    EXPECT_FALSE(retry_setting.IsValid());
    EXPECT_FALSE(config.ValidateAll());

    // Reset to valid value
    retry_setting.SetValue(3);
    EXPECT_TRUE(retry_setting.IsValid());
    EXPECT_TRUE(config.ValidateAll());
}

TEST_F(CppfigUnitTest, ValidationErrorMessages)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto connections_setting = config.GetSetting<ConfigKey::MaxConnections>();

    // Set invalid values and check error messages
    port_setting.SetValue(100);
    EXPECT_FALSE(port_setting.IsValid());
    auto error_msg = port_setting.GetValidationError();
    EXPECT_TRUE(error_msg.find("minimum") != std::string::npos || error_msg.find("range") != std::string::npos);

    connections_setting.SetValue(2000);
    EXPECT_FALSE(connections_setting.IsValid());
    error_msg = connections_setting.GetValidationError();
    EXPECT_TRUE(error_msg.find("maximum") != std::string::npos || error_msg.find("range") != std::string::npos);
}

TEST_F(CppfigUnitTest, BoundaryValues)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto connections_setting = config.GetSetting<ConfigKey::MaxConnections>();
    auto compression_setting = config.GetSetting<ConfigKey::CompressionRatio>();
    auto retry_setting = config.GetSetting<ConfigKey::RetryCount>();

    // Test exact boundary values - should be valid
    port_setting.SetValue(1024); // Minimum
    EXPECT_TRUE(port_setting.IsValid());

    port_setting.SetValue(65535); // Maximum
    EXPECT_TRUE(port_setting.IsValid());

    connections_setting.SetValue(1); // Minimum
    EXPECT_TRUE(connections_setting.IsValid());

    connections_setting.SetValue(1000); // Maximum
    EXPECT_TRUE(connections_setting.IsValid());

    compression_setting.SetValue(0.1f); // Minimum
    EXPECT_TRUE(compression_setting.IsValid());

    compression_setting.SetValue(1.0f); // Maximum
    EXPECT_TRUE(compression_setting.IsValid());

    retry_setting.SetValue(0); // Minimum
    EXPECT_TRUE(retry_setting.IsValid());

    retry_setting.SetValue(10); // Maximum
    EXPECT_TRUE(retry_setting.IsValid());

    EXPECT_TRUE(config.ValidateAll());
}

// ============================================================================
// SERIALIZATION TESTS
// ============================================================================

TEST_F(CppfigUnitTest, SerializationRoundTrip)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config1(test_config_path_, defaults);

    // Modify various settings
    auto db_setting = config1.GetSetting<ConfigKey::DatabaseUrl>();
    auto port_setting = config1.GetSetting<ConfigKey::ServerPort>();
    auto debug_setting = config1.GetSetting<ConfigKey::DebugMode>();
    auto timeout_setting = config1.GetSetting<ConfigKey::ApiTimeout>();
    auto compression_setting = config1.GetSetting<ConfigKey::CompressionRatio>();

    db_setting.SetValue(std::string("mysql://localhost:3306/myapp"));
    port_setting.SetValue(443);
    debug_setting.SetValue(true);
    timeout_setting.SetValue(60.0);
    compression_setting.SetValue(0.9f);

    // Save and verify file exists
    EXPECT_TRUE(config1.Save());
    EXPECT_TRUE(std::filesystem::exists(test_config_path_));

    // Load into new configuration
    TestConfig config2(test_config_path_, defaults);

    // Verify all values are preserved
    auto db2 = config2.GetSetting<ConfigKey::DatabaseUrl>();
    auto port2 = config2.GetSetting<ConfigKey::ServerPort>();
    auto debug2 = config2.GetSetting<ConfigKey::DebugMode>();
    auto timeout2 = config2.GetSetting<ConfigKey::ApiTimeout>();
    auto compression2 = config2.GetSetting<ConfigKey::CompressionRatio>();

    EXPECT_EQ(db2.Value(), "mysql://localhost:3306/myapp");
    EXPECT_EQ(port2.Value(), 443);
    EXPECT_TRUE(debug2.Value());
    EXPECT_DOUBLE_EQ(timeout2.Value(), 60.0);
    EXPECT_FLOAT_EQ(compression2.Value(), 0.9f);
}

// ============================================================================
// MODIFICATION TRACKING TESTS
// ============================================================================

TEST_F(CppfigUnitTest, ModificationTracking)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // Initially, nothing should be modified
    EXPECT_FALSE(config.IsModified<ConfigKey::ServerPort>());
    EXPECT_FALSE(config.IsModified<ConfigKey::ServerPort>());

    // Modify a setting using clean API
    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    port_setting.SetValue(9090);
    EXPECT_TRUE(config.IsModified<ConfigKey::ServerPort>());
    EXPECT_FALSE(config.IsModified<ConfigKey::DatabaseUrl>());

    // Reset all to defaults
    config.ResetAllToDefaults();
    EXPECT_FALSE(config.IsModified<ConfigKey::ServerPort>());
    EXPECT_FALSE(config.IsModified<ConfigKey::DatabaseUrl>());
}

TEST_F(CppfigUnitTest, ResetFunctionality)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // Modify a setting using clean API
    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    port_setting.SetValue(9090);
    auto modified_port = port_setting.Value();
    EXPECT_EQ(modified_port, 9090);
    EXPECT_TRUE(config.IsModified<ConfigKey::ServerPort>());

    // Reset to default
    config.ResetToDefault<ConfigKey::ServerPort>();
    auto reset_port = port_setting.Value();
    EXPECT_EQ(reset_port, 8080);
    EXPECT_FALSE(config.IsModified<ConfigKey::ServerPort>());
}

TEST_F(CppfigUnitTest, ResetAfterInvalidValues)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();

    // Set invalid value
    port_setting.SetValue(100);
    EXPECT_FALSE(port_setting.IsValid());
    EXPECT_TRUE(config.IsModified<ConfigKey::ServerPort>());

    // Reset to default
    config.ResetToDefault<ConfigKey::ServerPort>();
    EXPECT_TRUE(port_setting.IsValid());
    EXPECT_FALSE(config.IsModified<ConfigKey::ServerPort>());
    EXPECT_EQ(port_setting.Value(), 8080);
}

// ============================================================================
// COMPILE-TIME SAFETY TESTS
// ============================================================================

TEST_F(CppfigUnitTest, CompileTimeSafetyVerification)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // These should compile successfully - types are automatically deduced
    auto db_setting = config.GetSetting<ConfigKey::DatabaseUrl>();
    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto debug_setting = config.GetSetting<ConfigKey::DebugMode>();
    auto timeout_setting = config.GetSetting<ConfigKey::ApiTimeout>();
    auto compression_setting = config.GetSetting<ConfigKey::CompressionRatio>();

    // Verify that we get the correct types
    static_assert(std::is_same_v<decltype(db_setting.Value()), const std::string&>);
    static_assert(std::is_same_v<decltype(port_setting.Value()), const int&>);
    static_assert(std::is_same_v<decltype(debug_setting.Value()), const bool&>);
    static_assert(std::is_same_v<decltype(timeout_setting.Value()), const double&>);
    static_assert(std::is_same_v<decltype(compression_setting.Value()), const float&>);

    // Verify enum values are correct
    static_assert(db_setting.GetEnumValue() == ConfigKey::DatabaseUrl);
    static_assert(port_setting.GetEnumValue() == ConfigKey::ServerPort);
    static_assert(debug_setting.GetEnumValue() == ConfigKey::DebugMode);
    static_assert(timeout_setting.GetEnumValue() == ConfigKey::ApiTimeout);
    static_assert(compression_setting.GetEnumValue() == ConfigKey::CompressionRatio);

    // Actual test - just verify we can access values
    EXPECT_NO_THROW(db_setting.Value());
    EXPECT_NO_THROW(port_setting.Value());
    EXPECT_NO_THROW(debug_setting.Value());
    EXPECT_NO_THROW(timeout_setting.Value());
    EXPECT_NO_THROW(compression_setting.Value());
}

// ============================================================================
// LEGACY API COMPATIBILITY TESTS
// ============================================================================

TEST_F(CppfigUnitTest, LegacyAPICompatibility)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // Test legacy GetValue/SetValue API still works
    auto db_url = config.GetValue<ConfigKey::DatabaseUrl, std::string>();
    auto max_conn = config.GetValue<ConfigKey::MaxConnections, int>();
    auto logging = config.GetValue<ConfigKey::EnableLogging, bool>();

    EXPECT_EQ(db_url, "postgresql://localhost:5432/app");
    EXPECT_EQ(max_conn, 100);
    EXPECT_TRUE(logging);

    // Test legacy SetValue
    config.SetValue<ConfigKey::ServerPort, int>(9090);
    auto port = config.GetValue<ConfigKey::ServerPort, int>();
    EXPECT_EQ(port, 9090);
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

TEST_F(CppfigUnitTest, FileHandlingErrors)
{
    auto defaults = CreateDefaultConfig();

    // Test with invalid file path
    std::filesystem::path invalid_path = "/invalid/path/config.json";
    EXPECT_NO_THROW(TestConfig config(invalid_path, defaults));

    TestConfig config(invalid_path, defaults);
    // Save should fail gracefully
    EXPECT_FALSE(config.Save());
}

TEST_F(CppfigUnitTest, ValidationErrorCollection)
{
    auto defaults = CreateDefaultConfig();
    TestConfig config(test_config_path_, defaults);

    // Set multiple invalid values
    auto port_setting = config.GetSetting<ConfigKey::ServerPort>();
    auto connections_setting = config.GetSetting<ConfigKey::MaxConnections>();

    port_setting.SetValue(100); // Invalid
    connections_setting.SetValue(2000); // Invalid

    EXPECT_FALSE(config.ValidateAll());

    auto errors = config.GetValidationErrors();
    EXPECT_GT(errors.size(), 0);
}

} // namespace cppfig_unit_test
