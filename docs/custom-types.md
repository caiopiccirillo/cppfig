# Custom Types

cppfig supports custom types through `ConfigTraits<T>` template specialization. This allows you to use any type in your configuration settings.

## Manual ConfigTraits Specialization (Recommended)

Implement all four static functions to teach cppfig how to handle your type:

```cpp
#include <cppfig/cppfig.h>

struct Color {
    uint8_t r, g, b, a;
};

template <>
struct cppfig::ConfigTraits<Color> {
    // Serialize to a cppfig::Value
    static auto Serialize(const Color& value) -> cppfig::Value {
        auto obj = cppfig::Value::Object();
        obj["r"] = cppfig::Value(static_cast<int64_t>(value.r));
        obj["g"] = cppfig::Value(static_cast<int64_t>(value.g));
        obj["b"] = cppfig::Value(static_cast<int64_t>(value.b));
        obj["a"] = cppfig::Value(static_cast<int64_t>(value.a));
        return obj;
    }

    // Deserialize from a cppfig::Value
    static auto Deserialize(const cppfig::Value& val) -> std::optional<Color> {
        try {
            return Color{
                static_cast<uint8_t>(val["r"].Get<int64_t>()),
                static_cast<uint8_t>(val["g"].Get<int64_t>()),
                static_cast<uint8_t>(val["b"].Get<int64_t>()),
                static_cast<uint8_t>(val["a"].Get<int64_t>())
            };
        } catch (...) {
            return std::nullopt;
        }
    }

    // Convert to string (for logging/debugging)
    static auto ToString(const Color& value) -> std::string {
        return "rgba(" + std::to_string(value.r) + "," +
               std::to_string(value.g) + "," +
               std::to_string(value.b) + "," +
               std::to_string(value.a) + ")";
    }

    // Parse from string (for environment variables)
    static auto FromString(std::string_view str) -> std::optional<Color> {
        // Parse format like "rgba(r,g,b,a)" or fall back
        return std::nullopt;
    }
};
```

### Use in Settings

```cpp
struct BackgroundColor {
    static constexpr std::string_view path = "ui.background_color";
    using value_type = Color;
    static auto default_value() -> Color { return Color{255, 255, 255, 255}; }
};
```

## Using nlohmann::json ADL (Requires JSON Feature)

If your type already has `to_json` / `from_json` functions and you have the
JSON feature enabled, you can use the `ConfigTraitsFromJsonAdl` helper:

### Step 1: Enable JSON support

```cmake
# CMakeLists.txt
set(CPPFIG_ENABLE_JSON ON)
```

### Step 2: Define Your Type with JSON Serialization

```cpp
#include <cppfig/json.h>  // must be included for ConfigTraitsFromJsonAdl

struct Point {
    int x = 0;
    int y = 0;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    // nlohmann::json ADL functions
    friend void to_json(nlohmann::json& j, const Point& p) {
        j = nlohmann::json{{"x", p.x}, {"y", p.y}};
    }

    friend void from_json(const nlohmann::json& j, Point& p) {
        j.at("x").get_to(p.x);
        j.at("y").get_to(p.y);
    }
};
```

### Step 3: Specialize ConfigTraits

```cpp
template <>
struct cppfig::ConfigTraits<Point> : cppfig::ConfigTraitsFromJsonAdl<Point> {};
```

## ConfigTraits Interface

The `ConfigTraits<T>` template must provide these static functions:

| Function | Purpose |
|----------|---------|
| `Serialize(const T&) -> Value` | Serialize value to a `cppfig::Value` |
| `Deserialize(const Value&) -> std::optional<T>` | Deserialize from a `cppfig::Value` |
| `ToString(const T&) -> std::string` | Convert to human-readable string |
| `FromString(std::string_view) -> std::optional<T>` | Parse from string (env vars) |

All functions must be `static`. Return `std::nullopt` from `Deserialize` and
`FromString` on parse failure.

## Complete Example

```cpp
#include <cppfig/cppfig.h>

// Custom type — no external dependencies required
struct ServerEndpoint {
    std::string host;
    int port;
    bool use_ssl;
};

// Register with cppfig using Value-based traits
template <>
struct cppfig::ConfigTraits<ServerEndpoint> {
    static auto Serialize(const ServerEndpoint& e) -> cppfig::Value {
        auto obj = cppfig::Value::Object();
        obj["host"] = cppfig::Value(e.host);
        obj["port"] = cppfig::Value(e.port);
        obj["use_ssl"] = cppfig::Value(e.use_ssl);
        return obj;
    }

    static auto Deserialize(const cppfig::Value& val) -> std::optional<ServerEndpoint> {
        try {
            return ServerEndpoint{
                val["host"].Get<std::string>(),
                static_cast<int>(val["port"].Get<int64_t>()),
                val["use_ssl"].Get<bool>()
            };
        } catch (...) {
            return std::nullopt;
        }
    }

    static auto ToString(const ServerEndpoint& e) -> std::string {
        return e.host + ":" + std::to_string(e.port) +
               (e.use_ssl ? " (SSL)" : "");
    }

    static auto FromString(std::string_view) -> std::optional<ServerEndpoint> {
        return std::nullopt;  // not supported via env var
    }
};

// Use in settings
namespace settings {

struct ApiServer {
    static constexpr std::string_view path = "api.server";
    using value_type = ServerEndpoint;
    static auto default_value() -> ServerEndpoint {
        return {"api.example.com", 443, true};
    }
};

}  // namespace settings

using MySchema = cppfig::ConfigSchema<settings::ApiServer>;

int main() {
    // Uses default .conf serializer — no JSON dependency
    cppfig::Configuration<MySchema> config("config.conf");
    config.Load();

    ServerEndpoint api = config.Get<settings::ApiServer>();
    std::cout << "API: " << api.host << ":" << api.port << std::endl;
}
```

## Validating Custom Types

```cpp
struct ApiServer {
    static constexpr std::string_view path = "api.server";
    using value_type = ServerEndpoint;
    static auto default_value() -> ServerEndpoint {
        return {"api.example.com", 443, true};
    }

    static auto validator() -> cppfig::Validator<ServerEndpoint> {
        return cppfig::Predicate<ServerEndpoint>(
            [](const ServerEndpoint& e) {
                return !e.host.empty() && e.port > 0 && e.port < 65536;
            },
            "Server endpoint must have non-empty host and valid port"
        );
    }
};
```
