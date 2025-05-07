#pragma once

#include <string>

namespace config {

/**
 * @brief Interface for serialization and deserialization of configurations
 *
 * @tparam E Enum type that defines the configuration keys
 */
template <typename E>
class IConfigurationSerializer {
public:
    virtual ~IConfigurationSerializer() = default;

    // Methods for serialization and deserialization to be implemented by user
    virtual bool Serialize(const std::string& filepath) = 0;
    virtual bool Deserialize(const std::string& filepath) = 0;
};

} // namespace config
