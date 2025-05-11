#include <filesystem>
#include <iostream>

#include "ExampleConfiguration.h"

int main()
{
    try {
        std::cout << "Configuration Library Example" << '\n';
        std::cout << "----------------------------" << '\n';

        // Initialize configuration
        std::filesystem::path config_path = "config.json";
        example::Application app(config_path);

        // Get config instance
        auto& config = app.GetConfig();

        // Type-safe value retrieval
        auto db_url = config.GetSetting(example::AppConfigName::DatabaseUrl).Value<std::string>();
        std::cout << "Database URL: " << db_url << '\n';

        auto max_conn = config.GetSetting(example::AppConfigName::MaxConnections).Value<int>();
        std::cout << "Max Connections: " << max_conn << '\n';

        auto logging_enabled = config.GetSetting(example::AppConfigName::EnableLogging).Value<bool>();
        std::cout << "Logging Enabled: " << (logging_enabled ? "Yes" : "No") << '\n';

        auto retry_count = config.GetSetting(example::AppConfigName::RetryCount).Value<int>();
        std::cout << "Retry Count: " << retry_count << '\n';

        auto log_level = config.GetSetting(example::AppConfigName::LogLevel).Value<std::string>();
        std::cout << "Log Level: " << log_level << '\n';

        // Update a setting
        std::cout << "\nUpdating max connections to 200..." << '\n';
        config.UpdateSettingValue<int>(example::AppConfigName::MaxConnections, 200);

        int new_max_conn = config.GetSetting(example::AppConfigName::MaxConnections).Value<int>();
        std::cout << "New Max Connections: " << new_max_conn << '\n';

        // Save to file
        if (config.Save()) {
            std::cout << "\nChanges saved successfully to \"" << config_path.string() << "\"" << '\n';
        }
        else {
            std::cout << "\nFailed to save changes" << '\n';
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
