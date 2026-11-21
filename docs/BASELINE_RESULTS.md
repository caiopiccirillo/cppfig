# Pre-redesign baseline

This document records measurements taken before the frozen typed-registry redesign. It replaces the stale performance claims in `benchmark/README.md` as the reference point for the redesign comparison.

## Environment

- Date: 2026-11-18
- Architecture: x86-64
- CPU: Intel Core 5 120U, 12 logical CPUs
- Compiler: GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1)
- C++ standard: C++20
- Optimization for microbenchmarks: `-O3 -DNDEBUG` through the Release benchmark target
- Clang: not installed in the measurement environment

Absolute timings are specific to this environment. Allocation counts and object sizes are more useful for comparing implementations on the same standard library.

## Correctness baseline

A fresh Debug GCC build completed successfully. All 260 tests passed, including the three pre-existing modified test/source files in the working tree.

The previous `build/` directories could not be reused because their CMake caches referred to `/workspaces/cppfig`; fresh builds were created in a temporary directory.

## Allocation and layout probe

A standalone probe used the current `Configuration` implementation with two settings:

- `benchmark.string` (`std::string`, value `benchmark_value`)
- `benchmark.deeply.nested.int` (`int`, value `42`)

The configuration was loaded before counting. Each access was repeated 1,000 times after a warm-up, with global `operator new` instrumentation.

| Measurement | Result |
| --- | ---: |
| `sizeof(Value)` | 40 bytes |
| `alignof(Value)` | 8 bytes |
| `sizeof(Configuration<Schema>)` | 120 bytes |
| `alignof(Configuration<Schema>)` | 8 bytes |
| Integer `Get` allocations | 5 per call |
| Integer `Get` bytes allocated | 280 per call |
| String `Get` allocations | 4 per call |
| String `Get` bytes allocated | 130 per call |

These counts include temporary path-processing and value-copy allocations. They are not estimates of steady-state heap retention.

## Runtime benchmarks

The existing Google Benchmark suite was run in a fresh Release GCC build with three repetitions and aggregate reporting. The complete machine-readable output was retained outside the repository.

| Operation | Mean CPU time |
| --- | ---: |
| `Get` string | 165.380 ns |
| `Get` integer | 144.734 ns |
| `Get` double | 163.537 ns |
| `Get` Boolean | 152.675 ns |
| `Get` validated integer | 164.265 ns |
| Load, 1 setting | 4.856 μs |
| Load, 5 settings | 10.319 μs |
| Load, 14 settings | 30.862 μs |
| Save, 1 setting | 11.282 μs |
| Save, 5 settings | 13.312 μs |
| Save, 14 settings | 25.049 μs |
| Direct `Value::GetAtPath` | 200.445 ns |

## Registry-scale probe

Generated integer settings were used to probe initialization and compilation at the agreed registry sizes. The configuration object remained 120 bytes because its generic trees are heap-owned.

| Setting count | `sizeof(Configuration)` | Peak process RSS | Result |
| --- | ---: | ---: | --- |
| 16 | 120 bytes | 4,300 KiB | Passed |
| 128 | 120 bytes | 4,736 KiB | Passed |
| 1024 | Not produced | Not produced | Compilation failed |

At 1024 settings, GCC exceeded its default constexpr operation limit of 33,554,432 in `ConfigSchema::AllPathsUnique`. The current pairwise path validation therefore does not meet the agreed compilation-scale baseline without raising compiler limits. Peak RSS includes the whole short-lived process and is not a steady-state heap measurement.

## Interpretation rules

- Existing `benchmark/README.md` results refer to an older API and are not used.
- Comparisons must use equivalent initialized configurations and observable results.
- Read benchmarks must prevent the optimizer from removing the access.
- File benchmarks must distinguish initialization, migration, and save work.
- Redesigned reads will additionally be held to a permanent zero-allocation test rather than inferred from timings.
