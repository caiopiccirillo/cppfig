#pragma once

#include "cppfig/configuration.h"
#include "cppfig/diff.h"
#include "cppfig/interface.h"
#include "cppfig/logging.h"
#include "cppfig/schema.h"
#include "cppfig/serializer.h"
#include "cppfig/setting.h"
#include "cppfig/thread_policy.h"
#include "cppfig/traits.h"
#include "cppfig/validator.h"

/// @namespace cppfig
/// @brief C++20 compile-time type-safe configuration library.
///
/// cppfig is a header-only library for managing application configuration with:
/// - Compile-time type safety via C++20 concepts and templates
/// - Hierarchical configuration with dot-notation paths
/// - Environment variable overrides
/// - Validation with min/max ranges and custom validators
/// - Schema migration (automatic addition of new settings)
/// - Pluggable serialization (JSON by default, YAML/TOML possible)
///
/// @section usage Basic Usage
///
/// @code
/// #include <cppfig/cppfig.h>
///
/// // Define settings as structs
/// namespace settings {
///
/// struct AppName {
///     static constexpr std::string_view kPath = "app.name";
///     using ValueType = std::string;
///     static auto DefaultValue() -> std::string { return "MyApplication"; }
/// };
///
/// struct AppVersion {
///     static constexpr std::string_view kPath = "app.version";
///     using ValueType = std::string;
///     static auto DefaultValue() -> std::string { return "1.0.0"; }
/// };
///
/// struct ServerHost {
///     static constexpr std::string_view kPath = "server.host";
///     static constexpr std::string_view kEnvOverride = "SERVER_HOST";
///     using ValueType = std::string;
///     static auto DefaultValue() -> std::string { return "localhost"; }
/// };
///
/// struct ServerPort {
///     static constexpr std::string_view kPath = "server.port";
///     static constexpr std::string_view kEnvOverride = "SERVER_PORT";
///     using ValueType = int;
///     static auto DefaultValue() -> int { return 8080; }
///     static auto GetValidator() -> cppfig::Validator<int> {
///         return cppfig::Range(1, 65535);
///     }
/// };
///
/// struct LoggingEnabled {
///     static constexpr std::string_view kPath = "logging.enabled";
///     using ValueType = bool;
///     static auto DefaultValue() -> bool { return true; }
/// };
///
/// struct LoggingLevel {
///     static constexpr std::string_view kPath = "logging.level";
///     using ValueType = std::string;
///     static auto DefaultValue() -> std::string { return "info"; }
///     static auto GetValidator() -> cppfig::Validator<std::string> {
///         return cppfig::OneOf<std::string>({"debug", "info", "warn", "error"});
///     }
/// };
///
/// }  // namespace settings
///
/// // Create schema type
/// using MySchema = cppfig::ConfigSchema<
///     settings::AppName,
///     settings::AppVersion,
///     settings::ServerHost,
///     settings::ServerPort,
///     settings::LoggingEnabled,
///     settings::LoggingLevel
/// >;
///
/// int main() {
///     // Create configuration manager
///     cppfig::Configuration<MySchema> config("config.json");
///
///     // Load configuration (creates file with defaults if it doesn't exist)
///     auto status = config.Load();
///     if (!status.ok()) {
///         std::cerr << "Failed to load config: " << status.message() << std::endl;
///         return 1;
///     }
///
///     // Access values with compile-time type safety and IDE autocompletion!
///     std::string host = config.Get<settings::ServerHost>();
///     int port = config.Get<settings::ServerPort>();
///
///     // Set values with validation
///     status = config.Set<settings::ServerPort>(9000);
///     if (!status.ok()) {
///         std::cerr << "Invalid port: " << status.message() << std::endl;
///     }
///
///     // Save configuration
///     status = config.Save();
/// }
/// @endcode
///
/// @section custom_types Custom Types
///
/// To use custom types in configuration, specialize ConfigTraits:
///
/// @code
/// struct DatabaseConfig {
///     std::string host;
///     int port;
///
///     friend void to_json(nlohmann::json& j, const DatabaseConfig& c) {
///         j = nlohmann::json{{"host", c.host}, {"port", c.port}};
///     }
///     friend void from_json(const nlohmann::json& j, DatabaseConfig& c) {
///         j.at("host").get_to(c.host);
///         j.at("port").get_to(c.port);
///     }
/// };
///
/// // Use the ADL helper
/// template<>
/// struct cppfig::ConfigTraits<DatabaseConfig>
///     : cppfig::ConfigTraitsFromJsonAdl<DatabaseConfig> {};
///
/// // Now define a setting using the custom type
/// struct Database {
///     static constexpr std::string_view kPath = "database";
///     using ValueType = DatabaseConfig;
///     static auto DefaultValue() -> DatabaseConfig {
///         return DatabaseConfig{"localhost", 5432};
///     }
/// };
/// @endcode
///
/// @section testing Testing
///
/// For unit testing, include the mock header:
///
/// @code
/// #include <cppfig/testing/mock.h>
///
/// // Use MockConfiguration for in-memory testing
/// cppfig::testing::MockConfiguration<MySchema> mock_config;
/// mock_config.SetValue<settings::ServerPort>(9000);
///
/// // Or use MockVirtualConfigurationProvider with GMock
/// cppfig::testing::MockVirtualConfigurationProvider mock;
/// EXPECT_CALL(mock, Load()).WillOnce(Return(absl::OkStatus()));
/// @endcode
///
/// @section organization Organizing Settings
///
/// You can organize related settings in namespaces or nested structs:
///
/// @code
/// namespace settings::database {
///     struct Host {
///         static constexpr std::string_view kPath = "database.host";
///         using ValueType = std::string;
///         static auto DefaultValue() -> std::string { return "localhost"; }
///     };
///     struct Port {
///         static constexpr std::string_view kPath = "database.port";
///         using ValueType = int;
///         static auto DefaultValue() -> int { return 5432; }
///     };
/// }
///
/// // Access: config.Get<settings::database::Host>()
/// @endcode
