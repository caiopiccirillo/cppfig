#include <cppfig/cppfig.h>
#include <cppfig/json.h>
#include <cppfig/testing/mock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace cppfig::test {

namespace settings {

    struct AppName {
        static constexpr std::string_view path = "app.name";
        using value_type = std::string;
        static auto default_value() -> std::string { return "TestApp"; }
    };

    struct AppPort {
        static constexpr std::string_view path = "app.port";
        using value_type = int;
        static auto default_value() -> int { return 8080; }
    };

    struct AppVersion {
        static constexpr std::string_view path = "app.version";
        using value_type = std::string;
        static auto default_value() -> std::string { return "1.0.0"; }
    };

    struct ServerPort {
        static constexpr std::string_view path = "server.port";
        using value_type = int;
        static auto default_value() -> int { return 8080; }
        static auto validator() -> Validator<int> { return Range(1, 65535); }
    };

    struct AppHost {
        static constexpr std::string_view path = "app.host";
        static constexpr std::string_view env_override = "TEST_APP_HOST";
        using value_type = std::string;
        static auto default_value() -> std::string { return "localhost"; }
    };

    struct DatabaseHost {
        static constexpr std::string_view path = "database.connection.host";
        using value_type = std::string;
        static auto default_value() -> std::string { return "localhost"; }
    };

    struct DatabasePort {
        static constexpr std::string_view path = "database.connection.port";
        using value_type = int;
        static auto default_value() -> int { return 5432; }
    };

    struct DatabasePoolSize {
        static constexpr std::string_view path = "database.pool.max_size";
        using value_type = int;
        static auto default_value() -> int { return 10; }
    };

    struct LoggingLevel {
        static constexpr std::string_view path = "logging.level";
        using value_type = std::string;
        static auto default_value() -> std::string { return "info"; }
    };

    // Setting for testing environment variable parse failure
    struct PortWithEnv {
        static constexpr std::string_view path = "server.port";
        static constexpr std::string_view env_override = "TEST_SERVER_PORT";
        using value_type = int;
        static auto default_value() -> int { return 8080; }
    };

    // Bool setting with environment variable override
    struct DebugMode {
        static constexpr std::string_view path = "app.debug";
        static constexpr std::string_view env_override = "TEST_DEBUG_MODE";
        using value_type = bool;
        static auto default_value() -> bool { return false; }
    };

    // Double setting with environment variable override
    struct Ratio {
        static constexpr std::string_view path = "app.ratio";
        static constexpr std::string_view env_override = "TEST_APP_RATIO";
        using value_type = double;
        static auto default_value() -> double { return 1.0; }
    };

    // Float setting with environment variable override
    struct Scale {
        static constexpr std::string_view path = "app.scale";
        static constexpr std::string_view env_override = "TEST_APP_SCALE";
        using value_type = float;
        static auto default_value() -> float { return 1.0f; }
    };

    // Int64 setting with environment variable override
    struct BigNumber {
        static constexpr std::string_view path = "app.big_number";
        static constexpr std::string_view env_override = "TEST_BIG_NUMBER";
        using value_type = std::int64_t;
        static auto default_value() -> std::int64_t { return 0; }
    };

}  // namespace settings

class ConfigurationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override { file_path_ = testing::ConfigurationTestFixture::CreateTempFilePath("integration_test"); }

    void TearDown() override { testing::ConfigurationTestFixture::RemoveFile(file_path_); }

    std::string file_path_;
};

TEST_F(ConfigurationIntegrationTest, CreateFileWithDefaults)
{
    using Schema = ConfigSchema<settings::AppName, settings::AppPort>;
    Configuration<Schema, JsonSerializer> config(file_path_);

    auto status = config.Load();
    ASSERT_TRUE(status.ok()) << status.message();
    EXPECT_TRUE(std::filesystem::exists(file_path_));

    // Verify file contents
    std::ifstream file(file_path_);
    nlohmann::json json;
    file >> json;

    EXPECT_EQ(json["app"]["name"], "TestApp");
    EXPECT_EQ(json["app"]["port"], 8080);
}

TEST_F(ConfigurationIntegrationTest, LoadExistingFile)
{
    // Create a config file first
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "CustomApp", "port": 9000}})";
    }

    using Schema = ConfigSchema<settings::AppName, settings::AppPort>;
    Configuration<Schema, JsonSerializer> config(file_path_);

    auto status = config.Load();
    ASSERT_TRUE(status.ok()) << status.message();

    EXPECT_EQ(config.Get<settings::AppName>(), "CustomApp");
    EXPECT_EQ(config.Get<settings::AppPort>(), 9000);
}

