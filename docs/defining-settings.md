# Defining Settings

A setting is a struct that describes one configuration value. A registry lists the settings an application uses. Both are checked at compile time.

## Setting members

### Required

```cpp
struct ServerPort {
    static constexpr std::string_view path = "server.port";
    using value_type = int;
    static constexpr auto default_value() -> int { return 8080; }
};
```

| Member | Meaning |
| --- | --- |
| `path` | The key in the `.conf` file. Convertible to `std::string_view`. |
| `value_type` | The type `Get` returns a reference to. It needs a [`ConfigTraits`](custom-types.md) conversion. |
| `default_value()` | Returns exactly `value_type`. Called at most once per load or build, and only when the default is used: a setting present in the file never calls it. |

Make `default_value()` `constexpr` when you can: a constant default is checked against the validator at compile time.

### Optional: environment override

```cpp
static constexpr std::string_view env_override = "SERVER_PORT";
```

When the variable is set at load time, its value is parsed with the same conversion as the file and takes precedence over the file and the default. The environment is read once, in a single pass, during loading; later changes do not affect an existing configuration. `LoadOptions::environment` can replace the process environment with a fixed set of variables or none. Overrides are never written to the file.

### Optional: validator

```cpp
static constexpr auto validator() { return cppfig::Range(1, 65535); }
```

`validator()` returns any callable taking `const value_type&` and returning `cppfig::ValidationResult`. Every effective value is validated: the default, a file value and an override. See [Validators](validators.md).

## Registries

```cpp
struct AppRegistry : cppfig::Registry<settings::ServerHost, settings::ServerPort, settings::LogLevel> { };
```

or

```cpp
using AppRegistry = cppfig::Registry<settings::ServerHost, settings::ServerPort, settings::LogLevel>;
```

Both work everywhere. The derived struct gives the registry its own short name, so compiler errors, clangd hovers and symbol names show `Configuration<AppRegistry>` rather than the whole list of settings, and large registries compile slightly faster.

The registry holds no values, only compile-time metadata. Each `Configuration<AppRegistry>` stores one value per setting, laid out by alignment so that, with GCC and Clang, there is no padding between them.

## Compile-time checks

Instantiating a registry checks:

| Rule | Diagnostic |
| --- | --- |
| Every member is a valid setting: `path`, `value_type`, `default_value()` returning `value_type`, a `ConfigTraits` conversion, a validator of the right type | Unsatisfied `IsSetting` constraint naming the setting |
| Paths are well formed: non-empty dot-separated segments without whitespace, `=`, `#`, a double quote or a backslash | `Registry paths must be non-empty and contain only valid non-empty segments` |
| Paths are unique, and no path is a prefix of another (`server` next to `server.port`); this also rejects listing a setting twice | `Registry paths must be unique and neither path may be a prefix of another` |
| Environment variable names match `[A-Za-z_][A-Za-z0-9_]*` | `Environment override names must be non-empty and match [A-Za-z_][A-Za-z0-9_]*` |
| Environment variable names are unique | `Environment override names must be unique within a Registry` |
| A default that can be evaluated at compile time passes its validator | `A constexpr-evaluable default value failed its setting validator` |

Using the registry checks:

| Rule | Diagnostic |
| --- | --- |
| `Get<S>()` names a setting in the registry | No matching `Get`; the unsatisfied constraint is `has_setting<S>` |
| `Get` is not called on a temporary configuration | Use of the deleted `Get() const&&` |

Defaults that are not `constexpr`, such as most `std::string` defaults, are validated when the configuration is created instead.

## Value types

Built in: `bool`, every standard integer type, `float`, `double`, `long double` where the standard library supports it, `std::string` and `std::filesystem::path` (from `<cppfig/path.h>`, which `<cppfig/cppfig.h>` and `<cppfig/file.h>` include). With `#include <cppfig/chrono.h>`, `std::chrono` durations too. Enums and your own types need a `ConfigTraits` specialization. See [Custom Types](custom-types.md).

## Paths

Paths are flat keys. Dots are a naming convention that groups related settings; they do not create nested objects:

```conf
database.connection.host = localhost
database.connection.port = 5432
cache.enabled = true
```

Paths are case-sensitive.

## Value precedence

For each setting, the effective value is the first available of:

1. the environment override, when `env_override` is declared and the variable is set;
2. the value in the file;
3. the default.

Each value that is read is validated: a file value even when an override replaces it, and a default whenever the file lacks the setting, since it is then written to the file as well. Loading fails rather than skipping an invalid value.

## A complete example

```cpp
#include <cppfig/cppfig.h>

namespace settings {

struct DatabaseHost {
    static constexpr std::string_view path = "database.host";
    static constexpr std::string_view env_override = "DB_HOST";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
    static auto validator() { return cppfig::NotEmpty(); }
};

struct DatabasePort {
    static constexpr std::string_view path = "database.port";
    static constexpr std::string_view env_override = "DB_PORT";
    using value_type = std::uint16_t;
    static constexpr auto default_value() -> std::uint16_t { return 5432; }
    static constexpr auto validator() { return cppfig::Min<std::uint16_t>(1); }
};

struct PoolSize {
    static constexpr std::string_view path = "database.pool_size";
    using value_type = int;
    static constexpr auto default_value() -> int { return 10; }
    static constexpr auto validator() { return cppfig::Range(1, 100); }
};

struct LogLevel {
    static constexpr std::string_view path = "logging.level";
    using value_type = std::string;
    static constexpr auto default_value() -> std::string { return "info"; }
    static constexpr auto validator() { return cppfig::OneOf("debug", "info", "warn", "error"); }
};

}  // namespace settings

struct AppRegistry : cppfig::Registry<settings::DatabaseHost, settings::DatabasePort, settings::PoolSize, settings::LogLevel> { };
```
