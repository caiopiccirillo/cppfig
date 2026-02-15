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

## Thread Safety

By default, `Configuration` uses `SingleThreadedPolicy`, which has zero synchronization overhead. If you need to access the configuration from multiple threads concurrently, use `MultiThreadedPolicy`:

```cpp
#include <cppfig/cppfig.h>

// Thread-safe configuration with reader-writer locking:
cppfig::Configuration<MySchema, cppfig::JsonSerializer, cppfig::MultiThreadedPolicy>
    config("config.json");
```

With `MultiThreadedPolicy`:

- **Multiple threads may call `Get` concurrently** — reads acquire a shared (reader) lock.
- **`Set` and `Load` acquire an exclusive (writer) lock** — they mutate internal state and will block until all readers finish.
- **`Save`, `Diff`, and `ValidateAll` acquire a shared (reader) lock** — they only read internal state.
- **Validation in `Set` runs before the exclusive lock is acquired**, so invalid values never block readers.
- **`GetFilePath` requires no lock** — the file path is immutable after construction.

### Choosing a Policy

| Policy | Overhead | Use case |
|--------|----------|----------|
| `SingleThreadedPolicy` (default) | None | Single-threaded applications, or when access is externally synchronized |
| `MultiThreadedPolicy` | `std::shared_mutex` | Concurrent reads and writes from multiple threads |

### Example

```cpp
#include <cppfig/cppfig.h>
#include <thread>

using SafeConfig = cppfig::Configuration<
    MySchema, cppfig::JsonSerializer, cppfig::MultiThreadedPolicy>;

int main() {
    SafeConfig config("config.json");
    config.Load();

    // Reader threads — run concurrently
    std::thread reader1([&] {
        int port = config.Get<settings::ServerPort>();
    });
    std::thread reader2([&] {
        std::string host = config.Get<settings::ServerHost>();
    });

    // Writer thread — acquires exclusive access
    std::thread writer([&] {
        config.Set<settings::ServerPort>(9000);
        config.Save();
    });

    reader1.join();
    reader2.join();
    writer.join();
}
```

> **Note:** `GetFileValues()` and `GetDefaults()` return references to internal data and are **not** protected after the call returns. In multi-threaded code, prefer `Get<Setting>()` for safe access to individual values.

## Next Steps

- [Defining Settings](defining-settings.md) - Learn about all setting options
- [Custom Types](custom-types.md) - Add support for your own types
- [Validators](validators.md) - Validate configuration values
- [Testing](testing.md) - Mock configuration in unit tests
