#include <iostream>
#include <filesystem>

#include "cppfig.h"

// Step 1: Define custom types that you want to use in configuration

/**
 * @brief Custom enum for log levels
 */
enum class LogLevel : uint8_t {
    Debug,
    Info,
    Warning,
    Error
};

/**
 * @brief Custom struct for database configuration
 */
struct DatabaseConfig {
    std::string host;
    int port;
    std::string username;

    // Required for comparison operators in Setting class
    bool operator==(const DatabaseConfig& other) const {
        return host == other.host && port == other.port && username == other.username;
    }

    bool operator!=(const DatabaseConfig& other) const {
        return !(*this == other);
    }

    bool operator<(const DatabaseConfig& other) const {
        if (host != other.host) return host < other.host;
        if (port != other.port) return port < other.port;
        return username < other.username;
    }
};

// Step 2: Specialize ConfigurationTraits for your custom types

/**
 * @brief Traits specialization for LogLevel enum
 */
template <>
struct config::ConfigurationTraits<LogLevel> {
    static nlohmann::json ToJson(const LogLevel& value) {
        switch (value) {
            case LogLevel::Debug: return nlohmann::json("debug");
            case LogLevel::Info: return nlohmann::json("info");
            case LogLevel::Warning: return nlohmann::json("warning");
            case LogLevel::Error: return nlohmann::json("error");
            default: throw std::runtime_error("Invalid LogLevel value");
        }
    }

    static LogLevel FromJson(const nlohmann::json& json) {
        std::string str = json.get<std::string>();
        if (str == "debug") return LogLevel::Debug;
        if (str == "info") return LogLevel::Info;
        if (str == "warning") return LogLevel::Warning;
        if (str == "error") return LogLevel::Error;
        throw std::runtime_error("Invalid LogLevel string: " + str);
    }

    static std::string ToString(const LogLevel& value) {
        switch (value) {
            case LogLevel::Debug: return "debug";
            case LogLevel::Info: return "info";
            case LogLevel::Warning: return "warning";
            case LogLevel::Error: return "error";
            default: return "unknown";
        }
    }

    static bool IsValid(const LogLevel& value) {
        return value >= LogLevel::Debug && value <= LogLevel::Error;
    }

    static std::string GetValidationError(const LogLevel& value) {
        return "Invalid log level value: " + std::to_string(static_cast<int>(value));
    }
};

/**
 * @brief Traits specialization for DatabaseConfig struct
 */
template <>
struct config::ConfigurationTraits<DatabaseConfig> {
    static nlohmann::json ToJson(const DatabaseConfig& value) {
        nlohmann::json j;
        j["host"] = value.host;
        j["port"] = value.port;
        j["username"] = value.username;
        return j;
    }

    static DatabaseConfig FromJson(const nlohmann::json& json) {
        DatabaseConfig config;
        config.host = json.at("host").get<std::string>();
        config.port = json.at("port").get<int>();
        config.username = json.at("username").get<std::string>();
        return config;
    }

    static std::string ToString(const DatabaseConfig& value) {
        return value.username + "@" + value.host + ":" + std::to_string(value.port);
    }

    static bool IsValid(const DatabaseConfig& value) {
        return !value.host.empty() &&
               value.port > 0 && value.port < 65536 &&
               !value.username.empty();
    }

    static std::string GetValidationError(const DatabaseConfig& value) {
        if (value.host.empty()) return "Database host cannot be empty";
        if (value.port <= 0 || value.port >= 65536) return "Database port must be between 1-65535";
        if (value.username.empty()) return "Database username cannot be empty";
        return "Invalid database configuration";
    }
};

// Step 3: Define your configuration enum
enum class AppConfig : uint8_t {
    ApplicationName,
    LogLevel,
    MaxThreads,
    DatabaseConfig,
    DebugMode
};

// Step 4: Declare compile-time type mappings using BOTH built-in and custom types
namespace config {
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::ApplicationName, std::string);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::LogLevel, LogLevel);  // Custom enum!
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::MaxThreads, int);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::DatabaseConfig, DatabaseConfig);  // Custom struct!
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::DebugMode, bool);
} // namespace config

