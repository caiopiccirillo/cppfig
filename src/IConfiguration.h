#pragma once

#include "Setting.h"

namespace config {
/**
 * @brief Interface for the configuration class.
 *
 */
class IConfiguration {
public:
    virtual ~IConfiguration() = default;
    /**
     * @brief Get the setting according to configuration name
     *
     * @param config_name ConfigurationName to get the setting
     * @return Setting
     */
    virtual Setting GetSetting(ConfigurationName config_name) = 0;
};

} // namespace config
