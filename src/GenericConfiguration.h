#pragma once

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>

#include "IConfiguration.h"
#include "IConfigurationSerializer.h"
#include "Setting.h"
#include "TypedSetting.h"

namespace config {

/**
 * @brief Type trait to check if a serializer derives from IConfigurationSerializer
 */
template <typename E, typename Serializer>
struct is_valid_serializer : std::is_base_of<IConfigurationSerializer<E>, Serializer> { };

/**
 * @brief Modern configuration implementation with compile-time type safety
 *
 * This class provides a configuration system where all type checking happens
 * at compile time, ensuring zero runtime overhead and catching type errors
 * during compilation.
 *
 * @tparam E Enum type for configuration keys
 * @tparam Serializer The serializer to use for file operations
 */
template <typename E, typename Serializer>
class GenericConfiguration : public IConfiguration<E> {
public:
    static_assert(std::is_enum_v<E>, "Template parameter E must be an enum type");
    static_assert(is_valid_serializer<E, Serializer>::value,
                  "Serializer must inherit from IConfigurationSerializer<E>");

    using SettingVariant = typename IConfiguration<E>::SettingVariant;
    using DefaultConfigMap = std::unordered_map<E, SettingVariant>;

    /**
     * @brief Constructor with filepath and default configurations
     *
     * @param filepath Path to the configuration file
     * @param defaults Default configuration settings
     */
    GenericConfiguration(std::filesystem::path filepath, const DefaultConfigMap& defaults)
        : filepath_(std::move(filepath))
        , defaults_(defaults)
        , settings_(defaults)
        , serializer_(std::make_unique<Serializer>(*this))
    {
        if (std::filesystem::exists(filepath_)) {
            Load();
        }
        else {
            // Create file with defaults
            Save();
        }
    }

    /**
     * @brief Clean, ergonomic setting access with automatic type deduction (const version)
     *
     * This is the main user-facing API. The type is automatically deduced from
     * the enum value using the config_type_map, providing a clean interface:
     *
     * Usage:
     *   auto setting = config.GetSetting<MyEnum::SomeValue>();
     *   auto value = setting.Value();           // Type automatically deduced
     *   auto max = setting.MaxValue();          // Access metadata
     *
     * @tparam EnumValue The enum value to get (specified as template parameter)
     * @return TypedSetting wrapper with clean API
     */
    template <E EnumValue>
    auto GetSetting() const
    {
        using ValueType = config_type_t<E, EnumValue>;
        using SettingType = Setting<E, ValueType>;

        auto setting_it = settings_.find(EnumValue);
        if (setting_it != settings_.end()) {
            const auto& setting = std::get<SettingType>(setting_it->second);
            return TypedSetting<E, EnumValue>(setting);
        }

        auto default_it = defaults_.find(EnumValue);
        if (default_it != defaults_.end()) {
            const auto& setting = std::get<SettingType>(default_it->second);
            return TypedSetting<E, EnumValue>(setting);
        }

        throw std::runtime_error("Configuration setting not found");
    }

    /**
     * @brief Clean, ergonomic setting access (non-const version)
     *
     * @tparam EnumValue The enum value to get (specified as template parameter)
     * @return TypedSetting wrapper with clean API
     */
    template <E EnumValue>
    auto GetSetting()
    {
        using ValueType = config_type_t<E, EnumValue>;
        using SettingType = Setting<E, ValueType>;

        // Ensure the setting exists in settings_ (not just defaults)
        auto setting_it = settings_.find(EnumValue);
        if (setting_it == settings_.end()) {
            // Copy from defaults if not present
            auto default_it = defaults_.find(EnumValue);
            if (default_it != defaults_.end()) {
                settings_[EnumValue] = default_it->second;
                setting_it = settings_.find(EnumValue);
            }
            else {
                throw std::runtime_error("Configuration setting not found in defaults");
            }
        }

        auto& setting = std::get<SettingType>(setting_it->second);
        return TypedSetting<E, EnumValue>(setting);
    }

