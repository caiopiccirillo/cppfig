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

}  // namespace cppfig::test