// Step 5: Implement enum string conversion for JSON serialization
std::string ToString(AppConfig config)
{
    switch (config) {
    case AppConfig::ApplicationName:
        return "application_name";
    case AppConfig::LogLevel:
        return "log_level";
    case AppConfig::MaxThreads:
        return "max_threads";
    case AppConfig::DatabaseConfig:
        return "database_config";
    case AppConfig::DebugMode:
        return "debug_mode";
    default:
        return "unknown";
    }
}

AppConfig FromString(const std::string& str)
{
    if (str == "application_name") return AppConfig::ApplicationName;
    if (str == "log_level") return AppConfig::LogLevel;
    if (str == "max_threads") return AppConfig::MaxThreads;
    if (str == "database_config") return AppConfig::DatabaseConfig;
    if (str == "debug_mode") return AppConfig::DebugMode;
    throw std::runtime_error("Invalid config name: " + str);
}

// Step 6: Define the custom variant type that includes your custom types
using CustomSettingVariant = std::variant<
    config::Setting<AppConfig, std::string>,
    config::Setting<AppConfig, int>,
    config::Setting<AppConfig, bool>,
    config::Setting<AppConfig, LogLevel>,        // Custom enum!
    config::Setting<AppConfig, DatabaseConfig>   // Custom struct!
>;

// Step 7: Specialize JsonSerializer for your enum with the custom variant
namespace config {
template <>
inline std::string JsonSerializer<AppConfig, CustomSettingVariant>::ToString(AppConfig enumValue)
{
    return ::ToString(enumValue);
}

template <>
inline AppConfig JsonSerializer<AppConfig, CustomSettingVariant>::FromString(const std::string& str)
{
    return ::FromString(str);
}
} // namespace config

