#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

#include "GenericConfiguration.h"
#include "JsonSerializer.h"
#include "Setting.h"

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

// Define default configuration values
inline const AppConfig::DefaultConfigMap DefaultAppConfig = {
    { AppConfigName::DatabaseUrl,
      ::config::Setting<AppConfigName, std::string>(
          AppConfigName::DatabaseUrl,
          "mongodb://localhost:27017",
          std::nullopt,
          std::nullopt,
          std::nullopt,
          "URL for database connection") },
    { AppConfigName::MaxConnections,
      ::config::Setting<AppConfigName, int>(
          AppConfigName::MaxConnections,
          100,
          1000,
          1,
          "connections",
          "Maximum number of database connections") },
    { AppConfigName::EnableLogging,
      ::config::Setting<AppConfigName, bool>(
          AppConfigName::EnableLogging,
          true,
          std::nullopt,
          std::nullopt,
          std::nullopt,
          "Enable application logging") },
    { AppConfigName::RetryCount,
      ::config::Setting<AppConfigName, int>(
          AppConfigName::RetryCount,
          3,
          10,
          0,
          "retries",
          "Number of retry attempts for operations") },
    { AppConfigName::LogLevel,
      ::config::Setting<AppConfigName, std::string>(
          AppConfigName::LogLevel,
          "info",
          std::nullopt,
          std::nullopt,
          std::nullopt,
          "Logging level (debug, info, warning, error)") }
};

// Example of how to use the configuration
class Application {
public:
    explicit Application(const std::filesystem::path& config_path)
        : config_(config_path, DefaultAppConfig)
    {
        // Access configuration values in a type-safe way with natural syntax
        auto db_url = config_.GetSetting(AppConfigName::DatabaseUrl).Value<std::string>();
        auto max_conn = config_.GetSetting(AppConfigName::MaxConnections).Value<int>();
        auto logging_enabled = config_.GetSetting(AppConfigName::EnableLogging).Value<bool>();

        // Example of updating a setting with type checking
        config_.UpdateSettingValue<int>(AppConfigName::RetryCount, 5);
        config_.Save();
    }

    // Get the underlying config object
    AppConfig& GetConfig() { return config_; }

private:
    AppConfig config_;
};

} // namespace example
