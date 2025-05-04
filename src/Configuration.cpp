#include "Configuration.h"

#include <spdlog/spdlog.h>

#include <fstream>
#include <ostream>

#include "ConfigurationDefault.h"
#include "ConfigurationName.h"
#include "Setting.h"

namespace config {

Configuration::Configuration(std::filesystem::path filepath)
    : filepath_(std::move(filepath))
{

    if (!std::filesystem::exists(filepath_)) {
        bool creation_success = CreateDefaultConfigurationFile();
        if (!creation_success) {
            spdlog::error("Failed to create config file");
        }
    }
    else {
        bool load_success = LoadConfigurationFile();
        if (!load_success) {
            spdlog::error("Failed to load config file");
        }
    }
}

bool Configuration::LoadConfigurationFile()
{
    std::ifstream config_file(filepath_);
    config_file >> config_data_;
    return config_file.good();
}

bool Configuration::WriteConfigurationFile(json& jsonfile)
{
    if (jsonfile.empty()) {
        spdlog::error("JSON file is empty");
        return false;
    }

    std::ofstream config_file;
    config_file.open(filepath_);

    if (!config_file.is_open()) {
        int errnum = errno;
        spdlog::error("Failed to open file: {}", strerror(errnum));
    }
    try {
        config_file << std::setw(4) << jsonfile << '\n';
    }
    catch (const std::exception& e) {
        spdlog::error("Error writing JSON to file: {}", e.what());
        return false;
    }

    return config_file.good();
}

Setting Configuration::GetSetting(ConfigurationName config_name)
{
    auto has_name = [config_name](const Setting& setting) { return setting.Name() == config_name; };

    if (!config_data_.empty()) {
        if (auto iter = std::find_if(config_data_.begin(), config_data_.end(), has_name); iter != std::end(config_data_)) {
            return *iter;
        }
    }
    spdlog::info("Configuration {} not found in file, searching on default configuration.", ToString(config_name));

    if (auto iter = std::find_if(DefaultConfig.begin(), DefaultConfig.end(), has_name); iter != std::end(DefaultConfig)) {
        return *iter;
    }

    spdlog::warn("Configuration not found. The available configuration names are:");
    for (const auto& [key, value] : config_data_.items()) {
        spdlog::warn("{}", key);
    }

    std::string error = "Configuration not found: " + ToString(config_name);
    throw std::runtime_error(error);
}

bool Configuration::CreateDefaultConfigurationFile()
{
    json default_config;

    for (const auto& setting : DefaultConfig) {
        default_config += setting;
    }

    bool write_success = WriteConfigurationFile(default_config);

    return write_success;
}

void to_json(json& j, const Setting& setting)
{
    j = json {
        { "name", ToString(setting.Name()) },
        { "value", setting.Value() },
        { "max_value", setting.MaxValue() },
        { "min_value", setting.MinValue() },
        { "unit", setting.Unit() },
        { "description", setting.Description() }

    };
}

void from_json(const json& j, config::Setting& setting)
{
    config::ConfigurationName name = config::FromString(j.at("name").get<std::string>());
    config::SettingValueType value = j.at("value").get<config::SettingValueType>();
    std::optional<config::SettingValueType> max_value = j.at("max_value").get<std::optional<config::SettingValueType>>();
    std::optional<config::SettingValueType> min_value = j.at("min_value").get<std::optional<config::SettingValueType>>();
    std::optional<std::string> unit = j.at("unit").get<std::optional<std::string>>();
    std::optional<std::string> description = j.at("description").get<std::optional<std::string>>();
    setting = config::Setting { name, value, max_value, min_value, unit, description };
}

} // namespace config
