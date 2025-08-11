#include <filesystem>
#include <fstream>
#include <iostream>

#include "cppfig.h"

// Step 1: Define your configuration enum (simulating evolution over time)
enum class AppConfig : uint8_t {
    // Original settings (v1.0)
    AppName,
    Version,
    DebugMode,

    // New settings added in v1.1
    LogLevel,
    MaxConnections,

    // New settings added in v1.2
    CacheSize,
    EnableMetrics
};

// Step 2: Declare compile-time type mappings
namespace config {
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::AppName, std::string);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::Version, std::string);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::DebugMode, bool);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::LogLevel, std::string);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::MaxConnections, int);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::CacheSize, int);
DECLARE_CONFIG_TYPE(AppConfig, AppConfig::EnableMetrics, bool);
} // namespace config

// Step 3: Implement enum string conversion
std::string ToString(AppConfig config)
{
    switch (config) {
    case AppConfig::AppName:
        return "app_name";
    case AppConfig::Version:
        return "version";
    case AppConfig::DebugMode:
        return "debug_mode";
    case AppConfig::LogLevel:
        return "log_level";
    case AppConfig::MaxConnections:
        return "max_connections";
    case AppConfig::CacheSize:
        return "cache_size";
    case AppConfig::EnableMetrics:
        return "enable_metrics";
    default:
        return "unknown";
    }
}

AppConfig FromString(const std::string& str)
{
    if (str == "app_name")
        return AppConfig::AppName;
    if (str == "version")
        return AppConfig::Version;
    if (str == "debug_mode")
        return AppConfig::DebugMode;
    if (str == "log_level")
        return AppConfig::LogLevel;
    if (str == "max_connections")
        return AppConfig::MaxConnections;
    if (str == "cache_size")
        return AppConfig::CacheSize;
    if (str == "enable_metrics")
        return AppConfig::EnableMetrics;
    throw std::runtime_error("Invalid config name: " + str);
}

// Step 4: Specialize JsonSerializer for your enum
namespace config {
template <>
inline std::string JsonSerializer<AppConfig, BasicSettingVariant<AppConfig>>::ToString(AppConfig enumValue)
{
    return ::ToString(enumValue);
}

template <>
inline AppConfig JsonSerializer<AppConfig, BasicSettingVariant<AppConfig>>::FromString(const std::string& str)
{
    return ::FromString(str);
}
} // namespace config

// Helper function to create a minimal "old" config file
void CreateLegacyConfigFile(const std::filesystem::path& path)
{
    std::ofstream file(path);
    file << R"([
    {
        "description": "Application name",
        "name": "app_name",
        "value": "LegacyApp"
    },
    {
        "description": "Application version",
        "name": "version",
        "value": "1.0.0"
    },
    {
        "description": "Enable debug mode",
        "name": "debug_mode",
        "value": false
    }
])";
    file.close();
}

