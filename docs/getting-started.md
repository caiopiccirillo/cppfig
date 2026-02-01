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

### Requirements

- C++20 compatible compiler (GCC 11+, Clang 14+, MSVC 2022+)
- [nlohmann/json](https://github.com/nlohmann/json) library
- [Abseil](https://github.com/abseil/abseil-cpp) (for `absl::Status` and `absl::StatusOr`)

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
    static constexpr std::string_view kPath = "server.port";

    // Required: The value type
    using ValueType = int;

    // Required: Default value factory
    static auto DefaultValue() -> int { return 8080; }
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
    cppfig::Configuration<MySchema> config("config.json");

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

## Next Steps

- [Defining Settings](defining-settings.md) - Learn about all setting options
- [Custom Types](custom-types.md) - Add support for your own types
- [Validators](validators.md) - Validate configuration values
- [Testing](testing.md) - Mock configuration in unit tests
