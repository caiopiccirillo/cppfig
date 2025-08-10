#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <variant>

#include "ConfigHelpers.h"
#include "GenericConfiguration.h"
#include "JsonSerializer.h"
#include "Setting.h"
#include "TypedSetting.h"

namespace example {

// Define our configuration enum
enum class AppConfigName : uint8_t {
    DatabaseUrl,
    MaxConnections,
    EnableLogging,
    RetryCount,
    LogLevel
};

} // namespace example

// Declare compile-time type mappings for each configuration setting
// This enables strong compile-time type checking
namespace config {
DECLARE_CONFIG_TYPE(example::AppConfigName, example::AppConfigName::DatabaseUrl, std::string);
DECLARE_CONFIG_TYPE(example::AppConfigName, example::AppConfigName::MaxConnections, int);
DECLARE_CONFIG_TYPE(example::AppConfigName, example::AppConfigName::EnableLogging, bool);
DECLARE_CONFIG_TYPE(example::AppConfigName, example::AppConfigName::RetryCount, int);
DECLARE_CONFIG_TYPE(example::AppConfigName, example::AppConfigName::LogLevel, std::string);
} // namespace config

namespace example {

// Implement the toString and fromString functions for the enum
// (These are required by JsonSerializer)
inline std::string ToString(AppConfigName name)
{
    switch (name) {
    case AppConfigName::DatabaseUrl:
        return "database_url";
    case AppConfigName::MaxConnections:
        return "max_connections";
    case AppConfigName::EnableLogging:
        return "enable_logging";
    case AppConfigName::RetryCount:
        return "retry_count";
    case AppConfigName::LogLevel:
        return "log_level";
    default:
        return "unknown";
    }
}

inline AppConfigName FromString(const std::string& str)
{
    if (str == "database_url") {
        return AppConfigName::DatabaseUrl;
    }
    if (str == "max_connections") {
        return AppConfigName::MaxConnections;
    }
    if (str == "enable_logging") {
        return AppConfigName::EnableLogging;
    }
    if (str == "retry_count") {
        return AppConfigName::RetryCount;
    }
    if (str == "log_level") {
        return AppConfigName::LogLevel;
    }
    throw std::runtime_error("Invalid configuration name: " + str);
}

} // namespace example

// Provide template specializations for the JsonSerializer
namespace config {
template <>
inline std::string JsonSerializer<example::AppConfigName>::ToString(example::AppConfigName enumValue)
{
    return example::ToString(enumValue);
}

template <>
inline example::AppConfigName JsonSerializer<example::AppConfigName>::FromString(const std::string& str)
{
    return example::FromString(str);
}
} // namespace config

namespace example {

// Define the configuration type
using AppConfig = ::config::GenericConfiguration<AppConfigName, ::config::JsonSerializer<AppConfigName>>;

// Define the variant type for AppConfig settings
using AppSettingVariant = std::variant<
    ::config::Setting<AppConfigName, int>,
    ::config::Setting<AppConfigName, float>,
    ::config::Setting<AppConfigName, double>,
    ::config::Setting<AppConfigName, std::string>,
    ::config::Setting<AppConfigName, bool>>;

// Define default configuration values using the helper functions
inline const AppConfig::DefaultConfigMap DefaultAppConfig = {
    { AppConfigName::DatabaseUrl,
      ::config::ConfigHelpers<AppConfigName>::CreateStringSetting<AppConfigName::DatabaseUrl>(
          "mongodb://localhost:27017",
          "URL for database connection") },
    { AppConfigName::MaxConnections,
      ::config::ConfigHelpers<AppConfigName>::CreateIntSetting<AppConfigName::MaxConnections>(
          100, 1, 1000, "Maximum number of database connections", "connections") },
    { AppConfigName::EnableLogging,
      ::config::ConfigHelpers<AppConfigName>::CreateBoolSetting<AppConfigName::EnableLogging>(
          true, "Enable application logging") },
    { AppConfigName::RetryCount,
      ::config::ConfigHelpers<AppConfigName>::CreateIntSetting<AppConfigName::RetryCount>(
          3, 0, 10, "Number of retry attempts for operations", "retries") },
    { AppConfigName::LogLevel,
      ::config::ConfigHelpers<AppConfigName>::CreateStringSetting<AppConfigName::LogLevel>(
          "info", "Logging level (debug, info, warning, error)") }
};

// Example of how to use the configuration with compile-time type safety
class Application {
public:
    explicit Application(const std::filesystem::path& config_path)
        : config_(config_path, DefaultAppConfig)
    {
        // Access configuration values with compile-time type safety
        // These calls will fail at compile-time if types don't match
        auto db_url = config_.GetValue<AppConfigName::DatabaseUrl, std::string>();
        auto max_conn = config_.GetValue<AppConfigName::MaxConnections, int>();
        auto logging_enabled = config_.GetValue<AppConfigName::EnableLogging, bool>();

        // Example of updating a setting with compile-time type checking
        config_.SetValue<AppConfigName::RetryCount, int>(5);
        config_.Save();
    }

