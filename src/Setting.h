#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>

namespace config {

using SettingValueType = std::variant<int, float, double, std::string, bool>;

// Forward declaration of Setting
template <typename E, typename T>
class Setting;

/**
 * @brief Utility to extract Setting type from variant
 */
template <typename E, typename SettingVariant, E EnumValue>
struct default_setting_type;

// Partial specialization for std::variant
template <typename E, E EnumValue, typename... Ts>
struct default_setting_type<E, std::variant<Ts...>, EnumValue> {
    // Default type is void if not found (will cause a compile error)
    using type = void;
};

// Helper type alias to extract the type
template <typename E, typename SettingVariant, E EnumValue>
using default_setting_type_t = typename default_setting_type<E, SettingVariant, EnumValue>::type;

/**
 * @brief Compile-time check if a type is valid for an enum value based on defaults
 */
template <typename E, typename SettingVariant, E EnumValue, typename T>
inline constexpr bool is_valid_type_v = std::is_same_v<default_setting_type_t<E, SettingVariant, EnumValue>, T>; // NOLINT(readability-identifier-naming)

/**
 * @brief Type trait to associate enum values with their types at compile time
 * This must be specialized by users for their configuration enum types
 *
 * @tparam E Enum type that defines the configuration keys
 * @tparam EnumValue The specific enum value
 * @tparam Enable SFINAE enabler
 */
template <typename E, E EnumValue, typename Enable = void>
struct setting_type_trait { // NOLINT(readability-identifier-naming)
    // Default implementation provides a compile error
    static_assert(sizeof(E) == 0,
                  "You must specialize setting_type_trait for your enum values");

    using type = void; // This won't be used due to static_assert
};

/**
 * @brief Helper type to extract the associated type for an enum value
 *
 * @tparam E Enum type that defines the configuration keys
 * @tparam EnumValue The specific enum value
 */
template <typename E, E EnumValue>
using setting_type = typename setting_type_trait<E, EnumValue>::type;

/**
 * @brief Compile-time check if a given type matches the expected type for an enum value
 *
 * @tparam E Enum type that defines the configuration keys
 * @tparam EnumValue The specific enum value
 * @tparam T The type to check against
 */
template <typename E, E EnumValue, typename T>
inline constexpr bool is_correct_type_v = std::is_same_v<setting_type<E, EnumValue>, T>; // NOLINT(readability-identifier-naming)

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

    explicit GenericSetting(SettingVariant setting, E name = E {}, std::function<std::string(E)> type_checker = nullptr)
        : setting_(std::move(setting))
        , name_(name)
        , type_checker_(type_checker)
    {
    }

    /**
     * @brief Get the value of the setting as the specified type with type checking
     *
     * @tparam T Type to retrieve the value as
     * @return const T& The value
     * @throws std::runtime_error if T doesn't match the expected type
     * @throws std::bad_variant_access if T doesn't match the stored type
     */
    template <typename T>
    [[nodiscard]] const T& Value() const
    {
        if (type_checker_) {
            std::string expected_type = type_checker_(name_);

            std::string requested_type;
            if constexpr (std::is_same_v<T, int>) {
                requested_type = "int";
            }
            else if constexpr (std::is_same_v<T, float>) {
                requested_type = "float";
            }
            else if constexpr (std::is_same_v<T, double>) {
                requested_type = "double";
            }
            else if constexpr (std::is_same_v<T, std::string>) {
                requested_type = "string";
            }
            else if constexpr (std::is_same_v<T, bool>) {
                requested_type = "bool";
            }
            else {
                requested_type = "unknown";
            }

            if (expected_type != requested_type) {
                throw std::runtime_error("Type mismatch: Requested type '" + requested_type + "' doesn't match expected type '" + expected_type + "' for this setting");
            }
        }

        return std::get<Setting<E, T>>(setting_).Value();
    }

    /**
     * @brief Get the actual type of this setting
     *
     * @return std::type_info The runtime type information
     */
    [[nodiscard]] const std::type_info& ValueType() const
    {
        return std::visit([](const auto& s) -> const std::type_info& {
            return typeid(s.Value());
        },
                          setting_);
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
    E name_;
    std::function<std::string(E)> type_checker_;
};

} // namespace config
