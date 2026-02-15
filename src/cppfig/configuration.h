#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#include "cppfig/diff.h"
#include "cppfig/conf.h"
#include "cppfig/interface.h"
#include "cppfig/logging.h"
#include "cppfig/serializer.h"
#include "cppfig/setting.h"
#include "cppfig/thread_policy.h"
#include "cppfig/traits.h"
#include "cppfig/value.h"

namespace cppfig {

/// @brief Main configuration manager.
///
/// This class manages configuration values with:
/// - Compile-time type-safe access via setting types
/// - Environment variable overrides
/// - Validation
/// - Automatic file creation with defaults
/// - Schema migration (adding new settings)
/// - Optional thread safety via a pluggable ThreadPolicy
///
/// Thread Safety:
/// By default, the class uses @c SingleThreadedPolicy (zero overhead).
/// For concurrent access from multiple threads, specify @c MultiThreadedPolicy:
///
/// @code
/// // Single-threaded (default, zero overhead):
/// cppfig::Configuration<MySchema> config("config.json");
///
/// // Thread-safe (reader-writer locking):
/// cppfig::Configuration<MySchema, cppfig::JsonSerializer, cppfig::MultiThreadedPolicy>
///     config("config.json");
/// @endcode
///
/// With @c MultiThreadedPolicy:
/// - Multiple threads may call @c Get concurrently (shared/reader lock).
/// - Calls to @c Set, @c Load mutate internal state under an exclusive/writer lock.
/// - @c Save, @c Diff, @c ValidateAll acquire a shared/reader lock.
/// - Validation in @c Set is performed *before* acquiring the exclusive lock.
///
/// Usage:
/// @code
/// // Define settings
/// struct AppName {
///     static constexpr std::string_view path = "app.name";
///     using value_type = std::string;
///     static auto default_value() -> std::string { return "MyApp"; }
/// };
///
/// struct ServerPort {
///     static constexpr std::string_view path = "server.port";
///     static constexpr std::string_view env_override = "SERVER_PORT";
///     using value_type = int;
///     static auto default_value() -> int { return 8080; }
///     static auto validator() -> Validator<int> { return Range(1, 65535); }
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
/// @tparam SerializerT The serializer to use (defaults to ConfSerializer).
/// @tparam ThreadPolicy The threading policy (defaults to SingleThreadedPolicy).
template <typename Schema, Serializer SerializerT = ConfSerializer, typename ThreadPolicy = SingleThreadedPolicy>
class Configuration : public IConfigurationProvider<Configuration<Schema, SerializerT, ThreadPolicy>, Schema>,
                      public IConfigurationProviderVirtual {
public:
    using serializer_type = SerializerT;
    using data_type = Value;

    /// @brief Creates a configuration manager with a file path.
    ///
    /// @param file_path Path to the configuration file.
    explicit Configuration(std::string file_path)
        : file_path_(std::move(file_path))
        , file_values_(Value::Object())
    {
        BuildDefaults();
    }

    /// @brief Gets the value for a setting type.
    ///
    /// Resolution order:
    /// 1. Environment variable (if configured)
    /// 2. File value (if present)
    /// 3. Default value
    ///
    /// Thread safety: acquires a shared (reader) lock when reading file values.
    template <IsSetting S>
        requires(Schema::template has_setting<S>)
    [[nodiscard]] auto GetImpl() const -> typename S::value_type
    {
        using value_type = typename S::value_type;

        // 1. Check environment variable (no lock needed — no mutable state accessed)
        constexpr auto env_override = GetEnvOverride<S>();
        if constexpr (!env_override.empty()) {
            if (const char* env_value = std::getenv(std::string(env_override).c_str())) {
                auto parsed = ConfigTraits<value_type>::FromString(env_value);
                if (parsed.has_value()) {
                    return *parsed;
                }
                Logger::WarnF("Failed to parse environment variable %.*s='%s', using fallback",
                              static_cast<int>(env_override.size()), env_override.data(), env_value);
            }
        }

        // 2. Check file value (shared lock — concurrent readers allowed)
        {
            typename ThreadPolicy::shared_lock lock(mutex_);
            auto file_result = file_values_.GetAtPath(S::path);
            if (file_result.ok()) {
                auto parsed = ConfigTraits<value_type>::Deserialize(*file_result);
                if (parsed.has_value()) {
                    return *parsed;
                }
                Logger::WarnF("Failed to parse file value for '%.*s', using default",
                              static_cast<int>(S::path.size()), S::path.data());
            }
        }

        // 3. Return default value (immutable after construction — no lock needed)
        return S::default_value();
    }

    /// @brief Sets the value for a setting type.
    ///
    /// Thread safety: validation runs without holding any lock; the actual
    /// mutation of internal state acquires an exclusive (writer) lock.
    template <IsSetting S>
        requires(Schema::template has_setting<S>)
    auto SetImpl(typename S::value_type value) -> Status
    {
        using value_type = typename S::value_type;

        // Validate the value *before* acquiring the exclusive lock
        auto validator = GetSettingValidator<S>();
        auto validation = validator(value);
        if (!validation) {
            return InvalidArgumentError(validation.error_message);
        }

        // Set the value under exclusive lock
        typename ThreadPolicy::unique_lock lock(mutex_);
        auto serialized = ConfigTraits<value_type>::Serialize(value);
        file_values_.SetAtPath(S::path, serialized);

        return OkStatus();
    }

    /// @brief Loads configuration from the file.
    ///
    /// Thread safety: acquires an exclusive (writer) lock for the entire
    /// operation because it mutates @c file_values_.
    [[nodiscard]] auto LoadImpl() -> Status
    {
        typename ThreadPolicy::unique_lock lock(mutex_);
        return LoadUnlocked();
    }

    /// @brief Saves the current configuration to the file.
    ///
    /// Thread safety: acquires a shared (reader) lock because it only reads
    /// @c file_values_ (file I/O is serialized by the OS for the same path).
    [[nodiscard]] auto SaveImpl() const -> Status
    {
        typename ThreadPolicy::shared_lock lock(mutex_);
        return SaveUnlocked();
    }

    /// @brief Returns the diff between file values and defaults.
    ///
    /// Thread safety: acquires a shared (reader) lock.
    [[nodiscard]] auto DiffImpl() const -> ConfigDiff
    {
        typename ThreadPolicy::shared_lock lock(mutex_);
        return DiffFileFromDefaults(defaults_, file_values_);
    }

    /// @brief Validates all current values against their validators.
    ///
    /// Thread safety: acquires a shared (reader) lock.
    [[nodiscard]] auto ValidateAllImpl() const -> Status
    {
        typename ThreadPolicy::shared_lock lock(mutex_);
        return ValidateAllUnlocked();
    }

    /// @brief Returns the file path.
    ///
    /// Thread safety: @c file_path_ is immutable after construction — no lock needed.
    [[nodiscard]] auto GetFilePathImpl() const -> std::string_view { return file_path_; }

    /// @brief Returns the current file values.
    ///
    /// @warning The returned reference is *not* protected after the call returns.
    ///          In multi-threaded code, prefer @c Get<Setting>() for safe access.
    [[nodiscard]] auto GetFileValues() const -> const Value& { return file_values_; }

    /// @brief Returns the default values.
    ///
    /// Thread safety: @c defaults_ is immutable after construction — safe to call
    /// concurrently without synchronization.
    [[nodiscard]] auto GetDefaults() const -> const Value& { return defaults_; }

    [[nodiscard]] auto Load() -> Status override { return LoadImpl(); }

    [[nodiscard]] auto Save() const -> Status override { return SaveImpl(); }

    [[nodiscard]] auto GetFilePath() const -> std::string_view override { return GetFilePathImpl(); }

    [[nodiscard]] auto ValidateAll() const -> Status override { return ValidateAllImpl(); }

    [[nodiscard]] auto GetDiffString() const -> std::string override { return DiffImpl().ToString(); }

private:
    /// @brief Loads configuration from the file (caller must hold exclusive lock).
    [[nodiscard]] auto LoadUnlocked() -> Status
    {
        namespace fs = std::filesystem;

        if (!fs::exists(file_path_)) {
            // File doesn't exist - create with defaults
            Logger::InfoF("Configuration file '%s' not found, creating with defaults", file_path_.c_str());
            file_values_ = defaults_;
            return SaveUnlocked();
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
                // Copy the default value directly from the defaults tree
                auto default_val = defaults_.GetAtPath(entry.path);
                if (default_val.ok()) {
                    file_values_.SetAtPath(entry.path, *default_val);
                }
            }

            // Save the updated configuration
            auto save_status = SaveUnlocked();
            if (!save_status.ok()) {
                Logger::ErrorF("Failed to save migrated configuration: %s",
                               std::string(save_status.message()).c_str());
                return save_status;
            }
        }

        return OkStatus();
    }

