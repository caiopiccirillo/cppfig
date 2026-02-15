#pragma once

#include <string>
#include <string_view>

#include "cppfig/diff.h"
#include "cppfig/setting.h"

namespace cppfig {

/// @brief CRTP base class for configuration providers.
///
/// This class defines the interface that all configuration providers must
/// implement. It uses CRTP to allow compile-time polymorphism while still
/// being compatible with GMock for testing.
///
/// @tparam Derived The derived configuration provider class.
/// @tparam Schema The ConfigSchema type.
template <typename Derived, typename Schema>
class IConfigurationProvider {
public:
    using schema_type = Schema;

    /// @brief Gets the value for a setting type.
    ///
    /// Resolution order:
    /// 1. Environment variable (if configured)
    /// 2. File value (if present)
    /// 3. Default value
    ///
    /// Usage: config.Get<MySettings::ServerPort>()
    template <IsSetting S>
        requires(Schema::template has_setting<S>)
    [[nodiscard]] auto Get() const -> typename S::value_type
    {
        return static_cast<const Derived*>(this)->template GetImpl<S>();
    }

    /// @brief Sets the value for a setting type.
    ///
    /// The value is validated before being set. Returns an error if validation fails.
    ///
    /// Usage: config.Set<MySettings::ServerPort>(8080)
    template <IsSetting S>
        requires(Schema::template has_setting<S>)
    auto Set(typename S::value_type value) -> Status
    {
        return static_cast<Derived*>(this)->template SetImpl<S>(std::move(value));
    }

    /// @brief Loads configuration from the file.
    ///
    /// If the file doesn't exist, it creates it with default values.
    /// If new settings were added to the schema, they are appended to the file.
    [[nodiscard]] auto Load() -> Status { return static_cast<Derived*>(this)->LoadImpl(); }

    /// @brief Saves the current configuration to the file.
    [[nodiscard]] auto Save() const -> Status { return static_cast<const Derived*>(this)->SaveImpl(); }

    /// @brief Returns the diff between file values and defaults.
    [[nodiscard]] auto Diff() const -> ConfigDiff { return static_cast<const Derived*>(this)->DiffImpl(); }

    /// @brief Validates all current values against their validators.
    [[nodiscard]] auto ValidateAll() const -> Status
    {
        return static_cast<const Derived*>(this)->ValidateAllImpl();
    }

    /// @brief Returns the file path.
    [[nodiscard]] auto GetFilePath() const -> std::string_view
    {
        return static_cast<const Derived*>(this)->GetFilePathImpl();
    }

protected:
    IConfigurationProvider() = default;
    ~IConfigurationProvider() = default;
    IConfigurationProvider(const IConfigurationProvider&) = default;
    IConfigurationProvider(IConfigurationProvider&&) = default;
    auto operator=(const IConfigurationProvider&) -> IConfigurationProvider& = default;
    auto operator=(IConfigurationProvider&&) -> IConfigurationProvider& = default;
};

/// @brief Virtual interface for type-erased configuration access.
///
/// This interface can be used when compile-time type information is not needed,
/// such as in plugin systems or when configuration needs to be passed through
/// non-template code.
class IConfigurationProviderVirtual {
public:
    virtual ~IConfigurationProviderVirtual() = default;

    /// @brief Loads configuration from the file.
    [[nodiscard]] virtual auto Load() -> Status = 0;

    /// @brief Saves the current configuration to the file.
    [[nodiscard]] virtual auto Save() const -> Status = 0;

    /// @brief Returns the file path.
    [[nodiscard]] virtual auto GetFilePath() const -> std::string_view = 0;

    /// @brief Validates all current values.
    [[nodiscard]] virtual auto ValidateAll() const -> Status = 0;

    /// @brief Gets a string representation of the diff.
    [[nodiscard]] virtual auto GetDiffString() const -> std::string = 0;

protected:
    IConfigurationProviderVirtual() = default;
    IConfigurationProviderVirtual(const IConfigurationProviderVirtual&) = default;
    IConfigurationProviderVirtual(IConfigurationProviderVirtual&&) = default;
    auto operator=(const IConfigurationProviderVirtual&) -> IConfigurationProviderVirtual& = default;
    auto operator=(IConfigurationProviderVirtual&&) -> IConfigurationProviderVirtual& = default;
};

}  // namespace cppfig
