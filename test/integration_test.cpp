#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace cppfig::test {

namespace settings {

    struct AppName {
        static constexpr std::string_view kPath = "app.name";
        using ValueType = std::string;
        static auto DefaultValue() -> std::string { return "TestApp"; }
    };

    struct AppPort {
        static constexpr std::string_view kPath = "app.port";
        using ValueType = int;
        static auto DefaultValue() -> int { return 8080; }
    };

    struct AppVersion {
        static constexpr std::string_view kPath = "app.version";
        using ValueType = std::string;
        static auto DefaultValue() -> std::string { return "1.0.0"; }
    };

    struct ServerPort {
        static constexpr std::string_view kPath = "server.port";
        using ValueType = int;
        static auto DefaultValue() -> int { return 8080; }
        static auto GetValidator() -> Validator<int> { return Range(1, 65535); }
    };

    struct AppHost {
        static constexpr std::string_view kPath = "app.host";
        static constexpr std::string_view kEnvOverride = "TEST_APP_HOST";
        using ValueType = std::string;
        static auto DefaultValue() -> std::string { return "localhost"; }
    };

    struct DatabaseHost {
        static constexpr std::string_view kPath = "database.connection.host";
        using ValueType = std::string;
        static auto DefaultValue() -> std::string { return "localhost"; }
    };

    struct DatabasePort {
        static constexpr std::string_view kPath = "database.connection.port";
        using ValueType = int;
        static auto DefaultValue() -> int { return 5432; }
    };

    struct DatabasePoolSize {
        static constexpr std::string_view kPath = "database.pool.max_size";
        using ValueType = int;
        static auto DefaultValue() -> int { return 10; }
    };

    struct LoggingLevel {
        static constexpr std::string_view kPath = "logging.level";
        using ValueType = std::string;
        static auto DefaultValue() -> std::string { return "info"; }
    };

    // Setting for testing environment variable parse failure
    struct PortWithEnv {
        static constexpr std::string_view kPath = "server.port";
        static constexpr std::string_view kEnvOverride = "TEST_SERVER_PORT";
        using ValueType = int;
        static auto DefaultValue() -> int { return 8080; }
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
    Configuration<Schema> config(file_path_);

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
    Configuration<Schema> config(file_path_);

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
    Configuration<Schema> config(file_path_);

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
    EXPECT_TRUE(absl::IsInvalidArgument(status));
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
    Configuration<Schema> config(file_path_);

    ASSERT_TRUE(config.Load().ok());

    auto status = config.ValidateAll();
    EXPECT_FALSE(status.ok());
}

TEST_F(ConfigurationIntegrationTest, HierarchicalSettings)
{
    using Schema = ConfigSchema<settings::DatabaseHost, settings::DatabasePort,
                                settings::DatabasePoolSize, settings::LoggingLevel>;
    Configuration<Schema> config(file_path_);

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
        static constexpr std::string_view kPath = "origin";
        using ValueType = Point;
        static auto DefaultValue() -> Point { return Point { 0, 0 }; }
    };

    struct Target {
        static constexpr std::string_view kPath = "target";
        using ValueType = Point;
        static auto DefaultValue() -> Point { return Point { 100, 100 }; }
    };

}  // namespace custom_settings

TEST_F(ConfigurationIntegrationTest, CustomTypeInConfig)
{
    using Schema = ConfigSchema<custom_settings::Origin, custom_settings::Target>;
    Configuration<Schema> config(file_path_);

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
    Configuration<Schema> config(file_path_);

    auto status = config.Load();
    EXPECT_FALSE(status.ok());
    EXPECT_TRUE(absl::IsInvalidArgument(status));
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
    Configuration<EnvSchema> config(file_path_);
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
    Configuration<Schema> config(file_path_);
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

    auto& file_values = config.GetFileValues();
    EXPECT_EQ(file_values["app"]["name"], "TestApp");
}

TEST_F(ConfigurationIntegrationTest, GetDefaults)
{
    using Schema = ConfigSchema<settings::AppName>;
    Configuration<Schema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    auto& defaults = config.GetDefaults();
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
    Point p { 10, 20 };

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
    nlohmann::json invalid = 42;
    auto parsed = ConfigTraits<Point>::FromJson(invalid);
    EXPECT_FALSE(parsed.has_value());
}

TEST_F(ConfigurationIntegrationTest, ReadFileNotFound)
{
    // Test ReadFile with non-existent file
    auto result = ReadFile<JsonSerializer>("/nonexistent/path/to/file.json");
    EXPECT_FALSE(result.ok());
    EXPECT_TRUE(absl::IsNotFound(result.status()));
}

TEST_F(ConfigurationIntegrationTest, WriteFileToInvalidPath)
{
    // Test WriteFile to a path that cannot be opened (directory doesn't exist and path is invalid)
    nlohmann::json data = { { "key", "value" } };
    auto status = WriteFile<JsonSerializer>("/nonexistent_root_dir/cannot/write/here.json", data);
    EXPECT_FALSE(status.ok());
    EXPECT_TRUE(absl::IsInternal(status));
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
    Configuration<Schema> config(file_path_);
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
    Configuration<Schema> config(file_path_);
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

}  // namespace cppfig::test