    /**
     * @brief Validate all current configuration values
     *
     * @return true if all values are valid according to their constraints
     */
    bool ValidateAll() const override
    {
        for (const auto& [key, setting_var] : settings_) {
            bool is_valid = std::visit([](const auto& setting) {
                return setting.IsValid();
            },
                                       setting_var);

            if (!is_valid) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Get validation errors for all settings
     *
     * @return Vector of error messages for invalid settings
     */
    std::vector<std::string> GetValidationErrors() const override
    {
        std::vector<std::string> errors;

        for (const auto& [key, setting_var] : settings_) {
            std::visit([&errors](const auto& setting) {
                if (!setting.IsValid()) {
                    std::string error_msg = setting.GetValidationError();
                    if (!error_msg.empty()) {
                        errors.push_back(error_msg);
                    }
                }
            },
                       setting_var);
        }

        return errors;
    }

    /**
     * @brief Save the configuration to file
     *
     * @return true if successful, false otherwise
     */
    bool Save() override
    {
        return serializer_->Serialize(filepath_.string());
    }

    /**
     * @brief Load the configuration from file
     *
     * @return true if successful, false otherwise
     */
    bool Load() override
    {
        return serializer_->Deserialize(filepath_.string());
    }

    /**
     * @brief Get all settings (for serializer access)
     *
     * @return const reference to all settings
     */
    const std::unordered_map<E, SettingVariant>& GetAllSettings() const
    {
        return settings_;
    }

    /**
     * @brief Get the default settings map (for serializer access)
     *
     * @return const reference to default settings
     */
    const DefaultConfigMap& GetDefaults() const
    {
        return defaults_;
    }

    /**
     * @brief Update settings directly (for serializer use)
     *
     * @param new_settings New settings map
     */
    void UpdateSettings(const std::unordered_map<E, SettingVariant>& new_settings)
    {
        settings_ = new_settings;
    }

    /**
     * @brief Reset a setting to its default value
     *
     * @tparam EnumValue The setting to reset
     */
    template <E EnumValue>
    void ResetToDefault()
    {
        auto default_it = defaults_.find(EnumValue);
        if (default_it != defaults_.end()) {
            settings_[EnumValue] = default_it->second;
        }
    }

    /**
     * @brief Reset all settings to their default values
     */
    void ResetAllToDefaults() override
    {
        settings_ = defaults_;
    }

    /**
     * @brief Check if a setting has been modified from its default
     *
     * @tparam EnumValue The setting to check
     * @return true if the setting differs from its default value
     */
    template <E EnumValue>
    bool IsModified() const
    {
        auto setting_it = settings_.find(EnumValue);
        auto default_it = defaults_.find(EnumValue);

        if (setting_it == settings_.end() || default_it == defaults_.end()) {
            return false;
        }

        // Compare the setting variants
        return setting_it->second != default_it->second;
    }

    /**
     * @brief Get the file path of this configuration
     */
    const std::filesystem::path& GetFilePath() const
    {
        return filepath_;
    }

    /**
     * @brief Legacy API for backward compatibility - Get value with explicit types
     *
     * @deprecated Use GetSetting(enumValue).Value() instead
     */
    template <E EnumValue, typename T>
    std::enable_if_t<is_valid_config_type_v<E, EnumValue, T>, const T&>
    GetValue() const
    {
        return GetSetting<EnumValue>().Value();
    }

    /**
     * @brief Legacy API for backward compatibility - Set value with explicit types
     *
     * @deprecated Use GetSetting(enumValue).SetValue(value) instead
     */
    template <E EnumValue, typename T>
    std::enable_if_t<is_valid_config_type_v<E, EnumValue, T>, void>
    SetValue(T value)
    {
        GetSetting<EnumValue>().SetValue(value);
    }

private:
    std::filesystem::path filepath_;
    DefaultConfigMap defaults_;
    std::unordered_map<E, SettingVariant> settings_;
    std::unique_ptr<Serializer> serializer_;
};

} // namespace config