    /// @brief Saves the current configuration to the file (caller must hold at least a shared lock).
    [[nodiscard]] auto SaveUnlocked() const -> Status
    {
        namespace fs = std::filesystem;

        // Create parent directories if needed
        fs::path path(file_path_);
        if (path.has_parent_path()) {
            std::error_code error_code;
            fs::create_directories(path.parent_path(), error_code);
            if (error_code) {
                return InternalError("Failed to create directory: " + error_code.message());
            }
        }

        return WriteFile<SerializerT>(file_path_, file_values_);
    }

    /// @brief Validates all values (caller must hold at least a shared lock).
    [[nodiscard]] auto ValidateAllUnlocked() const -> Status
    {
        Status status = OkStatus();

        Schema::ForEachSetting([this, &status]<typename S>() {
            if (!status.ok()) {
                return;  // Stop on first error
            }

            using value_type = typename S::value_type;
            auto file_result = file_values_.GetAtPath(S::path);

            if (file_result.ok()) {
                auto parsed = ConfigTraits<value_type>::Deserialize(*file_result);
                if (parsed.has_value()) {
                    auto validator = GetSettingValidator<S>();
                    auto validation = validator(*parsed);
                    if (!validation) {
                        status = InvalidArgumentError(std::string(S::path) + ": " + validation.error_message);
                    }
                }
            }
        });

        return status;
    }

    void BuildDefaults()
    {
        defaults_ = Value::Object();

        Schema::ForEachSetting([this]<typename S>() {
            using value_type = typename S::value_type;
            auto serialized = ConfigTraits<value_type>::Serialize(S::default_value());
            defaults_.SetAtPath(S::path, serialized);
        });
    }

    std::string file_path_;
    Value file_values_;
    Value defaults_;
    mutable typename ThreadPolicy::mutex_type mutex_;
};

}  // namespace cppfig
