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

#include "IConfigurationSerializer.h"
#include "Setting.h"

namespace config {

// Forward declaration for GenericConfiguration
template <typename E, typename S>
class GenericConfiguration;

/**
 * @brief JSON serializer for configurations
 *
 * @tparam E Enum type for configuration keys
 */
template <typename E>
class JsonSerializer : public IConfigurationSerializer<E> {
public:
    // Forward declare the enum string conversion functions
    // These must be implemented by the user for their enum type
    static std::string ToString(E enumValue);
    static E FromString(const std::string& str);

    using SettingVariant = std::variant<
        Setting<E, int>,
        Setting<E, float>,
        Setting<E, double>,
        Setting<E, std::string>,
        Setting<E, bool>>;

    /**
     * @brief Constructor that takes a reference to the configuration
     *
     * @param config Reference to the configuration object
     */
    explicit JsonSerializer(GenericConfiguration<E, JsonSerializer<E>>& config)
        : config_(config)
    {
    }

    /**
     * @brief Serialize the configuration to a JSON file
     *
     * @param filepath Path to the file
     * @return true if successful, false otherwise
     */
    bool Serialize(const std::string& filepath) override
    {
        nlohmann::json json_config;

        for (const auto& [key, setting_var] : config_.GetAllSettings()) {
            // Convert each setting to JSON
            nlohmann::json setting_json = SettingToJson(setting_var);
            json_config.push_back(setting_json);
        }

        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }

        file << std::setw(4) << json_config << '\n';
        return file.good();
    }

    /**
     * @brief Deserialize the configuration from a JSON file
     *
     * @param filepath Path to the file
     * @return true if successful, false otherwise
     */
    bool Deserialize(const std::string& filepath) override
    {
        std::ifstream file(filepath);
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

        // Update configuration with parsed settings
        config_.UpdateSettings(settings);
        return true;
    }

private:
    GenericConfiguration<E, JsonSerializer<E>>& config_;

    /**
     * @brief Convert a setting variant to JSON
     */
    nlohmann::json SettingToJson(const SettingVariant& setting_var) const
    {
        return std::visit([this](const auto& setting) -> nlohmann::json {
            using ValueType = std::decay_t<decltype(setting.Value())>;

            nlohmann::json j;
            j["name"] = ToString(setting.Name());
            j["value"] = setting.Value();

            if (setting.MaxValue()) {
                j["max_value"] = *setting.MaxValue();
            }

            if (setting.MinValue()) {
                j["min_value"] = *setting.MinValue();
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
     * @brief Convert JSON to a setting variant
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

        // Get the type from the default setting
        return std::visit([this, &j, name](const auto& default_setting) -> SettingVariant {
            using ValueType = std::decay_t<decltype(default_setting.Value())>;
            return CreateSetting<ValueType>(j, name);
        },
                          it->second);
    }

    /**
     * @brief Create a setting of a specific type from JSON
     */
    template <typename T>
    Setting<E, T> CreateSetting(const nlohmann::json& j, E name) const
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
                value = j.at("value").get<T>();
            }
            catch (const nlohmann::json::exception& e) {
                std::cerr << "Warning: Type mismatch for setting '" << ToString(name)
                          << "'. Expected type matching default. Using default value instead. Error: "
                          << e.what() << std::endl;
                value = default_setting.Value();
            }
        }
        else {
            value = default_setting.Value();
        }

        std::optional<T> max_value = default_setting.MaxValue();
        if (j.contains("max_value") && !j.at("max_value").is_null()) {
            try {
                max_value = j.at("max_value").get<T>();
            }
            catch (const nlohmann::json::exception& e) {
                std::cerr << "Warning: Failed to parse max_value for setting '" << ToString(name)
                          << "'. Using default max value. Error: " << e.what() << std::endl;
            }
        }
        std::optional<T> min_value = default_setting.MinValue();
        if (j.contains("min_value") && !j.at("min_value").is_null()) {
            try {
                min_value = j.at("min_value").get<T>();
            }
            catch (const nlohmann::json::exception& e) {
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
};

} // namespace config
