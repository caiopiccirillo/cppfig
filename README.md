# cppfig - Modern C++20 Configuration Library

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/caiopiccirillo/cppfig)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A modern, header-only C++20 configuration library with **compile-time type safety**, **zero macros**, **IDE autocompletion**, and **pluggable serialization**.

## Features

- **Compile-time type safety** - All type errors caught at compile time
- **Zero macros** - Pure C++20 templates and concepts
- **IDE-friendly** - Full autocompletion: `config.Get<settings::ServerPort>()`
- **Hierarchical configuration** - Dot-notation paths create nested structures
- **Environment variable overrides** - Production-friendly configuration
- **Validation** - Built-in validators with custom extensions
- **Schema migration** - Automatically adds new settings to existing files
- **Mockable** - GMock-compatible interfaces for unit testing
- **Pluggable serialization** - Flat `.conf` default, JSON opt-in, extensible to YAML/TOML
- **Thread-safe** - Opt-in `MultiThreadedPolicy` with reader-writer locking (zero overhead by default)

## Quick Start

### 1. Define Settings

```cpp
#include <cppfig/cppfig.h>

namespace settings {

struct ServerPort {
    static constexpr std::string_view path = "server.port";
    static constexpr std::string_view env_override = "SERVER_PORT";  // Optional
    using value_type = int;
    static auto default_value() -> int { return 8080; }
    static auto validator() -> cppfig::Validator<int> {  // Optional
        return cppfig::Range(1, 65535);
    }
};

struct ServerHost {
    static constexpr std::string_view path = "server.host";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};

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

### 2. Create Schema and Use

```cpp
// Group settings into a schema
using MySchema = cppfig::ConfigSchema<
    settings::ServerPort,
    settings::ServerHost,
    settings::LogLevel
>;

int main() {
    // Create configuration manager (.conf format by default)
    cppfig::Configuration<MySchema> config("config.conf");

    // Load (creates file with defaults if missing)
    auto status = config.Load();
    if (!status.ok()) {
        std::cerr << "Error: " << status.message() << std::endl;
        return 1;
    }

    // Type-safe access with IDE autocompletion
    int port = config.Get<settings::ServerPort>();
    std::string host = config.Get<settings::ServerHost>();

    // Modify with validation
    status = config.Set<settings::ServerPort>(9000);
    if (!status.ok()) {
        std::cerr << "Validation error: " << status.message() << std::endl;
    }

    // Save changes
    config.Save();

    return 0;
}
```

### Generated .conf

```conf
server.port = 8080
server.host = localhost
logging.level = info
```

<details>
<summary>Using JSON instead?</summary>

```cpp
#include <cppfig/cppfig.h>
#include <cppfig/json.h>  // opt-in

cppfig::Configuration<MySchema, cppfig::JsonSerializer> config("config.json");
```

Enable with `-DCPPFIG_ENABLE_JSON=ON` or vcpkg feature `"json"`.
See [Serializers](docs/serializers.md) for details.
</details>
```

## Installation

### Requirements

- C++20 compiler (GCC 11+, Clang 14+)
- No external dependencies for the core library

### Optional Dependencies

