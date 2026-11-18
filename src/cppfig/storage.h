#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

namespace cppfig::detail {

template <std::size_t I, typename T>
struct IndexedType {
    using type = T;
};

template <typename Sequence, typename... Types>
struct TypeIndexer;

template <std::size_t... I, typename... Types>
struct TypeIndexer<std::index_sequence<I...>, Types...> : IndexedType<I, Types>... { };

template <std::size_t I, typename T>
auto SelectIndexedType(const IndexedType<I, T>&) -> IndexedType<I, T>;

template <std::size_t I, typename... Types>
using TypeAt = typename decltype(SelectIndexedType<I>(
    std::declval<const TypeIndexer<std::make_index_sequence<sizeof...(Types)>, Types...>&>()))::type;

template <std::size_t I, typename T>
struct StorageSlot {
    using value_type = T;

    T value;
};

template <std::size_t I, typename T>
auto SelectStorageSlot(const StorageSlot<I, T>&) -> StorageSlot<I, T>;

/// Non-recursive indexed storage. Unlike libstdc++'s recursive tuple
/// implementation, this supports large registries within default template-depth limits.
template <template <typename> typename Holder, typename Sequence, typename... Settings>
struct HeldStorage;

template <template <typename> typename Holder, std::size_t... I, typename... Settings>
struct HeldStorage<Holder, std::index_sequence<I...>, Settings...> : StorageSlot<I, Holder<typename Settings::value_type>>... {
    HeldStorage() = default;

    template <typename... Values>
        requires(sizeof...(Values) == sizeof...(Settings))
    explicit HeldStorage(Values&&... values)
        : StorageSlot<I, Holder<typename Settings::value_type>> { std::forward<Values>(values) }...
    {
    }

    template <std::size_t Index>
    using Slot = decltype(SelectStorageSlot<Index>(std::declval<HeldStorage&>()));

    template <std::size_t Index>
    [[nodiscard]] auto Get() -> typename Slot<Index>::value_type&
    {
        return static_cast<Slot<Index>&>(*this).value;
    }

    template <std::size_t Index>
    [[nodiscard]] auto Get() const -> const typename Slot<Index>::value_type&
    {
        return static_cast<const Slot<Index>&>(*this).value;
    }
};

template <template <typename> typename Holder, typename... Settings>
using MakeHeldStorage = HeldStorage<Holder, std::make_index_sequence<sizeof...(Settings)>, Settings...>;

}  // namespace cppfig::detail
