# cppfig - Modern C++20 Configuration Library

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](https://github.com/caiopiccirillo/cppfig)
[![Documentation](https://img.shields.io/badge/docs-online-blue.svg)](https://caiopiccirillo.github.io/cppfig/)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A header-only C++20 configuration library. You declare settings as types and list them in a registry; cppfig checks the registry at compile time, loads a flat `.conf` file and environment overrides once, and gives you a frozen configuration whose reads are a single load from typed storage.

```cpp
const int& port = config.Get<settings::ServerPort>();  // no lookup, no parsing, no allocation
```

## Features

- **Typed reads at the cost of a field access.** `Get<S>()` returns `const S::value_type&` from a slot chosen at compile time. Reading an integer takes about 0.2 ns, with zero allocations ([results](docs/BENCHMARK_RESULTS.md)).
- **Compile-time checks.** Duplicate or malformed paths, duplicate or malformed environment variable names, unregistered settings, missing conversions, mismatched validators and constant defaults that fail their validator are all compile errors.
- **IDE-friendly.** Settings are plain structs; clangd completes `Get<settings::...>()` and shows the real value type on hover.
- **Validated, all-or-nothing initialization.** Defaults, file values and environment overrides are converted and validated once. Loading either yields a complete valid configuration or every error found, each with `file:line:column`.
- **Lock-free concurrent reads.** The configuration is immutable once loaded, so any number of threads can read it without synchronization.
- **Comment-preserving saves and migrations.** Saving writes your file back untouched, appending only settings it is missing. Saves are atomic, are flushed to disk before they replace the file, and keep the file's permissions, its owner where allowed, and symlinks.
- **Lean memory.** The configuration holds the values and nothing else, ordered by alignment so that GCC and Clang leave no padding between them; the source text lives in a separate document you can discard.
- **No exceptions required.** Errors are returned as `Status`/`StatusOr`; the library builds with `-fno-exceptions`.
- **Testable.** `ConfigurationBuilder` makes a real configuration from values set in code, and `Environment::From` replaces the process environment, so tests need neither files nor `setenv`.

## Quick Start

### 1. Declare settings and a registry

```cpp
#include <cppfig/cppfig.h>

namespace settings {

struct ServerHost {
    static constexpr std::string_view path = "server.host";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};

struct ServerPort {
    static constexpr std::string_view path = "server.port";
    static constexpr std::string_view env_override = "SERVER_PORT";  // optional
    using value_type = std::uint16_t;
    static constexpr auto default_value() -> std::uint16_t { return 8080; }
    static constexpr auto validator() { return cppfig::Min<std::uint16_t>(1); }  // optional
};

}  // namespace settings

struct AppRegistry : cppfig::Registry<settings::ServerHost, settings::ServerPort> { };
```

### 2. Load, save if needed, and read

```cpp
auto loaded = cppfig::LoadFile<AppRegistry>("app.conf");
if (!loaded) {
    std::cerr << loaded.error().message() << '\n';  // e.g. app.conf:3:15: cannot parse 'abc' as a value of 'server.port'
    return 1;
}

// Creates the file, or appends settings missing from it, only when needed.
if (loaded->document.NeedsSave()) {
    if (auto status = cppfig::SaveFile("app.conf", loaded->document); !status.ok()) {
        std::cerr << status.message() << '\n';
    }
}

const auto& config = loaded->config;
std::cout << config.Get<settings::ServerHost>() << ':' << config.Get<settings::ServerPort>() << '\n';
```

### 3. The generated `app.conf`

```conf
server.host = localhost
server.port = 8080
```

Edit it freely: comments, ordering and spacing are preserved. Values come from, in order of precedence, the environment override, the file, and the default.

## Documentation

| Guide | Contents |
| --- | --- |
| [Getting Started](docs/getting-started.md) | Installation, first configuration, lifecycle |
| [Defining Settings](docs/defining-settings.md) | Setting members, registries, compile-time checks, precedence |
| [The `.conf` Format](docs/conf-format.md) | Grammar, quoting, comments, unknown keys, error messages |
| [Custom Types](docs/custom-types.md) | Built-in conversions, durations, enums, your own types |
| [Validators](docs/validators.md) | Built-in validators, composition, custom validators |
| [Testing](docs/testing.md) | `ConfigurationBuilder`, replaceable environment, loading from text |
| [Benchmark Results](docs/BENCHMARK_RESULTS.md) | Read, load, save, memory and compile-time measurements |

## Installation

### Requirements

- A C++20 compiler and standard library with `constexpr std::string`: GCC 12 or newer, or Clang with libstdc++ 12 or newer. Validators are `constexpr` so that constant defaults are checked at compile time, and their results carry a `std::string`.
- Linux x86-64 is the supported target. CI builds with GCC 15 and Clang 22; GCC 12 has also been checked.
- No dependencies beyond the standard library

### CMake

```cmake
add_subdirectory(cppfig)          # or FetchContent
target_link_libraries(your_target PRIVATE cppfig::cppfig)
```

or, after `cmake --install`:

```cmake
find_package(cppfig CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE cppfig::cppfig)
```

### Header-only

Copy `src/cppfig` into your include path and `#include <cppfig/cppfig.h>`.

## Development

```bash
cmake --workflow --preset=debug-gcc         # or debug-clang, release-gcc, release-clang
ctest --test-dir build/debug/gcc --output-on-failure
ctest --test-dir build/debug/gcc -LE compile-scale   # skip the 1024-setting compile-time test

cmake --workflow --preset=sanitize-clang    # ASan + UBSan, runs the tests
cmake --workflow --preset=tsan-clang        # ThreadSanitizer, runs the tests
cmake --workflow --preset=coverage          # then: ninja -C build/coverage coverage
cmake --workflow --preset=fuzz-clang        # libFuzzer + ASan/UBSan, 30 s per fuzzer

./build/release/gcc/benchmark/cppfig_benchmark
```

### Fuzzing

`fuzz/` holds three libFuzzer targets: the `.conf` parser and writer, the value conversions, and loading end to end. Each checks properties rather than only looking for crashes, for example that every value the parser reads is written back and read again unchanged. Every test build also replays the checked-in corpus in `fuzz/corpus/` with any compiler, so inputs that once failed stay fixed.

For a longer run than the preset's:

```bash
./build/fuzz/clang/fuzz/load_fuzz -max_total_time=600 -dict=fuzz/conf.dict \
    build/fuzz/clang/fuzz/load_fuzz_corpus fuzz/corpus/load_fuzz
```

New inputs collect in the build tree. When a fuzzer finds a failure, fix it and copy the crashing input into `fuzz/corpus/<target>/`.

### Project structure

```
src/cppfig/
├── cppfig.h          # Umbrella header
├── schema.h          # Registry: membership, indexing, compile-time checks
├── setting.h         # Setting concepts
├── storage.h         # Typed, alignment-ordered value storage
├── configuration.h   # Configuration, FromText, FromDefaults, ConfigurationBuilder
├── document.h        # ConfDocument: source text, migrations, unknown keys
├── environment.h     # Environment: the process environment or a fixed set
├── file.h            # LoadFile, SaveFile (atomic), ReadConfFile
├── conf.h            # .conf parser and value escaping
├── traits.h          # ConfigTraits conversions, EnumTraits
├── chrono.h          # Opt-in std::chrono::duration conversions
├── validator.h       # Validators
└── status.h          # Status and StatusOr
test/                 # Unit, compile-fail and compile-scale tests
fuzz/                 # libFuzzer targets and their corpus
benchmark/            # Google Benchmark suites
examples/             # Example program
docs/                 # Guides and measurements
```

## Contributing

1. Follow the existing code style (`clang-format`)
2. Run static analysis (`clang-tidy`)
3. Add tests for new features
4. Update the documentation

## License

MIT License - see [LICENSE](LICENSE) for details.