TEST_F(ConfigurationIntegrationTest, SchemaMigration)
{
    // Create a config file with old schema (missing new setting)
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "OldApp"}})";
    }

    // New schema has an additional setting
    using Schema = ConfigSchema<settings::AppName, settings::AppVersion>;
    Configuration<Schema, JsonSerializer> config(file_path_);

    auto status = config.Load();
    ASSERT_TRUE(status.ok()) << status.message();

    // Old setting preserved
    EXPECT_EQ(config.Get<settings::AppName>(), "OldApp");

    // New setting added with default
    EXPECT_EQ(config.Get<settings::AppVersion>(), "1.0.0");

    // Verify file was updated
    std::ifstream file(file_path_);
    nlohmann::json json;
    file >> json;

    EXPECT_EQ(json["app"]["version"], "1.0.0");
}

TEST_F(ConfigurationIntegrationTest, SetAndSave)
{
    using Schema = ConfigSchema<settings::ServerPort>;
    Configuration<Schema> config(file_path_);

    ASSERT_TRUE(config.Load().ok());

    // Set a new value
    auto set_status = config.Set<settings::ServerPort>(9000);
    ASSERT_TRUE(set_status.ok()) << set_status.message();

    // Save
    auto save_status = config.Save();
    ASSERT_TRUE(save_status.ok()) << save_status.message();

    // Reload and verify
    Configuration<Schema> config2(file_path_);
    ASSERT_TRUE(config2.Load().ok());

    EXPECT_EQ(config2.Get<settings::ServerPort>(), 9000);
}

TEST_F(ConfigurationIntegrationTest, ValidationRejectsInvalidValue)
{
    using Schema = ConfigSchema<settings::ServerPort>;
    Configuration<Schema> config(file_path_);

    ASSERT_TRUE(config.Load().ok());

    // Try to set an invalid value
    auto status = config.Set<settings::ServerPort>(99999);
    EXPECT_FALSE(status.ok());
    EXPECT_TRUE(cppfig::IsInvalidArgument(status));
}

TEST_F(ConfigurationIntegrationTest, EnvironmentVariableOverride)
{
    using Schema = ConfigSchema<settings::AppHost>;

    // Set environment variable
    setenv("TEST_APP_HOST", "example.com", 1);

    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // Environment variable should override file/default
    EXPECT_EQ(config.Get<settings::AppHost>(), "example.com");

    // Cleanup
    unsetenv("TEST_APP_HOST");
}

TEST_F(ConfigurationIntegrationTest, DiffShowsModifications)
{
    using Schema = ConfigSchema<settings::AppName, settings::AppPort>;
    Configuration<Schema> config(file_path_);

    ASSERT_TRUE(config.Load().ok());

    // Modify a setting
    ASSERT_TRUE(config.Set<settings::AppPort>(9000).ok());

    auto diff = config.Diff();
    EXPECT_TRUE(diff.HasDifferences());

    auto modified = diff.Modified();
    EXPECT_EQ(modified.size(), 1);
    EXPECT_EQ(modified[0].path, "app.port");
}

TEST_F(ConfigurationIntegrationTest, ValidateAll)
{
    // Create file with invalid value
    {
        std::ofstream file(file_path_);
        file << R"({"server": {"port": 99999}})";
    }

    using Schema = ConfigSchema<settings::ServerPort>;
    Configuration<Schema, JsonSerializer> config(file_path_);

    ASSERT_TRUE(config.Load().ok());

    auto status = config.ValidateAll();
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, HierarchicalSettings)
{
    using Schema = ConfigSchema<settings::DatabaseHost, settings::DatabasePort,
                                settings::DatabasePoolSize, settings::LoggingLevel>;
    Configuration<Schema, JsonSerializer> config(file_path_);

    ASSERT_TRUE(config.Load().ok());

    // Verify hierarchical structure in file
    std::ifstream file(file_path_);
    nlohmann::json json;
    file >> json;

    EXPECT_EQ(json["database"]["connection"]["host"], "localhost");
    EXPECT_EQ(json["database"]["connection"]["port"], 5432);
    EXPECT_EQ(json["database"]["pool"]["max_size"], 10);
    EXPECT_EQ(json["logging"]["level"], "info");
}

TEST_F(ConfigurationIntegrationTest, VirtualInterfaceWorks)
{
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(file_path_);

    // Use through virtual interface
    IConfigurationProviderVirtual& virtual_config = config;

    ASSERT_TRUE(virtual_config.Load().ok());
    EXPECT_EQ(virtual_config.GetFilePath(), file_path_);
    EXPECT_TRUE(virtual_config.ValidateAll().ok());
}

struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point& other) const { return x == other.x && y == other.y; }

    friend void to_json(nlohmann::json& j, const Point& p) { j = nlohmann::json { { "x", p.x }, { "y", p.y } }; }

    friend void from_json(const nlohmann::json& j, Point& p)
    {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
    }
};

}  // namespace cppfig::test

// Specialize ConfigTraits for Point using the ADL helper
template <>
struct cppfig::ConfigTraits<cppfig::test::Point> : cppfig::ConfigTraitsFromJsonAdl<cppfig::test::Point> { };

namespace cppfig::test {

namespace custom_settings {

