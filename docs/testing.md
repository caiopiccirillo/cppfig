# Testing with cppfig

Code that reads configuration is easiest to test when it does not touch files or the environment. cppfig offers two pieces for that: the `ConfigurationProvider` concept, which code can depend on instead of the concrete configuration, and `MockConfiguration`, an in-memory configuration for tests.

## Depending on a provider

Write components against the concept rather than `Configuration<AppRegistry>`:

```cpp
#include <cppfig/cppfig.h>

template <cppfig::ConfigurationProvider<AppRegistry> Config>
class Server {
public:
    explicit Server(const Config& config)
        : config_(config)
    {
    }

    [[nodiscard]] auto Address() const -> std::string
    {
        return config_.template Get<settings::ServerHost>() + ":" + std::to_string(config_.template Get<settings::ServerPort>());
    }

private:
    const Config& config_;
};
```

A type satisfies `ConfigurationProvider<AppRegistry>` when it has `Get<S>()` returning `const S::value_type&` for every setting in the registry. Production code instantiates `Server<cppfig::Configuration<AppRegistry>>`, tests use `Server<cppfig::testing::MockConfiguration<AppRegistry>>`, and neither pays for virtual calls.

For a single setting there is `SettingProvider<Provider, S>`.

## MockConfiguration

`MockConfiguration` behaves like a loaded configuration: it is frozen, satisfies the same concept and has the same `Get`. Build one with `MockConfigurationBuilder`:

```cpp
#include <cppfig/testing/mock.h>
#include <gtest/gtest.h>

TEST(ServerTest, FormatsTheAddress)
{
    cppfig::testing::MockConfigurationBuilder<AppRegistry> builder;
    builder.Set<settings::ServerHost>("example.com").Set<settings::ServerPort>(9000);
    auto config = builder.Build();
    ASSERT_TRUE(config.ok()) << config.status().message();

    Server server(config.value());
    EXPECT_EQ(server.Address(), "example.com:9000");
}
```

- Settings you do not `Set` take their defaults.
- `Build()` validates every value with the settings' validators, as `Load` would, and returns an error naming the setting when one fails. Tests cannot build a configuration that production code could never receive.
- `Build()` moves the values out of the builder; make a new builder for another configuration.
- `MockConfiguration<AppRegistry>::FromDefaults()` builds one from defaults alone.

`Set` only accepts settings in the registry, with their own value type, so a test cannot set a value the application never declared.

The mock reads no files and no environment variables, so tests that use it are independent of the machine they run on.

### Getting the header

`<cppfig/testing/mock.h>` ships and installs with the library and needs nothing beyond `cppfig::cppfig`. It does not depend on GoogleTest or any other test framework.

## Testing loading itself

To test how your application reacts to a real file, write one to a temporary directory and load it:

```cpp
TEST(ConfigTest, RejectsAnInvalidPort)
{
    const auto path = std::filesystem::temp_directory_path() / "config_test.conf";
    std::ofstream(path) << "server.port = 0\n";

    auto loaded = cppfig::Configuration<AppRegistry>::Load(path.string());
    EXPECT_FALSE(loaded.ok());

    std::filesystem::remove(path);
}
```

Tests that set environment variables should restore them afterwards, because the environment is shared by the whole test process.