    // Get the underlying config object
    AppConfig& GetConfig() { return config_; }
    const AppConfig& GetConfig() const { return config_; }

    /**
     * @brief Clean, ergonomic configuration access with automatic type deduction
     *
     * This provides the cleanest API where types are automatically inferred:
     * - auto setting = app.GetSetting(AppConfigName::DatabaseUrl);
     * - auto value = setting.Value();  // Type automatically deduced
     * - setting.SetValue(newValue);    // Type automatically deduced
     */
    template <AppConfigName EnumValue>
    auto GetSetting() const
    {
        return config_.template GetSetting<EnumValue>();
    }

    template <AppConfigName EnumValue>
    auto GetSetting()
    {
        return config_.template GetSetting<EnumValue>();
    }

    /**
     * @brief Legacy API for backward compatibility
     * @deprecated Use GetSetting(enumValue).Value() instead
     */
    template <AppConfigName EnumValue, typename T>
    const T& GetConfigValue() const
    {
        return config_.template GetValue<EnumValue, T>();
    }

    /**
     * @brief Legacy API for backward compatibility
     * @deprecated Use GetSetting(enumValue).SetValue(value) instead
     */
    template <AppConfigName EnumValue, typename T>
    void SetConfigValue(const T& value)
    {
        config_.template SetValue<EnumValue, T>(value);
    }

    /**
     * @brief Validate current configuration values
     * @return true if all values are valid, false otherwise
     */
    bool ValidateConfiguration() const
    {
        // Use the built-in validation
        if (!config_.ValidateAll()) {
            return false;
        }

        // Additional custom validation using the clean API
        try {
            // Clean API - types automatically deduced!
            auto max_conn_setting = config_.GetSetting<AppConfigName::MaxConnections>();
            auto retry_setting = config_.GetSetting<AppConfigName::RetryCount>();
            auto log_level_setting = config_.GetSetting<AppConfigName::LogLevel>();

            auto max_conn = max_conn_setting.Value(); // int automatically deduced
            auto retry_count = retry_setting.Value(); // int automatically deduced
            auto log_level = log_level_setting.Value(); // string automatically deduced

            // Basic validation using simple validators
            auto conn_validator = ::config::SimpleValidators<int>::Range(1, 1000);
            auto retry_validator = ::config::SimpleValidators<int>::Range(0, 10);
            auto log_level_validator = ::config::SimpleValidators<std::string>::OneOf(
                { "debug", "info", "warning", "error" });

            return conn_validator(max_conn) && retry_validator(retry_count) && log_level_validator(log_level);
        }
        catch (...) {
            return false;
        }
    }

    /**
     * @brief Get validation errors as strings
     */
    std::vector<std::string> GetValidationErrors() const
    {
        auto errors = config_.GetValidationErrors();

        // Add custom validation errors using the clean API
        try {
            // Clean API - no type specifications needed!
            auto max_conn = config_.GetSetting<AppConfigName::MaxConnections>().Value();
            auto retry_count = config_.GetSetting<AppConfigName::RetryCount>().Value();
            auto log_level = config_.GetSetting<AppConfigName::LogLevel>().Value();

            if (max_conn < 1 || max_conn > 1000) {
                errors.push_back("MaxConnections must be between 1 and 1000");
            }
            if (retry_count < 0 || retry_count > 10) {
                errors.push_back("RetryCount must be between 0 and 10");
            }

            std::vector<std::string> valid_levels = { "debug", "info", "warning", "error" };
            if (std::find(valid_levels.begin(), valid_levels.end(), log_level) == valid_levels.end()) {
                errors.push_back("LogLevel must be one of: debug, info, warning, error");
            }
        }
        catch (...) {
            errors.push_back("Failed to validate configuration values");
        }

        return errors;
    }

private:
    AppConfig config_;
};

} // namespace example
