# Serializers

cppfig uses a pluggable serialization system. **The flat `.conf` format is the default** — zero extra dependencies. JSON, TOML, and other formats are available as opt-in features.

## Default Conf Serializer

The built-in `ConfSerializer` uses a simple flat key-value format and requires no additional dependencies:

```cpp
#include <cppfig/cppfig.h>

// Default — uses .conf serialization
cppfig::Configuration<MySchema> config("config.conf");
```

### Generated .conf

```conf
# Each line is a full dot-path = value
server.host = localhost
server.port = 8080
logging.enabled = true
logging.level = info
```

### Format Details

- Each line is `full.dot.path = value` — what you see is exactly what you write in code.
- Lines starting with `#` are comments.
- No sections, no grouping — one flat list per file.
- Value types are inferred automatically:
  - `true` / `false` / `yes` / `no` / `on` / `off` → `bool`
  - Integer literals → `int64_t`
  - Decimal literals → `double`
  - Everything else → `std::string`
- Quoted strings (`"hello world"`) preserve literal content including spaces.

## JSON Serializer (Optional)

Enable JSON support by adding the dependency and CMake option:

### CMake

```cmake
# Option 1: set the CMake option explicitly
set(CPPFIG_ENABLE_JSON ON)

# Option 2: if nlohmann-json is already available via find_package(),
#            it is auto-detected and CPPFIG_ENABLE_JSON is set automatically.
find_package(nlohmann_json CONFIG REQUIRED)
```

### vcpkg

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

### Usage

```cpp
#include <cppfig/cppfig.h>
#include <cppfig/json.h>  // opt-in header

// Explicitly select JsonSerializer as the second template parameter
cppfig::Configuration<MySchema, cppfig::JsonSerializer> config("config.json");
```

### Generated JSON

```json
{
    "server": {
        "host": "localhost",
        "port": 8080
    },
    "logging": {
        "enabled": true,
        "level": "info"
    }
}
```

## Future Serializers

| Format | Header | CMake Option | vcpkg Feature | Status |
|--------|--------|--------------|---------------|--------|
| Conf | `<cppfig/cppfig.h>` | *(always available)* | — |  Built-in |
| JSON | `<cppfig/json.h>` | `CPPFIG_ENABLE_JSON` | `json` |  Available |
| TOML | `<cppfig/toml.h>` | `CPPFIG_ENABLE_TOML` | `toml` |  Planned |
| YAML | `<cppfig/yaml.h>` | `CPPFIG_ENABLE_YAML` | `yaml` |  Planned |

## Implementing a Custom Serializer

To create a custom serializer, implement a struct that satisfies the `Serializer` concept:

```cpp
#include <cppfig/serializer.h>

struct MyFormatSerializer {
    // Required: type alias (must be cppfig::Value)
    using data_type = cppfig::Value;

    // Required: parse from input stream into a Value tree
    static auto Parse(std::istream& is) -> absl::StatusOr<cppfig::Value> {
        // ... read and convert to cppfig::Value ...
    }

    // Required: convert a Value tree to a string for saving
    static auto Stringify(const cppfig::Value& data) -> std::string {
        // ... serialize the Value tree ...
    }
};
```

### Using a Custom Serializer

```cpp
cppfig::Configuration<MySchema, MyFormatSerializer> config("config.myformat");

// Or create a template alias for convenience
template <typename Schema>
using MyFormatConfig = cppfig::Configuration<Schema, MyFormatSerializer>;

MyFormatConfig<MySchema> config("config.myformat");
```

## Serializer Concept Requirements

A serializer must provide:

| Member | Signature | Purpose |
|--------|-----------|---------|
| `data_type` | type alias (`Value`) | Internal data representation |
| `Parse` | `(std::istream&) → absl::StatusOr<Value>` | Parse from stream |
| `Stringify` | `(const Value&) → std::string` | Convert to string |

Path navigation (`GetAtPath`, `SetAtPath`, `HasPath`) and merging are handled
by `cppfig::Value` directly — serializers only need to convert between their
file format and a `Value` tree.

## File I/O Helpers

cppfig provides helper functions for file operations:

```cpp
// Read file into a Value tree via any serializer
auto result = cppfig::ReadFile<cppfig::ConfSerializer>("config.conf");
if (result.ok()) {
    cppfig::Value data = *result;
}

// Write a Value tree to a file
absl::Status status = cppfig::WriteFile<cppfig::ConfSerializer>(
    "config.conf", my_data
);
```

## Type Conversion

Serializers work with `ConfigTraits<T>` for type conversion:

```cpp
// In Configuration::Get<Setting>():
// 1. Get raw value from the Value tree
Value raw = data_.GetAtPath(Setting::path);

// 2. Convert to Setting's value type using ConfigTraits
auto value = ConfigTraits<value_type>::Deserialize(*raw);
```

`ConfigTraits<T>` works with `cppfig::Value` (not any specific serializer
format), so custom type traits are serializer-agnostic.
