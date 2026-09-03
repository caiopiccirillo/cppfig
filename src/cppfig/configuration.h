#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

#include "cppfig/conf.h"
#include "cppfig/diff.h"
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
/// - @c Save also takes the exclusive lock: it rewrites the file, so
///   concurrent savers would race over the same path.
/// - @c Diff and @c ValidateAll acquire a shared/reader lock.
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
                if (!parsed.has_value()) {
                    Logger::WarnF("Failed to parse environment variable %.*s='%s', using fallback",
                                  static_cast<int>(env_override.size()), env_override.data(), env_value);
                }
                else {
                    // An override that fails the setting's own validator must
                    // not slip through unchecked; fall back and say so.
                    auto validation = GetSettingValidator<S>()(*parsed);
                    if (validation) {
                        return *parsed;
                    }
                    Logger::WarnF("Environment variable %.*s='%s' is invalid (%s), using fallback",
                                  static_cast<int>(env_override.size()), env_override.data(), env_value,
                                  validation.error_message.c_str());
                }
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
    /// Thread safety: acquires an exclusive (writer) lock. Although only
    /// @c file_values_ is read, the call also rewrites the file, and two
    /// threads holding a shared lock would race over the same path.
    /// Serializing saves costs nothing next to the file I/O they perform.
    [[nodiscard]] auto SaveImpl() const -> Status
    {
        typename ThreadPolicy::unique_lock lock(mutex_);
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
            auto save_status = SaveUnlocked();
            if (!save_status.ok()) {
                return save_status;
            }
            return ValidateFileValuesUnlocked();
        }

        // Load existing file
        auto result = ReadFile<SerializerT>(file_path_);
        if (!result.ok()) {
            return result.status();
        }

        file_values_ = *result;
        CoerceToSchemaTypes();

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

        // A successful Load must leave the configuration holding only values
        // the schema accepts; otherwise every caller of Get inherits an
        // invalid value that nothing ever objected to.
        return ValidateFileValuesUnlocked();
    }

    /// @brief Saves the current configuration to the file (caller must hold the exclusive lock).
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

    /// @brief Validates the loaded file values (caller must hold at least a shared lock).
    ///
    /// Covers only what @c Load read, so a successful @c Load does not depend
    /// on the ambient environment.
    [[nodiscard]] auto ValidateFileValuesUnlocked() const -> Status
    {
        Status status = OkStatus();

        Schema::ForEachSetting([this, &status]<typename S>() {
            if (!status.ok()) {
                return;  // Stop on first error
            }

            using value_type = typename S::value_type;
            auto file_result = file_values_.GetAtPath(S::path);
            if (!file_result.ok()) {
                return;
            }

            auto parsed = ConfigTraits<value_type>::Deserialize(*file_result);
            if (!parsed.has_value()) {
                return;
            }

            auto validation = GetSettingValidator<S>()(*parsed);
            if (!validation) {
                status = InvalidArgumentError(std::string(S::path) + ": " + validation.error_message);
            }
        });

        return status;
    }

    /// @brief Validates all values (caller must hold at least a shared lock).
    ///
    /// Checks every source that can supply a value: the environment override,
    /// if one is set, and the file value. @c Get falls back past an invalid
    /// override so the process keeps running, but the operator still has to
    /// be able to find out that the override is wrong.
    [[nodiscard]] auto ValidateAllUnlocked() const -> Status
    {
        Status status = ValidateEnvOverridesUnlocked();
        if (!status.ok()) {
            return status;
        }
        return ValidateFileValuesUnlocked();
    }

    /// @brief Validates the environment overrides that are currently set.
    [[nodiscard]] auto ValidateEnvOverridesUnlocked() const -> Status
    {
        Status status = OkStatus();

        Schema::ForEachSetting([&status]<typename S>() {
            if (!status.ok()) {
                return;  // Stop on first error
            }

            constexpr auto env_override = GetEnvOverride<S>();
            if constexpr (!env_override.empty()) {
                const char* env_value = std::getenv(std::string(env_override).c_str());
                if (env_value == nullptr) {
                    return;
                }

                using value_type = typename S::value_type;
                auto parsed = ConfigTraits<value_type>::FromString(env_value);
                if (!parsed.has_value()) {
                    status = InvalidArgumentError(std::string(env_override) + ": cannot parse '" + env_value + "' as a value for '" + std::string(S::path) + "'");
                    return;
                }

                auto validation = GetSettingValidator<S>()(*parsed);
                if (!validation) {
                    status = InvalidArgumentError(std::string(env_override) + ": " + validation.error_message);
                }
            }
        });

        return status;
    }

    /// @brief Reinterprets parsed leaves as the types the schema declares.
    ///
    /// A flat format has to infer a leaf's type from its text, and the
    /// inference cannot know that `2.5` was written for a std::string setting
    /// or that `"8080"` was written for an int one. Where the parsed value
    /// does not deserialize as its declared type, re-read it from its textual
    /// form, so a hand-edited file keeps the value the user wrote instead of
    /// silently reverting to the default.
    ///
    /// A value that cannot be reinterpreted either — an out-of-range int, say
    /// — is left as parsed, so it still fails validation rather than being
    /// coerced into something plausible.
    void CoerceToSchemaTypes()
    {
        Schema::ForEachSetting([this]<typename S>() {
            using value_type = typename S::value_type;

            auto file_result = file_values_.GetAtPath(S::path);
            if (!file_result.ok()) {
                return;
            }
            if (ConfigTraits<value_type>::Deserialize(*file_result).has_value()) {
                return;
            }

            auto text = file_result->TryToText();
            if (!text.has_value()) {
                return;
            }

            auto parsed = ConfigTraits<value_type>::FromString(*text);
            if (parsed.has_value()) {
                file_values_.SetAtPath(S::path, ConfigTraits<value_type>::Serialize(*parsed));
            }
        });
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
