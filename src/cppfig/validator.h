#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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
///
/// Its constructor stays public: validators are aggregates, and aggregate
/// initialization of a derived validator needs an accessible base constructor.
template <typename Derived>
struct ValidatorOps {  // NOLINT(bugprone-crtp-constructor-accessibility)
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
        // Stated positively so that NaN, which compares false, is rejected.
        if (!(value >= minimum)) {
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
        if (!(value <= maximum)) {
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
        if (!(value >= minimum && value <= maximum)) {
            if (std::is_constant_evaluated()) {
                return ValidationResult::Error("Value is outside the allowed range");
            }
            return ValidationResult::Error("Value " + std::to_string(value) + " is outside [" + std::to_string(minimum) + ", "
                                           + std::to_string(maximum) + "]");
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

    [[nodiscard]] constexpr auto operator()(const std::string& value) const -> ValidationResult
    {
        if (value.size() < minimum) {
            if (std::is_constant_evaluated()) {
                return ValidationResult::Error("String is shorter than the minimum length");
            }
            return ValidationResult::Error("String length " + std::to_string(value.size()) + " is less than minimum "
                                           + std::to_string(minimum));
        }
        return ValidationResult::Ok();
    }
};

struct MaxLengthValidator : ValidatorOps<MaxLengthValidator> {
    std::size_t maximum;

    [[nodiscard]] constexpr auto operator()(const std::string& value) const -> ValidationResult
    {
        if (value.size() > maximum) {
            if (std::is_constant_evaluated()) {
                return ValidationResult::Error("String is longer than the maximum length");
            }
            return ValidationResult::Error("String length " + std::to_string(value.size()) + " exceeds maximum "
                                           + std::to_string(maximum));
        }
        return ValidationResult::Ok();
    }
};

/// Accepts values equal to one of N allowed values, held in place. It can run
/// at compile time, and building and running it never allocates unless the
/// allowed values are std::string objects, which it copies.
template <typename Element, std::size_t N>
struct OneOfValidator : ValidatorOps<OneOfValidator<Element, N>> {
    std::array<Element, N> allowed_values;

    template <typename T>
        requires requires(const T& value, const Element& allowed) {
            { value == allowed } -> std::convertible_to<bool>;
        }
    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        for (const auto& allowed : allowed_values) {
            if (value == allowed) {
                return ValidationResult::Ok();
            }
        }
        return ValidationResult::Error("Value is not in the list of allowed values");
    }
};

/// Accepts values for which predicate returns true. The message is not copied,
/// so it must outlive the validator; a string literal always does.
template <typename T, typename PredicateType>
struct PredicateValidator : ValidatorOps<PredicateValidator<T, PredicateType>> {
    PredicateType predicate;
    std::string_view message;

    [[nodiscard]] constexpr auto operator()(const T& value) const -> ValidationResult
    {
        return predicate(value) ? ValidationResult::Ok() : ValidationResult::Error(std::string(message));
    }
};

template <typename T>
struct AlwaysValidValidator : ValidatorOps<AlwaysValidValidator<T>> {
    [[nodiscard]] constexpr auto operator()(const T& /*value*/) const -> ValidationResult { return ValidationResult::Ok(); }
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

namespace detail {

    /// Character pointers are held as std::string_view, so string literals are
    /// not copied. A std::string is copied: it is often a temporary, and a view
    /// of it would dangle as soon as the OneOf call returns.
    template <typename T>
    struct OneOfElementOf {
        using type = T;
    };

    template <>
    struct OneOfElementOf<const char*> {
        using type = std::string_view;
    };

    template <>
    struct OneOfElementOf<char*> {
        using type = std::string_view;
    };

    template <typename Element, typename... Values>
    using OneOfElement = OneOfElementOf<
        std::conditional_t<std::is_void_v<Element>, std::common_type_t<std::decay_t<Values>...>, Element>>::type;

}  // namespace detail

/// @brief Accepts one of the listed values. The element type is deduced, or
/// given explicitly as in OneOf<unsigned>(80, 443).
///
/// String literals, character pointers and std::string_view values are held as
/// views, so the text they refer to must outlive the validator; a string literal
/// always does. std::string values are copied.
template <typename Element = void, typename... Values>
[[nodiscard]] constexpr auto OneOf(const Values&... values)
    -> OneOfValidator<detail::OneOfElement<Element, Values...>, sizeof...(Values)>
{
    static_assert(sizeof...(Values) > 0, "OneOf needs at least one allowed value");
    using ElementType = detail::OneOfElement<Element, Values...>;
    return { {}, { static_cast<ElementType>(values)... } };
}

template <typename T, typename PredicateType>
    requires std::predicate<PredicateType, const T&>
[[nodiscard]] constexpr auto Predicate(PredicateType predicate, std::string_view message)
    -> PredicateValidator<T, PredicateType>
{
    return { {}, std::move(predicate), message };
}

template <typename T>
[[nodiscard]] constexpr auto AlwaysValid() -> AlwaysValidValidator<T>
{
    return {};
}

}  // namespace cppfig
