#pragma once

#include <cstdio>
#include <cstdlib>

/// Aborts with a message when a fuzzed property does not hold. Unlike assert,
/// it stays active in release builds, where fuzzers run fastest.
#define CPPFIG_FUZZ_CHECK(condition)                                                              \
    do {                                                                                          \
        if (!(condition)) {                                                                       \
            std::fprintf(stderr, "%s:%d: property failed: %s\n", __FILE__, __LINE__, #condition); \
            std::abort();                                                                         \
        }                                                                                         \
    } while (false)
