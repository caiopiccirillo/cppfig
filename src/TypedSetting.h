#pragma once

#include <optional>
#include <string>
#include <type_traits>

#include "Setting.h"

namespace config {

/**
 * @brief Clean, ergonomic wrapper for settings with automatic type deduction
 *
 * This class provides a user-friendly interface where the type is automatically
 * deduced from the enum value, eliminating the need for verbose template syntax.
 *
 * Usage:
 *   auto setting = config.GetSetting(MyEnum::SomeValue);
 *   auto value = setting.Value();           // Type automatically deduced
 *   setting.SetValue(newValue);             // Type automatically deduced
 *   auto max = setting.MaxValue();          // Access metadata
 *
 * @tparam E Enum type for configuration keys
 * @tparam EnumValue The specific enum value (known at compile time)
 */
template <typename E, E EnumValue>
class TypedSetting {
public:
    // Automatically deduce the type from the config_type_map
    using ValueType = config_type_t<E, EnumValue>;
    using SettingType = Setting<E, ValueType>;

    /**
     * @brief Constructor that takes a reference to the underlying setting
     */
    explicit TypedSetting(SettingType& setting)
        : setting_(setting)
    {
    }
    explicit TypedSetting(const SettingType& setting)
        : setting_(const_cast<SettingType&>(setting))
    {
    }

    /**
     * @brief Get the value with automatic type deduction
     *
     * This is the main ergonomic improvement - no need to specify types!
     *
     * @return const ValueType& The setting value with the correct type
     */
    const ValueType& Value() const
    {
        return setting_.Value();
    }

    /**
     * @brief Get the value with explicit type checking (optional)
     *
     * This version allows explicit type specification for extra safety,
     * but will cause a compilation error if the type doesn't match.
     *
     * @tparam T The expected type (must match ValueType)
     * @return const T& The setting value
     */
    template <typename T>
    const T& Value() const
    {
        static_assert(std::is_same_v<T, ValueType>,
                      "Explicit type must match the configured type for this setting");
        return setting_.Value();
    }

    /**
     * @brief Set the value with automatic type deduction
     *
     * @param new_value The new value (type automatically deduced)
     */
    void SetValue(const ValueType& new_value)
    {
        setting_.SetValue(new_value);
    }

    /**
     * @brief Set the value with explicit type checking (optional)
     *
     * @tparam T The value type (must match ValueType)
     * @param new_value The new value
     */
    template <typename T>
    void SetValue(const T& new_value)
    {
        static_assert(std::is_same_v<T, ValueType>,
                      "Value type must match the configured type for this setting");
        setting_.SetValue(new_value);
    }

    /**
     * @brief Get the enum name of this setting
     */
    E Name() const
    {
        return setting_.Name();
    }

    /**
     * @brief Get the maximum value constraint (if any)
     */
    std::optional<ValueType> MaxValue() const
    {
        return setting_.MaxValue();
    }

    /**
     * @brief Get the minimum value constraint (if any)
     */
    std::optional<ValueType> MinValue() const
    {
        return setting_.MinValue();
    }

    /**
     * @brief Get the unit string (if any)
     */
    std::optional<std::string> Unit() const
    {
        return setting_.Unit();
    }

    /**
     * @brief Get the description string (if any)
     */
    std::optional<std::string> Description() const
    {
        return setting_.Description();
    }

    /**
     * @brief Check if the current value is valid according to constraints
     */
    bool IsValid() const
    {
        return setting_.IsValid();
    }

    /**
     * @brief Get validation error message if value is invalid
     */
    std::string GetValidationError() const
    {
        return setting_.GetValidationError();
    }

    /**
     * @brief Check if this setting has a maximum value constraint
     */
    bool HasMaxValue() const
    {
        return setting_.MaxValue().has_value();
    }

    /**
     * @brief Check if this setting has a minimum value constraint
     */
    bool HasMinValue() const
    {
        return setting_.MinValue().has_value();
    }

    /**
     * @brief Check if this setting has a unit specified
     */
    bool HasUnit() const
    {
        return setting_.Unit().has_value();
    }

    /**
     * @brief Check if this setting has a description
     */
    bool HasDescription() const
    {
        return setting_.Description().has_value();
    }

    /**
     * @brief Get access to the underlying setting (for advanced use cases)
     */
    const SettingType& GetUnderlyingSetting() const
    {
        return setting_;
    }

    /**
     * @brief Get access to the underlying setting (for advanced use cases)
     */
    SettingType& GetUnderlyingSetting()
    {
        return setting_;
    }

    /**
     * @brief Type information for this setting
     */
    static constexpr E GetEnumValue() { return EnumValue; }

    /**
     * @brief Get the C++ type name as a string (for debugging/logging)
     */
    std::string GetTypeName() const
    {
        if constexpr (std::is_same_v<ValueType, int>) {
            return "int";
        }
        else if constexpr (std::is_same_v<ValueType, float>) {
            return "float";
        }
        else if constexpr (std::is_same_v<ValueType, double>) {
            return "double";
        }
        else if constexpr (std::is_same_v<ValueType, std::string>) {
            return "string";
        }
        else if constexpr (std::is_same_v<ValueType, bool>) {
            return "bool";
        }
        else {
            return "unknown";
        }
    }

    /**
     * @brief Create a formatted string representation of this setting
     */
    std::string ToString() const
    {
        std::string result = GetTypeName() + " setting";

        if (HasDescription()) {
            result += ": " + *Description();
        }

        result += " = ";

        if constexpr (std::is_same_v<ValueType, std::string>) {
            result += "\"" + Value() + "\"";
        }
        else if constexpr (std::is_same_v<ValueType, bool>) {
            result += Value() ? "true" : "false";
        }
        else {
            result += std::to_string(Value());
        }

        if (HasUnit()) {
            result += " " + *Unit();
        }

        if (HasMinValue() || HasMaxValue()) {
            result += " (range: ";
            if (HasMinValue()) {
                if constexpr (!std::is_same_v<ValueType, std::string>) {
                    result += std::to_string(*MinValue());
                }
                else {
                    result += *MinValue();
                }
            }
            else {
                result += "-∞";
            }
            result += " to ";
            if (HasMaxValue()) {
                if constexpr (!std::is_same_v<ValueType, std::string>) {
                    result += std::to_string(*MaxValue());
                }
                else {
                    result += *MaxValue();
                }
            }
            else {
                result += "+∞";
            }
            result += ")";
        }

        return result;
    }

private:
    SettingType& setting_;
};

} // namespace config
