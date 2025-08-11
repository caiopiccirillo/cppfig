#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include "ConfigurationTraits.h"

namespace config {

/**
 * @brief Compile-time type mapping for configuration settings
 * This provides stronger type safety by associating enum values with their expected types
 */
template <typename E, E EnumValue>
struct config_type_map {
    // By default, no type is associated - users must specialize this
    // This will cause a compile error if not specialized, ensuring type safety
    static_assert(sizeof(E) == 0,
                  "config_type_map must be specialized for each enum value. "
                  "Use template specialization to define type mappings.");
};

/**
 * @brief Helper to extract the type associated with an enum value
 */
template <typename E, E EnumValue>
using config_type_t = typename config_type_map<E, EnumValue>::type;

/**
 * @brief Compile-time check if a type matches the expected type for an enum value
 */
template <typename E, E EnumValue, typename T>
inline constexpr bool is_valid_config_type_v = std::is_same_v<config_type_t<E, EnumValue>, T>;

/**
 * @brief Macro to easily declare type mappings for configuration enum values
 * Usage: DECLARE_CONFIG_TYPE(MyEnum, MyEnum::SomeValue, int)
 */
#define DECLARE_CONFIG_TYPE(EnumType, EnumValue, Type) \
    template <>                                        \
    struct config_type_map<EnumType, EnumValue> {      \
        using type = Type;                             \
    }

/**
 * @brief Example usage to reduce boilerplate when declaring config types
 *
 * Instead of writing verbose template specializations:
 *
 * template <> struct config_type_map<MyEnum, MyEnum::Value1> { using type = int; };
 * template <> struct config_type_map<MyEnum, MyEnum::Value2> { using type = std::string; };
 * template <> struct config_type_map<MyEnum, MyEnum::Value3> { using type = bool; };
 *
 * Simply use the macro:
 *
 * DECLARE_CONFIG_TYPE(MyEnum, MyEnum::Value1, int);
 * DECLARE_CONFIG_TYPE(MyEnum, MyEnum::Value2, std::string);
 * DECLARE_CONFIG_TYPE(MyEnum, MyEnum::Value3, bool);
 */

/**
 * @brief Strongly typed Setting class
 *
 * @tparam E Enum type that defines the configuration keys
 * @tparam T Type of the setting value
 */
template <typename E, typename T>
class Setting {
public:
    using value_type = T;
    Setting(E name,
            T value,
            std::optional<T> maximum_value = std::nullopt,
            std::optional<T> minimum_value = std::nullopt,
            std::optional<std::string> unit = std::nullopt,
            std::optional<std::string> description = std::nullopt)
        : name_(name)
        , value_(std::move(value))
        , max_value_(std::move(maximum_value))
        , min_value_(std::move(minimum_value))
        , unit_(std::move(unit))
        , description_(std::move(description))
    {
    }

    Setting() = default;

    [[nodiscard]] E Name() const { return name_; }
    [[nodiscard]] const T& Value() const { return value_; }
    [[nodiscard]] std::optional<T> MaxValue() const { return max_value_; }
    [[nodiscard]] std::optional<T> MinValue() const { return min_value_; }
    [[nodiscard]] std::optional<std::string> Unit() const { return unit_; }
    [[nodiscard]] std::optional<std::string> Description() const { return description_; }

    /**
     * @brief Update the value with type safety
     */
    void SetValue(const T& new_value)
    {
        value_ = new_value;
    }

    /**
     * @brief Compile-time type-safe value access
     * Only callable if T matches the expected type for this enum value
     */
    template <E EnumValue>
    [[nodiscard]] std::enable_if_t<is_valid_config_type_v<E, EnumValue, T>, const T&>
    GetTypedValue() const
    {
        static_assert(EnumValue == name_, "Enum value must match setting name");
        return value_;
    }

    /**
     * @brief Compile-time type-safe value update
     * Only callable if T matches the expected type for this enum value
     */
    template <E EnumValue>
    std::enable_if_t<is_valid_config_type_v<E, EnumValue, T>, void>
    SetTypedValue(const T& new_value)
    {
        static_assert(EnumValue == name_, "Enum value must match setting name");
        value_ = new_value;
    }

    /**
     * @brief Validate value against constraints
     */
    bool IsValid() const
    {
        // Only check range constraints for types that support comparison
        return IsValidImpl();
    }

private:
    // Implementation for types that support comparison operators
    template<typename U = T>
    typename std::enable_if<
        std::is_arithmetic<U>::value || std::is_same<U, std::string>::value,
        bool
    >::type IsValidImpl() const
    {
        // Check range constraints for arithmetic types and strings
        if (min_value_ && value_ < *min_value_)
            return false;
        if (max_value_ && value_ > *max_value_)
            return false;
        
        // Use trait-based validation if available
        if (has_configuration_traits_v<T>) {
            return ConfigurationTraits<T>::IsValid(value_);
        }
        
        return true;
    }
    
