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
auto loaded = cppfig::LoadFile<AppRegistry>("config.conf");
if (!loaded) {
    std::cerr << "Configuration error:\n" << loaded.error().message() << '\n';
    return 1;
}
```

`LoadFile` reads the file, applies environment overrides, converts and validates every value, and returns a `LoadResult` holding two objects:

- `config`, the frozen `Configuration<AppRegistry>`: just the typed values.
- `document`, a `ConfDocument`: the file's text plus any settings that were missing from it.

If anything is wrong, such as a syntax error, an unknown or duplicate key, an unparsable value or a value its validator rejects, loading returns an error and no configuration. The error lists every problem found, one per line, so a file can be fixed in one go. Errors that point into the file start with `config.conf:line:column:`. The values of environment variables are never quoted in errors, because they often hold secrets.

A missing file is not an error: every setting takes its default, and the document holds a complete new file.

`LoadFile` lives in `<cppfig/file.h>`, which `<cppfig/cppfig.h>` includes. Code that only reads a configuration can include `<cppfig/configuration.h>` alone and stay clear of the file system and POSIX headers.

### Loading from text

`Configuration<AppRegistry>::FromText(text, source)` does the same from text you already have, such as an embedded default file or a string received over the network. `source` names the text in error messages. `LoadFile` is a thin wrapper that reads the file and calls it.

### Choosing the environment

Overrides come from the process environment unless `LoadOptions::environment` says otherwise:

```cpp
const std::array<cppfig::EnvironmentVariable, 1> variables { { { "SERVER_PORT", "9000" } } };
auto loaded = cppfig::LoadFile<AppRegistry>("config.conf", { .environment = cppfig::Environment::From(variables) });
```

`Environment::None()` ignores the environment entirely. The environment is read once, in a single pass; like `getenv`, reading the process environment must not race with `setenv`.

## 4. Save if needed

```cpp
if (loaded->document.NeedsSave()) {
    if (auto status = cppfig::SaveFile("config.conf", loaded->document); !status.ok()) {
        std::cerr << "Could not save: " << status.message() << '\n';
    }
}
```

`NeedsSave()` is true when settings were missing from the file, which includes a file that did not exist. Saving writes the original text back unchanged and appends the missing settings with their defaults, so comments and formatting survive. Environment overrides are never written. The save is atomic: readers see either the old or the new file.

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

`FromDefaults()` builds a configuration from defaults alone, without reading a file or the environment. `ConfigurationBuilder` does the same with some values set in code; it is what tests use:

```cpp
auto defaults = cppfig::Configuration<AppRegistry>::FromDefaults();
auto custom = cppfig::ConfigurationBuilder<AppRegistry> {}.Set<settings::ServerPort>(9000).Build();
```

Defaults are built only when they are used. When loading, a setting present in the file never runs its `default_value()`.

## Threads

A configuration never changes after it is created, so any number of threads may call `Get` on it at the same time without locks. Create and publish it before other threads start reading, as you would any object.

## Reloading

A configuration is never modified; to pick up changes to the file or the environment, load a new one and swap it in. Readers that hold a `std::shared_ptr` keep the version they started with until they are done:

```cpp
std::atomic<std::shared_ptr<const cppfig::Configuration<AppRegistry>>> current;

void Reload()
{
    auto loaded = cppfig::LoadFile<AppRegistry>("config.conf");
    if (!loaded) {
        log_error(loaded.error().message());  // keep serving the previous configuration
        return;
    }
    current.store(std::make_shared<const cppfig::Configuration<AppRegistry>>(std::move(loaded->config)));
}

void HandleRequest()
{
    const auto config = current.load();  // one consistent snapshot for the whole request
    Serve(config->Get<settings::ServerPort>());
}
```

Keeping the configuration on the heap this way also keeps it off the stack, which matters for registries with hundreds of settings: a configuration is as large as its values.

## Next steps

- [Defining Settings](defining-settings.md): every setting option and the compile-time checks
- [The `.conf` Format](conf-format.md): grammar, quoting, unknown keys, error messages
- [Custom Types](custom-types.md): durations, enums and your own types
- [Validators](validators.md): built-in and custom validation
- [Testing](testing.md): building configurations in tests
