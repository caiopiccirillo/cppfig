#pragma once

#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <ostream>
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
    // Reference to the configuration object
    GenericConfiguration<E, JsonSerializer<E>>& config_;

    // Helper methods for JSON serialization

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
            j["type"] = GetTypeName<ValueType>();

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
        // Get the setting name
        std::string name_str = j.at("name").get<std::string>();
        E name = FromString(name_str);

        // Get the value type
        std::string type = j.at("type").get<std::string>();

        // Create the appropriate setting based on the type
        if (type == "int") {
            return CreateSetting<int>(j, name);
        }
        if (type == "float") {
            return CreateSetting<float>(j, name);
        }
        if (type == "double") {
            return CreateSetting<double>(j, name);
        }
        if (type == "string" || type == "std::string") {
            return CreateSetting<std::string>(j, name);
        }
        if (type == "bool") {
            return CreateSetting<bool>(j, name);
        }

        // Default to string if type is unknown
        return CreateSetting<std::string>(j, name);
    }

    /**
     * @brief Create a setting of a specific type from JSON
     */
    template <typename T>
    Setting<E, T> CreateSetting(const nlohmann::json& j, E name) const
    {
        T value = j.at("value").get<T>();

        std::optional<T> max_value;
        if (j.contains("max_value") && !j.at("max_value").is_null()) {
            max_value = j.at("max_value").get<T>();
        }

        std::optional<T> min_value;
        if (j.contains("min_value") && !j.at("min_value").is_null()) {
            min_value = j.at("min_value").get<T>();
        }

        std::optional<std::string> unit;
        if (j.contains("unit") && !j.at("unit").is_null()) {
            unit = j.at("unit").get<std::string>();
        }

        std::optional<std::string> description;
        if (j.contains("description") && !j.at("description").is_null()) {
            description = j.at("description").get<std::string>();
        }

        return Setting<E, T>(name, value, max_value, min_value, unit, description);
    }

    /**
     * @brief Get the name of a type as a string
     */
    template <typename T>
    static std::string GetTypeName()
    {
        if constexpr (std::is_same_v<T, int>) {
            return "int";
        }
        else if constexpr (std::is_same_v<T, float>) {
            return "float";
        }
        else if constexpr (std::is_same_v<T, double>) {
            return "double";
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return "string";
        }
        else if constexpr (std::is_same_v<T, bool>) {
            return "bool";
        }
        else {
            return "unknown";
        }
    }
};

} // namespace config
