#include <filesystem>
#include <iostream>

#include "ExampleConfiguration.h"

int main()
{
    try {
        std::cout << "Configuration Library Example" << '\n';
        std::cout << "----------------------------" << '\n';

        // Initialize the configuration with a file path
        std::filesystem::path config_path = "config.json";
        example::Application app(config_path);

        // Access the configuration object
        auto& config = app.GetConfig();

        // Demonstrate retrieving values with the new, more natural syntax
        std::cout << "Database URL: "
                  << config.GetSetting(example::AppConfigName::DatabaseUrl).Value<std::string>()
                  << '\n';

        std::cout << "Max Connections: "
                  << config.GetSetting(example::AppConfigName::MaxConnections).Value<int>()
                  << '\n';

        std::cout << "Logging Enabled: "
                  << (config.GetSetting(example::AppConfigName::EnableLogging).Value<bool>() ? "Yes" : "No")
                  << '\n';

        std::cout << "Retry Count: "
                  << config.GetSetting(example::AppConfigName::RetryCount).Value<int>()
                  << '\n';

        std::cout << "Log Level: "
                  << config.GetSetting(example::AppConfigName::LogLevel).Value<std::string>()
                  << '\n';

        // Demonstrate updating a setting
        std::cout << "\nUpdating max connections to 200..." << '\n';
        config.UpdateSetting(example::AppConfigName::MaxConnections, 200);

        std::cout << "New Max Connections: "
                  << config.GetSetting(example::AppConfigName::MaxConnections).Value<int>()
                  << '\n';

        // Save changes
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
