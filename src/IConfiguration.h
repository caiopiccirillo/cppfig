#pragma once

#include <string>
#include <type_traits>
#include <variant>

#include "Setting.h"

namespace config {

/**
 * @brief Interface for the configuration class.
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
     * @brief Get the setting as a variant according to configuration name
     *
     * @param config_name Configuration name
     * @return A variant containing the setting of the appropriate type
     */
    virtual SettingVariant GetSettingVariant(E config_name) = 0;

    /**
     * @brief Get the setting according to configuration name with improved access API
     * This returns a GenericSetting that performs type checking when Value<T>() is called
     *
     * @param config_name Configuration name
     * @return GenericSetting<E> A wrapper that provides natural Value<T>() access with type checking
     */
    virtual GenericSetting<E> GetSetting(E config_name) = 0;

    /**
     * @brief Type-safe getter that infers the correct type from default settings
     *
     * @tparam EnumValue The enum value to get (known at compile time)
     * @tparam T The type to retrieve
     * @return const T& The value with the correct type
     */
    template <E EnumValue, typename T>
    const T& GetAs() const
    {
        auto setting_var = const_cast<IConfiguration*>(this)->GetSettingVariant(EnumValue);
        return std::get<Setting<E, T>>(setting_var).Value();
    }

    /**
     * @brief Type-safe update method
     *
     * @tparam EnumValue Enum value to update (known at compile time)
     * @tparam T Type of the value
     * @param value New value
     * @return true if update was successful
     */
    template <E EnumValue, typename T>
    bool UpdateTypedSetting(T value)
    {
        if constexpr (std::is_same_v<T, int>) {
            return UpdateSettingInt(EnumValue, value);
        }
        else if constexpr (std::is_same_v<T, float>) {
            return UpdateSettingFloat(EnumValue, value);
        }
        else if constexpr (std::is_same_v<T, double>) {
            return UpdateSettingDouble(EnumValue, value);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return UpdateSettingString(EnumValue, value);
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return UpdateSettingBool(EnumValue, value);
        }
        else {
            static_assert(!sizeof(T), "Unsupported type for UpdateTypedSetting");
            return false;
        }
    }

    /**
     * @brief Template method to get the setting with a specific type (legacy API)
     *
     * @tparam T The expected type of the setting
     * @param config_name Configuration name
     * @return SettingT<E, T> The setting with the specified type
     * @throws std::bad_variant_access if the setting is not of type T
     * @deprecated Use GetSetting(name).Value<T>() instead
     */
    template <typename T>
    Setting<E, T> GetSettingAs(E config_name)
    {
        auto setting = GetSettingVariant(config_name);
        return std::get<Setting<E, T>>(setting);
    }

    /**
     * @brief Non-templated interface for updating settings
     */
    virtual bool UpdateSettingInt(E config_name, int value) = 0;
    virtual bool UpdateSettingFloat(E config_name, float value) = 0;
    virtual bool UpdateSettingDouble(E config_name, double value) = 0;
    virtual bool UpdateSettingString(E config_name, const std::string& value) = 0;
    virtual bool UpdateSettingBool(E config_name, bool value) = 0;

    /**
     * @brief Update setting with automatic type routing
     */
    template <typename T>
    bool UpdateSetting(E config_name, T value)
    {
        if constexpr (std::is_same_v<T, int>) {
            return UpdateSettingInt(config_name, value);
        }
        else if constexpr (std::is_same_v<T, float>) {
            return UpdateSettingFloat(config_name, value);
        }
        else if constexpr (std::is_same_v<T, double>) {
            return UpdateSettingDouble(config_name, value);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return UpdateSettingString(config_name, value);
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return UpdateSettingBool(config_name, value);
        }
        else {
            static_assert(!sizeof(T), "Unsupported type for UpdateSetting");
            return false;
        }
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