    struct Origin {
        static constexpr std::string_view path = "origin";
        using value_type = Point;
        static auto default_value() -> Point { return Point { .x = 0, .y = 0 }; }
    };

    struct Target {
        static constexpr std::string_view path = "target";
        using value_type = Point;
        static auto default_value() -> Point { return Point { .x = 100, .y = 100 }; }
    };

}  // namespace custom_settings

TEST_F(ConfigurationIntegrationTest, CustomTypeInConfig)
{
    using Schema = ConfigSchema<custom_settings::Origin, custom_settings::Target>;
    Configuration<Schema, JsonSerializer> config(file_path_);

    ASSERT_TRUE(config.Load().ok());

    EXPECT_EQ(config.Get<custom_settings::Origin>(), (Point { 0, 0 }));
    EXPECT_EQ(config.Get<custom_settings::Target>(), (Point { 100, 100 }));

    // Verify JSON structure
    std::ifstream file(file_path_);
    nlohmann::json json;
    file >> json;

    EXPECT_EQ(json["origin"]["x"], 0);
    EXPECT_EQ(json["origin"]["y"], 0);
    EXPECT_EQ(json["target"]["x"], 100);
    EXPECT_EQ(json["target"]["y"], 100);
}

TEST_F(ConfigurationIntegrationTest, InvalidJsonFile)
{
    // Create an invalid JSON file
    {
        std::ofstream file(file_path_);
        file << "this is not valid json {{{";
    }

    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema, JsonSerializer> config(file_path_);

    auto status = config.Load();
    EXPECT_FALSE(status.ok());
    EXPECT_TRUE(cppfig::IsInvalidArgument(status));
}

TEST_F(ConfigurationIntegrationTest, EnvironmentVariableParseFailure)
{
    // Test when environment variable cannot be parsed

    // Create config file first
    {
        std::ofstream file(file_path_);
        file << R"({"server": {"port": 8080}})";
    }

    // Set env var to invalid value for int parsing
    setenv("TEST_SERVER_PORT", "not_a_number", 1);

    using EnvSchema = ConfigSchema<settings::PortWithEnv>;
    Configuration<EnvSchema, JsonSerializer> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // Should fall back to file value since env var parse failed
    // (Logger will warn about it)
    ::testing::internal::CaptureStderr();
    int port = config.Get<settings::PortWithEnv>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    // Verify warning was logged
    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("TEST_SERVER_PORT"), std::string::npos);

    // Should get file value
    EXPECT_EQ(port, 8080);

    unsetenv("TEST_SERVER_PORT");
}

TEST_F(ConfigurationIntegrationTest, FileValueParseFailure)
{
    // Create config with wrong type in JSON
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"port": "not_an_int"}})";
    }

    using Schema = ConfigSchema<settings::AppPort>;
    Configuration<Schema, JsonSerializer> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // Should fall back to default since file value can't be parsed
    ::testing::internal::CaptureStderr();
    int port = config.Get<settings::AppPort>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    // Verify warning was logged
    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);

    // Should get default value
    EXPECT_EQ(port, 8080);
}

TEST_F(ConfigurationIntegrationTest, GetDiffString)
{
    using Schema = ConfigSchema<settings::AppName, settings::AppPort>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // Modify a setting
    ASSERT_TRUE(config.Set<settings::AppPort>(9000).ok());

    // Get diff string via virtual interface
    IConfigurationProviderVirtual& virtual_config = config;
    auto diff_str = virtual_config.GetDiffString();

    EXPECT_NE(diff_str.find("app.port"), std::string::npos);
    EXPECT_NE(diff_str.find("MODIFIED"), std::string::npos);
}

TEST_F(ConfigurationIntegrationTest, GetFileValues)
{
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    const auto& file_values = config.GetFileValues();
    EXPECT_EQ(file_values["app"]["name"], "TestApp");
}

TEST_F(ConfigurationIntegrationTest, GetDefaults)
{
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    const auto& defaults = config.GetDefaults();
    EXPECT_EQ(defaults["app"]["name"], "TestApp");
}

TEST_F(ConfigurationIntegrationTest, SaveCreatesParentDirectories)
{
    std::string nested_path = file_path_ + "_nested/subdir/config.json";

    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(nested_path);
    ASSERT_TRUE(config.Load().ok());

    // Verify nested directories were created
    EXPECT_TRUE(std::filesystem::exists(nested_path));

    // Cleanup
    std::filesystem::remove_all(file_path_ + "_nested");
}

// Test custom type FromString path (ConfigTraitsFromJsonAdl)
TEST_F(ConfigurationIntegrationTest, CustomTypeToAndFromString)
{
    // Test the ToString and FromString methods on ConfigTraitsFromJsonAdl
    Point p { .x=10, .y=20 };

    auto str = ConfigTraits<Point>::ToString(p);
    EXPECT_NE(str.find("10"), std::string::npos);
    EXPECT_NE(str.find("20"), std::string::npos);

    auto parsed = ConfigTraits<Point>::FromString(str);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->x, 10);
    EXPECT_EQ(parsed->y, 20);
}

