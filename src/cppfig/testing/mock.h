#pragma once

#include <absl/status/status.h>
#include <gmock/gmock.h>

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>

#include "cppfig/interface.h"
#include "cppfig/setting.h"
#include "cppfig/traits.h"

namespace cppfig::testing {

/// @brief Mock implementation of configuration for testing.
///
/// This class provides a simple in-memory configuration that can be used
/// in unit tests. It stores values in a map and doesn't require file I/O.
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
/// struct AppPort {
///     static constexpr std::string_view kPath = "app.port";
///     using ValueType = int;
///     static auto DefaultValue() -> int { return 8080; }
/// };
///
/// using Schema = ConfigSchema<AppName, AppPort>;
/// MockConfiguration<Schema> config;
///
/// // Set values for testing
/// config.SetValue<AppPort>(9000);
///
/// // Use in tests
/// EXPECT_EQ(config.Get<AppPort>(), 9000);
/// @endcode
template <typename Schema>
class MockConfiguration {
public:
    using SchemaType = Schema;

    MockConfiguration() { BuildDefaults(); }

    /// @brief Gets the value for a setting type.
    template <IsSetting S>
        requires(Schema::template HasSetting<S>)
    [[nodiscard]] auto Get() const -> typename S::ValueType
    {
        using ValueType = typename S::ValueType;
        std::string key(S::kPath);

        auto it = values_.find(key);
        if (it != values_.end()) {
            auto parsed = ConfigTraits<ValueType>::FromJson(it->second);
            if (parsed.has_value()) {
                return *parsed;
            }
        }

        // Return default
        return S::DefaultValue();
    }

    /// @brief Sets the value for a setting type (bypasses validation for testing).
    template <IsSetting S>
        requires(Schema::template HasSetting<S>)
    void SetValue(typename S::ValueType value)
    {
        using ValueType = typename S::ValueType;
        std::string key(S::kPath);
        values_[key] = ConfigTraits<ValueType>::ToJson(value);
    }

    /// @brief Sets the value with validation.
    template <IsSetting S>
        requires(Schema::template HasSetting<S>)
    auto Set(typename S::ValueType value) -> absl::Status
    {
        auto validator = GetSettingValidator<S>();
        auto validation = validator(value);
        if (!validation) {
            return absl::InvalidArgumentError(validation.error_message);
        }

        SetValue<S>(std::move(value));
        return absl::OkStatus();
    }

    /// @brief Simulates loading (no-op for mock).
    [[nodiscard]] auto Load() -> absl::Status { return absl::OkStatus(); }

    /// @brief Simulates saving (no-op for mock).
    [[nodiscard]] auto Save() const -> absl::Status { return absl::OkStatus(); }

    /// @brief Resets all values to defaults.
    void Reset()
    {
        values_.clear();
        BuildDefaults();
    }

    /// @brief Sets a raw JSON value for testing parse failure scenarios.
    ///
    /// This method allows tests to inject invalid JSON values that will
    /// cause FromJson to fail, triggering the default value fallback path.
    /// @param path The setting path (e.g., "app.name").
    /// @param value The raw JSON value to set.
    void SetRawJson(std::string_view path, nlohmann::json value) { values_[std::string(path)] = std::move(value); }

    /// @brief Clears a value by path for testing scenarios where key is not found.
    ///
    /// This method removes a value from the internal map, allowing tests
    /// to verify the behavior when a setting key is not present.
    /// @param path The setting path to clear.
    void ClearValue(std::string_view path) { values_.erase(std::string(path)); }

private:
    void BuildDefaults()
    {
        Schema::ForEachSetting([this]<typename S>() {
            using ValueType = typename S::ValueType;
            std::string key(S::kPath);
            values_[key] = ConfigTraits<ValueType>::ToJson(S::DefaultValue());
        });
    }

    std::unordered_map<std::string, nlohmann::json> values_;
};

/// @brief GMock-compatible mock for IConfigurationProviderVirtual.
///
/// This class uses GMock's MOCK_METHOD to create a fully mockable
/// configuration provider for unit testing.
///
/// Usage:
/// @code
/// MockVirtualConfigurationProvider mock;
/// EXPECT_CALL(mock, Load()).WillOnce(Return(absl::OkStatus()));
/// EXPECT_CALL(mock, GetFilePath()).WillOnce(Return("config.json"));
/// @endcode
// LCOV_EXCL_START — GMock macro internals generate uninstrumented stubs
class MockVirtualConfigurationProvider : public IConfigurationProviderVirtual {
public:
    MOCK_METHOD(absl::Status, Load, (), (override));
    MOCK_METHOD(absl::Status, Save, (), (const, override));
    MOCK_METHOD(std::string_view, GetFilePath, (), (const, override));
    MOCK_METHOD(absl::Status, ValidateAll, (), (const, override));
    MOCK_METHOD(std::string, GetDiffString, (), (const, override));
};

/// @brief Test fixture helper for configuration tests.
///
/// Provides utilities for setting up test configurations with
/// temporary files and cleanup.
class ConfigurationTestFixture {
public:
    /// @brief Creates a temporary file path for testing.
    [[nodiscard]] static auto CreateTempFilePath(std::string_view prefix = "test_config") -> std::string
    {
        return std::string("/tmp/") + std::string(prefix) + "_" + std::to_string(std::rand()) + ".json";
    }

    /// @brief Removes a file if it exists.
    static void RemoveFile(const std::string& path) { std::remove(path.c_str()); }
};
// LCOV_EXCL_STOP

}  // namespace cppfig::testing
