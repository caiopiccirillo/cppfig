#pragma once

#include <string>
#include <type_traits>

namespace config {

/**
 * @brief Generic interface for configuration serializers
 *
 * This interface provides a generic serialization contract that is not tied
 * to any specific format (JSON, XML, YAML, etc.). External developers can
 * implement this interface to support different serialization formats.
 *
 * @tparam E Enum type for configuration keys
 * @tparam SettingVariant The variant type that defines supported setting types
 */
template <typename E, typename SettingVariant>
class IConfigurationSerializer {
public:
    static_assert(std::is_enum_v<E>, "Template parameter E must be an enum type");

    using EnumType = E;
    using VariantType = SettingVariant;

    virtual ~IConfigurationSerializer() = default;

    /**
     * @brief Serialize the configuration to a target (file, string, etc.)
     *
     * The target parameter is intentionally generic to support different
     * serialization destinations (files, memory buffers, network streams, etc.)
     *
     * @param target The serialization target (typically a file path)
     * @return true if successful, false otherwise
     */
    virtual bool Serialize(const std::string& target) = 0;

    /**
     * @brief Deserialize the configuration from a source (file, string, etc.)
     *
     * The source parameter is intentionally generic to support different
     * serialization sources (files, memory buffers, network streams, etc.)
     *
     * @param source The serialization source (typically a file path)
     * @return true if successful, false otherwise
     */
    virtual bool Deserialize(const std::string& source) = 0;

    /**
     * @brief Deserialize the configuration with optional schema migration
     *
     * This method allows deserializing with automatic schema migration,
     * where missing settings from defaults are automatically added and
     * the configuration file is updated.
     *
     * @param source The serialization source (typically a file path)
     * @param auto_migrate_schema If true, automatically adds missing settings from defaults
     * @return true if successful, false otherwise
     */
    virtual bool Deserialize(const std::string& source, bool auto_migrate_schema) = 0;

    /**
     * @brief Get the format name (for debugging/logging purposes)
     *
     * @return String identifier for the serialization format (e.g., "JSON", "XML", "YAML")
     */
    virtual std::string GetFormatName() const = 0;

    /**
     * @brief Check if the serializer supports a specific file extension
     *
     * This allows the configuration system to automatically select the
     * appropriate serializer based on file extensions.
     *
     * @param extension File extension (e.g., ".json", ".xml", ".yaml")
     * @return true if this serializer supports the extension
     */
    virtual bool SupportsExtension(const std::string& extension) const = 0;
};

} // namespace config
