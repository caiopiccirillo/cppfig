#pragma once

#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppfig {

/// @brief Result of validating one candidate value.
struct ValidationResult {
    bool is_valid = true;
    std::string error_message;

    [[nodiscard]] static constexpr auto Ok() -> ValidationResult { return {}; }

    [[nodiscard]] static constexpr auto Error(std::string message) -> ValidationResult
    {
        return { .is_valid = false, .error_message = std::move(message) };
    }

    [[nodiscard]] constexpr explicit operator bool() const { return is_valid; }
};

template <typename V, typename T>
concept ValidatorFor = requires(const V& validator, const T& value) {
    { validator(value) } -> std::same_as<ValidationResult>;
};

template <typename Left, typename Right>
struct AndValidator;

template <typename Left, typename Right>
struct OrValidator;

/// @brief CRTP mixin providing allocation-free validator composition.
template <typename Derived>
struct ValidatorOps {
    template <typename Right>
    [[nodiscard]] constexpr auto And(Right right) const -> AndValidator<Derived, Right>
    {
        return { {}, static_cast<const Derived&>(*this), std::move(right) };
    }

    template <typename Right>
    [[nodiscard]] constexpr auto Or(Right right) const -> OrValidator<Derived, Right>
    {
        return { {}, static_cast<const Derived&>(*this), std::move(right) };
    }
};

template <typename Left, typename Right>
struct AndValidator : ValidatorOps<AndValidator<Left, Right>> {
    Left left;
    Right right;

    template <typename T>
        requires ValidatorFor<Left, T> && ValidatorFor<Right, T>
    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        auto result = left(value);
        if (!result) {
            return result;
        }
        return right(value);
    }
};

template <typename Left, typename Right>
struct OrValidator : ValidatorOps<OrValidator<Left, Right>> {
    Left left;
    Right right;

    template <typename T>
        requires ValidatorFor<Left, T> && ValidatorFor<Right, T>
    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        auto result = left(value);
        if (result) {
            return result;
        }
        return right(value);
    }
};

template <typename T>
struct MinValidator : ValidatorOps<MinValidator<T>> {
    T minimum;

    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        if (value < minimum) {
            if (std::is_constant_evaluated()) {
                return ValidationResult::Error("Value is less than the minimum");
            }
            return ValidationResult::Error("Value " + std::to_string(value) + " is less than minimum " + std::to_string(minimum));
        }
        return ValidationResult::Ok();
    }
};

template <typename T>
struct MaxValidator : ValidatorOps<MaxValidator<T>> {
    T maximum;

    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        if (value > maximum) {
            if (std::is_constant_evaluated()) {
                return ValidationResult::Error("Value exceeds the maximum");
            }
            return ValidationResult::Error("Value " + std::to_string(value) + " exceeds maximum " + std::to_string(maximum));
        }
        return ValidationResult::Ok();
    }
};

template <typename T>
struct RangeValidator : ValidatorOps<RangeValidator<T>> {
    T minimum;
    T maximum;

    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        if (value < minimum || value > maximum) {
            if (std::is_constant_evaluated()) {
                return ValidationResult::Error("Value is outside the allowed range");
            }
            return ValidationResult::Error("Value " + std::to_string(value) + " is outside [" + std::to_string(minimum) + ", " +
                                           std::to_string(maximum) + "]");
        }
        return ValidationResult::Ok();
    }
};

template <typename T>
struct PositiveValidator : ValidatorOps<PositiveValidator<T>> {
    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        return value > T { 0 } ? ValidationResult::Ok() : ValidationResult::Error("Value must be positive");
    }
};

template <typename T>
struct NonNegativeValidator : ValidatorOps<NonNegativeValidator<T>> {
    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        return value >= T { 0 } ? ValidationResult::Ok() : ValidationResult::Error("Value must be non-negative");
    }
};

struct NotEmptyValidator : ValidatorOps<NotEmptyValidator> {
    [[nodiscard]] constexpr auto operator()(const std::string& value) const -> ValidationResult
    {
        return value.empty() ? ValidationResult::Error("Value must not be empty") : ValidationResult::Ok();
    }
};

struct MinLengthValidator : ValidatorOps<MinLengthValidator> {
    std::size_t minimum;

    [[nodiscard]] auto operator()(const std::string& value) const -> ValidationResult
    {
        if (value.size() < minimum) {
            return ValidationResult::Error("String length " + std::to_string(value.size()) + " is less than minimum " +
                                           std::to_string(minimum));
        }
        return ValidationResult::Ok();
    }
};

struct MaxLengthValidator : ValidatorOps<MaxLengthValidator> {
    std::size_t maximum;

    [[nodiscard]] auto operator()(const std::string& value) const -> ValidationResult
    {
        if (value.size() > maximum) {
            return ValidationResult::Error("String length " + std::to_string(value.size()) + " exceeds maximum " +
                                           std::to_string(maximum));
        }
        return ValidationResult::Ok();
    }
};

template <typename T>
struct OneOfValidator : ValidatorOps<OneOfValidator<T>> {
    std::vector<T> allowed_values;

    [[nodiscard]] auto operator()(const T& value) const -> ValidationResult
    {
        for (const auto& allowed : allowed_values) {
            if (value == allowed) {
                return ValidationResult::Ok();
            }
        }
        return ValidationResult::Error("Value is not in the list of allowed values");
    }
};

template <typename T, typename PredicateType>
struct PredicateValidator : ValidatorOps<PredicateValidator<T, PredicateType>> {
    PredicateType predicate;
    std::string message;

    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        return predicate(value) ? ValidationResult::Ok() : ValidationResult::Error(message);
    }
};

template <typename T>
struct AlwaysValidValidator : ValidatorOps<AlwaysValidValidator<T>> {
    [[nodiscard]] constexpr auto operator()(const T&) const -> ValidationResult { return ValidationResult::Ok(); }
};

template <typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] constexpr auto Min(T minimum) -> MinValidator<T>
{
    return { {}, minimum };
}

template <typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] constexpr auto Max(T maximum) -> MaxValidator<T>
{
    return { {}, maximum };
}

template <typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] constexpr auto Range(T minimum, T maximum) -> RangeValidator<T>
{
    return { {}, minimum, maximum };
}

template <typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] constexpr auto Positive() -> PositiveValidator<T>
{
    return {};
}

template <typename T>
    requires std::is_arithmetic_v<T>
[[nodiscard]] constexpr auto NonNegative() -> NonNegativeValidator<T>
{
    return {};
}

[[nodiscard]] constexpr auto NotEmpty() -> NotEmptyValidator { return {}; }
[[nodiscard]] constexpr auto MinLength(std::size_t minimum) -> MinLengthValidator { return { {}, minimum }; }
[[nodiscard]] constexpr auto MaxLength(std::size_t maximum) -> MaxLengthValidator { return { {}, maximum }; }

template <typename T>
[[nodiscard]] auto OneOf(std::vector<T> allowed_values) -> OneOfValidator<T>
{
    return { {}, std::move(allowed_values) };
}

template <typename T, typename PredicateType>
    requires std::predicate<PredicateType, const T&>
[[nodiscard]] constexpr auto Predicate(PredicateType predicate, std::string message) -> PredicateValidator<T, PredicateType>
{
    return { {}, std::move(predicate), std::move(message) };
}

template <typename T>
[[nodiscard]] constexpr auto AlwaysValid() -> AlwaysValidValidator<T>
{
    return {};
}

}  // namespace cppfig
