# Validators

cppfig provides a composable validation system for ensuring configuration values meet your requirements.

## Built-in Validators

### Numeric Validators

```cpp
#include <cppfig/cppfig.h>
using namespace cppfig;

// Minimum value
auto v1 = Min(0);           // value >= 0
auto v2 = Min(1.5);         // value >= 1.5

// Maximum value
auto v3 = Max(100);         // value <= 100
auto v4 = Max(99.9);        // value <= 99.9

// Range (inclusive)
auto v5 = Range(1, 65535);  // 1 <= value <= 65535
auto v6 = Range(0.0, 1.0);  // 0.0 <= value <= 1.0

// Positive (> 0)
auto v7 = Positive<int>();
auto v8 = Positive<double>();

// Non-negative (>= 0)
auto v9 = NonNegative<int>();
auto v10 = NonNegative<double>();
```

### String Validators

```cpp
// Not empty
auto v1 = NotEmpty();

// Length constraints
auto v2 = MinLength(3);     // length >= 3
auto v3 = MaxLength(255);   // length <= 255
```

### Generic Validators

```cpp
// Value must be one of the allowed values
auto v1 = OneOf<std::string>({"debug", "info", "warn", "error"});
auto v2 = OneOf<int>({1, 2, 3, 5, 8, 13});

// Custom predicate
auto v3 = Predicate<std::string>(
    [](const std::string& s) { return s.find('@') != std::string::npos; },
    "Value must contain '@'"
);

// Always valid (default)
auto v4 = AlwaysValid<int>();
```

## Combining Validators

Validators can be combined using `And` and `Or`:

```cpp
// Both conditions must pass
auto port_validator = Min(1).And(Max(65535));

// Alternative: use Range
auto port_validator2 = Range(1, 65535);

// Either condition can pass
auto level_validator = OneOf<std::string>({"low", "medium", "high"})
    .Or(Predicate<std::string>(
        [](const std::string& s) { return s.starts_with("custom_"); },
        "Must be a standard level or start with 'custom_'"
    ));

// Complex validation
auto password_validator = MinLength(8)
    .And(MaxLength(128))
    .And(Predicate<std::string>(
        [](const std::string& s) {
            return std::any_of(s.begin(), s.end(), ::isdigit);
        },
        "Must contain at least one digit"
    ))
    .And(Predicate<std::string>(
        [](const std::string& s) {
            return std::any_of(s.begin(), s.end(), ::isupper);
        },
        "Must contain at least one uppercase letter"
    ));
```

## Using Validators in Settings

Add a `GetValidator()` static function to your setting:

```cpp
struct ServerPort {
    static constexpr std::string_view kPath = "server.port";
    using ValueType = int;
    static auto DefaultValue() -> int { return 8080; }

    static auto GetValidator() -> cppfig::Validator<int> {
        return cppfig::Range(1, 65535);
    }
};

struct LogLevel {
    static constexpr std::string_view kPath = "logging.level";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "info"; }

    static auto GetValidator() -> cppfig::Validator<std::string> {
        return cppfig::OneOf<std::string>({"debug", "info", "warn", "error"});
    }
};

struct ApiKey {
    static constexpr std::string_view kPath = "api.key";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return ""; }

    static auto GetValidator() -> cppfig::Validator<std::string> {
        return cppfig::MinLength(32).And(cppfig::MaxLength(64));
    }
};
```

## Validation Behavior

### On Set

When calling `config.Set<Setting>(value)`, the validator is checked:

```cpp
auto status = config.Set<settings::ServerPort>(99999);
if (!status.ok()) {
    // Validation failed
    std::cerr << status.message() << std::endl;
    // Output: "Value 99999 exceeds maximum 65535"
}
```

### ValidateAll

Validate all current values at once:

```cpp
auto status = config.ValidateAll();
if (!status.ok()) {
    std::cerr << "Configuration invalid: " << status.message() << std::endl;
}
```

### Validation on Load

Values loaded from file are **not** automatically validated. Call `ValidateAll()` after loading if needed:

```cpp
auto status = config.Load();
if (!status.ok()) { /* handle error */ }

status = config.ValidateAll();
if (!status.ok()) {
    std::cerr << "Invalid configuration in file: " << status.message() << std::endl;
}
```

## Custom Validator Function

For complex validation logic, use a lambda:

```cpp
auto ip_validator = cppfig::Predicate<std::string>(
    [](const std::string& ip) {
        // Simple IPv4 validation
        std::regex pattern(R"(^(\d{1,3}\.){3}\d{1,3}$)");
        if (!std::regex_match(ip, pattern)) return false;

        // Check each octet
        std::istringstream iss(ip);
        std::string octet;
        while (std::getline(iss, octet, '.')) {
            int n = std::stoi(octet);
            if (n < 0 || n > 255) return false;
        }
        return true;
    },
    "Must be a valid IPv4 address"
);
```

## ValidationResult

Validators return a `ValidationResult`:

```cpp
cppfig::ValidationResult result = validator(value);

if (result) {
    // or: if (result.is_valid)
    std::cout << "Valid!" << std::endl;
} else {
    std::cout << "Invalid: " << result.error_message << std::endl;
}
```

You can also create results manually:

```cpp
return cppfig::ValidationResult::Ok();
return cppfig::ValidationResult::Error("Something went wrong");
```
