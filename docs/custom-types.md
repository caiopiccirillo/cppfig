# Custom Types

Every value type needs a `cppfig::ConfigTraits<T>` specialization that converts between the type and its text in the `.conf` file. cppfig provides conversions for common types; you add your own the same way.

## Built-in conversions

| Type | Text | Notes |
| --- | --- | --- |
| `bool` | `true`/`false`, `yes`/`no`, `on`/`off`, `1`/`0` | Lowercase only; written as `true`/`false` |
| `short`, `int`, `long`, `long long`, their unsigned forms, `signed char`, `unsigned char`, and the `<cstdint>` aliases | Decimal, as `std::from_chars` reads it | Exact: no `+`, no prefix, no trailing text, range-checked. `char`, `wchar_t` and `charN_t` are text, not numbers, and have no conversion. |
| `float`, `double`, `long double` | `std::from_chars` general format | `long double` only where the standard library supports it |
| `std::string` | The text as is | Written quoted when needed; see [the format](conf-format.md#writing) |
| `std::filesystem::path` | The path in the native narrow encoding | |
| `std::chrono::duration<Rep, Period>` | An integer and a unit: `250ms`, `30s`, `5min`, `2h` | Needs `#include <cppfig/chrono.h>`. See below. |

### Durations

```cpp
#include <cppfig/chrono.h>

struct RequestTimeout {
    static constexpr std::string_view path = "http.timeout";
    using value_type = std::chrono::milliseconds;
    static constexpr auto default_value() -> std::chrono::milliseconds { return std::chrono::seconds { 30 }; }
};
```

- The unit is required. The units are `ns`, `us`, `ms`, `s`, `min` and `h`.
- Any unit can be used for any duration type, as long as the result is exact and fits: `2s` reads as 2000 milliseconds, and `1500us` is rejected for milliseconds.
- `Rep` must be an integer type, and `Period` one of those six units.
- Durations are written in their own unit, so the default above is saved as `30000ms`.

`<cppfig/chrono.h>` is separate from `<cppfig/cppfig.h>` because the full `<chrono>` header roughly doubles cppfig's parse time. Include it in the files that declare duration settings.

## Enums

Map each enumerator to a name with `EnumTraits` and a `constexpr` table:

```cpp
enum class LogLevel { kDebug, kInfo, kWarning, kError };

inline constexpr std::array<cppfig::EnumName<LogLevel>, 4> kLogLevelNames { {
    { .value = LogLevel::kDebug, .name = "debug" },
    { .value = LogLevel::kInfo, .name = "info" },
    { .value = LogLevel::kWarning, .name = "warning" },
    { .value = LogLevel::kError, .name = "error" },
} };

template <>
struct cppfig::ConfigTraits<LogLevel> : cppfig::EnumTraits<kLogLevelNames> { };
```

Names are matched exactly, so `INFO` and `1` are rejected. Names and values must each be unique, which is checked at compile time. List every value a setting can hold: an enumerator missing from the table is written as its underlying number, which then fails to load.

## Your own types

Specialize `ConfigTraits` with two static functions:

```cpp
struct Endpoint {
    std::string host;
    std::uint16_t port = 0;
};

template <>
struct cppfig::ConfigTraits<Endpoint> {
    static auto FromString(std::string_view text) -> std::optional<Endpoint>
    {
        const auto colon = text.rfind(':');
        if (colon == std::string_view::npos || colon == 0) {
            return std::nullopt;
        }
        auto port = ConfigTraits<std::uint16_t>::FromString(text.substr(colon + 1));
        if (!port) {
            return std::nullopt;
        }
        return Endpoint { std::string(text.substr(0, colon)), *port };
    }

    static auto ToString(const Endpoint& value) -> std::string
    {
        return value.host + ":" + ConfigTraits<std::uint16_t>::ToString(value.port);
    }
};

struct Upstream {
    static constexpr std::string_view path = "proxy.upstream";
    using value_type = Endpoint;
    static auto default_value() -> Endpoint { return { "localhost", 9000 }; }
};
```

```conf
proxy.upstream = backend.internal:9000
```

### Requirements

| Function | Contract |
| --- | --- |
| `static auto FromString(std::string_view) -> std::optional<T>` | Parses the whole text. Returns `std::nullopt` on any error rather than throwing. |
| `static auto ToString(const T&) -> std::string` | Produces text that `FromString` reads back to an equal value. |

- The text is a single value; quoting and escaping in the file are handled for you.
- Conversions run only while loading and when migrated defaults are written; `Get` never calls them.
- To build with `-fno-exceptions`, the conversions must not throw.
- Compose the built-in conversions, as `Endpoint` does with `ConfigTraits<std::uint16_t>`, instead of hand-parsing numbers.

The type itself needs no default constructor and no copy constructor: move-only types work.

A type without a conversion fails the `Configurable<T>` concept, and the setting fails `IsSetting`, at compile time.

## Validating custom types

Add a validator for rules beyond what parsing checks:

```cpp
struct Upstream {
    static constexpr std::string_view path = "proxy.upstream";
    using value_type = Endpoint;
    static auto default_value() -> Endpoint { return { "localhost", 9000 }; }
    static auto validator()
    {
        return cppfig::Predicate<Endpoint>([](const Endpoint& endpoint) { return endpoint.port != 0; }, "port must not be 0");
    }
};
```
