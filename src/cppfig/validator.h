#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace cppfig {

/// @brief Result of a validation operation.
struct ValidationResult {
    bool is_valid = true;
    std::string error_message;

    /// @brief Creates a successful validation result.
    static auto Ok() -> ValidationResult { return { .is_valid=true, .error_message="" }; }

    /// @brief Creates a failed validation result with an error message.
    static auto Error(std::string message) -> ValidationResult { return { .is_valid=false, .error_message=std::move(message) }; }

    explicit operator bool() const { return is_valid; }
};

/// @brief Concept for validator types.
template <typename V, typename T>
concept ValidatorFor = requires(const V& validator, const T& value) {
    { validator(value) } -> std::same_as<ValidationResult>;
};

/// @brief Type-erased validator that can hold any validation function.
template <typename T>
class Validator {
public:
    using ValidatorFn = std::function<ValidationResult(const T&)>;

    /// @brief Creates an always-valid validator.
    Validator()
        : fn_([](const T&) { return ValidationResult::Ok(); })
    {
    }

    /// @brief Creates a validator from a function.
    explicit Validator(ValidatorFn fn)
        : fn_(std::move(fn))
    {
    }

    /// @brief Validates a value.
    auto operator()(const T& value) const -> ValidationResult { return fn_(value); }

    /// @brief Combines this validator with another (both must pass).
    auto And(Validator<T> other) const -> Validator<T>
    {
        auto this_fn = fn_;
        auto other_fn = other.fn_;
        return Validator<T>([this_fn, other_fn](const T& value) -> ValidationResult {
            auto result = this_fn(value);
            if (!result) {
                return result;
            }
            return other_fn(value);
        });
    }

    /// @brief Combines this validator with another (either must pass).
    auto Or(Validator<T> other) const -> Validator<T>
    {
        auto this_fn = fn_;
        auto other_fn = other.fn_;
        return Validator<T>([this_fn, other_fn](const T& value) -> ValidationResult {
            auto result = this_fn(value);
            if (result) {
                return result;
            }
            return other_fn(value);
        });
    }

private:
    ValidatorFn fn_;
};

/// @brief Creates a validator that checks if a numeric value is at least min.
template <typename T>
    requires std::is_arithmetic_v<T>
auto Min(T min_value) -> Validator<T>
{
    return Validator<T>([min_value](const T& value) -> ValidationResult {
        if (value < min_value) {
            return ValidationResult::Error("Value " + std::to_string(value) + " is less than minimum " + std::to_string(min_value));
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates a validator that checks if a numeric value is at most max.
template <typename T>
    requires std::is_arithmetic_v<T>
auto Max(T max_value) -> Validator<T>
{
    return Validator<T>([max_value](const T& value) -> ValidationResult {
        if (value > max_value) {
            return ValidationResult::Error("Value " + std::to_string(value) + " exceeds maximum " + std::to_string(max_value));
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates a validator that checks if a numeric value is within [min, max].
template <typename T>
    requires std::is_arithmetic_v<T>
auto Range(T min_value, T max_value) -> Validator<T>
{
    return Min(min_value).And(Max(max_value));
}

/// @brief Creates a validator that checks if a numeric value is positive.
template <typename T>
    requires std::is_arithmetic_v<T>
auto Positive() -> Validator<T>
{
    return Validator<T>([](const T& value) -> ValidationResult {
        if (value <= T { 0 }) {
            return ValidationResult::Error("Value must be positive");
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates a validator that checks if a numeric value is non-negative.
template <typename T>
    requires std::is_arithmetic_v<T>
auto NonNegative() -> Validator<T>
{
    return Validator<T>([](const T& value) -> ValidationResult {
        if (value < T { 0 }) {
            return ValidationResult::Error("Value must be non-negative");
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates a validator that checks if a string is not empty.
inline auto NotEmpty() -> Validator<std::string>
{
    return Validator<std::string>([](const std::string& value) -> ValidationResult {
        if (value.empty()) {
            return ValidationResult::Error("Value must not be empty");
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates a validator that checks if a string length is at most max.
inline auto MaxLength(std::size_t max_len) -> Validator<std::string>
{
    return Validator<std::string>([max_len](const std::string& value) -> ValidationResult {
        if (value.size() > max_len) {
            return ValidationResult::Error("String length " + std::to_string(value.size()) + " exceeds maximum " + std::to_string(max_len));
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates a validator that checks if a string length is at least min.
inline auto MinLength(std::size_t min_len) -> Validator<std::string>
{
    return Validator<std::string>([min_len](const std::string& value) -> ValidationResult {
        if (value.size() < min_len) {
            return ValidationResult::Error("String length " + std::to_string(value.size()) + " is less than minimum " + std::to_string(min_len));
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates a validator that checks if a value is one of the allowed values.
template <typename T>
auto OneOf(std::vector<T> allowed_values) -> Validator<T>
{
    return Validator<T>([allowed = std::move(allowed_values)](const T& value) -> ValidationResult {
        for (const auto& allowed_value : allowed) {
            if (value == allowed_value) {
                return ValidationResult::Ok();
            }
        }
        return ValidationResult::Error("Value is not in the list of allowed values");
    });
}

/// @brief Creates a validator from a predicate function.
template <typename T, typename Pred>
    requires std::predicate<Pred, const T&>
auto Predicate(Pred pred, std::string error_message) -> Validator<T>
{
    return Validator<T>([p = std::move(pred), msg = std::move(error_message)](const T& value) -> ValidationResult {
        if (!p(value)) {
            return ValidationResult::Error(msg);
        }
        return ValidationResult::Ok();
    });
}

/// @brief Creates an always-valid validator.
template <typename T>
auto AlwaysValid() -> Validator<T>
{
    return Validator<T>();
}

}  // namespace cppfig