TEST_F(ConfigurationIntegrationTest, CustomTypeFromStringInvalid)
{
    auto parsed = ConfigTraits<Point>::FromString("not valid json");
    EXPECT_FALSE(parsed.has_value());
}

TEST_F(ConfigurationIntegrationTest, CustomTypeFromJsonInvalid)
{
    // JSON that doesn't match Point structure
    Value invalid = 42;
    auto parsed = ConfigTraits<Point>::Deserialize(invalid);
    EXPECT_FALSE(parsed.has_value());
}

TEST_F(ConfigurationIntegrationTest, ReadFileNotFound)
{
    // Test ReadFile with non-existent file
    auto result = ReadFile<JsonSerializer>("/nonexistent/path/to/file.json");
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(cppfig::IsNotFound(result.status()));
}

TEST_F(ConfigurationIntegrationTest, WriteFileToInvalidPath)
{
    // Test WriteFile to a path that cannot be opened (directory doesn't exist and path is invalid)
    auto data = Value::Object();
    data["key"] = Value("value");
    auto status = WriteFile<JsonSerializer>("/nonexistent_root_dir/cannot/write/here.json", data);
    EXPECT_FALSE(status.ok());
    EXPECT_TRUE(cppfig::IsInternal(status));
}

TEST_F(ConfigurationIntegrationTest, EnvironmentVariableSuccessfulParse)
{
    // Test when environment variable IS successfully parsed (covers line 96 return path)
    setenv("TEST_APP_HOST", "env-host.example.com", 1);

    using Schema = ConfigSchema<settings::AppHost>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // Environment variable should be successfully parsed and returned
    std::string host = config.Get<settings::AppHost>();
    EXPECT_EQ(host, "env-host.example.com");

    unsetenv("TEST_APP_HOST");
}

TEST_F(ConfigurationIntegrationTest, FileValueSuccessfulParse)
{
    // Create config with valid value in JSON (covers line 107 return path)
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "FileApp"}})";
    }

    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema, JsonSerializer> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // File value should be successfully parsed and returned
    std::string name = config.Get<settings::AppName>();
    EXPECT_EQ(name, "FileApp");
}

TEST_F(ConfigurationIntegrationTest, SchemaMigrationAddsMultipleSettings)
{
    // Create a config file with only one setting (covers migration loop lines 164-166)
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "OldApp"}})";
    }

    // New schema has multiple additional settings
    using Schema = ConfigSchema<settings::AppName, settings::AppPort, settings::AppVersion>;

    ::testing::internal::CaptureStderr();
    Configuration<Schema, JsonSerializer> config(file_path_);
    auto status = config.Load();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    ASSERT_TRUE(status.ok()) << status.message();

    // Verify migration warnings were logged for each new setting
    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("app.port"), std::string::npos);
    EXPECT_NE(stderr_output.find("app.version"), std::string::npos);

    // Verify all settings are now available
    EXPECT_EQ(config.Get<settings::AppName>(), "OldApp");
    EXPECT_EQ(config.Get<settings::AppPort>(), 8080);
    EXPECT_EQ(config.Get<settings::AppVersion>(), "1.0.0");
}

// Alias for thread-safe configuration with the same schemas used above.
using MTSchemaBasic = ConfigSchema<settings::AppName, settings::AppPort>;
using MTConfig = Configuration<MTSchemaBasic, JsonSerializer, MultiThreadedPolicy>;

using MTSchemaWithVersion = ConfigSchema<settings::AppName, settings::AppPort, settings::AppVersion>;
using MTConfigWithVersion = Configuration<MTSchemaWithVersion, JsonSerializer, MultiThreadedPolicy>;

using MTSchemaWithValidator = ConfigSchema<settings::ServerPort>;
using MTConfigValidated = Configuration<MTSchemaWithValidator, JsonSerializer, MultiThreadedPolicy>;

using MTSchemaWithEnv = ConfigSchema<settings::AppHost>;
using MTConfigEnv = Configuration<MTSchemaWithEnv, JsonSerializer, MultiThreadedPolicy>;

using MTSchemaPortEnv = ConfigSchema<settings::PortWithEnv>;
using MTConfigPortEnv = Configuration<MTSchemaPortEnv, JsonSerializer, MultiThreadedPolicy>;

