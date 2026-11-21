#pragma once

#include <array>
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
    [[nodiscard]] auto Get() const& noexcept -> const typename S::value_type&
    {
        using Slot = detail::StorageSlot<RegistryType::template index<S>, typename S::value_type>;
        return static_cast<const Slot&>(values_).value;
    }

    /// Reading from a temporary would return a reference that dangles at the
    /// end of the full expression; keep the configuration in a variable instead.
    template <typename S>
        requires(RegistryType::template has_setting<S>)
    auto Get() const&& -> const typename S::value_type& = delete;

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
        using Slot = detail::StorageSlot<RegistryType::template index<S>, std::optional<typename S::value_type>>;
        static_cast<Slot&>(values_).value.emplace(std::move(value));
        return *this;
    }

    /// Validates the supplied values, fills the rest from defaults and freezes
    /// them. The builder's values are moved into the result.
    [[nodiscard]] auto Build() -> StatusOr<MockConfiguration<RegistryType>>
    {
        const auto slots = slot_table::Addresses(values_);
        for (std::size_t i = 0; i < RegistryType::size; ++i) {
            auto status = slot_table::check[i](slots[i]);
            if (!status.ok()) {
                return status;
            }
        }
        return MockConfiguration<RegistryType>(values_type(detail::from_engaged, values_));
    }

private:
    using candidate_storage = typename RegistryType::template held_values_type<std::optional>;
    using values_type = typename RegistryType::values_type;
    using slot_table = detail::SlotTable<RegistryType>;

    candidate_storage values_;
};

}  // namespace cppfig::testing
