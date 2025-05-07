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

namespace config {

/**
 * @brief Type trait to check if a serializer derives from IConfigurationSerializer
 */
template <typename E, typename Serializer>
struct is_valid_serializer : std::is_base_of<IConfigurationSerializer<E>, Serializer> { };

/**
 * @brief Generic configuration implementation
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
        , serializer_(std::make_unique<Serializer>(*this))
    {
        if (!std::filesystem::exists(filepath_)) {
            // Use defaults if file doesn't exist
            settings_ = defaults_;
            Save();
        }
        else {
            Load();
        }
    }

    /**
     * @brief Get a setting variant from the configuration
     *
     * @param config_name Configuration name
     * @return SettingVariant The setting as a variant
     * @throws std::runtime_error if configuration doesn't exist
     */
    SettingVariant GetSettingVariant(E config_name) override
    {
        // Check if setting exists in active settings
        auto it = settings_.find(config_name);
        if (it != settings_.end()) {
            return it->second;
        }

        // Fall back to defaults if not in active settings
        auto default_it = defaults_.find(config_name);
        if (default_it != defaults_.end()) {
            return default_it->second;
        }

        throw std::runtime_error("Configuration not found");
    }

    /**
     * @brief Implementation of UpdateSettingInt from IConfiguration
     */
    bool UpdateSettingInt(E config_name, int value) override
    {
        return UpdateSettingImpl<int>(config_name, value);
    }

    /**
     * @brief Implementation of UpdateSettingFloat from IConfiguration
     */
    bool UpdateSettingFloat(E config_name, float value) override
    {
        return UpdateSettingImpl<float>(config_name, value);
    }

    /**
     * @brief Implementation of UpdateSettingDouble from IConfiguration
     */
    bool UpdateSettingDouble(E config_name, double value) override
    {
        return UpdateSettingImpl<double>(config_name, value);
    }

    /**
     * @brief Implementation of UpdateSettingString from IConfiguration
     */
    bool UpdateSettingString(E config_name, const std::string& value) override
    {
        return UpdateSettingImpl<std::string>(config_name, value);
    }

    /**
     * @brief Implementation of UpdateSettingBool from IConfiguration
     */
    bool UpdateSettingBool(E config_name, bool value) override
    {
        return UpdateSettingImpl<bool>(config_name, value);
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
     * @brief Get all settings
     *
     * @return const std::unordered_map<E, SettingVariant>& All settings
     */
    const std::unordered_map<E, SettingVariant>& GetAllSettings() const
    {
        return settings_;
    }

    /**
     * @brief Update settings directly (useful for serializers)
     *
     * @param new_settings New settings map
     */
    void UpdateSettings(const std::unordered_map<E, SettingVariant>& new_settings)
    {
        settings_ = new_settings;
    }

private:
    /**
     * @brief Implementation of setting update logic
     */
    template <typename T>
    bool UpdateSettingImpl(E config_name, T value)
    {
        auto setting_var = GetSettingVariant(config_name);

        try {
            // Try to get the setting as the specified type
            auto setting = std::get<Setting<E, T>>(setting_var);

            // Create a new setting with the updated value
            Setting<E, T> updated_setting(
                setting.Name(),
                value,
                setting.MaxValue(),
                setting.MinValue(),
                setting.Unit(),
                setting.Description());

            // Update the settings map
            settings_[config_name] = updated_setting;
            return true;
        }
        catch (const std::bad_variant_access&) {
            // Type mismatch
            return false;
        }
    }

    std::filesystem::path filepath_;
    DefaultConfigMap defaults_;
    std::unordered_map<E, SettingVariant> settings_;
    std::unique_ptr<Serializer> serializer_;
};

} // namespace config