TEST_F(ConfigurationIntegrationTest, MultiThreadedCreateFileWithDefaults)
{
    MTConfig config(file_path_);

    auto status = config.Load();
    ASSERT_TRUE(status.ok()) << status.message();
    EXPECT_TRUE(std::filesystem::exists(file_path_));

    std::ifstream file(file_path_);
    nlohmann::json json;
    file >> json;
    EXPECT_EQ(json["app"]["name"], "TestApp");
    EXPECT_EQ(json["app"]["port"], 8080);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedLoadExistingFile)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "Loaded", "port": 9090}})";
    }

    MTConfig config(file_path_);
    auto status = config.Load();
    ASSERT_TRUE(status.ok()) << status.message();

    EXPECT_EQ(config.Get<settings::AppName>(), "Loaded");
    EXPECT_EQ(config.Get<settings::AppPort>(), 9090);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedSetWithValidation)
{
    MTConfigValidated config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // Valid value
    auto status = config.Set<settings::ServerPort>(443);
    ASSERT_TRUE(status.ok());
    EXPECT_EQ(config.Get<settings::ServerPort>(), 443);

    // Invalid value — validation failure
    status = config.Set<settings::ServerPort>(0);
    EXPECT_FALSE(status.ok());

    // Value must remain unchanged after rejected Set
    EXPECT_EQ(config.Get<settings::ServerPort>(), 443);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedSetAndSave)
{
    MTConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ASSERT_TRUE(config.Set<settings::AppPort>(9999).ok());
    ASSERT_TRUE(config.Save().ok());

    // Reload in a new instance
    MTConfig config2(file_path_);
    ASSERT_TRUE(config2.Load().ok());
    EXPECT_EQ(config2.Get<settings::AppPort>(), 9999);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedDiff)
{
    MTConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ASSERT_TRUE(config.Set<settings::AppPort>(3000).ok());

    auto diff = config.Diff();
    EXPECT_TRUE(diff.HasDifferences());

    auto modified = diff.Modified();
    EXPECT_FALSE(modified.empty());
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedValidateAll)
{
    MTConfigValidated config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    auto status = config.ValidateAll();
    EXPECT_TRUE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedGetFilePath)
{
    MTConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    EXPECT_EQ(config.GetFilePath(), file_path_);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedGetFileValues)
{
    MTConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    const auto& fv = config.GetFileValues();
    EXPECT_EQ(fv["app"]["name"], "TestApp");
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedGetDefaults)
{
    MTConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    const auto& defaults = config.GetDefaults();
    EXPECT_EQ(defaults["app"]["port"], 8080);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedGetDiffString)
{
    MTConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    ASSERT_TRUE(config.Set<settings::AppPort>(1234).ok());

    IConfigurationProviderVirtual& vconfig = config;
    auto diff_str = vconfig.GetDiffString();
    EXPECT_NE(diff_str.find("app.port"), std::string::npos);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedEnvVarSuccessfulParse)
{
    setenv("TEST_APP_HOST", "mt-host.example.com", 1);

    MTConfigEnv config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    std::string host = config.Get<settings::AppHost>();
    EXPECT_EQ(host, "mt-host.example.com");

    unsetenv("TEST_APP_HOST");
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedEnvVarParseFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"server": {"port": 8080}})";
    }

    setenv("TEST_SERVER_PORT", "bad_value", 1);

    MTConfigPortEnv config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    int port = config.Get<settings::PortWithEnv>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("TEST_SERVER_PORT"), std::string::npos);
    EXPECT_EQ(port, 8080);

    unsetenv("TEST_SERVER_PORT");
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedFileValueParseFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"port": "not_an_int"}})";
    }

    using Schema = ConfigSchema<settings::AppPort>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    int port = config.Get<settings::AppPort>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_EQ(port, 8080);  // falls back to default
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedDefaultValueFallback)
{
    // Write a file that does NOT contain "app.port"
    {
        std::ofstream file(file_path_);
        file << R"({"other": {"key": "value"}})";
    }

    using Schema = ConfigSchema<settings::AppPort>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // app.port will be added by schema migration, but let's test the fallback
    // by reading a setting whose path doesn't exist at all in the JSON.
    // Schema migration adds it, so let's use a setting not in the schema
    // Actually, schema migration adds the setting. So instead, confirm
    // that the default value was written during migration.
    int port = config.Get<settings::AppPort>();
    EXPECT_EQ(port, 8080);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedLoadInvalidJsonFile)
{
    {
        std::ofstream file(file_path_);
        file << "this is not valid json {{{";
    }

    MTConfig config(file_path_);
    auto status = config.Load();
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedSchemaMigration)
{
    // Create a file with only one setting
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "OldApp"}})";
    }

    ::testing::internal::CaptureStderr();
    MTConfigWithVersion config(file_path_);
    auto status = config.Load();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    ASSERT_TRUE(status.ok()) << status.message();

    // Migration should have added the missing settings
    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("app.port"), std::string::npos);
    EXPECT_NE(stderr_output.find("app.version"), std::string::npos);

    EXPECT_EQ(config.Get<settings::AppName>(), "OldApp");
    EXPECT_EQ(config.Get<settings::AppPort>(), 8080);
    EXPECT_EQ(config.Get<settings::AppVersion>(), "1.0.0");
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedSchemaMigrationSaveFailure)
{
    // Create a valid file with a subset of settings
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "OldApp"}})";
    }

    // Make the file read-only so Save fails during migration
    std::filesystem::permissions(file_path_,
                                 std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);

    ::testing::internal::CaptureStderr();
    MTConfigWithVersion config(file_path_);
    auto status = config.Load();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    // Migration save should fail because the file is read-only
    EXPECT_FALSE(status.ok());
    EXPECT_NE(stderr_output.find("Failed to save migrated configuration"), std::string::npos);

    // Cleanup: restore permissions so TearDown can remove the file
    std::filesystem::permissions(file_path_, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedSaveDirectoryCreationFailure)
{
    // Use /proc/... which can't have subdirectories created on Linux
    std::string bad_path = "/proc/fakedir/subdir/config.json";

    MTConfig config(bad_path);
    // Don't load — just build defaults and try to save directly
    auto status = config.Save();
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, MultiThreadedValidateAllWithInvalidValue)
{
    // Create a file with an out-of-range port
    {
        std::ofstream file(file_path_);
        file << R"({"server": {"port": 99999}})";
    }

    MTConfigValidated config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    auto status = config.ValidateAll();
    EXPECT_FALSE(status.ok());
    EXPECT_NE(std::string(status.message()).find("server.port"), std::string::npos);
}

