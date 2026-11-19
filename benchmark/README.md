# cppfig benchmarks

Google Benchmark suites for reads, initialization, saving, allocations and memory. Current results and the comparison with the pre-redesign implementation are in [docs/BENCHMARK_RESULTS.md](../docs/BENCHMARK_RESULTS.md); the baseline is in [docs/BASELINE_RESULTS.md](../docs/BASELINE_RESULTS.md).

## Building and running

Benchmarks are built by default when cppfig is the top-level project (`CPPFIG_BUILD_BENCHMARKS`). Use a Release build:

```bash
cmake --workflow --preset=release-gcc
./build/release/gcc/benchmark/cppfig_benchmark
```

Useful options:

```bash
# Statistics over repetitions, as recorded in the results document
./build/release/gcc/benchmark/cppfig_benchmark --benchmark_repetitions=3 --benchmark_report_aggregates_only=true

# A subset
./build/release/gcc/benchmark/cppfig_benchmark --benchmark_filter='Load'

# Machine-readable output
./build/release/gcc/benchmark/cppfig_benchmark --benchmark_out=results.json --benchmark_out_format=json

# Measure saves on a real disk rather than tmpfs
TMPDIR=/path/on/disk ./build/release/gcc/benchmark/cppfig_benchmark --benchmark_filter='Save'
```

## Suites

### `benchmark_main.cpp`: comparison with the baseline

The same settings, registry sizes and file format as the pre-redesign suite, so results compare directly.

| Benchmark | Measures |
| --- | --- |
| `GetString`, `GetInt`, `GetDouble`, `GetBool`, `GetValidated` | Reading a setting into a copy, as the old API returned |
| `GetStringReference` | Reading a string through the returned reference |
| `LoadSmallRegistry`, `LoadMediumRegistry`, `LoadLargeRegistry` | Loading a complete existing file of 1, 5 and 14 settings |
| `SaveSmallRegistry`, `SaveMediumRegistry`, `SaveLargeRegistry` | Saving a loaded document, wall time |

### `benchmark_scale.cpp`: registry scale

Generated registries of 16, 128 and 1024 settings: a quarter each of validated `int`s with environment overrides, `std::string`s, `bool`s and validated `double`s. The generator is shared with the compile-time test in `test/compile_scale/generated_registry.h`.

| Benchmark | Measures |
| --- | --- |
| `LoadCompleteFile/N` | Loading a file that has every setting |
| `LoadMissingFile/N` | Loading with no file: every default is serialized into a new document |
| `LoadWithEnvironment/N` | Loading a complete file with every environment override set |
| `FromDefaults/N` | Creating a configuration from defaults only |
| `SaveCompleteDocument/N` | Saving a document, wall time |
| `ReadEverySetting/N` | Reading every setting once |

Counters:

| Counter | Meaning |
| --- | --- |
| `allocs`, `alloc_bytes` | Heap allocations and bytes requested per iteration, counted by replacing the global `operator new` |
| `config_bytes`, `bytes_per_setting` | `sizeof(Configuration)` and that size divided by the number of settings |
| `document_bytes` | Size of the retained source text |

## Keeping reads honest

A read is a single load, so the optimizer can easily delete it, or hoist it out of the loop, and report a meaningless time. The read benchmarks:

- let the configuration's address escape before the loop, so the compiler cannot assume its contents;
- pass each value through `cppfig::bench::KeepValue` (`barrier.h`), an empty `asm volatile` with a memory clobber.

`KeepValue` replaces `benchmark::DoNotOptimize` for reads because GCC 15.2 removes `DoNotOptimize`'s `"+m,r"` asm statement entirely at `-O2` and `-O3`, turning the loops into empty ones. After changing a read benchmark, check the loop in the disassembly:

```bash
objdump -d --no-show-raw-insn -C build/release/gcc/benchmark/cppfig_benchmark | less   # search for the benchmark's name
```

## Caveats

- Timings depend on the machine; allocation counts and sizes are more portable across machines using the same standard library.
- `/tmp` is often tmpfs, where `fsync` costs almost nothing. Real disks make saving about a millisecond, dominated by the two `fsync` calls that make it durable.
- CPU frequency scaling adds noise; compare runs on the same machine, with repetitions.
