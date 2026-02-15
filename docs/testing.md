# Testing with cppfig

cppfig provides utilities for testing code that uses configuration. Include `<cppfig/testing/mock.h>` to access testing helpers.

## MockConfiguration

`MockConfiguration<Schema>` is an in-memory configuration that doesn't require file I/O:

```cpp
#include <gtest/gtest.h>
#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>

// Your settings
namespace settings {
struct ServerPort {
    static constexpr std::string_view path = "server.port";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
};

struct ServerHost {
    static constexpr std::string_view path = "server.host";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};
}

using TestSchema = cppfig::ConfigSchema<settings::ServerPort, settings::ServerHost>;

TEST(MyServerTest, UsesConfiguredPort) {
    // Create mock configuration
    cppfig::testing::MockConfiguration<TestSchema> config;

    // Set test values
    config.SetValue<settings::ServerPort>(9000);
    config.SetValue<settings::ServerHost>("test.example.com");

    // Use in your code
    MyServer server(config.Get<settings::ServerHost>(),
                    config.Get<settings::ServerPort>());

    EXPECT_EQ(server.GetPort(), 9000);
}

TEST(MyServerTest, DefaultValues) {
    cppfig::testing::MockConfiguration<TestSchema> config;

    // Uses default values
    EXPECT_EQ(config.Get<settings::ServerPort>(), 8080);
    EXPECT_EQ(config.Get<settings::ServerHost>(), "localhost");
}

TEST(MyServerTest, Reset) {
    cppfig::testing::MockConfiguration<TestSchema> config;

    config.SetValue<settings::ServerPort>(9000);
    EXPECT_EQ(config.Get<settings::ServerPort>(), 9000);

    config.Reset();  // Reset to defaults
    EXPECT_EQ(config.Get<settings::ServerPort>(), 8080);
}
```

## MockConfiguration API

| Method | Description |
|--------|-------------|
| `Get<Setting>()` | Get value (same as real Configuration) |
| `SetValue<Setting>(value)` | Set value without validation |
| `Set<Setting>(value)` | Set value with validation |
| `Load()` | No-op, returns OkStatus |
| `Save()` | No-op, returns OkStatus |
| `Reset()` | Reset all values to defaults |

## Dependency Injection Pattern

Design your classes to accept configuration through interfaces:

```cpp
// Your application code
template <typename Config>
class HttpServer {
public:
    explicit HttpServer(const Config& config)
        : port_(config.template Get<settings::ServerPort>())
        , host_(config.template Get<settings::ServerHost>()) {}

    void Start() {
        // Use port_ and host_
    }

private:
    int port_;
    std::string host_;
};

// Production usage
int main() {
    cppfig::Configuration<MySchema> config("config.json");
    config.Load();

    HttpServer server(config);
    server.Start();
}

// Test usage
TEST(HttpServerTest, StartsWithConfiguredPort) {
    cppfig::testing::MockConfiguration<MySchema> config;
    config.SetValue<settings::ServerPort>(9000);

    HttpServer server(config);
    // Test server behavior
}
```

## Using Virtual Interface for Mocking

For code that can't use templates, use `IConfigurationProviderVirtual`:

```cpp
// Your code using virtual interface
class Service {
public:
    explicit Service(cppfig::IConfigurationProviderVirtual& config)
        : config_(config) {}

    void Initialize() {
        auto status = config_.Load();
        if (!status.ok()) {
            throw std::runtime_error(std::string(status.message()));
        }
    }

private:
    cppfig::IConfigurationProviderVirtual& config_;
};

// Test with GMock
TEST(ServiceTest, InitializeLoadsConfig) {
    cppfig::testing::MockVirtualConfigurationProvider mock_config;

    EXPECT_CALL(mock_config, Load())
        .WillOnce(testing::Return(absl::OkStatus()));

    Service service(mock_config);
    EXPECT_NO_THROW(service.Initialize());
}

TEST(ServiceTest, InitializeFailsOnLoadError) {
    cppfig::testing::MockVirtualConfigurationProvider mock_config;

    EXPECT_CALL(mock_config, Load())
        .WillOnce(testing::Return(absl::InternalError("File not found")));

    Service service(mock_config);
    EXPECT_THROW(service.Initialize(), std::runtime_error);
}
```

## MockVirtualConfigurationProvider

GMock-compatible mock for the virtual interface:

```cpp
class MockVirtualConfigurationProvider : public IConfigurationProviderVirtual {
public:
    MOCK_METHOD(absl::Status, Load, (), (override));
    MOCK_METHOD(absl::Status, Save, (), (const, override));
    MOCK_METHOD(std::string_view, GetFilePath, (), (const, override));
    MOCK_METHOD(absl::Status, ValidateAll, (), (const, override));
    MOCK_METHOD(std::string, GetDiffString, (), (const, override));
};
```

## Testing with Temporary Files

For integration tests that need real file I/O:

```cpp
#include <filesystem>

class ConfigurationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp file path
        temp_path_ = std::filesystem::temp_directory_path() / "test_config.json";
    }

    void TearDown() override {
        // Clean up
        std::filesystem::remove(temp_path_);
    }

    std::filesystem::path temp_path_;
};

TEST_F(ConfigurationIntegrationTest, SaveAndLoad) {
    // First instance - save
    {
        cppfig::Configuration<MySchema> config(temp_path_.string());
        ASSERT_TRUE(config.Load().ok());
        ASSERT_TRUE(config.Set<settings::ServerPort>(9000).ok());
        ASSERT_TRUE(config.Save().ok());
    }

    // Second instance - load saved value
    {
        cppfig::Configuration<MySchema> config(temp_path_.string());
        ASSERT_TRUE(config.Load().ok());
        EXPECT_EQ(config.Get<settings::ServerPort>(), 9000);
    }
}
```

## ConfigurationTestFixture Helper

Use the provided fixture helper:

```cpp
#include <cppfig/testing/mock.h>

class MyTest : public ::testing::Test {
protected:
    void SetUp() override {
        file_path_ = cppfig::testing::ConfigurationTestFixture::CreateTempFilePath("mytest");
    }

    void TearDown() override {
        cppfig::testing::ConfigurationTestFixture::RemoveFile(file_path_);
    }

    std::string file_path_;
};
```

## Testing Validators

Test your validators independently:

```cpp
TEST(ValidatorTest, PortRange) {
    auto validator = cppfig::Range(1, 65535);

    EXPECT_TRUE(validator(1));
    EXPECT_TRUE(validator(8080));
    EXPECT_TRUE(validator(65535));

    EXPECT_FALSE(validator(0));
    EXPECT_FALSE(validator(-1));
    EXPECT_FALSE(validator(65536));
}

TEST(ValidatorTest, LogLevel) {
    auto validator = cppfig::OneOf<std::string>({"debug", "info", "warn", "error"});

    EXPECT_TRUE(validator("info"));
    EXPECT_TRUE(validator("error"));

    auto result = validator("invalid");
    EXPECT_FALSE(result);
    EXPECT_FALSE(result.error_message.empty());
}
```