int main()
{
    std::cout << "🔄 C++fig Schema Migration Example\n\n";

    using Config = config::BasicJsonConfiguration<AppConfig>;
    const std::filesystem::path configPath = "app_config_migration.json";

    // Step 1: Create a "legacy" config file (simulating an old version)
    std::cout << "📁 Creating legacy configuration file (v1.0 with only 3 settings)...\n";
    CreateLegacyConfigFile(configPath);
    std::cout << "  Created: " << configPath << "\n\n";

    // Step 2: Define CURRENT configuration with ALL settings (including new ones)
    const Config::DefaultConfigMap currentDefaults = {
        // Original settings (v1.0)
        { AppConfig::AppName,
          config::ConfigHelpers<AppConfig>::CreateStringSetting<AppConfig::AppName>(
              "MyAwesomeApp", "Application name") },
        { AppConfig::Version,
          config::ConfigHelpers<AppConfig>::CreateStringSetting<AppConfig::Version>(
              "1.2.0", "Application version") },
        { AppConfig::DebugMode,
          config::ConfigHelpers<AppConfig>::CreateBoolSetting<AppConfig::DebugMode>(
              false, "Enable debug mode") },

        // New settings added in v1.1
        { AppConfig::LogLevel,
          config::ConfigHelpers<AppConfig>::CreateStringSetting<AppConfig::LogLevel>(
              "INFO", "Logging level (DEBUG, INFO, WARN, ERROR)") },
        { AppConfig::MaxConnections,
          config::ConfigHelpers<AppConfig>::CreateIntSetting<AppConfig::MaxConnections>(
              100, 1, 1000, "Maximum number of concurrent connections", "connections") },

        // New settings added in v1.2
        { AppConfig::CacheSize,
          config::ConfigHelpers<AppConfig>::CreateIntSetting<AppConfig::CacheSize>(
              1024, 64, 8192, "Cache size in MB", "MB") },
        { AppConfig::EnableMetrics,
          config::ConfigHelpers<AppConfig>::CreateBoolSetting<AppConfig::EnableMetrics>(
              true, "Enable application metrics collection") }
    };

    std::cout << "🔍 Loading configuration with automatic schema migration...\n";

    // Step 3: Load configuration - this will automatically detect missing settings
    Config appConfig(configPath, currentDefaults);

    std::cout << "✅ Configuration loaded successfully!\n\n";

    // Step 4: Show that all settings are now available (including newly added ones)
    std::cout << "📋 Current configuration values:\n";

    // Original settings (should preserve user values from legacy file)
    std::cout << "  Original settings (v1.0):\n";
    std::cout << "    App Name: " << appConfig.GetSetting<AppConfig::AppName>().Value() << "\n";
    std::cout << "    Version: " << appConfig.GetSetting<AppConfig::Version>().Value() << "\n";
    std::cout << "    Debug Mode: " << (appConfig.GetSetting<AppConfig::DebugMode>().Value() ? "Yes" : "No") << "\n\n";

    // New settings (should have default values)
    std::cout << "  New settings (v1.1):\n";
    std::cout << "    Log Level: " << appConfig.GetSetting<AppConfig::LogLevel>().Value() << "\n";
    std::cout << "    Max Connections: " << appConfig.GetSetting<AppConfig::MaxConnections>().Value() << "\n\n";

    std::cout << "  New settings (v1.2):\n";
    std::cout << "    Cache Size: " << appConfig.GetSetting<AppConfig::CacheSize>().Value() << " MB\n";
    std::cout << "    Enable Metrics: " << (appConfig.GetSetting<AppConfig::EnableMetrics>().Value() ? "Yes" : "No") << "\n\n";

    // Step 5: Demonstrate manual schema synchronization check
    std::cout << "🔍 Checking for missing settings manually...\n";
    auto missing = appConfig.GetMissingSettings();
    if (missing.empty()) {
        std::cout << "  ✅ All settings are present in the configuration\n";
    }
    else {
        std::cout << "  ⚠️  Missing settings detected:\n";
        for (const auto& setting : missing) {
            std::cout << "    - " << ToString(setting) << "\n";
        }
    }
    std::cout << "\n";

    // Step 6: Show the updated configuration file
    std::cout << "📄 The configuration file has been automatically updated:\n";
    std::ifstream file(configPath);
    std::string line;
    int lineNum = 1;
    while (std::getline(file, line) && lineNum <= 10) { // Show first 10 lines
        std::cout << "  " << line << "\n";
        lineNum++;
    }
    if (lineNum > 10) {
        std::cout << "  ... (file continues)\n";
    }
    file.close();
    std::cout << "\n";

    // Step 7: Demonstrate user modification preservation
    std::cout << "🛠️  Modifying a setting and reloading to show preservation...\n";
    appConfig.GetSetting<AppConfig::LogLevel>().SetValue(std::string("DEBUG"));
    appConfig.GetSetting<AppConfig::MaxConnections>().SetValue(250);
    appConfig.Save();

    // Reload without auto-migration to show values are preserved
    Config reloadedConfig(configPath, currentDefaults);
    std::cout << "  After reload:\n";
    std::cout << "    Log Level: " << reloadedConfig.GetSetting<AppConfig::LogLevel>().Value() << " (modified)\n";
    std::cout << "    Max Connections: " << reloadedConfig.GetSetting<AppConfig::MaxConnections>().Value() << " (modified)\n";
    std::cout << "    Cache Size: " << reloadedConfig.GetSetting<AppConfig::CacheSize>().Value() << " MB (default)\n\n";

    // Step 8: Demonstrate disabling auto-migration
    std::cout << "⚙️  Demonstrating manual control over schema migration...\n";

    // Create another legacy file
    const std::filesystem::path manualPath = "manual_migration_test.json";
    CreateLegacyConfigFile(manualPath);

    // Load without auto-migration
    Config manualConfig(manualPath, currentDefaults);
    if (!manualConfig.Load(false)) { // Disable auto-migration
        std::cout << "  ❌ Failed to load configuration\n";
    }
    else {
        std::cout << "  📁 Loaded without auto-migration\n";

        // Check for missing settings
        auto manualMissing = manualConfig.GetMissingSettings();
        std::cout << "  ⚠️  Missing settings: " << manualMissing.size() << "\n";

        // Manually sync schema
        std::cout << "  🔄 Manually synchronizing schema...\n";
        if (manualConfig.SyncSchemaWithDefaults()) {
            std::cout << "  ✅ Schema synchronized successfully\n";
        }

        // Check again
        auto afterSync = manualConfig.GetMissingSettings();
        std::cout << "  ✅ Missing settings after sync: " << afterSync.size() << "\n";
    }

    std::cout << "\n🎉 Schema migration example completed!\n\n";

    std::cout << "📝 Key features demonstrated:\n";
    std::cout << "  ✅ Automatic detection and addition of new settings\n";
    std::cout << "  ✅ Preservation of existing user-modified values\n";
    std::cout << "  ✅ Seamless schema evolution without breaking existing configs\n";
    std::cout << "  ✅ Manual control over when migration occurs\n";
    std::cout << "  ✅ Safe handling of missing or invalid settings\n\n";

    // Cleanup
    std::filesystem::remove(manualPath);

    return 0;
}
