# Serializers

cppfig uses a pluggable serialization system. JSON is the default, but you can implement custom serializers for YAML, TOML, or any other format.

## Default JSON Serializer

The built-in `JsonSerializer` uses nlohmann::json:

```cpp
// Default usage - JSON serialization
cppfig::Configuration<MySchema> config("config.json");
// or explicitly:
cppfig::Configuration<MySchema, cppfig::JsonSerializer> config("config.json");
```

## Implementing a Custom Serializer

To create a custom serializer, implement a struct that satisfies the `Serializer` concept:

```cpp
#include <cppfig/serializer.h>

struct YamlSerializer {
    // The data type used internally (usually the library's document type)
    using data_type = YAML::Node;  // Example using yaml-cpp

    // Parse from input stream
    static auto Parse(std::istream& is) -> absl::StatusOr<data_type> {
        try {
            return YAML::Load(is);
        } catch (const YAML::Exception& e) {
            return absl::InvalidArgumentError(
                std::string("YAML parse error: ") + e.what()
            );
        }
    }

    // Convert to string for saving
    static auto Stringify(const data_type& data, int indent = 2) -> std::string {
        YAML::Emitter out;
        out << data;
        return out.c_str();
    }

    // Merge two data structures (for schema migration)
    static auto Merge(const data_type& base, const data_type& overlay) -> data_type {
        // Deep merge implementation
        data_type result = Clone(base);
        MergeNodes(result, overlay);
        return result;
    }

    // Get value at dot-separated path
    static auto GetAtPath(const data_type& data, std::string_view path)
        -> absl::StatusOr<data_type> {
        // Navigate path like "server.port"
        data_type current = data;
        // ... path navigation logic
        return current;
    }

    // Set value at dot-separated path
    static void SetAtPath(data_type& data, std::string_view path,
                          const data_type& value) {
        // Navigate and set
    }

    // Check if path exists
    static auto HasPath(const data_type& data, std::string_view path) -> bool {
        return GetAtPath(data, path).ok();
    }
};
```

## Serializer Concept Requirements

A serializer must provide:

| Function | Signature | Purpose |
|----------|-----------|---------|
| `data_type` | type alias | Internal data representation |
| `Parse` | `(istream&) -> StatusOr<data_type>` | Parse from stream |
| `Stringify` | `(data_type) -> string` | Convert to string |
| `Merge` | `(data_type, data_type) -> data_type` | Deep merge for migration |

Additionally, these are used by Configuration internally:
| Function | Purpose |
|----------|---------|
| `GetAtPath` | Navigate hierarchical structure |
| `SetAtPath` | Set value at path |
| `HasPath` | Check if path exists |

## Using a Custom Serializer

```cpp
// Use YAML serializer
cppfig::Configuration<MySchema, YamlSerializer> config("config.yaml");

// Or with template alias
template <typename Schema>
using YamlConfiguration = cppfig::Configuration<Schema, YamlSerializer>;

YamlConfiguration<MySchema> config("config.yaml");
```

## JsonSerializer API

The built-in `JsonSerializer` provides:

```cpp
struct JsonSerializer {
    using data_type = nlohmann::json;

    // Parse from stream
    static auto Parse(std::istream& is) -> absl::StatusOr<data_type>;

    // Parse from string (convenience)
    static auto ParseString(std::string_view str) -> absl::StatusOr<data_type>;

    // Convert to formatted string
    static auto Stringify(const data_type& data, int indent = 4) -> std::string;

    // Deep merge two JSON objects
    static auto Merge(const data_type& base, const data_type& overlay) -> data_type;

    // Get value at dot-separated path
    static auto GetAtPath(const data_type& data, std::string_view path)
        -> absl::StatusOr<data_type>;

    // Set value at dot-separated path (creates intermediate objects)
    static void SetAtPath(data_type& data, std::string_view path,
                          const data_type& value);

    // Check if path exists
    static auto HasPath(const data_type& data, std::string_view path) -> bool;
};
```

## File I/O Helpers

cppfig provides helper functions for file operations:

```cpp
// Read file into serializer's data type
auto result = cppfig::ReadFile<cppfig::JsonSerializer>("config.json");
if (result.ok()) {
    nlohmann::json data = *result;
}

// Write data to file
absl::Status status = cppfig::WriteFile<cppfig::JsonSerializer>(
    "config.json",
    my_json_data
);
```

## Example: TOML Serializer Sketch

```cpp
#include <toml++/toml.h>

struct TomlSerializer {
    using data_type = toml::table;

    static auto Parse(std::istream& is) -> absl::StatusOr<data_type> {
        try {
            return toml::parse(is);
        } catch (const toml::parse_error& e) {
            return absl::InvalidArgumentError(e.what());
        }
    }

    static auto Stringify(const data_type& data, int = 0) -> std::string {
        std::ostringstream ss;
        ss << data;
        return ss.str();
    }

    static auto Merge(const data_type& base, const data_type& overlay) -> data_type {
        data_type result = base;
        for (auto&& [k, v] : overlay) {
            if (auto* base_table = result[k].as_table();
                base_table && v.is_table()) {
                *base_table = Merge(*base_table, *v.as_table());
            } else {
                result.insert_or_assign(k, v);
            }
        }
        return result;
    }

    // ... GetAtPath, SetAtPath, HasPath implementations
};
```

## Type Conversion

Serializers work with `ConfigTraits<T>` for type conversion:

```cpp
// In Configuration::Get<Setting>():
// 1. Get raw value from serializer's data type
auto json_value = Serializer::GetAtPath(data_, Setting::path);

// 2. Convert to Setting's value type using ConfigTraits
auto value = ConfigTraits<value_type>::FromJson(*json_value);
```

Ensure your custom types' `ConfigTraits` can work with your serializer's `data_type`. For non-JSON serializers, you may need to implement custom conversion logic.
