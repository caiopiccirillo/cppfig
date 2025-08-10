#include <filesystem>
#include <iostream>

#include "ExampleConfiguration.h"

int main()
{
    try {
        std::cout << "C++fig Configuration Library - Clean Ergonomic API" << '\n';
        std::cout << "=================================================" << '\n';

        // Initialize configuration
        std::filesystem::path config_path = "config.json";
        example::Application app(config_path);

        // Get config instance
        auto& config = app.GetConfig();

        std::cout << "\n1. Clean, Ergonomic Value Retrieval" << '\n';
        std::cout << "----------------------------------" << '\n';
        std::cout << "No need to specify types - they're automatically deduced!" << '\n';

        // Beautiful, clean API - types are automatically deduced from enum values!
        auto db_setting = app.GetSetting<example::AppConfigName::DatabaseUrl>();
        std::cout << "Database URL: " << db_setting.Value() << '\n';

        auto max_conn_setting = app.GetSetting<example::AppConfigName::MaxConnections>();
        std::cout << "Max Connections: " << max_conn_setting.Value() << '\n';

        auto logging_setting = app.GetSetting<example::AppConfigName::EnableLogging>();
        std::cout << "Logging Enabled: " << (logging_setting.Value() ? "Yes" : "No") << '\n';

        auto retry_setting = app.GetSetting<example::AppConfigName::RetryCount>();
        std::cout << "Retry Count: " << retry_setting.Value() << '\n';

        auto log_level_setting = app.GetSetting<example::AppConfigName::LogLevel>();
        std::cout << "Log Level: " << log_level_setting.Value() << '\n';

        std::cout << "\n2. Rich Setting Metadata Access" << '\n';
        std::cout << "------------------------------" << '\n';

        // Access rich metadata easily
        std::cout << "Max connections setting details:" << '\n';
        std::cout << "  Description: " << (max_conn_setting.HasDescription() ? *max_conn_setting.Description() : "None") << '\n';
        std::cout << "  Unit: " << (max_conn_setting.HasUnit() ? *max_conn_setting.Unit() : "None") << '\n';
        std::cout << "  Min value: " << (max_conn_setting.HasMinValue() ? std::to_string(*max_conn_setting.MinValue()) : "None") << '\n';
        std::cout << "  Max value: " << (max_conn_setting.HasMaxValue() ? std::to_string(*max_conn_setting.MaxValue()) : "None") << '\n';
        std::cout << "  Type: " << max_conn_setting.GetTypeName() << '\n';

        std::cout << "\n3. Configuration Validation" << '\n';
        std::cout << "-------------------------" << '\n';

        // Validate current configuration
        bool is_valid = app.ValidateConfiguration();
        std::cout << "Configuration is " << (is_valid ? "valid" : "invalid") << '\n';

        if (!is_valid) {
            auto errors = app.GetValidationErrors();
            for (const auto& error : errors) {
                std::cout << "Error: " << error << '\n';
            }
        }

        std::cout << "\n4. Clean, Type-Safe Updates" << '\n';
        std::cout << "-------------------------" << '\n';

        // Beautiful, clean updates - no type specifications needed!
        constexpr int new_max_connections = 200;
        std::cout << "Updating max connections to " << new_max_connections << "..." << '\n';
        max_conn_setting.SetValue(new_max_connections);

        std::cout << "New Max Connections: " << max_conn_setting.Value() << '\n';

        std::cout << "Updating log level to 'debug'..." << '\n';
        log_level_setting.SetValue(std::string("debug"));

        std::cout << "New Log Level: " << log_level_setting.Value() << '\n';

        std::cout << "\n5. Validation Results After Changes" << '\n';
        std::cout << "----------------------------------" << '\n';

        // Re-validate after changes
        bool is_still_valid = app.ValidateConfiguration();
        std::cout << "Configuration after changes is " << (is_still_valid ? "valid" : "invalid") << '\n';

        std::cout << "\n6. Pretty String Representation" << '\n';
        std::cout << "------------------------------" << '\n';

        // Get formatted string representation of settings
        std::cout << "Database setting: " << db_setting.ToString() << '\n';
        std::cout << "Max connections: " << max_conn_setting.ToString() << '\n';
        std::cout << "Retry count: " << retry_setting.ToString() << '\n';

        std::cout << "\n7. Setting Modification Tracking" << '\n';
        std::cout << "-------------------------------" << '\n';

        // Check which settings have been modified from defaults
        bool max_conn_modified = config.IsModified<example::AppConfigName::MaxConnections>();
        bool log_level_modified = config.IsModified<example::AppConfigName::LogLevel>();
        bool db_url_modified = config.IsModified<example::AppConfigName::DatabaseUrl>();

        std::cout << "MaxConnections modified: " << (max_conn_modified ? "Yes" : "No") << '\n';
        std::cout << "LogLevel modified: " << (log_level_modified ? "Yes" : "No") << '\n';
        std::cout << "DatabaseUrl modified: " << (db_url_modified ? "Yes" : "No") << '\n';

        std::cout << "\n8. Reset to Defaults" << '\n';
        std::cout << "------------------" << '\n';

        // Reset log level back to default
        std::cout << "Resetting log level to default..." << '\n';
        config.ResetToDefault<example::AppConfigName::LogLevel>();

        // Check the reset value using clean API
        auto reset_log_level_setting = app.GetSetting<example::AppConfigName::LogLevel>();
        std::cout << "Reset Log Level: " << reset_log_level_setting.Value() << '\n';

        std::cout << "\n9. Save Configuration" << '\n';
        std::cout << "-------------------" << '\n';

        // Save to file
        if (config.Save()) {
            std::cout << "Changes saved successfully to \"" << config.GetFilePath().string() << "\"" << '\n';
        }
        else {
            std::cout << "Failed to save changes" << '\n';
        }

        std::cout << "\n10. API Comparison" << '\n';
        std::cout << "----------------" << '\n';
        std::cout << "❌ Old verbose API:" << '\n';
        std::cout << "   config.GetValue<MyEnum::SomeValue, int>()" << '\n';
        std::cout << "   config.SetValue<MyEnum::SomeValue, int>(42)" << '\n';
        std::cout << '\n';
        std::cout << "✅ New clean API:" << '\n';
        std::cout << "   config.GetSetting<MyEnum::SomeValue>().Value()" << '\n';
        std::cout << "   config.GetSetting<MyEnum::SomeValue>().SetValue(42)" << '\n';
        std::cout << '\n';
        std::cout << "✅ Benefits:" << '\n';
        std::cout << "   • Types automatically deduced from enum values" << '\n';
        std::cout << "   • Clean, readable code" << '\n';
        std::cout << "   • Easy access to metadata (min/max/unit/description)" << '\n';
        std::cout << "   • Full compile-time type safety" << '\n';
        std::cout << "   • Zero runtime overhead" << '\n';

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
