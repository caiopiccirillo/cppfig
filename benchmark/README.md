# CPPFIG Performance Benchmarks

This directory contains comprehensive performance benchmarks for the CPPFIG configuration library using Google Benchmark.

## Overview

The benchmarks measure all critical aspects of the library's performance:

- **Setting Access**: Time to retrieve configuration settings
- **Value Access**: Time to access setting values
- **Metadata Operations**: Time for validation, description access, etc.
- **Multiple Setting Workloads**: Realistic usage patterns
- **File I/O Operations**: Save/load performance
- **Comparison Tests**: Legacy API vs modern API performance

## Building and Running

### Prerequisites

- Google Benchmark (automatically installed via vcpkg)
- CMake 3.22+
- C++17 compatible compiler

### Build Instructions

```bash
# Build in release mode for accurate performance measurements
cmake --workflow --preset=release-dev

# Or build just the benchmark
cd build/release/dev
make cppfig_benchmark  # or ninja cppfig_benchmark
```

### Running Benchmarks

```bash
cd build/release/dev

# Run all benchmarks
./benchmark/cppfig_benchmark

# Run specific benchmark categories
./benchmark/cppfig_benchmark --benchmark_filter="BM_GetSetting.*"
./benchmark/cppfig_benchmark --benchmark_filter="BM_ValueAccess.*"
./benchmark/cppfig_benchmark --benchmark_filter="BM_Multiple.*"

# Run with repetitions for statistical reliability
./benchmark/cppfig_benchmark --benchmark_repetitions=5

# Generate JSON output for analysis
./benchmark/cppfig_benchmark --benchmark_format=json --benchmark_out=results.json

# Run specific time duration
./benchmark/cppfig_benchmark --benchmark_min_time=2.0
```

### Benchmark Categories

#### Core Performance

- `BM_GetSetting_*`: Time to retrieve setting objects
- `BM_ValueAccess_*`: Time to access setting values
- `BM_GetSettingAndValue_*`: Combined operations (realistic usage)

#### Workload Tests

- `BM_MultipleSettingAccess_*`: Access multiple settings (5, 10, etc.)
- `BM_RandomAccess`: Random access patterns

#### Validation & Metadata

- `BM_ValidationCheck`: Individual setting validation
- `BM_ValidateAll`: Full configuration validation
- `BM_MetadataAccess`: Description, unit, min/max access
- `BM_ToString`: String representation generation

#### Modification Operations

- `BM_SetValue_*`: Setting value updates
- `BM_Reset*`: Reset operations

#### File I/O

- `BM_ConfigurationSave`: JSON serialization performance
- `BM_ConfigurationLoad`: JSON deserialization performance
- `BM_ConfigurationCreation`: Full configuration setup

#### API Comparison

- `BM_LegacyAPI_*`: Performance of legacy `GetValue<>()` API

## Performance Results Summary

**Latest benchmark results (Release mode, Clang 20.1.8, -O3):**

| Operation                | Time     | Performance Level      |
| ------------------------ | -------- | ---------------------- |
| GetSetting<Int>()        | 1.36 ns  | ⭐⭐⭐⭐⭐ Excellent   |
| setting.Value() (int)    | 0.233 ns | ⭐⭐⭐⭐⭐ Outstanding |
| GetSetting + Value (int) | 1.55 ns  | ⭐⭐⭐⭐⭐ Excellent   |
| 10 settings access       | 37.7 ns  | ⭐⭐⭐⭐⭐ Excellent   |
| Validation check         | 0.602 ns | ⭐⭐⭐⭐⭐ Outstanding |
| Configuration save       | 35.6 μs  | ⭐⭐⭐⭐ Good          |

**Key Performance Highlights:**

- **154x faster** than nlohmann::json for integer access
- **Sub-nanosecond** value access for primitives
- **Linear scalability** with setting count
- **Zero runtime overhead** for type safety
- **Production-ready** for high-frequency applications

## Understanding Results

### Time Scales

- **Sub-nanosecond (< 1ns)**: Outstanding for hot paths
- **Low nanosecond (1-10ns)**: Excellent for frequent operations
- **Tens of nanoseconds (10-100ns)**: Very good for batch operations
- **Microseconds (μs)**: Good for I/O operations
- **Milliseconds (ms)**: Acceptable for startup/shutdown

### Iteration Counts

Higher iteration counts indicate more reliable measurements:

- **Billions (1B+)**: Extremely fast, sub-nanosecond operations
- **Millions (1M+)**: Fast nanosecond operations
- **Thousands (1K+)**: Slower operations (I/O, allocation)

### Statistical Metrics

- **Mean**: Average time across repetitions
- **Median**: Middle value (less affected by outliers)
- **StdDev**: Standard deviation (lower = more consistent)
- **CV**: Coefficient of variation (should be < 10% for reliable results)

## Benchmarking Best Practices

1. **Always use Release builds** (`-O3 -DNDEBUG`)
2. **Run multiple repetitions** for statistical confidence
3. **Close other applications** to reduce system noise
4. **Use consistent hardware** for comparisons
5. **Warm up the benchmark** before measuring
6. **Consider CPU scaling warnings** (may affect precision)

## Extending Benchmarks

To add new benchmarks:

1. Add benchmark function in `benchmark_main.cpp`:

```cpp
static void BM_YourBenchmark(benchmark::State& state) {
    SetupBenchmark();
    for (auto _ : state) {
        // Your code to benchmark
        auto result = SomeOperation();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_YourBenchmark);
```

2. Rebuild and run:

```bash
make cppfig_benchmark
./benchmark/cppfig_benchmark --benchmark_filter="BM_YourBenchmark"
```

## Troubleshooting

### Common Issues

**"No rule to make target 'cppfig_benchmark'"**

- Ensure you're in the build directory
- Check that cmake configuration succeeded
- Verify benchmark dependency is available

**CPU scaling warnings**

- These are normal and don't affect relative performance comparisons
- For absolute measurements, disable CPU scaling in your system

**High variation in results**

- Close other applications
- Run on dedicated hardware if possible
- Increase repetitions: `--benchmark_repetitions=10`

**Unrealistic performance numbers**

- Verify `DoNotOptimize()` is used correctly
- Check that operations aren't being optimized away
- Ensure sufficient work in the benchmark loop

## Reference Links

- [Google Benchmark Documentation](https://github.com/google/benchmark)
- [CPPFIG Performance Analysis](../docs/PERFORMANCE_ANALYSIS.md)
- [Complete Benchmark Results](../docs/BENCHMARK_RESULTS.md)
