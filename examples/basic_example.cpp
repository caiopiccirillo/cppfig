#include <cppfig/cppfig.h>

#include <iostream>

namespace settings {

struct AppName {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "MyApplication"; }
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
    static constexpr auto validator() { return cppfig::Range(1, 65535); }
};

struct LoggingEnabled {
    static constexpr std::string_view path = "logging.enabled";
    using value_type = bool;
    static auto default_value() -> bool { return true; }
};

}  // namespace settings

using AppRegistry = cppfig::Registry<settings::AppName, settings::ServerHost, settings::ServerPort, settings::LoggingEnabled>;

int main()
{
    auto loaded = cppfig::Configuration<AppRegistry>::Load("/tmp/cppfig_example.conf");
    if (!loaded.ok()) {
        std::cerr << "Failed to load configuration: " << loaded.status().message() << '\n';
        return 1;
    }

    const auto& config = loaded.value();
    std::cout << "Application: " << config.Get<settings::AppName>() << '\n';
    std::cout << "Server: " << config.Get<settings::ServerHost>() << ":" << config.Get<settings::ServerPort>() << '\n';
    std::cout << "Logging enabled: " << (config.Get<settings::LoggingEnabled>() ? "yes" : "no") << '\n';

    const auto status = config.Save();
    if (!status.ok()) {
        std::cerr << "Failed to save configuration: " << status.message() << '\n';
        return 1;
    }
    return 0;
}
