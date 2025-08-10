#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>

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
 * @brief Strongly typed Setting class
 *
 * @tparam E Enum type that defines the configuration keys
 * @tparam T Type of the setting value
 */
template <typename E, typename T>
class Setting {
public:
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
        if (min_value_ && value_ < *min_value_)
            return false;
        if (max_value_ && value_ > *max_value_)
            return false;
        return true;
    }

    /**
     * @brief Get validation error message if value is invalid
     */
    std::string GetValidationError() const
    {
        if (!IsValid()) {
            if (min_value_ && value_ < *min_value_) {
                return "Value below minimum: " + std::to_string(value_) + " < " + std::to_string(*min_value_);
            }
            if (max_value_ && value_ > *max_value_) {
                return "Value above maximum: " + std::to_string(value_) + " > " + std::to_string(*max_value_);
            }
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
        return true; // Basic implementation - can be extended for string validation
    }

    std::string GetValidationError() const
    {
        return ""; // No validation errors for basic string settings
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
