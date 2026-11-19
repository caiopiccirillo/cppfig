# Validators

A validator checks a value that has the right type but may still be wrong: a port of 0, an empty host name, an unknown log level. Validators run while a configuration is created, never on `Get`.

## Declaring a validator

```cpp
struct ServerPort {
    static constexpr std::string_view path = "server.port";
    using value_type = int;
    static constexpr auto default_value() -> int { return 8080; }
    static constexpr auto validator() { return cppfig::Range(1, 65535); }
};
```

`validator()` may return any callable that takes `const value_type&` and returns `cppfig::ValidationResult`. Its type is checked at compile time: a validator for the wrong type does not compile.

## When validation runs

- When the registry is instantiated, for defaults that can be evaluated at compile time. A failing constant default is a compile error.
- During `Load`, for the default, the file value and the environment override, whichever are present. The first failure stops the load: the error names the setting and the source, and for file values gives `file:line:column`.
- During `FromDefaults`, for every default.
- During `MockConfigurationBuilder::Build`, for every value supplied by a test and every default.

A configuration therefore never contains an invalid value.

## Built-in validators

Built-in validators are small value types composed without allocation or type erasure. Most are `constexpr`.

### Numeric

| Validator | Accepts |
| --- | --- |
| `Min(minimum)` | `value >= minimum` |
| `Max(maximum)` | `value <= maximum` |
| `Range(minimum, maximum)` | `minimum <= value <= maximum` |
| `Positive<T>()` | `value > 0` |
| `NonNegative<T>()` | `value >= 0` |

The numeric validators take their type from the arguments, so match the setting's type: `Min<std::uint16_t>(1)` for a `std::uint16_t` setting, `Range(0.0, 1.0)` for a `double`.

### Strings

| Validator | Accepts |
| --- | --- |
| `NotEmpty()` | A non-empty string |
| `MinLength(n)` | At least `n` characters |
| `MaxLength(n)` | At most `n` characters |

### Any type

| Validator | Accepts |
| --- | --- |
| `OneOf<T>({ a, b, c })` | One of the listed values |
| `Predicate<T>(function, message)` | Values for which `function(value)` is true; otherwise fails with `message` |
| `AlwaysValid<T>()` | Everything |

## Combining validators

`And` requires both validators to pass and reports the first failure. `Or` passes when either does:

```cpp
static constexpr auto validator() { return cppfig::Min(1).And(cppfig::Max(100)); }

static auto validator() { return cppfig::NotEmpty().And(cppfig::MaxLength(255)); }

static constexpr auto validator() { return cppfig::Positive<int>().Or(cppfig::Range(-10, -1)); }
```

Combined validators are ordinary types, so combining them adds no allocation or indirect call.

## Custom validators

Any callable with the right signature works, including a lambda:

```cpp
struct Hostname {
    static constexpr std::string_view path = "server.host";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
    static auto validator()
    {
        return [](const std::string& host) -> cppfig::ValidationResult {
            if (host.find(' ') != std::string::npos) {
                return cppfig::ValidationResult::Error("host name '" + host + "' contains a space");
            }
            return cppfig::ValidationResult::Ok();
        };
    }
};
```

For a simple yes-or-no rule, `Predicate` is shorter:

```cpp
static auto validator()
{
    return cppfig::Predicate<int>([](int value) { return value % 2 == 0; }, "value must be even");
}
```

Make the validator and the default `constexpr` to have the default checked at compile time. The built-in numeric validators and `NotEmpty` are `constexpr`.

To build with `-fno-exceptions`, validators must not throw.

## ValidationResult

```cpp
struct ValidationResult {
    bool is_valid = true;
    std::string error_message;

    static constexpr auto Ok() -> ValidationResult;
    static constexpr auto Error(std::string message) -> ValidationResult;
    constexpr explicit operator bool() const;  // is_valid
};
```

The message is included in the error that `Load` returns, for example:

```
app.conf:7:15: server.port file value is invalid: Value 0 is outside [1, 65535]
```
