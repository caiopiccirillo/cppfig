#pragma once

#include <cstdint>
#include <iostream>
#include <ostream>

namespace config {

enum class ConfigurationName : std::uint8_t {
    RoomTemperature,
    PathToText,
};

/**
 * @brief Converts the given ConfigurationName into the equivalent name
 *
 * @param config_name ConfigurationName to be converted
 * @return std::string name of the configuration
 *
 * @remark 	For logging purposes but also to overload the << operator so that GTest can print the errors correctly
 */
inline std::string ToString(ConfigurationName config_name)
{
    switch (config_name) {
    case ConfigurationName::RoomTemperature:
        return "room_temperature";
    case ConfigurationName::PathToText:
        return "path_to_text";
    } // Don't add the default case, so that the compiler can warn you.
    return {};
}

inline ConfigurationName FromString(const std::string& config_name)
{
    if (config_name == "room_temperature") {
        return ConfigurationName::RoomTemperature;
    }
    if (config_name == "path_to_text") {
        return ConfigurationName::PathToText;
    }
    std::cerr << "Trying to parse " << config_name << " as ConfigurationName"  << '\n';
    return {};
}

inline std::ostream& operator<<(std::ostream& ostream, const ConfigurationName& config_name)
{
    ostream << ToString(config_name);
    return ostream;
}

} // namespace config
