#include <cppfig/cppfig.h>

#include <iostream>

namespace settings {

struct AppName {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "MyApplication"; }
};

struct AppVersion {
    static constexpr std::string_view path = "app.version";
    using value_type = std::string;
    static auto default_value() -> std::string { return "1.0.0"; }
};

struct ServerHost {
    static constexpr std::string_view path = "server.host";
    static constexpr std::string_view env_override = "SERVER_HOST";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};

struct ServerPort {
    static constexpr std::string_view path = "server.port";
    static constexpr std::string_view env_override = "SERVER_PORT";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
    static auto validator() -> cppfig::Validator<int> { return cppfig::Range(1, 65535); }
};

struct ServerMaxConnections {
    static constexpr std::string_view path = "server.max_connections";
    using value_type = int;
    static auto default_value() -> int { return 100; }
    static auto validator() -> cppfig::Validator<int> { return cppfig::Range(1, 10000); }
};

struct LoggingEnabled {
    static constexpr std::string_view path = "logging.enabled";
    using value_type = bool;
    static auto default_value() -> bool { return true; }
};

struct LoggingLevel {
    static constexpr std::string_view path = "logging.level";
    using value_type = std::string;
    static auto default_value() -> std::string { return "info"; }
};

struct FeaturesExperimental {
    static constexpr std::string_view path = "features.experimental";
    static constexpr std::string_view env_override = "ENABLE_EXPERIMENTAL";
    using value_type = bool;
    static auto default_value() -> bool { return false; }
};

}  // namespace settings

// Create schema type - lists all settings
using MySchema = cppfig::ConfigSchema<settings::AppName, settings::AppVersion, settings::ServerHost,
                                      settings::ServerPort, settings::ServerMaxConnections, settings::LoggingEnabled,
                                      settings::LoggingLevel, settings::FeaturesExperimental>;

int main()
{
    // Create configuration manager
    cppfig::Configuration<MySchema> config("/tmp/cppfig_example.json");

    // Load configuration (creates file with defaults if it doesn't exist)
    auto status = config.Load();
    if (!status.ok()) {
        std::cerr << "Failed to load configuration: " << status.message() << std::endl;
        return 1;
    }

    // Access configuration values - fully type-safe with IDE autocompletion!
    std::cout << "Application: " << config.Get<settings::AppName>() << " v" << config.Get<settings::AppVersion>()
              << std::endl;
    std::cout << "Server: " << config.Get<settings::ServerHost>() << ":" << config.Get<settings::ServerPort>()
              << std::endl;
    std::cout << "Max connections: " << config.Get<settings::ServerMaxConnections>() << std::endl;
    std::cout << "Logging enabled: " << (config.Get<settings::LoggingEnabled>() ? "yes" : "no") << std::endl;
    std::cout << "Logging level: " << config.Get<settings::LoggingLevel>() << std::endl;
    std::cout << "Experimental features: " << (config.Get<settings::FeaturesExperimental>() ? "enabled" : "disabled")
              << std::endl;

    // Show the diff between file and defaults
    std::cout << "\n"
              << config.Diff().ToString();

    // Modify a setting (with validation)
    status = config.Set<settings::ServerPort>(9000);
    if (!status.ok()) {
        std::cerr << "Failed to set port: " << status.message() << std::endl;
    }

    // Try to set an invalid value
    status = config.Set<settings::ServerPort>(99999);  // Out of range!
    if (!status.ok()) {
        std::cout << "\nExpected validation error: " << status.message() << std::endl;
    }

    // Save updated configuration
    status = config.Save();
    if (!status.ok()) {
        std::cerr << "Failed to save configuration: " << status.message() << std::endl;
        return 1;
    }

    std::cout << "\nConfiguration saved to: " << config.GetFilePath() << std::endl;

    return 0;
}
