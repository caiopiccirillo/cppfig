#pragma once

#include <type_traits>

namespace cppfig::bench {

/// Forces value to be materialized and treats all memory as clobbered, so the
/// compiler must reload anything an escaped pointer can reach on the next use.
///
/// Used instead of benchmark::DoNotOptimize for the read benchmarks: its GCC
/// form, asm volatile("" : "+m,r"(value) : : "memory"), is removed entirely by
/// GCC 15.2 at -O2/-O3, which turned the loops being measured into empty loops.
/// The single-alternative constraints below are kept.
template <typename T>
inline __attribute__((always_inline)) void KeepValue(T& value)
{
    // The branches differ only in their asm constraint, which the check ignores.
    if constexpr (std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(void*)) {  // NOLINT(bugprone-branch-clone)
        asm volatile("" : "+r"(value) : : "memory");
    }
    else {
        asm volatile("" : "+m"(value) : : "memory");
    }
}

}  // namespace cppfig::bench
