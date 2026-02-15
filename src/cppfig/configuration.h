#pragma once

#include <absl/status/status.h>
#include <absl/status/statusor.h>

#include <cstdlib>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

#include "cppfig/diff.h"
#include "cppfig/interface.h"
#include "cppfig/logging.h"
#include "cppfig/serializer.h"
#include "cppfig/setting.h"
#include "cppfig/traits.h"

namespace cppfig {

/// @brief Main configuration manager.
///
/// This class manages configuration values with:
/// - Compile-time type-safe access via setting types
/// - Environment variable overrides
/// - Validation
/// - Automatic file creation with defaults
/// - Schema migration (adding new settings)
///
/// Usage:
/// @code
/// // Define settings
/// struct AppName {
///     static constexpr std::string_view kPath = "app.name";
///     using ValueType = std::string;
///     static auto DefaultValue() -> std::string { return "MyApp"; }
/// };
///
/// struct ServerPort {
///     static constexpr std::string_view kPath = "server.port";
///     static constexpr std::string_view kEnvOverride = "SERVER_PORT";
///     using ValueType = int;
///     static auto DefaultValue() -> int { return 8080; }
///     static auto GetValidator() -> Validator<int> { return Range(1, 65535); }
/// };
///
/// // Create schema and configuration
/// using MySchema = ConfigSchema<AppName, ServerPort>;
/// Configuration<MySchema> config("config.json");
///
/// // Load and use
/// config.Load();
/// std::string name = config.Get<AppName>();
/// int port = config.Get<ServerPort>();
/// config.Set<ServerPort>(9000);
/// config.Save();
/// @endcode
///
/// @tparam Schema The ConfigSchema type defining all settings.
/// @tparam SerializerT The serializer to use (defaults to JsonSerializer).
template <typename Schema, Serializer SerializerT = JsonSerializer>
class Configuration : public IConfigurationProvider<Configuration<Schema, SerializerT>, Schema>,
                      public IConfigurationProviderVirtual {
public:
    using SerializerType = SerializerT;
    using DataType = typename SerializerT::DataType;

    /// @brief Creates a configuration manager with a file path.
    ///
    /// @param file_path Path to the configuration file.
    explicit Configuration(std::string file_path)
        : file_path_(std::move(file_path))
        , file_values_(DataType::object())
    {
        BuildDefaults();
    }

    /// @brief Gets the value for a setting type.
    ///
    /// Resolution order:
    /// 1. Environment variable (if configured)
    /// 2. File value (if present)
    /// 3. Default value
    template <IsSetting S>
        requires(Schema::template HasSetting<S>)
    [[nodiscard]] auto GetImpl() const -> typename S::ValueType
    {
        using ValueType = typename S::ValueType;

        // 1. Check environment variable
        constexpr auto env_override = GetEnvOverride<S>();
        if constexpr (!env_override.empty()) {
            if (const char* env_value = std::getenv(std::string(env_override).c_str())) {
                auto parsed = ConfigTraits<ValueType>::FromString(env_value);
                if (parsed.has_value()) {
                    return *parsed;
                }
                Logger::WarnF("Failed to parse environment variable %.*s='%s', using fallback",
                              static_cast<int>(env_override.size()), env_override.data(), env_value);
            }
        }

        // 2. Check file value
        auto file_result = SerializerT::GetAtPath(file_values_, S::kPath);
        if (file_result.ok()) {
            auto parsed = ConfigTraits<ValueType>::FromJson(*file_result);
            if (parsed.has_value()) {
                return *parsed;
            }
            Logger::WarnF("Failed to parse file value for '%.*s', using default", static_cast<int>(S::kPath.size()),
                          S::kPath.data());
        }

        // 3. Return default value
        return S::DefaultValue();
    }

    /// @brief Sets the value for a setting type.
    template <IsSetting S>
        requires(Schema::template HasSetting<S>)
    auto SetImpl(typename S::ValueType value) -> absl::Status
    {
        using ValueType = typename S::ValueType;

        // Validate the value
        auto validator = GetSettingValidator<S>();
        auto validation = validator(value);
        if (!validation) {
            return absl::InvalidArgumentError(validation.error_message);
        }

        // Set the value
        auto json_value = ConfigTraits<ValueType>::ToJson(value);
        SerializerT::SetAtPath(file_values_, S::kPath, json_value);

        return absl::OkStatus();
    }

    /// @brief Loads configuration from the file.
    [[nodiscard]] auto LoadImpl() -> absl::Status
    {
        namespace fs = std::filesystem;

        if (!fs::exists(file_path_)) {
            // File doesn't exist - create with defaults
            Logger::InfoF("Configuration file '%s' not found, creating with defaults", file_path_.c_str());
            file_values_ = defaults_;
            return SaveImpl();
        }

        // Load existing file
        auto result = ReadFile<SerializerT>(file_path_);
        if (!result.ok()) {
            return result.status();
        }

        file_values_ = *result;

        // Check for schema migration (new settings in defaults not in file)
        auto diff = DiffDefaultsFromFile(defaults_, file_values_);
        auto added = diff.Added();

        if (!added.empty()) {
            Logger::Warn("New settings detected in schema, adding to configuration file:");
            for (const auto& entry : added) {
                Logger::WarnF("  - %s = %s", entry.path.c_str(), entry.new_value.c_str());
                SerializerT::SetAtPath(file_values_, entry.path, nlohmann::json::parse(entry.new_value));
            }

            // Save the updated configuration
            auto save_status = SaveImpl();
            if (!save_status.ok()) {
                Logger::ErrorF("Failed to save migrated configuration: %s",
                               std::string(save_status.message()).c_str());
                return save_status;
            }
        }

        return absl::OkStatus();
    }

    /// @brief Saves the current configuration to the file.
    [[nodiscard]] auto SaveImpl() const -> absl::Status
    {
        namespace fs = std::filesystem;

        // Create parent directories if needed
        fs::path path(file_path_);
        if (path.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            if (ec) {
                return absl::InternalError("Failed to create directory: " + ec.message());
            }
        }

        return WriteFile<SerializerT>(file_path_, file_values_);
    }

    /// @brief Returns the diff between file values and defaults.
    [[nodiscard]] auto DiffImpl() const -> ConfigDiff { return DiffFileFromDefaults(defaults_, file_values_); }

    /// @brief Validates all current values against their validators.
    [[nodiscard]] auto ValidateAllImpl() const -> absl::Status
    {
        absl::Status status = absl::OkStatus();

        Schema::ForEachSetting([this, &status]<typename S>() {
            if (!status.ok()) {
                return;  // Stop on first error
            }

            using ValueType = typename S::ValueType;
            auto file_result = SerializerT::GetAtPath(file_values_, S::kPath);

            if (file_result.ok()) {
                auto parsed = ConfigTraits<ValueType>::FromJson(*file_result);
                if (parsed.has_value()) {
                    auto validator = GetSettingValidator<S>();
                    auto validation = validator(*parsed);
                    if (!validation) {
                        status = absl::InvalidArgumentError(std::string(S::kPath) + ": " + validation.error_message);
                    }
                }
            }
        });

        return status;
    }

    /// @brief Returns the file path.
    [[nodiscard]] auto GetFilePathImpl() const -> std::string_view { return file_path_; }

    /// @brief Returns the current file values as JSON.
    [[nodiscard]] auto GetFileValues() const -> const DataType& { return file_values_; }

    /// @brief Returns the default values as JSON.
    [[nodiscard]] auto GetDefaults() const -> const DataType& { return defaults_; }

    [[nodiscard]] auto Load() -> absl::Status override { return LoadImpl(); }

    [[nodiscard]] auto Save() const -> absl::Status override { return SaveImpl(); }

    [[nodiscard]] auto GetFilePath() const -> std::string_view override { return GetFilePathImpl(); }

    [[nodiscard]] auto ValidateAll() const -> absl::Status override { return ValidateAllImpl(); }

    [[nodiscard]] auto GetDiffString() const -> std::string override { return DiffImpl().ToString(); }

private:
    void BuildDefaults()
    {
        defaults_ = DataType::object();

        Schema::ForEachSetting([this]<typename S>() {
            using ValueType = typename S::ValueType;
            auto json_value = ConfigTraits<ValueType>::ToJson(S::DefaultValue());
            SerializerT::SetAtPath(defaults_, S::kPath, json_value);
        });
    }

    std::string file_path_;
    DataType file_values_;
    DataType defaults_;
};

}  // namespace cppfig
