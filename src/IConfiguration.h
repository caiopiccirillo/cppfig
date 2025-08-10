#pragma once

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "Setting.h"

namespace config {

/**
 * @brief Interface for modern compile-time type-safe configuration
 *
 * This interface defines the contract for configuration classes that provide
 * compile-time type safety and zero runtime overhead for type checking.
 *
 * @tparam E Enum type for configuration keys (must be an enum)
 */
template <typename E>
class IConfiguration {
public:
    static_assert(std::is_enum_v<E>, "Template parameter E must be an enum type");

    using SettingVariant = std::variant<
        Setting<E, int>,
        Setting<E, float>,
        Setting<E, double>,
        Setting<E, std::string>,
        Setting<E, bool>>;

    virtual ~IConfiguration() = default;

    /**
     * @brief Compile-time type-safe value getter
     *
     * This method provides the strongest type safety by checking types at compile time.
     * Type mismatches will result in compilation errors.
     *
     * @tparam EnumValue The enum value to retrieve (known at compile time)
     * @tparam T The requested type (must match config_type_map)
     * @return The setting value with the correct type
     */
    template <E EnumValue, typename T>
    std::enable_if_t<is_valid_config_type_v<E, EnumValue, T>, const T&>
    GetValue() const
    {
        return static_cast<const IConfiguration*>(this)->template GetValue<EnumValue, T>();
    }

    /**
     * @brief Compile-time type-safe value setter
     *
     * This method provides compile-time type checking for setting updates.
     *
     * @tparam EnumValue Enum value to update (known at compile time)
     * @tparam T Type of the value (must match config_type_map)
     * @param value New value
     */
    template <E EnumValue, typename T>
    std::enable_if_t<is_valid_config_type_v<E, EnumValue, T>, void>
    SetValue(T value)
    {
        static_cast<IConfiguration*>(this)->template SetValue<EnumValue, T>(value);
    }

    /**
     * @brief Get a setting object for metadata access
     *
     * @tparam EnumValue The enum value to get
     * @tparam T The type of the setting
     * @return The complete setting object
     */
    template <E EnumValue, typename T>
    std::enable_if_t<is_valid_config_type_v<E, EnumValue, T>, const Setting<E, T>&>
    GetSetting() const
    {
        return static_cast<const IConfiguration*>(this)->template GetSetting<EnumValue, T>();
    }

    /**
     * @brief Validate all current configuration values
     *
     * @return true if all values are valid according to their constraints
     */
    virtual bool ValidateAll() const = 0;

    /**
     * @brief Get validation errors for all settings
     *
     * @return Vector of error messages for invalid settings
     */
    virtual std::vector<std::string> GetValidationErrors() const = 0;

    /**
     * @brief Reset a setting to its default value
     *
     * @tparam EnumValue The setting to reset
     */
    template <E EnumValue>
    void ResetToDefault()
    {
        static_cast<IConfiguration*>(this)->template ResetToDefault<EnumValue>();
    }

    /**
     * @brief Reset all settings to their default values
     */
    virtual void ResetAllToDefaults() = 0;

    /**
     * @brief Check if a setting has been modified from its default
     *
     * @tparam EnumValue The setting to check
     * @return true if the setting differs from its default value
     */
    template <E EnumValue>
    bool IsModified() const
    {
        return static_cast<const IConfiguration*>(this)->template IsModified<EnumValue>();
    }

    /**
     * @brief Save the current configuration to a file
     *
     * @return true if successful, false otherwise
     */
    virtual bool Save() = 0;

    /**
     * @brief Load configuration from a file
     *
     * @return true if successful, false otherwise
     */
    virtual bool Load() = 0;
};

} // namespace config