struct ValidatedA {
    static constexpr std::string_view path = "val.a";
    using value_type = int;
    static auto default_value() -> int { return 10; }
    static auto validator() -> Validator<int> { return Range(1, 100); }
};

struct ValidatedB {
    static constexpr std::string_view path = "val.b";
    using value_type = int;
    static auto default_value() -> int { return 20; }
    static auto validator() -> Validator<int> { return Range(1, 100); }
};

TEST_F(ConfigurationIntegrationTest, ValidateAllStopsOnFirstError)
{
    using Schema2V = ConfigSchema<ValidatedA, ValidatedB>;

    // Write a file where BOTH values are invalid
    {
        std::ofstream file(file_path_);
        file << R"({"val": {"a": 999, "b": 0}})";
    }

    Configuration<Schema2V, JsonSerializer> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    auto status = config.ValidateAll();
    EXPECT_FALSE(status.ok());
    // Should report the first error (val.a)
    EXPECT_NE(std::string(status.message()).find("val.a"), std::string::npos);
}

TEST_F(ConfigurationIntegrationTest, SingleThreadedSaveCreatesDeepNestedDirectories)
{
    std::string nested = file_path_ + "_deep/a/b/c/config.json";

    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(nested);
    ASSERT_TRUE(config.Load().ok());
    EXPECT_TRUE(std::filesystem::exists(nested));

    std::filesystem::remove_all(file_path_ + "_deep");
}

TEST_F(ConfigurationIntegrationTest, SingleThreadedLoadInvalidJsonFile)
{
    {
        std::ofstream file(file_path_);
        file << "NOT JSON!!!";
    }

    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema, JsonSerializer> config(file_path_);
    auto status = config.Load();
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, SingleThreadedValidateAllInvalidValue)
{
    {
        std::ofstream file(file_path_);
        file << R"({"server": {"port": -5}})";
    }

    using Schema = ConfigSchema<settings::ServerPort>;
    Configuration<Schema, JsonSerializer> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    auto status = config.ValidateAll();
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, SingleThreadedSchemaMigrationSaveFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "OldApp"}})";
    }

    // Make the file read-only so Save fails during migration
    std::filesystem::permissions(file_path_,
                                 std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);

    using Schema = ConfigSchema<settings::AppName, settings::AppPort>;
    ::testing::internal::CaptureStderr();
    Configuration<Schema, JsonSerializer> config(file_path_);
    auto status = config.Load();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_FALSE(status.ok());
    EXPECT_NE(stderr_output.find("Failed to save migrated configuration"), std::string::npos);

    // Cleanup: restore permissions so TearDown can remove the file
    std::filesystem::permissions(file_path_, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
}

TEST_F(ConfigurationIntegrationTest, SingleThreadedSaveDirectoryCreationFailure)
{
    std::string bad_path = "/proc/fakedir/subdir/config.json";
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(bad_path);
    auto status = config.Save();
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, ConfigTraitsFromJsonAdlToJson)
{
    Point p { .x=5, .y=15 };
    auto val = ConfigTraits<Point>::Serialize(p);
    EXPECT_EQ(val["x"], 5);
    EXPECT_EQ(val["y"], 15);
}

TEST_F(ConfigurationIntegrationTest, WriteFilePostWriteFailure)
{
    // /dev/full simulates a disk-full condition on Linux.
    // With a small payload, the buffered stream absorbs the write and only
    // fails on flush/close.  A payload larger than the stream buffer
    // (~8 KiB) triggers a real write() syscall which fails immediately,
    // setting the failbit that line 156 in serializer.h checks.
    if (!std::filesystem::exists("/dev/full")) {
        GTEST_SKIP() << "/dev/full not available";
    }

    // Build a JSON object large enough to exceed the ofstream buffer
    auto data = Value::Object();
    for (int i = 0; i < 200; ++i) {
        data["key_" + std::to_string(i)] = Value(std::string(500, 'X'));
    }
    auto status = WriteFile<JsonSerializer>("/dev/full", data);
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, PredicateValidatorStringFailure)
{
    auto validator = Predicate<std::string>(
        [](const std::string& s) { return s.size() <= 3; }, "too long");

    auto ok_result = validator("ab");
    EXPECT_TRUE(ok_result);

    auto fail_result = validator("toolong");
    EXPECT_FALSE(fail_result);
    EXPECT_EQ(fail_result.error_message, "too long");
}

