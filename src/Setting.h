#pragma once

#include <optional>
#include <string>
#include <variant>

namespace config {

// Legacy variant type for backward compatibility
using SettingValueType = std::variant<int, float, double, std::string, bool>;

/**
 * @brief Generic Setting class that is strongly typed
 *
 * @tparam E Enum type that defines the configuration keys
 * @tparam T Type of the setting value
 */
template <typename E, typename T>
class Setting {
public:
    Setting(E name,
            T value,
            std::optional<T> max_value = std::nullopt,
            std::optional<T> min_value = std::nullopt,
            std::optional<std::string> unit = std::nullopt,
            std::optional<std::string> description = std::nullopt)
        : name_(name)
        , value_(std::move(value))
        , max_value_(std::move(max_value))
        , min_value_(std::move(min_value))
        , unit_(std::move(unit))
        , description_(std::move(description))
    {
    }

    Setting() = default;

    [[nodiscard]] E Name() const { return name_; }
    [[nodiscard]] const T& Value() const { return value_; }
    [[nodiscard]] std::optional<T> MaxValue() const { return max_value_; }
    [[nodiscard]] std::optional<T> MinValue() const { return min_value_; }
    [[nodiscard]] std::optional<std::string> Unit() const { return unit_; }
    [[nodiscard]] std::optional<std::string> Description() const { return description_; }

private:
    E name_;
    T value_;
    std::optional<T> max_value_;
    std::optional<T> min_value_;
    std::optional<std::string> unit_;
    std::optional<std::string> description_;
};

/**
 * @brief A generic Setting wrapper that contains a variant of typed settings
 * and provides a more natural Value<T>() access method
 *
 * @tparam E Enum type that defines the configuration keys
 */
template <typename E>
class GenericSetting {
public:
    using SettingVariant = std::variant<
        Setting<E, int>,
        Setting<E, float>,
        Setting<E, double>,
        Setting<E, std::string>,
        Setting<E, bool>>;

    explicit GenericSetting(SettingVariant setting)
        : setting_(std::move(setting))
    {
    }

    /**
     * @brief Get the value of the setting as the specified type
     *
     * @tparam T Type to retrieve the value as
     * @return const T& The value
     * @throws std::bad_variant_access if T doesn't match the stored type
     */
    template <typename T>
    [[nodiscard]] const T& Value() const
    {
        return std::get<Setting<E, T>>(setting_).Value();
    }

    /**
     * @brief Get the name of the setting
     */
    [[nodiscard]] E Name() const
    {
        return std::visit([](const auto& s) { return s.Name(); }, setting_);
    }

    /**
     * @brief Get maximum value if defined
     *
     * @tparam T Type of the maximum value
     * @return std::optional<T> The maximum value if defined
     * @throws std::bad_variant_access if T doesn't match the stored type
     */
    template <typename T>
    [[nodiscard]] std::optional<T> MaxValue() const
    {
        return std::get<Setting<E, T>>(setting_).MaxValue();
    }

    /**
     * @brief Get minimum value if defined
     *
     * @tparam T Type of the minimum value
     * @return std::optional<T> The minimum value if defined
     * @throws std::bad_variant_access if T doesn't match the stored type
     */
    template <typename T>
    [[nodiscard]] std::optional<T> MinValue() const
    {
        return std::get<Setting<E, T>>(setting_).MinValue();
    }

    /**
     * @brief Get the unit of the setting
     */
    [[nodiscard]] std::optional<std::string> Unit() const
    {
        return std::visit([](const auto& s) { return s.Unit(); }, setting_);
    }

    /**
     * @brief Get the description of the setting
     */
    [[nodiscard]] std::optional<std::string> Description() const
    {
        return std::visit([](const auto& s) { return s.Description(); }, setting_);
    }

    /**
     * @brief Get the underlying setting variant
     */
     [[nodiscard]] const SettingVariant& GetVariant() const
    {
        return setting_;
    }

private:
    SettingVariant setting_;
};

} // namespace config
