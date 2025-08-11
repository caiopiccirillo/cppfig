#pragma once

#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "ConfigurationTraits.h"
#include "IConfigurationSerializer.h"
#include "Setting.h"

namespace config {

// Forward declaration for GenericConfiguration
template <typename E, typename SettingVariant, typename S>
class GenericConfiguration;

/**
 * @brief JSON serializer for configurations with configurable type support
 *
 * This serializer uses ConfigurationTraits to serialize/deserialize any type
 * that has proper trait specializations. It works with any SettingVariant
 * that the user defines.
 *
 * @tparam E Enum type for configuration keys
 * @tparam SettingVariant The variant type containing all supported Setting types
 */
template <typename E, typename SettingVariant>
class JsonSerializer : public IConfigurationSerializer<E, SettingVariant> {
public:
    // Forward declare the enum string conversion functions
    // These must be implemented by the user for their enum type
    static std::string ToString(E enumValue);
    static E FromString(const std::string& str);

    using EnumType = E;
    using VariantType = SettingVariant;

    /**
     * @brief Constructor that takes a reference to the configuration
     *
     * @param config Reference to the configuration object
     */
    explicit JsonSerializer(GenericConfiguration<E, SettingVariant, JsonSerializer<E, SettingVariant>>& config)
        : config_(config)
    {
    }

    /**
     * @brief Serialize the configuration to a JSON file
     *
     * @param target Path to the target file
     * @return true if successful, false otherwise
     */
    bool Serialize(const std::string& target) override
    {
        nlohmann::json json_config;

        for (const auto& [key, setting_var] : config_.GetAllSettings()) {
            // Convert each setting to JSON using std::visit
            nlohmann::json setting_json = SettingToJson(setting_var);
            json_config.push_back(setting_json);
        }

        // Write to file
        std::ofstream file(target);
        if (!file.is_open()) {
            return false;
        }

        file << std::setw(4) << json_config << '\n';
        return file.good();
    }

    /**
     * @brief Deserialize the configuration from a JSON file
     *
     * @param source Path to the source file
     * @return true if successful, false otherwise
     */
    bool Deserialize(const std::string& source) override
    {
        return Deserialize(source, true);
    }

    /**
     * @brief Deserialize the configuration from a JSON file with optional schema migration
     *
     * @param source Path to the source file
     * @param auto_migrate_schema If true, automatically adds missing settings from defaults and saves the file
     * @return true if successful, false otherwise
     */
    bool Deserialize(const std::string& source, bool auto_migrate_schema) override
    {
        std::ifstream file(source);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json json_config;
        try {
            file >> json_config;
        }
        catch (const nlohmann::json::exception&) {
            return false;
        }

        std::unordered_map<E, SettingVariant> settings;

        // Parse each setting from JSON
        for (const auto& setting_json : json_config) {
            try {
                auto setting_var = JsonToSetting(setting_json);
                E key = std::visit([](const auto& s) { return s.Name(); }, setting_var);
                settings[key] = setting_var;
            }
            catch (...) {
                // Skip invalid settings
                continue;
            }
        }

        // Check if we need to add missing settings from defaults
        bool schema_updated = false;
        if (auto_migrate_schema) {
            const auto& defaults = config_.GetDefaults();
            for (const auto& [default_key, default_setting] : defaults) {
                if (settings.find(default_key) == settings.end()) {
                    // Setting is missing in the loaded file, add it from defaults
                    settings[default_key] = default_setting;
                    schema_updated = true;
                }
            }
        }

        // Update configuration with parsed settings
        config_.UpdateSettings(settings);

        // If we added new settings, save the updated configuration back to file
        if (schema_updated) {
            std::cout << "Info: Configuration schema updated with new settings. Saving to " << source << std::endl;
            if (!Serialize(source)) {
                std::cerr << "Warning: Failed to save updated configuration schema to " << source << std::endl;
                // Don't return false here - the loading itself was successful
            }
        }

        return true;
    }

    /**
     * @brief Get the format name
     */
    std::string GetFormatName() const override
    {
        return "JSON";
    }

    /**
     * @brief Check if this serializer supports a specific file extension
     */
    bool SupportsExtension(const std::string& extension) const override
    {
        return extension == ".json" || extension == ".JSON";
    }

private:
    GenericConfiguration<E, SettingVariant, JsonSerializer<E, SettingVariant>>& config_;

    /**
     * @brief Convert a setting variant to JSON using ConfigurationTraits
     *
     * This method works with any Setting type that has ConfigurationTraits specialized.
     * It uses std::visit to handle the variant and ConfigurationTraits to serialize values.
     */
    nlohmann::json SettingToJson(const SettingVariant& setting_var) const
    {
        return std::visit([this](const auto& setting) -> nlohmann::json {
            using SettingType = std::decay_t<decltype(setting)>;
            using ValueType = typename SettingType::value_type;

            nlohmann::json j;
            j["name"] = ToString(setting.Name());

            // Use ConfigurationTraits for value serialization
            j["value"] = SerializeValue(setting.Value());

            if (setting.MaxValue()) {
                j["max_value"] = SerializeValue(*setting.MaxValue());
            }

            if (setting.MinValue()) {
                j["min_value"] = SerializeValue(*setting.MinValue());
            }

            if (setting.Unit()) {
                j["unit"] = *setting.Unit();
            }

            if (setting.Description()) {
                j["description"] = *setting.Description();
            }

            return j;
        },
                          setting_var);
    }