    // Implementation for custom types that may not support comparison
    template<typename U = T>
    typename std::enable_if<
        !std::is_arithmetic<U>::value && !std::is_same<U, std::string>::value,
        bool
    >::type IsValidImpl() const
    {
        // Skip range constraints for custom types, use only trait-based validation
        if (has_configuration_traits_v<T>) {
            return ConfigurationTraits<T>::IsValid(value_);
        }
        
        return true;
    }

private:

public:
    /**
     * @brief Get validation error message if value is invalid
     */
    std::string GetValidationError() const
    {
        return GetValidationErrorImpl();
    }

private:
    // Implementation for types that support comparison operators
    template<typename U = T>
    typename std::enable_if<
        std::is_arithmetic<U>::value || std::is_same<U, std::string>::value,
        std::string
    >::type GetValidationErrorImpl() const
    {
        if (!IsValid()) {
            // Check range constraints first
            if (min_value_ && value_ < *min_value_) {
                if (has_configuration_traits_v<T>) {
                    return "Value below minimum: " + ConfigurationTraits<T>::ToString(value_) + 
                           " < " + ConfigurationTraits<T>::ToString(*min_value_);
                } else {
                    return "Value below minimum";
                }
            }
            if (max_value_ && value_ > *max_value_) {
                if (has_configuration_traits_v<T>) {
                    return "Value above maximum: " + ConfigurationTraits<T>::ToString(value_) + 
                           " > " + ConfigurationTraits<T>::ToString(*max_value_);
                } else {
                    return "Value above maximum";
                }
            }
            
            // Use trait-based validation error if available
            if (has_configuration_traits_v<T>) {
                return ConfigurationTraits<T>::GetValidationError(value_);
            }
        }
        return "";
    }
    
    // Implementation for custom types that may not support comparison
    template<typename U = T>
    typename std::enable_if<
        !std::is_arithmetic<U>::value && !std::is_same<U, std::string>::value,
        std::string
    >::type GetValidationErrorImpl() const
    {
        if (!IsValid()) {
            // Use trait-based validation error if available
            if (has_configuration_traits_v<T>) {
                return ConfigurationTraits<T>::GetValidationError(value_);
            }
        }
        return "";
    }

public:
    /**
     * @brief Equality comparison operator
     */
    bool operator==(const Setting& other) const
    {
        return name_ == other.name_ && value_ == other.value_ && max_value_ == other.max_value_ && min_value_ == other.min_value_ && unit_ == other.unit_ && description_ == other.description_;
    }

    /**
     * @brief Inequality comparison operator
     */
    bool operator!=(const Setting& other) const
    {
        return !(*this == other);
    }

private:
    E name_;
    T value_;
    std::optional<T> max_value_;
    std::optional<T> min_value_;
    std::optional<std::string> unit_;
    std::optional<std::string> description_;
};

/**
 * @brief Template specialization for string validation error messages
 */
template <typename E>
class Setting<E, std::string> {
public:
    using value_type = std::string;
    Setting(E name,
            std::string value,
            std::optional<std::string> maximum_value = std::nullopt,
            std::optional<std::string> minimum_value = std::nullopt,
            std::optional<std::string> unit = std::nullopt,
            std::optional<std::string> description = std::nullopt)
        : name_(name)
        , value_(std::move(value))
        , max_value_(std::move(maximum_value))
        , min_value_(std::move(minimum_value))
        , unit_(std::move(unit))
        , description_(std::move(description))
    {
    }

    Setting() = default;

    [[nodiscard]] E Name() const { return name_; }
    [[nodiscard]] const std::string& Value() const { return value_; }
    [[nodiscard]] std::optional<std::string> MaxValue() const { return max_value_; }
    [[nodiscard]] std::optional<std::string> MinValue() const { return min_value_; }
    [[nodiscard]] std::optional<std::string> Unit() const { return unit_; }
    [[nodiscard]] std::optional<std::string> Description() const { return description_; }

    void SetValue(const std::string& new_value)
    {
        value_ = new_value;
    }

    template <E EnumValue>
    [[nodiscard]] std::enable_if_t<is_valid_config_type_v<E, EnumValue, std::string>, const std::string&>
    GetTypedValue() const
    {
        static_assert(EnumValue == name_, "Enum value must match setting name");
        return value_;
    }

    template <E EnumValue>
    std::enable_if_t<is_valid_config_type_v<E, EnumValue, std::string>, void>
    SetTypedValue(const std::string& new_value)
    {
        static_assert(EnumValue == name_, "Enum value must match setting name");
        value_ = new_value;
    }

    bool IsValid() const
    {
        // For strings, we can check length constraints if min/max are set as length limits
        // Use trait-based validation
        return ConfigurationTraits<std::string>::IsValid(value_);
    }

    std::string GetValidationError() const
    {
        if (!IsValid()) {
            return ConfigurationTraits<std::string>::GetValidationError(value_);
        }
        return "";
    }

    /**
     * @brief Equality comparison operator
     */
    bool operator==(const Setting& other) const
    {
        return name_ == other.name_ && value_ == other.value_ && max_value_ == other.max_value_ && min_value_ == other.min_value_ && unit_ == other.unit_ && description_ == other.description_;
    }

    /**
     * @brief Inequality comparison operator
     */
    bool operator!=(const Setting& other) const
    {
        return !(*this == other);
    }

private:
    E name_;
    std::string value_;
    std::optional<std::string> max_value_; // Could represent max length as string
    std::optional<std::string> min_value_; // Could represent min length as string
    std::optional<std::string> unit_;
    std::optional<std::string> description_;
};

} // namespace config
