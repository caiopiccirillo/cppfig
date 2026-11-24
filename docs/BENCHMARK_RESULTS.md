# Redesign results

Measurements of the typed frozen registry, compared with the pre-redesign baseline in [BASELINE_RESULTS.md](BASELINE_RESULTS.md). Both were taken on the same machine with the same compiler and equivalent workloads.

## Environment

- Date: 2026-11-19
- Revision: `89004ba` (branch `feat/typed-frozen-registry`)
- CPU: Intel Core 5 120U, 12 logical CPUs, x86-64; CPU frequency scaling enabled
- OS: Linux 7.0.0-43-generic
- Compiler: GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), C++20, Release (`-O3 -DNDEBUG`)
- Google Benchmark, 3 repetitions, mean reported; coefficient of variation below 2% unless noted
- Temporary files on tmpfs (`/tmp`) unless noted; ext4 for the disk-backed save figures
- Clang: not installed in the measurement environment

Reproduce with:

```sh
cmake --workflow --preset=release-gcc
./build/release/gcc/benchmark/cppfig_benchmark --benchmark_repetitions=3 --benchmark_report_aggregates_only=true
```

## A note on read benchmarks under GCC 15

Google Benchmark's `DoNotOptimize` uses `asm volatile("" : "+m,r"(value) : : "memory")` on GCC. GCC 15.2 removes that statement entirely at `-O2`/`-O3`, so loops that only read a value compile to empty loops and report about 0.1 ns whatever they do. The read benchmarks therefore use `cppfig::bench::KeepValue` (`benchmark/barrier.h`), whose single-alternative constraints are kept, and the generated loops were checked in the disassembly to contain one load per read.

The baseline read figures are unaffected: each pre-redesign read allocated, which the optimizer cannot remove.

## Comparison with the baseline

Same settings, same flat `.conf` format, same registry sizes (1, 5 and 14 settings).

| Operation | Before | After | Change |
| --- | ---: | ---: | ---: |
| `Get` integer (copy) | 144.7 ns, 5 allocations | 0.21 ns, 0 allocations | ~700x faster |
| `Get` double (copy) | 163.5 ns | 0.20 ns, 0 allocations | ~800x faster |
| `Get` Boolean (copy) | 152.7 ns | 0.20 ns, 0 allocations | ~760x faster |
| `Get` validated integer (copy) | 164.3 ns | 0.20 ns, 0 allocations | ~820x faster |
| `Get` string (copy, 15 chars) | 165.4 ns, 4 allocations | 1.07 ns, 0 allocations | ~150x faster |
| `Get` string (by reference) | not available | 0.20 ns, 0 allocations | |
| Load, 1 setting | 4.86 µs | 1.86 µs | 2.6x faster |
| Load, 5 settings | 10.32 µs | 2.33 µs | 4.4x faster |
| Load, 14 settings | 30.86 µs | 3.41 µs | 9.1x faster |
| Save, 1 setting (tmpfs) | 11.28 µs | 8.88 µs | 1.3x faster |
| Save, 5 settings (tmpfs) | 13.31 µs | 8.89 µs | 1.5x faster |
| Save, 14 settings (tmpfs) | 25.05 µs | 8.82 µs | 2.8x faster |
| `sizeof(Configuration)`, 2 settings | 120 bytes plus heap trees | values only (see below) | |
| Compile, 1024 settings | failed (constexpr limit) | 23.6 s at `-O2` (whole load path) | |

Reads are now a load from a fixed offset in the configuration object: no lookup, no parsing, no allocation. A copy of a string costs only the string copy.

### Saving is durable now, and that costs time on real disks

The old save wrote the file in place with no flush. The new save writes a temporary file, `fsync`s it, renames it over the destination and `fsync`s the directory, so a crash leaves either the old or the new file. The directory `fsync` is best effort: if it fails, the file has already been replaced, so the save still reports success. On systems without POSIX I/O, the save is atomic but not flushed. On tmpfs `fsync` is nearly free, which is why the table above shows a small gain. On ext4 the flushes dominate:

| Save on ext4 | Wall time | CPU time |
| --- | ---: | ---: |
| 1 setting | 1.36 ms (cv 19%) | 77 µs |
| 5 settings | 1.20 ms | 84 µs |
| 14 settings | 1.16 ms | 90 µs |
| 1024 settings | 1.20 ms | 95 µs |

Saving is a cold operation, and with `ConfDocument::NeedsSave()` callers only save when the file is missing or settings were appended, so this cost is normally paid once per schema change rather than on every start.

## Registry scale

Generated registries with equal numbers of validated `int` settings (with environment overrides), `std::string`, `bool` and validated `double` settings.

The figures in this section were re-measured after the memory and architecture review, on the same machine inside the Ubuntu 26.04 development container (GCC 15.2, `-O3`, temporary files on tmpfs), mean of 3 repetitions. The comparison with the code before that review was measured the same way, side by side.

### Before and after the review

