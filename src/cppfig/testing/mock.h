#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "cppfig/configuration.h"

namespace cppfig::testing {

template <typename RegistryType>
class MockConfigurationBuilder;

/// @brief Frozen in-memory configuration for tests.
template <typename RegistryType>
class MockConfiguration {
public:
    using values_type = typename RegistryType::values_type;

    MockConfiguration(const MockConfiguration&) = delete;
    auto operator=(const MockConfiguration&) -> MockConfiguration& = delete;
    MockConfiguration(MockConfiguration&&) = default;
    auto operator=(MockConfiguration&&) -> MockConfiguration& = default;

    [[nodiscard]] static auto FromDefaults() -> StatusOr<MockConfiguration>
    {
        MockConfigurationBuilder<RegistryType> builder;
        return builder.Build();
    }

    template <typename S>
        requires(RegistryType::template has_setting<S>)
    [[nodiscard]] auto Get() const noexcept -> const typename S::value_type&
    {
        return values_.template Get<RegistryType::template index<S>>();
    }

private:
    friend class MockConfigurationBuilder<RegistryType>;

    explicit MockConfiguration(values_type values)
        : values_(std::move(values))
    {
    }

    values_type values_;
};

/// @brief Mutable setup stage for MockConfiguration; values freeze at Build.
template <typename RegistryType>
class MockConfigurationBuilder {
public:
    template <typename S>
        requires(RegistryType::template has_setting<S>)
    auto Set(typename S::value_type value) -> MockConfigurationBuilder&
    {
        values_.template Get<RegistryType::template index<S>>().emplace(std::move(value));
        return *this;
    }

    [[nodiscard]] auto Build() -> StatusOr<MockConfiguration<RegistryType>>
    {
        return Build(std::make_index_sequence<RegistryType::size>());
    }

private:
    using candidate_storage = typename RegistryType::template held_values_type<std::optional>;
    using values_type = typename RegistryType::values_type;

    template <typename T>
    using result_holder = std::optional<StatusOr<T>>;
    using result_storage = typename RegistryType::template held_values_type<result_holder>;

    template <std::size_t I>
    auto BuildOne() -> StatusOr<typename RegistryType::template SettingAt<I>::value_type>
    {
        using S = typename RegistryType::template SettingAt<I>;
        auto& candidate = values_.template Get<I>();
        if (candidate) {
            auto validation = ValidateSetting<S>(*candidate);
            if (!validation) {
                return InvalidArgumentError(std::string(S::path) + " test value is invalid: " + validation.error_message);
            }
            return std::move(*candidate);
        }

        auto value = S::default_value();
        auto validation = ValidateSetting<S>(value);
        if (!validation) {
            return InvalidArgumentError(std::string(S::path) + " default is invalid: " + validation.error_message);
        }
        return value;
    }

    template <std::size_t I, typename T>
    auto StoreResult(result_storage& results, StatusOr<T> result, Status& error) -> bool
    {
        if (!result.ok()) {
            error = std::move(result).status();
            return false;
        }
        results.template Get<I>().emplace(std::move(result));
        return true;
    }

    template <std::size_t... I>
    auto Build(std::index_sequence<I...>) -> StatusOr<MockConfiguration<RegistryType>>
    {
        result_storage results;
        Status error = OkStatus();
        const bool valid = (StoreResult<I>(results, BuildOne<I>(), error) && ...);
        if (!valid) {
            return error;
        }

        return MockConfiguration<RegistryType>(values_type { std::move(results.template Get<I>().value()).value()... });
    }

    candidate_storage values_;
};

}  // namespace cppfig::testing