TEST_F(ConfigurationIntegrationTest, OrCombinatorBothFail)
{
    auto v = Min(10).Or(Max(-10));
    auto result = v(5);  // 5 < 10 fails Min, 5 > -10 fails Max
    EXPECT_FALSE(result);
}

TEST_F(ConfigurationIntegrationTest, OrCombinatorSecondPasses)
{
    auto v = Min(10).Or(Max(3));
    auto result = v(2);  // 2 < 10 fails Min, 2 <= 3 passes Max
    EXPECT_TRUE(result);
}

TEST_F(ConfigurationIntegrationTest, Int64Traits)
{
    std::int64_t val = 1234567890123LL;
    auto serialized = ConfigTraits<std::int64_t>::Serialize(val);
    auto parsed = ConfigTraits<std::int64_t>::Deserialize(serialized);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, val);

    auto str = ConfigTraits<std::int64_t>::ToString(val);
    auto from_str = ConfigTraits<std::int64_t>::FromString(str);
    ASSERT_TRUE(from_str.has_value());
    EXPECT_EQ(*from_str, val);

    // Invalid string
    EXPECT_FALSE(ConfigTraits<std::int64_t>::FromString("not_a_number").has_value());

    // Partial parse (trailing chars)
    EXPECT_FALSE(ConfigTraits<std::int64_t>::FromString("123abc").has_value());

    // Wrong JSON type
    Value wrong("string");
    EXPECT_FALSE(ConfigTraits<std::int64_t>::Deserialize(wrong).has_value());
}

struct OrphanSetting {
    static constexpr std::string_view path = "orphan.value";
    using value_type = std::string;
    static auto default_value() -> std::string { return "fallback"; }
};

TEST_F(ConfigurationIntegrationTest, SingleThreadedDefaultFallbackNoFileKey)
{
    {
        std::ofstream file(file_path_);
        // Intentionally empty object — Orphan's path is missing
        file << R"({})";
    }

    using Schema = ConfigSchema<OrphanSetting>;
    Configuration<Schema, JsonSerializer> config(file_path_);
    // Load will trigger migration and add Orphan to the file.
    // But before migration: the setting is not in the file.
    // After migration: it IS in the file with the default.
    ASSERT_TRUE(config.Load().ok());

    // After migration the value should equal the default
    EXPECT_EQ(config.Get<OrphanSetting>(), "fallback");
}

TEST_F(ConfigurationIntegrationTest, SaveToFileWithNoParentPath)
{
    // A bare filename with no directory component
    std::string bare = "bare_config_test_temp.json";

    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(bare);
    ASSERT_TRUE(config.Load().ok());
    ASSERT_TRUE(config.Save().ok());

    EXPECT_TRUE(std::filesystem::exists(bare));
    std::filesystem::remove(bare);
}

TEST_F(ConfigurationIntegrationTest, BoolEnvVarParseFailure)
{
    setenv("TEST_DEBUG_MODE", "not_a_bool", 1);

    using Schema = ConfigSchema<settings::DebugMode>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    bool debug = config.Get<settings::DebugMode>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("TEST_DEBUG_MODE"), std::string::npos);
    EXPECT_FALSE(debug);  // falls back to default

    unsetenv("TEST_DEBUG_MODE");
}

TEST_F(ConfigurationIntegrationTest, DoubleEnvVarParseFailure)
{
    setenv("TEST_APP_RATIO", "not_a_number", 1);

    using Schema = ConfigSchema<settings::Ratio>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    double ratio = config.Get<settings::Ratio>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("TEST_APP_RATIO"), std::string::npos);
    EXPECT_DOUBLE_EQ(ratio, 1.0);  // falls back to default

    unsetenv("TEST_APP_RATIO");
}

TEST_F(ConfigurationIntegrationTest, FloatEnvVarParseFailure)
{
    setenv("TEST_APP_SCALE", "not_a_number", 1);

    using Schema = ConfigSchema<settings::Scale>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    float scale = config.Get<settings::Scale>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("TEST_APP_SCALE"), std::string::npos);
    EXPECT_FLOAT_EQ(scale, 1.0f);  // falls back to default

    unsetenv("TEST_APP_SCALE");
}

TEST_F(ConfigurationIntegrationTest, Int64EnvVarParseFailure)
{
    setenv("TEST_BIG_NUMBER", "not_a_number", 1);

    using Schema = ConfigSchema<settings::BigNumber>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    std::int64_t big = config.Get<settings::BigNumber>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("TEST_BIG_NUMBER"), std::string::npos);
    EXPECT_EQ(big, 0);  // falls back to default

    unsetenv("TEST_BIG_NUMBER");
}

