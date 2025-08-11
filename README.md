# C++fig - Type-Safe Configuration Library

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/caiopiccirillo/cppfig)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A modern, high-performance C++ configuration library that provides **compile-time type safety**, **zero-runtime overhead**, and **blazing-fast access** to your application settings.

## Why C++fig?

### **Compile-Time Type Safety**
- **No runtime type errors** - All type mismatches caught at compile time
- **Zero performance overhead** - Types resolved at compile time
- **IDE-friendly** - Full autocomplete and error detection

### **Exceptional Performance**
- **Sub-nanosecond value access** (0.238 ns for integers)
- **O(1) setting lookup** with std::unordered_map
- **Production-ready** for high-frequency applications

### **Developer Experience**
- **Minimal boilerplate** - One-line setting declarations
- **Clean API** - Intuitive and ergonomic interface
- **Automatic type deduction** - No need to specify types repeatedly
- **Rich validation** - Built-in validators with custom extensions

## Performance Benchmarks

| Operation | Time |
|-----------|------|
| Get Setting | 1.39 ns |
| Value Access | 0.238 ns |
| Combined Access | 1.32 ns |
| 10 Settings | 30.6 ns |
| Validation | 0.608 ns |
| Save to JSON | 41.3 μs |

*Benchmarked on Clang 20.1.8 with -O3 optimization*

## Quick Start

### 1. Define Your Configuration

```cpp
#include "cppfig.h"

// Define your configuration enum
enum class AppConfig : uint8_t {
    DatabaseUrl,
    MaxConnections,
    EnableLogging,
    RetryCount
};

// One-line type declarations (compile-time type safety!)
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::DatabaseUrl, std::string);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::MaxConnections, int);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::EnableLogging, bool);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::RetryCount, int);
```

### 2. Create Configuration Instance

```cpp
// Define default values with validation
const auto DefaultConfig = {
    { AppConfig::DatabaseUrl,
      CreateStringSetting<AppConfig::DatabaseUrl>(
          "mongodb://localhost:27017",
          "Database connection URL") },
    { AppConfig::MaxConnections,
      CreateIntSetting<AppConfig::MaxConnections>(
          100, 1, 1000, "Max database connections", "connections") },
    { AppConfig::EnableLogging,
      CreateBoolSetting<AppConfig::EnableLogging>(
          true, "Enable application logging") },
    { AppConfig::RetryCount,
      CreateIntSetting<AppConfig::RetryCount>(
          3, 0, 10, "Operation retry attempts", "retries") }
};

// Load configuration from file with type safety
using Config = GenericConfiguration<AppConfig, JsonSerializer<AppConfig>>;
Config config("app_config.json", DefaultConfig);
```

### 3. Access Configuration Values

```cpp
// Clean, type-safe API with automatic type deduction
auto db_setting = config.GetSetting<AppConfig::DatabaseUrl>();
auto db_url = db_setting.Value();  // std::string automatically deduced!

auto conn_setting = config.GetSetting<AppConfig::MaxConnections>();
auto max_conn = conn_setting.Value();  // int automatically deduced!

// Update values with compile-time type checking
conn_setting.SetValue(150);  // ✅ Compiles - correct type
// conn_setting.SetValue("invalid");  // ❌ Compile error - type mismatch!

// Validate and save
if (config.ValidateAll()) {
    config.Save();
}
```

> **See Complete Example**: Check out [`examples/simple_config_example.cpp`](examples/simple_config_example.cpp) for a fully working game configuration example with detailed explanations!

## Key Features

### Compile-Time Type Safety

```cpp
// These will FAIL TO COMPILE if types don't match:
auto value = config.GetSetting<AppConfig::MaxConnections>().Value();
// value is guaranteed to be 'int' - no runtime checks needed!

config.GetSetting<AppConfig::MaxConnections>().SetValue(42);    // ✅ Works
config.GetSetting<AppConfig::MaxConnections>().SetValue("42");  // ❌ Compile error
```

### Zero Runtime Overhead

```cpp
// All type resolution happens at compile time
// Runtime performance identical to raw variable access
auto setting = config.GetSetting<AppConfig::DatabaseUrl>();  // 1.39 ns
auto value = setting.Value();                                // 0.238 ns
```

### Rich Validation

```cpp
// Built-in validators
auto validator = SimpleValidators<int>::Range(1, 1000);
if (!validator(max_connections)) {
    // Handle validation error
}

// Custom validators
auto custom_validator = [](const std::string& url) {
    return url.starts_with("mongodb://") || url.starts_with("postgresql://");
};
```

### JSON Serialization

```cpp
// Automatic JSON save/load
config.Save("config.json");
config.Load("config.json");

// Configuration automatically serialized as:
{
    "database_url": "mongodb://localhost:27017",
    "max_connections": 100,
    "enable_logging": true,
    "retry_count": 3
}
```

## Architecture Highlights

- **Template-based design** for zero-runtime overhead
- **std::variant** for type-safe setting storage
- **std::unordered_map** for O(1) setting lookup
- **SFINAE** for compile-time type validation
- **Modern C++17** features throughout

## Testing & Quality

- **Comprehensive unit tests** covering all APIs
- **Performance benchmarks** with Google Benchmark
- **Memory safety** verified with sanitizers
- **clang-format** and **clang-tidy** integration
- **Zero warnings** on high warning levels

## Development Setup

The project uses **devcontainer** for consistent development environments:

1. Install [Visual Studio Code](https://code.visualstudio.com/) and [Remote - Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Clone the repository
3. Open in VS Code and reopen in container
4. Build with CMake: `cmake --workflow --preset=debug-dev`

### Building

```bash
# Debug build
cmake --workflow --preset=debug-dev

# Release build (for performance testing)
cmake --workflow --preset=release-dev

# Run tests
cd build/debug/dev && ctest

# Run example
cd build/debug/dev && ./examples/cppfig_example

# Run benchmarks
cd build/release/dev && ./benchmark/cppfig_benchmark
```

## Advanced Usage

### Custom Types

```cpp
struct DatabaseConfig {
    std::string host;
    int port;
    std::string username;
};

// Add support for custom types
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::DatabaseConfig, DatabaseConfig);
```

### Validation Chains

```cpp
auto validator = [](const int& value) {
    return SimpleValidators<int>::Range(1, 1000)(value) &&
           SimpleValidators<int>::NonZero()(value);
};
```

### Configuration Inheritance

```cpp
// Base configuration
const auto BaseConfig = { /* base settings */ };

// Extended configuration
const auto ExtendedConfig = {
    BaseConfig.begin(), BaseConfig.end(),
    { AppConfig::NewSetting, CreateStringSetting<AppConfig::NewSetting>(...) }
};
```

## Examples

- **[Simple Configuration Example](examples/simple_config_example.cpp)** - Complete walkthrough of defining and using a game configuration with detailed explanations and best practices

## Contributing

Contributions are welcome! Please ensure:
- Follow the existing code style (`clang-format`)
- Add tests for new features
- Run static analysis (`clang-tidy`)
- Update documentation as needed

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Ready to supercharge your configuration management?** 
Give C++fig a try and experience the power of compile-time type safety with zero runtime overhead!