| Operation | Before | After | Change |
| --- | ---: | ---: | ---: |
| Load, complete file, 1024 settings | 175 µs, 17 allocations (137 KB) | 122–129 µs, 7 allocations (56 KB) | 26–30% faster, 59% fewer bytes |
| Load, complete file, 128 settings | 14.3 µs, 14 allocations (24.6 KB) | 11.8 µs, 7 allocations (14.4 KB) | 18% faster |
| Load with every override set, 1024 settings | 256 µs | 152 µs | 41% faster |
| Load, missing file, 1024 settings | 61.8 µs (147 KB) | 50.9 µs (109 KB) | 18% faster |
| `FromDefaults`, 1024 settings | 19.6 µs (66.8 KB) | 11.4 µs (16.9 KB) | 42% faster, 75% fewer bytes |
| Heap retained by a loaded result, 1024 settings | 30,784 bytes | 18,072 bytes | 41% less |
| Heap retained by a loaded result, 128 settings | 3,888 bytes | 2,136 bytes | 45% less |
| Linked 1024-setting load path (`-O2`, PIE) | 1,174 KB, 5,900 relocations | 929 KB, 3,852 relocations | 21% smaller |
| Compile, 1024 settings, `-O2` | 24.0 s | 22.6 s | 6% faster |
| Reads | 0.20 ns, 0 allocations | 0.20 ns, 0 allocations | unchanged |
| Save (tmpfs) | 11 allocations | 14 allocations | same time within noise |

Where the gains come from:

- The file is read into one allocation of its size, and migrations are appended into one new string reserved at the size it needs (the standard library may round a reservation up). Before, the text grew geometrically while it was read and the document kept the slack: the retained heap was 1.7 times the text.
- Loading parses values straight into a single set of candidate slots instead of separate file and effective copies, records lines in 4 bytes, and builds a default only when the file lacks the setting.
- The environment is read in one pass and looked up among the sorted override names, instead of one `getenv` scan of the whole environment per override.
- Every per-setting operation goes through one function and one table, instead of three: fewer load-time relocations, unwind entries and functions to compile.
- Save builds a `std::filesystem::path` from the caller's path, which accounts for its extra allocations.

### Initialization

| Operation | 16 | 128 | 1024 |
| --- | ---: | ---: | ---: |
| Load, complete file | 3.51 µs | 11.8 µs | 129 µs |
| Load, missing file (all defaults migrated) | 1.69 µs | 6.46 µs | 50.9 µs |
| Load, complete file, every environment override set | 3.67 µs | 13.1 µs | 152 µs |
| `FromDefaults` | 0.13 µs | 1.26 µs | 11.4 µs |
| Save, complete document (tmpfs) | 9.30 µs | 9.42 µs | 11.9 µs |

With every override set, the 1024-setting load reads an environment holding 256 extra variables, in one pass.

### Allocations per operation

| Operation | 16 | 128 | 1024 |
| --- | ---: | ---: | ---: |
| Load, complete file | 7 (9.3 KB) | 7 (14.4 KB) | 7 (55.9 KB) |
| Load, missing file | 11 (2.1 KB) | 14 (13.9 KB) | 17 (109 KB) |
| `FromDefaults` | 1 (0.3 KB) | 1 (2.1 KB) | 1 (16.9 KB) |
| Save | 14 | 14 | 14 |
| Reading every setting | 0 | 0 | 0 |

In this workload, loading a complete file allocates a fixed number of times, whatever the number of settings or the size of the file, because every string value fits the small-string buffer. A string value too long for that buffer, or a custom type that owns memory, adds its own allocations, so in general the count grows with the number of such values. With a missing file, the migrations string grows with the number of settings, so the count grows with its logarithm. The single `FromDefaults` allocation is the temporary candidate storage, kept on the heap so that large registries do not need a large stack.

### Memory

| | 16 | 128 | 1024 |
| --- | ---: | ---: | ---: |
| `sizeof(Configuration)` | 184 bytes | 1,440 bytes | 11,520 bytes |
| Bytes per setting | 11.5 | 11.25 | 11.25 |
| Document text (`ConfDocument`) | 251 bytes | 2,134 bytes | 18,068 bytes |
| Heap retained by a loaded result | 264 bytes | 2,136 bytes | 18,072 bytes |

The average value size in this mix is (4 + 32 + 1 + 8) / 4 = 11.25 bytes, so the configuration holds the values and nothing else: storage is ordered by alignment, leaving no padding between values. This is a property of the Itanium C++ ABI that GCC and Clang use, which lays base classes out in order; the C++ standard leaves that layout to the implementation. The 16-setting figure includes 4 bytes of tail padding to the 8-byte alignment of the whole object. The document is separate and can be discarded once saved. The heap a loaded result keeps is the document's text and nothing more, because these strings fit the small-string buffer and no key is unknown. Long strings, owning custom types and preserved unknown keys add to it.

### Reads

| Operation | 16 | 128 |
| --- | ---: | ---: |
| Read every setting once | 1.09 ns | 8.64 ns |
| Per setting | 0.068 ns | 0.068 ns |

### Compile time

Full load, save, builder and read path for a generated registry, GCC 15.2, as first measured; after the review the 1024-setting case compiles in 22.6 s against 24.0 s before it, measured side by side:

| Settings | `-O2` | Peak compiler memory |
| --- | ---: | ---: |
| 16 | 1.6 s | 238 MB |
| 128 | 2.7 s | 320 MB |
| 1024 | 23.6 s | 757 MB |
| 1024, declared as a derived struct | 20.9 s | 753 MB |

`test/compile_scale` enforces a 120-second limit for 1024 settings in CTest. Before the redesign's compile-time work, the same 1024-setting registry took 258 s and 3.2 GB of compiler memory for an integer-only load path, and the pre-redesign implementation did not compile at that size at all.
