# Testing with cppfig

Code that reads configuration is easiest to test when the test controls the values without touching files or the environment. With cppfig the test builds a real `Configuration`, so the code under test needs no interface, template parameter or test double.

## Take the configuration by reference

```cpp
#include <cppfig/configuration.h>

class Server {
public:
    explicit Server(const cppfig::Configuration<AppRegistry>& config)
        : config_(config)
    {
    }

    [[nodiscard]] auto Address() const -> std::string
    {
        return config_.Get<settings::ServerHost>() + ":" + std::to_string(config_.Get<settings::ServerPort>());
    }

private:
    const cppfig::Configuration<AppRegistry>& config_;
};
```

Production code passes the configuration it loaded; tests pass one they built. Both are the same type, so there are no virtual calls and no templates in your code.

If a component should not see the whole registry, for example to keep its header from depending on every setting, give it a small struct holding just the values it needs and fill that struct from the configuration where the component is created.

## ConfigurationBuilder

`ConfigurationBuilder` makes a configuration from values set in code:

```cpp
#include <cppfig/configuration.h>
#include <gtest/gtest.h>

TEST(ServerTest, FormatsTheAddress)
{
    cppfig::ConfigurationBuilder<AppRegistry> builder;
    builder.Set<settings::ServerHost>("example.com").Set<settings::ServerPort>(9000);
    auto config = builder.Build();
    ASSERT_TRUE(config) << config.error().message();

    Server server(config.value());
    EXPECT_EQ(server.Address(), "example.com:9000");
}
```

- Settings you do not `Set` take their defaults.
- `Build()` validates every value with the settings' validators, as loading would, and reports every invalid value, each naming its setting. Tests cannot build a configuration that production code could never receive.
- `Build()` moves the values out of the builder, which is empty afterwards and can build again.
- `Set` only accepts settings in the registry, with their own value type, so a test cannot set a value the application never declared.

The builder reads no files and no environment variables, so tests that use it are independent of the machine they run on.

## Testing loading itself

To test how your application reacts to particular file contents, load them from text:

```cpp
TEST(ConfigTest, RejectsAnInvalidPort)
{
    auto loaded = cppfig::Configuration<AppRegistry>::FromText("server.port = 0\n", "test.conf");
    ASSERT_FALSE(loaded);
    EXPECT_TRUE(loaded.error().message().starts_with("test.conf:1:15: "));
}
```

To test environment overrides, pass the variables instead of changing the process environment, which is shared by every test in the process and is not safe to change while other threads read it:

```cpp
TEST(ConfigTest, PortComesFromTheEnvironment)
{
    const std::array<cppfig::EnvironmentVariable, 1> variables { { { "SERVER_PORT", "9000" } } };
    auto loaded = cppfig::Configuration<AppRegistry>::FromText("", "test.conf", { .environment = cppfig::Environment::From(variables) });
    ASSERT_TRUE(loaded) << loaded.error().message();
    EXPECT_EQ(loaded->config.Get<settings::ServerPort>(), 9000);
}
```

`Environment::None()` ignores the environment, so a test gives the same result whatever variables the machine running it has set.