TEST_F(ConfigurationIntegrationTest, BoolFileValueParseFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"debug": "not_a_bool"}})";
    }

    using Schema = ConfigSchema<settings::DebugMode>;
    Configuration<Schema, JsonSerializer> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    bool debug = config.Get<settings::DebugMode>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_FALSE(debug);  // falls back to default
}

TEST_F(ConfigurationIntegrationTest, DoubleFileValueParseFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"ratio": "not_a_number"}})";
    }

    using Schema = ConfigSchema<settings::Ratio>;
    Configuration<Schema, JsonSerializer> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    double ratio = config.Get<settings::Ratio>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_DOUBLE_EQ(ratio, 1.0);  // falls back to default
}

TEST_F(ConfigurationIntegrationTest, BoolEnvVarSuccessfulParse)
{
    setenv("TEST_DEBUG_MODE", "true", 1);

    using Schema = ConfigSchema<settings::DebugMode>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    bool debug = config.Get<settings::DebugMode>();
    EXPECT_TRUE(debug);

    unsetenv("TEST_DEBUG_MODE");
}

TEST_F(ConfigurationIntegrationTest, DoubleEnvVarSuccessfulParse)
{
    setenv("TEST_APP_RATIO", "3.14", 1);

    using Schema = ConfigSchema<settings::Ratio>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    double ratio = config.Get<settings::Ratio>();
    EXPECT_DOUBLE_EQ(ratio, 3.14);

    unsetenv("TEST_APP_RATIO");
}

TEST_F(ConfigurationIntegrationTest, CRTPInterfaceSet)
{
    using Schema = ConfigSchema<settings::ServerPort>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // Call Set through the CRTP base class reference
    IConfigurationProvider<Configuration<Schema>, Schema>& crtp_ref = config;
    auto status = crtp_ref.Set<settings::ServerPort>(9090);
    ASSERT_TRUE(status.ok());
    EXPECT_EQ(crtp_ref.Get<settings::ServerPort>(), 9090);
}

TEST_F(ConfigurationIntegrationTest, CRTPInterfaceGet)
{
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    IConfigurationProvider<Configuration<Schema>, Schema>& crtp_ref = config;
    EXPECT_EQ(crtp_ref.Get<settings::AppName>(), "TestApp");
}

TEST_F(ConfigurationIntegrationTest, CRTPInterfaceLoad)
{
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(file_path_);

    IConfigurationProvider<Configuration<Schema>, Schema>& crtp_ref = config;
    ASSERT_TRUE(crtp_ref.Load().ok());
    EXPECT_EQ(crtp_ref.Get<settings::AppName>(), "TestApp");
}

TEST_F(ConfigurationIntegrationTest, CRTPInterfaceSaveAndDiff)
{
    using Schema = ConfigSchema<settings::AppName, settings::AppPort>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    IConfigurationProvider<Configuration<Schema>, Schema>& crtp_ref = config;
    (void)crtp_ref.Set<settings::AppPort>(9999);

    auto diff = crtp_ref.Diff();
    EXPECT_TRUE(diff.HasDifferences());

    ASSERT_TRUE(crtp_ref.Save().ok());
}

TEST_F(ConfigurationIntegrationTest, CRTPInterfaceValidateAll)
{
    using Schema = ConfigSchema<settings::ServerPort>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    IConfigurationProvider<Configuration<Schema>, Schema>& crtp_ref = config;
    EXPECT_TRUE(crtp_ref.ValidateAll().ok());
}

TEST_F(ConfigurationIntegrationTest, CRTPInterfaceGetFilePath)
{
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    IConfigurationProvider<Configuration<Schema>, Schema>& crtp_ref = config;
    EXPECT_EQ(crtp_ref.GetFilePath(), file_path_);
}

TEST(MockVirtualConfigTest, AllMethods)
{
    testing::MockVirtualConfigurationProvider mock;

    EXPECT_CALL(mock, Load()).WillOnce(::testing::Return(cppfig::OkStatus()));
    EXPECT_CALL(mock, Save()).WillOnce(::testing::Return(cppfig::OkStatus()));
    EXPECT_CALL(mock, GetFilePath()).WillOnce(::testing::Return("mock.json"));
    EXPECT_CALL(mock, ValidateAll()).WillOnce(::testing::Return(cppfig::OkStatus()));
    EXPECT_CALL(mock, GetDiffString()).WillOnce(::testing::Return("no diff"));

    EXPECT_TRUE(mock.Load().ok());
    EXPECT_TRUE(mock.Save().ok());
    EXPECT_EQ(mock.GetFilePath(), "mock.json");
    EXPECT_TRUE(mock.ValidateAll().ok());
    EXPECT_EQ(mock.GetDiffString(), "no diff");
}

}  // namespace cppfig::test
