#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "ConfigurationName.h"

namespace config {

// Variant types for the setting value
using SettingValueType = std::variant<int, float, double, std::string, bool>;

/**
 * @brief Class representing the configuration data.
 */
class Setting {
public:
    /**
     * @brief Setting data.
     *
     * @param name Configuration name
     * @param value Setting value
     * @param max_value Maximum value
     * @param min_value Minimum value
     * @param unit Unit of the value
     * @param description Description of the setting
     */
    Setting(ConfigurationName name,
            SettingValueType value,
            std::optional<SettingValueType> max_value, // NOLINT(bugprone-easily-swappable-parameters)
            std::optional<SettingValueType> min_value,
            std::optional<std::string> unit,
            std::optional<std::string> description)
        : name_(name)
        , value_(std::move(value))
        , max_value_(std::move(max_value))
        , min_value_(std::move(min_value))
        , unit_(std::move(unit))
        , description_(std::move(description)) { };

    /**
     * @brief Get the name of the configuration.
     *
     * @return ConfigurationName
     */
    [[nodiscard]] ConfigurationName Name() const { return this->name_; };

    Setting() = default;

    /**
     * @brief Check if the variant holds a value of type T.
     *
     * @return The value of type T if it holds. If not, throw an exception
     */
    [[nodiscard]] SettingValueType Value() const { return this->value_; }
    [[nodiscard]] std::optional<SettingValueType> MaxValue() const { return this->max_value_; };
    [[nodiscard]] std::optional<SettingValueType> MinValue() const { return this->min_value_; };
    [[nodiscard]] std::optional<std::string> Unit() const { return this->unit_; };
    [[nodiscard]] std::optional<std::string> Description() const { return this->description_; };

    template <typename T>
    [[nodiscard]] T ValueByType() const
    {
        return std::visit([](auto&& arg) -> T {
            using ArgType = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<ArgType, T>) {
                return arg;
            }
            else if constexpr (std::is_arithmetic_v<ArgType> && std::is_arithmetic_v<T>) {
                return static_cast<T>(arg);
            }
            else {
                throw std::bad_variant_access();
            }
        },
                          this->value_);
    }

private:
    ConfigurationName name_;
    SettingValueType value_;
    std::optional<SettingValueType> max_value_;
    std::optional<SettingValueType> min_value_;
    std::optional<std::string> unit_;
    std::optional<std::string> description_;
};

} // namespace config
