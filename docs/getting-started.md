# Getting Started with cppfig

This guide will help you get started with cppfig, a modern C++20 header-only configuration library.

## Installation

### Header-Only Integration

cppfig is a header-only library. Simply copy the `src/cppfig/` directory to your project's include path:

```bash
cp -r cppfig/src/cppfig your_project/include/
```

### CMake Integration

Add cppfig as a subdirectory:

```cmake
add_subdirectory(cppfig)
target_link_libraries(your_target PRIVATE cppfig)
```

### vcpkg

```json
{
    "dependencies": [
        "cppfig"
    ]
}
```

To also enable the optional JSON serializer:

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

### Requirements

- C++20 compatible compiler (GCC 11+, Clang 14+)
- No external dependencies for the core library

#### Optional Dependencies

| Dependency | Required For | CMake Option |
|------------|-------------|--------------|
| [nlohmann/json](https://github.com/nlohmann/json) | JSON serializer | `CPPFIG_ENABLE_JSON` |

## Quick Start

### 1. Include the Header

```cpp
#include <cppfig/cppfig.h>
```

### 2. Define Your Settings

Each setting is defined as a struct with specific static members:

```cpp
namespace settings {

struct ServerPort {
    // Required: The path in the configuration file (dot-separated for hierarchy)
    static constexpr std::string_view path = "server.port";

    // Required: The value type
    using value_type = int;

    // Required: Default value factory
    static auto default_value() -> int { return 8080; }
};

}  // namespace settings
```

### 3. Create a Schema

Group all settings into a schema:

```cpp
using MySchema = cppfig::ConfigSchema<
    settings::ServerPort,
    settings::ServerHost,
    settings::LogLevel
>;
```

### 4. Use the Configuration

```cpp
int main() {
    // Create configuration manager with file path
    // Default serializer is ConfSerializer — no extra dependencies needed
    cppfig::Configuration<MySchema> config("config.conf");

    // Load configuration (creates file with defaults if missing)
    auto status = config.Load();
    if (!status.ok()) {
        std::cerr << "Failed to load: " << status.message() << std::endl;
        return 1;
    }

    // Read values with full type safety and IDE autocompletion
    int port = config.Get<settings::ServerPort>();

    // Modify values
    status = config.Set<settings::ServerPort>(9000);

    // Save changes
    config.Save();

    return 0;
}
```

### Generated config.conf

```conf
server.port = 8080
server.host = localhost
logging.level = info
```

### Using JSON Instead

If you prefer JSON files, enable the JSON feature and include the optional header:

```cpp
#include <cppfig/cppfig.h>
#include <cppfig/json.h>  // opt-in

cppfig::Configuration<MySchema, cppfig::JsonSerializer> config("config.json");
```

See [Serializers](serializers.md) for details on enabling JSON and other formats.

## Thread Safety

By default, `Configuration` uses `SingleThreadedPolicy`, which has zero synchronization overhead. If you need to access the configuration from multiple threads concurrently, use `MultiThreadedPolicy`:

```cpp
#include <cppfig/cppfig.h>

// Thread-safe configuration (.conf format, multi-threaded)
cppfig::Configuration<MySchema, cppfig::ConfSerializer, cppfig::MultiThreadedPolicy>
    config("config.conf");

// Thread-safe configuration (JSON format, multi-threaded)
// #include <cppfig/json.h>
// cppfig::Configuration<MySchema, cppfig::JsonSerializer, cppfig::MultiThreadedPolicy>
//     config("config.json");
```

| Policy | Overhead | Use case |
|--------|----------|----------|
| `SingleThreadedPolicy` (default) | None | Single-threaded or externally synchronized |
| `MultiThreadedPolicy` | `std::shared_mutex` | Concurrent reads and writes from multiple threads |

## Next Steps

- [Defining Settings](defining-settings.md) — All setting options (validators, env vars)
- [Custom Types](custom-types.md) — Use your own types in configuration
- [Serializers](serializers.md) — JSON, custom formats, and the built-in `.conf`
- [Validators](validators.md) — Built-in and custom validation
- [Testing](testing.md) — Mock configuration in unit tests
