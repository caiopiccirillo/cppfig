# cppfig - Modern C++20 Configuration Library

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/caiopiccirillo/cppfig)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A modern, header-only C++20 configuration library with **compile-time type safety**, **zero macros**, **IDE autocompletion**, and **pluggable serialization**.

## Features

- **Compile-time type safety** - All type errors caught at compile time
- **Zero macros** - Pure C++20 templates and concepts
- **IDE-friendly** - Full autocompletion: `config.Get<settings::ServerPort>()`
- **Hierarchical configuration** - Dot-notation paths create nested JSON
- **Environment variable overrides** - Production-friendly configuration
- **Validation** - Built-in validators with custom extensions
- **Schema migration** - Automatically adds new settings to existing files
- **Mockable** - GMock-compatible interfaces for unit testing
- **Pluggable serialization** - JSON default, extensible to YAML/TOML

## Quick Start

### 1. Define Settings

```cpp
#include <cppfig/cppfig.h>

namespace settings {

struct ServerPort {
    static constexpr std::string_view kPath = "server.port";
    static constexpr std::string_view kEnvOverride = "SERVER_PORT";  // Optional
    using ValueType = int;
    static auto DefaultValue() -> int { return 8080; }
    static auto GetValidator() -> cppfig::Validator<int> {  // Optional
        return cppfig::Range(1, 65535);
    }
};

struct ServerHost {
    static constexpr std::string_view kPath = "server.host";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "localhost"; }
};

struct LogLevel {
    static constexpr std::string_view kPath = "logging.level";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "info"; }
    static auto GetValidator() -> cppfig::Validator<std::string> {
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
    // Create configuration manager
    cppfig::Configuration<MySchema> config("config.json");

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

### Generated JSON

```json
{
    "server": {
        "port": 8080,
        "host": "localhost"
    },
    "logging": {
        "level": "info"
    }
}
```

## Installation

### Requirements

- C++20 compiler (GCC 11+, Clang 14+, MSVC 2022+)
- [nlohmann/json](https://github.com/nlohmann/json)
- [Abseil](https://github.com/abseil/abseil-cpp) (for `absl::Status`)

### Header-Only Integration

```bash
cp -r cppfig/src/cppfig your_project/include/
```

### CMake Integration

```cmake
add_subdirectory(cppfig)
target_link_libraries(your_target PRIVATE cppfig)
```

### vcpkg

```bash
# Dependencies
vcpkg install nlohmann-json abseil
```

## Documentation

| Guide | Description |
|-------|-------------|
| [Getting Started](docs/getting-started.md) | Installation and first steps |
| [Defining Settings](docs/defining-settings.md) | Complete setting options |
| [Custom Types](docs/custom-types.md) | Add support for your own types |
| [Validators](docs/validators.md) | Built-in and custom validation |
| [Testing](docs/testing.md) | Mock configuration in unit tests |
| [Serializers](docs/serializers.md) | Custom serialization formats |

## Key Concepts

### Setting Structure

Every setting is a struct with:

```cpp
struct MySetting {
    // Required
    static constexpr std::string_view kPath = "path.to.setting";
    using ValueType = int;
    static auto DefaultValue() -> int { return 42; }

    // Optional
    static constexpr std::string_view kEnvOverride = "MY_SETTING";
    static auto GetValidator() -> cppfig::Validator<int> { return cppfig::Min(0); }
};
```

### Value Resolution Order

1. **Environment variable** (if `kEnvOverride` defined and env var set)
2. **File value** (if present in configuration file)
3. **Default value** (from `DefaultValue()`)

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

    friend void to_json(nlohmann::json& j, const Point& p) {
        j = {{"x", p.x}, {"y", p.y}};
    }
    friend void from_json(const nlohmann::json& j, Point& p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
    }
};

// Register with cppfig
template <>
struct cppfig::ConfigTraits<Point> : cppfig::ConfigTraitsFromJsonAdl<Point> {};

// Use in settings
struct Origin {
    static constexpr std::string_view kPath = "origin";
    using ValueType = Point;
    static auto DefaultValue() -> Point { return {0, 0}; }
};
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
cppfig::Configuration<Schema, Serializer = JsonSerializer>

// Methods
auto Load() -> absl::Status;
auto Save() const -> absl::Status;
auto Get<Setting>() const -> typename Setting::ValueType;
auto Set<Setting>(value) -> absl::Status;
auto Diff() const -> ConfigDiff;
auto ValidateAll() const -> absl::Status;
auto GetFilePath() const -> std::string_view;
```

### ConfigSchema

```cpp
cppfig::ConfigSchema<Settings...>

// Compile-time checks
static constexpr bool HasSetting<S>;
static constexpr std::size_t kSize;

// Runtime utilities
static auto GetPaths() -> std::array<std::string_view, kSize>;
static void ForEachSetting(auto&& fn);
```

## Development

### Building

```bash
# Debug build
cmake --workflow --preset=debug-dev

# Release build
cmake --workflow --preset=release-dev

# Run tests
ctest --test-dir build/debug/dev --output-on-failure

# Run example
./build/debug/dev/examples/cppfig_example

# Run benchmarks
./build/release/dev/benchmark/cppfig_benchmark
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
│   ├── traits.h          # Type traits
│   ├── validator.h       # Validators
│   ├── serializer.h      # Serialization
│   ├── interface.h       # Mockable interfaces
│   ├── diff.h            # Configuration diff
│   ├── logging.h         # Logging utilities
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