| Dependency | Feature | CMake Option |
|------------|---------|-------------|
| [nlohmann/json](https://github.com/nlohmann/json) | JSON serializer | `CPPFIG_ENABLE_JSON` |

### Header-Only Integration

```bash
cp -r cppfig/src/cppfig your_project/include/
```

### CMake Integration

```cmake
add_subdirectory(cppfig)
target_link_libraries(your_target PRIVATE cppfig)

# Optional: enable JSON support
set(CPPFIG_ENABLE_JSON ON)
```

<!--
### vcpkg

Core (`.conf` only, no extra dependencies):

```bash
# No additional packages needed — cppfig is fully self-contained
```

With JSON support:

```json
{
    "dependencies": [
        {
            "name": "cppfig",
            "features": ["json"]
        }
    ]
}
```
-->

## Documentation

| Guide | Description |
|-------|-------------|
| [Getting Started](docs/getting-started.md) | Installation and first steps |
| [Defining Settings](docs/defining-settings.md) | Complete setting options |
| [Custom Types](docs/custom-types.md) | Add support for your own types |
| [Validators](docs/validators.md) | Built-in and custom validation |
| [Testing](docs/testing.md) | Mock configuration in unit tests |
| [Serializers](docs/serializers.md) | Custom serialization formats |
| [Thread Safety](docs/getting-started.md#thread-safety) | Concurrent access with `MultiThreadedPolicy` |

## Key Concepts

### Thread Safety

By default, `Configuration` uses `SingleThreadedPolicy` (zero overhead). For concurrent access, opt in to reader-writer locking:

```cpp
// Thread-safe configuration:
cppfig::Configuration<MySchema, cppfig::ConfSerializer, cppfig::MultiThreadedPolicy>
    config("config.conf");
```

| Policy | Overhead | Use case |
|--------|----------|----------|
| `SingleThreadedPolicy` (default) | None | Single-threaded or externally synchronized |
| `MultiThreadedPolicy` | `std::shared_mutex` | Concurrent reads and writes from multiple threads |

- `Get` acquires a **shared** (reader) lock — multiple concurrent readers allowed.
- `Set` / `Load` acquire an **exclusive** (writer) lock.
- `Save` / `Diff` / `ValidateAll` acquire a **shared** lock (read-only).
- Validation in `Set` runs **before** the exclusive lock, so invalid values never block readers.

### Setting Structure

Every setting is a struct with:

```cpp
struct MySetting {
    // Required
    static constexpr std::string_view path = "path.to.setting";
    using value_type = int;
    static auto default_value() -> int { return 42; }

    // Optional
    static constexpr std::string_view env_override = "MY_SETTING";
    static auto validator() -> cppfig::Validator<int> { return cppfig::Min(0); }
};
```

### Value Resolution Order

1. **Environment variable** (if `env_override` defined and env var set)
2. **File value** (if present in configuration file)
3. **Default value** (from `default_value()`)

### Built-in Validators

```cpp
// Numeric
cppfig::Min(0)
cppfig::Max(100)
cppfig::Range(1, 65535)
cppfig::Positive<int>()
cppfig::NonNegative<int>()

// String
cppfig::NotEmpty()
cppfig::MinLength(8)
cppfig::MaxLength(255)

// Generic
cppfig::OneOf<std::string>({"a", "b", "c"})
cppfig::Predicate<int>([](int x) { return x % 2 == 0; }, "must be even")

// Combine
cppfig::Min(1).And(cppfig::Max(100))
```

### Custom Types

```cpp
struct Point {
    int x, y;
};

// Register with cppfig — works with any serializer
template <>
struct cppfig::ConfigTraits<Point> {
    static auto Serialize(const Point& p) -> cppfig::Value {
        auto obj = cppfig::Value::Object();
        obj["x"] = cppfig::Value(p.x);
        obj["y"] = cppfig::Value(p.y);
        return obj;
    }
    static auto Deserialize(const cppfig::Value& v) -> std::optional<Point> {
        try {
            return Point{static_cast<int>(v["x"].Get<int64_t>()),
                         static_cast<int>(v["y"].Get<int64_t>())};
        } catch (...) {
            return std::nullopt;
        }
    }
    static auto ToString(const Point& p) -> std::string {
        return "(" + std::to_string(p.x) + "," + std::to_string(p.y) + ")";
    }
    static auto FromString(std::string_view) -> std::optional<Point> {
        return std::nullopt;
    }
};

// Use in settings
struct Origin {
    static constexpr std::string_view path = "origin";
    using value_type = Point;
    static auto default_value() -> Point { return {0, 0}; }
};
```

If you have the JSON feature enabled and your type already has nlohmann ADL functions, you can use the shortcut:

```cpp
#include <cppfig/json.h>

template <>
struct cppfig::ConfigTraits<Point> : cppfig::ConfigTraitsFromJsonAdl<Point> {};
```

### Testing

```cpp
#include <cppfig/testing/mock.h>

TEST(MyTest, UsesConfiguration) {
    cppfig::testing::MockConfiguration<MySchema> config;
    config.SetValue<settings::ServerPort>(9000);

    MyService service(config.Get<settings::ServerHost>(),
                      config.Get<settings::ServerPort>());

    EXPECT_EQ(service.GetPort(), 9000);
}
```

## API Reference

### Configuration Class

```cpp
cppfig::Configuration<Schema,
                       Serializer = ConfSerializer,
                       ThreadPolicy = SingleThreadedPolicy>

// Methods
auto Load() -> cppfig::Status;
auto Save() const -> cppfig::Status;
auto Get<Setting>() const -> typename Setting::value_type;
auto Set<Setting>(value) -> cppfig::Status;
auto Diff() const -> ConfigDiff;
auto ValidateAll() const -> cppfig::Status;
auto GetFilePath() const -> std::string_view;

// Thread policies
cppfig::SingleThreadedPolicy   // Zero-overhead (default)
cppfig::MultiThreadedPolicy    // std::shared_mutex reader-writer locking

// Serializers
cppfig::ConfSerializer         // Built-in flat .conf (default)
cppfig::JsonSerializer         // Requires CPPFIG_ENABLE_JSON
```

### ConfigSchema

```cpp
cppfig::ConfigSchema<Settings...>

// Compile-time checks
static constexpr bool has_setting<S>;
static constexpr std::size_t size;

// Runtime utilities
static auto GetPaths() -> std::array<std::string_view, size>;
static void ForEachSetting(auto&& fn);
```

## Development

### Building

```bash
# Debug build (Clang)
cmake --workflow --preset=debug-clang

# Debug build (GCC)
cmake --workflow --preset=debug-gcc

# Release build (Clang)
cmake --workflow --preset=release-clang

# Run tests
ctest --test-dir build/debug/clang --output-on-failure

# Run example
./build/debug/clang/examples/cppfig_example

# Run benchmarks
./build/release/clang/benchmark/cppfig_benchmark
```

### Code Coverage

```bash
# Full workflow: configure, build, test, and generate report
cmake --workflow --preset=coverage

# Generate HTML report (after running tests)
ninja -C build/coverage coverage

# Generate text summary
ninja -C build/coverage coverage-text

# Generate XML report (Cobertura format for CI)
ninja -C build/coverage coverage-xml
```

Coverage reports are generated in `build/coverage/coverage/`:
- `index.html` - Detailed HTML report with line-by-line coverage
- `coverage.xml` - Cobertura XML for CI integration

### Project Structure

```
cppfig/
├── src/cppfig/           # Library headers
│   ├── cppfig.h          # Main include
│   ├── configuration.h   # Configuration class
│   ├── schema.h          # ConfigSchema template
│   ├── setting.h         # Setting concepts
│   ├── traits.h          # Type traits (Serialize/Deserialize)
│   ├── value.h           # Core Value type
│   ├── validator.h       # Validators
│   ├── serializer.h      # Serializer concept
│   ├── conf.h            # Built-in .conf serializer
│   ├── json.h            # Optional JSON serializer
│   ├── interface.h       # Mockable interfaces
│   ├── diff.h            # Configuration diff
│   ├── logging.h         # Logging utilities
│   ├── thread_policy.h   # Thread safety policies
│   └── testing/mock.h    # Testing helpers
├── examples/             # Example code
├── test/                 # Unit & integration tests
├── benchmark/            # Performance benchmarks
└── docs/                 # Documentation
```

## Contributing

1. Follow existing code style (`clang-format`)
2. Run static analysis (`clang-tidy`)
3. Add tests for new features
4. Update documentation

## License

MIT License - see [LICENSE](LICENSE) for details.
