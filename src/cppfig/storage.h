#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
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
using TypeAt = decltype(SelectIndexedType<I>(
    std::declval<const TypeIndexer<std::make_index_sequence<sizeof...(Types)>, Types...>&>()))::type;

/// One stored value, keyed by its setting's declaration index I.
template <std::size_t I, typename T>
struct StorageSlot {
    using value_type = T;

    T value;
};

template <std::size_t I, typename T>
auto SelectStorageSlot(const StorageSlot<I, T>&) -> StorageSlot<I, T>;

/// Tag selecting the HeldStorage constructor that moves values out of a
/// storage whose slots are all engaged optionals.
struct FromEngagedTag {
    explicit FromEngagedTag() = default;
};
inline constexpr FromEngagedTag from_engaged {};

/// Declaration indices sorted by descending value alignment (ties keep
/// declaration order). Laying slots out in this order leaves no padding between
/// them, because every type's size is a multiple of its alignment. The slots
/// are base classes, so this relies on the ABI laying bases out in order, as
/// the Itanium C++ ABI used by GCC and Clang does; C++ does not require it.
template <typename... Settings>
struct StorageOrder {
    static constexpr std::size_t size = sizeof...(Settings);

    static constexpr std::array<std::size_t, size> order = [] {
        constexpr std::array<std::size_t, size> alignments { alignof(typename Settings::value_type)... };
        std::array<std::size_t, size> indices {};
        for (std::size_t i = 0; i < size; ++i) {
            indices[i] = i;
        }
        std::sort(indices.begin(), indices.end(), [&](std::size_t left, std::size_t right) {
            return alignments[left] != alignments[right] ? alignments[left] > alignments[right] : left < right;
        });
        return indices;
    }();

    template <std::size_t... K>
    static auto Make(std::index_sequence<K...>) -> std::index_sequence<order[K]...>;

    using type = decltype(Make(std::make_index_sequence<size>()));
};

/// Non-recursive indexed storage, so large registries stay within default
/// template-depth limits (a recursive std::tuple, as libstdc++ implements it,
/// does not). Slots are laid out in storage order but addressed by declaration
/// index.
///
/// Access a slot by casting to StorageSlot<I, Holder<T>> rather than through a
/// per-index member function: any function instantiated per index on this type
/// carries the entire registry in its mangled name, and for large registries
/// those names dominate compile time and object size.
template <template <typename> typename Holder, typename Order, typename... Settings>
struct HeldStorage;

template <template <typename> typename Holder, std::size_t... P, typename... Settings>
struct HeldStorage<Holder, std::index_sequence<P...>, Settings...>  // NOLINT(misc-multiple-inheritance): one base per slot
    : StorageSlot<P, Holder<typename TypeAt<P, Settings...>::value_type>>... {
    using OptionalStorage = HeldStorage<std::optional, std::index_sequence<P...>, Settings...>;

    HeldStorage() = default;

    /// Moves each value out of the engaged optional at the same declaration
    /// index in source. Every slot of source must be engaged.
    HeldStorage(FromEngagedTag /*tag*/, OptionalStorage& source)
        : StorageSlot<P, Holder<typename TypeAt<P, Settings...>::value_type>> {
            // NOLINTNEXTLINE(bugprone-unchecked-optional-access): the caller guarantees every slot is engaged
            std::move(*static_cast<StorageSlot<P, std::optional<typename TypeAt<P, Settings...>::value_type>>&>(source).value)
        }...
    {
    }
};

template <template <typename> typename Holder, typename... Settings>
using MakeHeldStorage = HeldStorage<Holder, typename StorageOrder<Settings...>::type, Settings...>;

}  // namespace cppfig::detail
