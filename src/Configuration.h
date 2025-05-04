#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>

#include "ConfigurationInterface.h"
#include "ConfigurationName.h"
#include "Setting.h"

NLOHMANN_JSON_NAMESPACE_BEGIN

/**
 * @brief Serializer. Uses Argument-dependent lookup to choose to_json/from_json functions from the types' namespaces.
Overloaded to work with std::optional
 *
 * @tparam T Type
 */
template <typename T>
struct adl_serializer<std::optional<T>> {
    /**
     * @brief Converts any value type to a JSON value
     *
     * @param json json
     * @param t optional value
     */
    static void to_json(json& json, const std::optional<T>& t) // NOLINT(readability-identifier-naming)
    {
        if (t.has_value()) {
            json = *t;
        }
        else {

            json = nullptr;
        }
    }
    /**
     * @brief Converts a JSON value to any value type
     *
     * @param j json
     * @param opt optional value
     */
    static void from_json(const json& j, std::optional<T>& opt) // NOLINT(readability-identifier-naming)
    {
        if (j.is_null()) {
            opt = std::nullopt;
        }
        else {
            opt = j.get<T>();
        }
    }
};

/**
 * @brief Helper function to convert a JSON value to a std::variant
 *
 * @tparam T Type
 * @tparam Ts Types
 * @param j json
 * @param data data
 */
template <typename T, typename... Ts>
void variant_from_json(const nlohmann::json& j, std::variant<Ts...>& data) // NOLINT(readability-identifier-naming)
{
    try {
        data = j.get<T>();
    }
    catch (...) {
    }
}

/**
 * @brief Serializer. Uses Argument-dependent lookup to choose to_json/from_json functions from the types' namespaces.
Overloaded to work with std::variant
 *
 * @tparam Ts Type list
 */
template <typename... Ts>
struct adl_serializer<std::variant<Ts...>> {
    /**
     * @brief Converts any value type to a JSON value
     *
     * @param j json
     * @param data data
     */
    static void to_json(nlohmann::json& j, const std::variant<Ts...>& data) // NOLINT(readability-identifier-naming)
    {
        std::visit([&j](const auto& v) { j = v; }, data);
    }
    /**
     * @brief Converts a JSON value to any value type
     *
     * @param j json
     * @param data data
     */
    static void from_json(const nlohmann::json& j, std::variant<Ts...>& data) // NOLINT(readability-identifier-naming)
    {
        (variant_from_json<Ts>(j, data), ...);
    }
};

NLOHMANN_JSON_NAMESPACE_END

namespace config {

using json = nlohmann::json;
class Configuration : public IConfiguration {
public:
    /**
     * @brief Configuration used to load and save configuration settings.
     *
     * @param filepath Path to the configuration file
     */
    explicit Configuration(std::filesystem::path filepath);
    /**
     * @brief Get a setting from the configuration file.
     *
     * @param config_name Configuration name
     * @return Setting Setting value
     */
    [[nodiscard]] Setting GetSetting(ConfigurationName config_name) override;

private:
    /**
     * @brief Load the configuration file.
     *
     * @return true if the file was loaded successfully
     */
    [[nodiscard]] bool LoadConfigurationFile();
    /**
     * @brief Write the configuration file.
     *
     * @param jsonfile JSON file
     * @return true if the file was written successfully
     */
    [[nodiscard]] bool WriteConfigurationFile(json& jsonfile);
    /**
     * @brief Create a default configuration file.
     *
     * @return true if the file was created successfully
     */
    [[nodiscard]] bool CreateDefaultConfigurationFile();

    std::filesystem::path filepath_;
    json config_data_;
};

/**
 * @brief Convert a Setting to a JSON object.
 *
 * @param j JSON object
 * @param setting Setting object
 */
void to_json(json& j, const Setting& setting); // NOLINT(readability-identifier-naming)
/**
 * @brief Convert a JSON object to a Setting.
 *
 * @param j JSON object
 * @param setting Setting object
 */
void from_json(const json& j, config::Setting& setting); // NOLINT(readability-identifier-naming)

} // namespace config