    /**
     * @brief Convert JSON to a setting variant using ConfigurationTraits
     *
     * This method works with any Setting type by using the default configuration
     * to determine the expected type, then using ConfigurationTraits to deserialize.
     */
    SettingVariant JsonToSetting(const nlohmann::json& j) const
    {
        std::string name_str = j.at("name").get<std::string>();
        E name = FromString(name_str);

        auto& defaults = config_.GetDefaults();
        auto it = defaults.find(name);
        if (it == defaults.end()) {
            throw std::runtime_error("No default setting found for: " + name_str);
        }

        // Get the type from the default setting and create the new setting
        return std::visit([this, &j, name](const auto& default_setting) -> SettingVariant {
            using SettingType = std::decay_t<decltype(default_setting)>;
            using ValueType = typename SettingType::value_type;
            return CreateSetting<ValueType>(j, name);
        },
                          it->second);
    }

    /**
     * @brief Create a setting of a specific type from JSON using ConfigurationTraits
     */
    template <typename T>
    auto CreateSetting(const nlohmann::json& j, E name) const
    {
        auto& defaults = config_.GetDefaults();
        auto it = defaults.find(name);

        if (it == defaults.end()) {
            throw std::runtime_error("Default setting not found for: " + ToString(name));
        }

        auto default_setting = std::get<Setting<E, T>>(it->second);

        T value;
        if (j.contains("value") && !j.at("value").is_null()) {
            try {
                value = DeserializeValue<T>(j.at("value"));
            }
            catch (const std::exception& e) {
                std::cerr << "Warning: Failed to deserialize value for setting '" << ToString(name)
                          << "'. Using default value instead. Error: " << e.what() << std::endl;
                value = default_setting.Value();
            }
        }
        else {
            value = default_setting.Value();
        }

        std::optional<T> max_value = default_setting.MaxValue();
        if (j.contains("max_value") && !j.at("max_value").is_null()) {
            try {
                max_value = DeserializeValue<T>(j.at("max_value"));
            }
            catch (const std::exception& e) {
                std::cerr << "Warning: Failed to parse max_value for setting '" << ToString(name)
                          << "'. Using default max value. Error: " << e.what() << std::endl;
            }
        }
        std::optional<T> min_value = default_setting.MinValue();
        if (j.contains("min_value") && !j.at("min_value").is_null()) {
            try {
                min_value = DeserializeValue<T>(j.at("min_value"));
            }
            catch (const std::exception& e) {
                std::cerr << "Warning: Failed to parse min_value for setting '" << ToString(name)
                          << "'. Using default min value. Error: " << e.what() << std::endl;
            }
        }

        std::optional<std::string> unit = default_setting.Unit();
        if (j.contains("unit") && !j.at("unit").is_null()) {
            unit = j.at("unit").get<std::string>();
        }

        std::optional<std::string> description = default_setting.Description();
        if (j.contains("description") && !j.at("description").is_null()) {
            description = j.at("description").get<std::string>();
        }

        return Setting<E, T>(name, value, max_value, min_value, unit, description);
    }

    /**
     * @brief Serialize a value using ConfigurationTraits (with fallback for built-in types)
     */
    template <typename T>
    nlohmann::json SerializeValue(const T& value) const
    {
        return SerializeValueImpl(value, std::bool_constant<has_configuration_traits_v<T>> {});
    }

    /**
     * @brief Deserialize a value using ConfigurationTraits (with fallback for built-in types)
     */
    template <typename T>
    T DeserializeValue(const nlohmann::json& json) const
    {
        return DeserializeValueImpl<T>(json, std::bool_constant<has_configuration_traits_v<T>> {});
    }

    // SFINAE helpers for trait-based serialization
    template <typename T>
    nlohmann::json SerializeValueImpl(const T& value, std::true_type) const
    {
        return ConfigurationTraits<T>::ToJson(value);
    }

    template <typename T>
    nlohmann::json SerializeValueImpl(const T& value, std::false_type) const
    {
        // Fallback to direct JSON conversion for built-in types
        return nlohmann::json(value);
    }

    template <typename T>
    T DeserializeValueImpl(const nlohmann::json& json, std::true_type) const
    {
        return ConfigurationTraits<T>::FromJson(json);
    }

    template <typename T>
    T DeserializeValueImpl(const nlohmann::json& json, std::false_type) const
    {
        // Fallback to direct JSON conversion for built-in types
        return json.get<T>();
    }
};

} // namespace config