int main()
{
    std::cout << "🔧 C++fig Custom Types Example\n\n";

    // Step 8: Define your configuration type using the custom variant
    using Config = config::GenericConfiguration<AppConfig, CustomSettingVariant, config::JsonSerializer<AppConfig, CustomSettingVariant>>;

    // Step 9: Create default configuration with MIXED built-in and custom types
    const Config::DefaultConfigMap defaultConfig = {
        { AppConfig::ApplicationName,
          config::ConfigHelpers<AppConfig>::CreateStringSetting<AppConfig::ApplicationName>(
              "MyAwesomeApp", "Name of the application") },

        // Custom enum type with validation!
        { AppConfig::LogLevel,
          config::Setting<AppConfig, LogLevel>(
              AppConfig::LogLevel,
              LogLevel::Info,  // Default value
              std::nullopt,    // Max value (not applicable for enums)
              std::nullopt,    // Min value (not applicable for enums)
              std::nullopt,    // Unit (not applicable for enums)
              "Application logging level") },

        { AppConfig::MaxThreads,
          config::ConfigHelpers<AppConfig>::CreateIntSetting<AppConfig::MaxThreads>(
              4, 1, 32, "Maximum number of worker threads", "threads") },

        // Custom struct type with validation!
        { AppConfig::DatabaseConfig,
          config::Setting<AppConfig, DatabaseConfig>(
              AppConfig::DatabaseConfig,
              DatabaseConfig{"localhost", 5432, "admin"},  // Default value
              std::nullopt,    // Max value (not applicable for structs)
              std::nullopt,    // Min value (not applicable for structs)
              std::nullopt,    // Unit (not applicable for structs)
              "Database connection configuration") },

        { AppConfig::DebugMode,
          config::ConfigHelpers<AppConfig>::CreateBoolSetting<AppConfig::DebugMode>(
              false, "Enable debug mode") }
    };

    // Step 10: Create configuration instance
    const std::filesystem::path configPath = "custom_types_config.json";
    Config appConfig(configPath, defaultConfig);

    std::cout << "📁 Configuration loaded from: " << configPath << "\n\n";

    // Step 11: Access configuration values with compile-time type safety for ALL types
    std::cout << "🔍 Reading configuration values:\n";

    // Built-in types work as before
    auto appNameSetting = appConfig.GetSetting<AppConfig::ApplicationName>();
    auto appName = appNameSetting.Value(); // std::string

    auto maxThreadsSetting = appConfig.GetSetting<AppConfig::MaxThreads>();
    auto maxThreads = maxThreadsSetting.Value(); // int

    auto debugSetting = appConfig.GetSetting<AppConfig::DebugMode>();
    auto debugMode = debugSetting.Value(); // bool

    // Custom types work exactly the same way!
    auto logLevelSetting = appConfig.GetSetting<AppConfig::LogLevel>();
    auto logLevel = logLevelSetting.Value(); // LogLevel (custom enum!)

    auto dbConfigSetting = appConfig.GetSetting<AppConfig::DatabaseConfig>();
    auto dbConfig = dbConfigSetting.Value(); // DatabaseConfig (custom struct!)

    std::cout << "  Application Name: " << appName << "\n";
    std::cout << "  Max Threads: " << maxThreads << "\n";
    std::cout << "  Debug Mode: " << (debugMode ? "Yes" : "No") << "\n";
    std::cout << "  Log Level: " << config::ConfigurationTraits<LogLevel>::ToString(logLevel) << "\n";
    std::cout << "  Database: " << config::ConfigurationTraits<DatabaseConfig>::ToString(dbConfig) << "\n\n";

    // Step 12: Modify custom types with compile-time type checking
    std::cout << "✏️  Modifying custom type values:\n";

    // These will compile - correct types
    logLevelSetting.SetValue(LogLevel::Warning);
    dbConfigSetting.SetValue(DatabaseConfig{"prod-server", 3306, "app_user"});

    // These would cause COMPILE ERRORS (uncomment to see):
    // logLevelSetting.SetValue("invalid");           // ❌ string cannot be assigned to LogLevel
    // dbConfigSetting.SetValue(42);                  // ❌ int cannot be assigned to DatabaseConfig
    // logLevelSetting.SetValue(dbConfig);            // ❌ DatabaseConfig cannot be assigned to LogLevel

    std::cout << "  Updated Log Level: " <<
        config::ConfigurationTraits<LogLevel>::ToString(appConfig.GetSetting<AppConfig::LogLevel>().Value()) << "\n";
    std::cout << "  Updated Database: " <<
        config::ConfigurationTraits<DatabaseConfig>::ToString(appConfig.GetSetting<AppConfig::DatabaseConfig>().Value()) << "\n\n";

    // Step 13: Validation works for custom types too!
    std::cout << "✅ Validating configuration (including custom types):\n";

    // Test built-in validation
    bool isValid = appConfig.ValidateAll();
    std::cout << "  Built-in validation: " << (isValid ? "PASSED" : "FAILED") << "\n";

    // Test custom validation by setting invalid values
    DatabaseConfig invalidDb{"", -1, ""};  // Invalid: empty host, negative port, empty username
    dbConfigSetting.SetValue(invalidDb);

    // The trait-based validation will catch this!
    bool customValid = dbConfigSetting.IsValid();
    std::cout << "  Custom validation (invalid DB): " << (customValid ? "PASSED" : "FAILED") << "\n";

    if (!customValid) {
        std::cout << "  Validation Error: " << dbConfigSetting.GetValidationError() << "\n";
    }

    // Fix the value
    dbConfigSetting.SetValue(DatabaseConfig{"valid-host", 5432, "valid_user"});
    std::cout << "  Custom validation (fixed DB): " << (dbConfigSetting.IsValid() ? "PASSED" : "FAILED") << "\n\n";

    // Step 14: Save configuration (custom types automatically serialized to JSON!)
    std::cout << "💾 Saving configuration to file...\n";
    appConfig.Save();
    std::cout << "  Configuration saved successfully!\n\n";

    // Step 15: Display the power of the trait system
    std::cout << "🎉 Custom Type System Features:\n";
    std::cout << "  ✅ Compile-time type safety for ANY type\n";
    std::cout << "  ✅ Automatic JSON serialization for custom types\n";
    std::cout << "  ✅ Custom validation logic per type\n";
    std::cout << "  ✅ Unified API for built-in and custom types\n";
    std::cout << "  ✅ Zero runtime overhead\n";
    std::cout << "  ✅ Full metadata support\n\n";

    std::cout << "📄 Check 'custom_types_config.json' to see how custom types are serialized!\n";

    return 0;
}
