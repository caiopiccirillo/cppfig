# Defining Settings

Settings in cppfig are defined as structs with specific static members. This design enables compile-time type safety and IDE autocompletion.

## Setting Structure

### Required Members

Every setting must have these three members:

```cpp
struct MySetting {
    // Path in the config file (dot-separated for hierarchy)
    static constexpr std::string_view path = "category.subcategory.name";

    // The value type
    using value_type = int;

    // Default value factory function
    static auto default_value() -> int { return 42; }
};
```

### Optional Members

#### Environment Variable Override

Allow the setting to be overridden via environment variable:

```cpp
struct ServerHost {
    static constexpr std::string_view path = "server.host";
    static constexpr std::string_view env_override = "SERVER_HOST";  // Optional
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};
```

When `SERVER_HOST` environment variable is set, it takes precedence over the file value.

#### Validator

Add validation to ensure values are within acceptable bounds:

```cpp
struct ServerPort {
    static constexpr std::string_view path = "server.port";
    using value_type = int;
    static auto default_value() -> int { return 8080; }

    // Optional: Validator for this setting
    static auto validator() -> cppfig::Validator<int> {
        return cppfig::Range(1, 65535);
    }
};
```

## Complete Example

Here's a comprehensive example with all features:

```cpp
namespace settings {

// Simple setting
struct AppName {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "MyApplication"; }
};

// Setting with environment override
struct DatabaseUrl {
    static constexpr std::string_view path = "database.url";
    static constexpr std::string_view env_override = "DATABASE_URL";
    using value_type = std::string;
    static auto default_value() -> std::string {
        return "postgresql://localhost:5432/mydb";
    }
};

// Setting with validator
struct MaxConnections {
    static constexpr std::string_view path = "database.pool.max_connections";
    using value_type = int;
    static auto default_value() -> int { return 10; }
    static auto validator() -> cppfig::Validator<int> {
        return cppfig::Range(1, 100);
    }
};

// Setting with environment override AND validator
struct ServerPort {
    static constexpr std::string_view path = "server.port";
    static constexpr std::string_view env_override = "PORT";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
    static auto validator() -> cppfig::Validator<int> {
        return cppfig::Range(1, 65535);
    }
};

// Boolean setting
struct DebugMode {
    static constexpr std::string_view path = "app.debug";
    static constexpr std::string_view env_override = "DEBUG";
    using value_type = bool;
    static auto default_value() -> bool { return false; }
};

// Enum-like string setting
struct LogLevel {
    static constexpr std::string_view path = "logging.level";
    using value_type = std::string;
    static auto default_value() -> std::string { return "info"; }
    static auto validator() -> cppfig::Validator<std::string> {
        return cppfig::OneOf<std::string>({"debug", "info", "warn", "error"});
    }
};

}  // namespace settings
```

## Supported Value Types

Out of the box, cppfig supports:

| Type | JSON Type | Environment Variable |
|------|-----------|---------------------|
| `bool` | boolean | `true`, `false`, `1`, `0`, `yes`, `no`, `on`, `off` |
| `int` | number | Integer string |
| `std::int64_t` | number | Integer string |
| `float` | number | Decimal string |
| `double` | number | Decimal string |
| `std::string` | string | As-is |

For custom types, see [Custom Types](custom-types.md).

## Hierarchical Configuration

Use dot-separated paths to create hierarchical JSON structure:

```cpp
struct DatabaseHost {
    static constexpr std::string_view path = "database.connection.host";
    // ...
};

struct DatabasePort {
    static constexpr std::string_view path = "database.connection.port";
    // ...
};

struct DatabasePoolSize {
    static constexpr std::string_view path = "database.pool.max_size";
    // ...
};
```

This generates JSON like:

```json
{
    "database": {
        "connection": {
            "host": "localhost",
            "port": 5432
        },
        "pool": {
            "max_size": 10
        }
    }
}
```

## Value Resolution Order

When calling `config.Get<MySetting>()`, values are resolved in this order:

1. **Environment variable** (if `env_override` is defined and the env var is set)
2. **File value** (if present in the configuration file)
3. **Default value** (from `default_value()`)

This means environment variables always take precedence, making it easy to override configuration in production without modifying files.
