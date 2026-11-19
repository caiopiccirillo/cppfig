# Getting Started with cppfig

This guide installs cppfig, declares a few settings and walks through the lifecycle of a configuration: load, save if needed, read.

## Installation

cppfig is header-only and depends only on the standard library. It needs a C++20 compiler; GCC and Clang on Linux x86-64 are the supported targets.

### CMake

```cmake
add_subdirectory(cppfig)
target_link_libraries(your_target PRIVATE cppfig::cppfig)
```

After `cmake --install`, use the package instead:

```cmake
find_package(cppfig CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE cppfig::cppfig)
```

When cppfig is added with `add_subdirectory`, its tests, benchmarks and examples are off by default, so you do not need GoogleTest or Google Benchmark.

### Header-only

Copy `src/cppfig` into your include path.

## 1. Declare settings

A setting is a struct with a path, a value type and a default:

```cpp
#include <cppfig/cppfig.h>

namespace settings {

struct AppName {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "MyApplication"; }
};

struct ServerPort {
    static constexpr std::string_view path = "server.port";
    static constexpr std::string_view env_override = "SERVER_PORT";
    using value_type = int;
    static constexpr auto default_value() -> int { return 8080; }
    static constexpr auto validator() { return cppfig::Range(1, 65535); }
};

struct LoggingEnabled {
    static constexpr std::string_view path = "logging.enabled";
    using value_type = bool;
    static constexpr auto default_value() -> bool { return true; }
};

}  // namespace settings
```

[Defining Settings](defining-settings.md) covers every member.

## 2. List them in a registry

```cpp
struct AppRegistry : cppfig::Registry<settings::AppName, settings::ServerPort, settings::LoggingEnabled> { };
```

A `using AppRegistry = cppfig::Registry<...>;` alias works too. Deriving gives the registry a short name of its own, which keeps compiler errors and clangd hovers readable as the registry grows.

The registry is checked when it is instantiated. A duplicate path, a path that is a prefix of another, a malformed or duplicate environment variable name, or a `constexpr` default that fails its validator is a compile error.

## 3. Load

```cpp
auto loaded = cppfig::Configuration<AppRegistry>::Load("config.conf");
if (!loaded.ok()) {
    std::cerr << "Configuration error: " << loaded.status().message() << '\n';
    return 1;
}
```

`Load` reads the file, applies environment overrides, converts and validates every value, and returns a `LoadResult` holding two objects:

- `config`, the frozen `Configuration<AppRegistry>`: just the typed values.
- `document`, a `ConfDocument`: the file's text plus any settings that were missing from it.

If anything is wrong, such as a syntax error, an unknown or duplicate key, an unparsable value or a value its validator rejects, `Load` returns an error and no configuration. Errors that point into the file start with `config.conf:line:column:`.

A missing file is not an error: every setting takes its default, and the document holds a complete new file.

## 4. Save if needed

```cpp
if (loaded->document.NeedsSave()) {
    if (auto status = loaded->document.Save(); !status.ok()) {
        std::cerr << "Could not save: " << status.message() << '\n';
    }
}
```

`NeedsSave()` is true when the file did not exist or settings were missing from it. Saving writes the original text back unchanged and appends the missing settings with their defaults, so comments and formatting survive. Environment overrides are never written. The save is atomic: readers see either the old or the new file.

## 5. Read

```cpp
const auto& config = loaded->config;

const std::string& name = config.Get<settings::AppName>();
int port = config.Get<settings::ServerPort>();
bool logging = config.Get<settings::LoggingEnabled>();
```

`Get` returns a `const` reference into the configuration. It performs no lookup, parsing, validation, locking or allocation; the slot is chosen at compile time. Asking for a setting that is not in the registry does not compile.

### Keeping the configuration

The references `Get` returns live as long as the configuration. To keep the configuration and drop the document, move it out:

```cpp
cppfig::Configuration<AppRegistry> config = std::move(loaded->config);
```

Do this before taking references: moving a configuration invalidates references into the old object. Calling `Get` on a temporary configuration is a compile error, because the reference would dangle.

## The generated file

```conf
app.name = MyApplication
server.port = 8080
logging.enabled = true
```

See [The `.conf` Format](conf-format.md) for the full grammar.

## Configuration without a file

`FromDefaults()` builds a configuration from defaults alone, without reading a file or the environment. It is useful in tests and tools:

```cpp
auto defaults = cppfig::Configuration<AppRegistry>::FromDefaults();
```

## Threads

A configuration never changes after it is created, so any number of threads may call `Get` on it at the same time without locks. Create and publish it before other threads start reading, as you would any object. To pick up changes to the file or the environment, load a new configuration.

## Next steps

- [Defining Settings](defining-settings.md): every setting option and the compile-time checks
- [The `.conf` Format](conf-format.md): grammar, quoting, unknown keys, error messages
- [Custom Types](custom-types.md): durations, enums and your own types
- [Validators](validators.md): built-in and custom validation
- [Testing](testing.md): mock configurations for unit tests
