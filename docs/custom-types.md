# Custom Types

cppfig supports custom types through the `ConfigTraits<T>` template specialization. This allows you to use any type in your configuration.

## Using nlohmann::json ADL (Recommended)

If your type already has `to_json` and `from_json` functions (nlohmann::json ADL), you can use the helper:

### Step 1: Define Your Type with JSON Serialization

```cpp
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

### Step 2: Specialize ConfigTraits

```cpp
template <>
struct cppfig::ConfigTraits<Point> : cppfig::ConfigTraitsFromJsonAdl<Point> {};
```

### Step 3: Use in Settings

```cpp
struct Origin {
    static constexpr std::string_view path = "geometry.origin";
    using value_type = Point;
    static auto default_value() -> Point { return Point{0, 0}; }
};
```

## Manual ConfigTraits Specialization

For full control, implement all four functions:

```cpp
struct Color {
    uint8_t r, g, b, a;
};

template <>
struct cppfig::ConfigTraits<Color> {
    // Convert to JSON
    static auto ToJson(const Color& value) -> nlohmann::json {
        return nlohmann::json{
            {"r", value.r},
            {"g", value.g},
            {"b", value.b},
            {"a", value.a}
        };
    }

    // Convert from JSON
    static auto FromJson(const nlohmann::json& json) -> std::optional<Color> {
        try {
            return Color{
                json.at("r").get<uint8_t>(),
                json.at("g").get<uint8_t>(),
                json.at("b").get<uint8_t>(),
                json.at("a").get<uint8_t>()
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
        // Parse "rgba(r,g,b,a)" format or JSON format
        try {
            auto json = nlohmann::json::parse(str);
            return FromJson(json);
        } catch (...) {
            return std::nullopt;
        }
    }
};
```

## Complete Example

```cpp
#include <cppfig/cppfig.h>

// Custom type
struct ServerEndpoint {
    std::string host;
    int port;
    bool use_ssl;

    friend void to_json(nlohmann::json& j, const ServerEndpoint& e) {
        j = nlohmann::json{
            {"host", e.host},
            {"port", e.port},
            {"use_ssl", e.use_ssl}
        };
    }

    friend void from_json(const nlohmann::json& j, ServerEndpoint& e) {
        j.at("host").get_to(e.host);
        j.at("port").get_to(e.port);
        j.at("use_ssl").get_to(e.use_ssl);
    }
};

// Register with cppfig
template <>
struct cppfig::ConfigTraits<ServerEndpoint>
    : cppfig::ConfigTraitsFromJsonAdl<ServerEndpoint> {};

// Use in settings
namespace settings {

struct ApiServer {
    static constexpr std::string_view path = "api.server";
    using value_type = ServerEndpoint;
    static auto default_value() -> ServerEndpoint {
        return {"api.example.com", 443, true};
    }
};

struct DatabaseServer {
    static constexpr std::string_view path = "database.server";
    using value_type = ServerEndpoint;
    static auto default_value() -> ServerEndpoint {
        return {"localhost", 5432, false};
    }
};

}  // namespace settings

// Schema
using MySchema = cppfig::ConfigSchema<
    settings::ApiServer,
    settings::DatabaseServer
>;

int main() {
    cppfig::Configuration<MySchema> config("config.json");
    config.Load();

    // Use custom type
    ServerEndpoint api = config.Get<settings::ApiServer>();
    std::cout << "API: " << api.host << ":" << api.port << std::endl;

    // Modify
    api.port = 8443;
    config.Set<settings::ApiServer>(api);
    config.Save();
}
```

## ConfigTraits Interface

The `ConfigTraits<T>` template must provide these static functions:

| Function | Purpose |
|----------|---------|
| `ToJson(const T&) -> nlohmann::json` | Serialize value to JSON |
| `FromJson(const nlohmann::json&) -> std::optional<T>` | Deserialize from JSON |
| `ToString(const T&) -> std::string` | Convert to human-readable string |
| `FromString(std::string_view) -> std::optional<T>` | Parse from string (env vars) |

All functions must be `static`. Return `std::nullopt` from `FromJson` and `FromString` on parse failure.

## Validating Custom Types

You can add validators for custom types:

```cpp
struct ServerEndpoint {
    static constexpr std::string_view path = "server";
    using value_type = ServerEndpoint;
    static auto default_value() -> ServerEndpoint { return {"localhost", 8080, false}; }

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
