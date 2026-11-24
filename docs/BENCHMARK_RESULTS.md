# Benchmark results

Measurements of cppfig's reads, loading, saving, memory use and compile time. The suites and how to run them are described in `benchmark/README.md`.

## Environment

- Measured: November 2026
- CPU: Intel Core 5 120U, 12 logical CPUs, x86-64; CPU frequency scaling enabled
- OS: Linux 7.0
- Compiler: GCC 15.2.0 (Ubuntu 15.2.0-16ubuntu1), C++20, Release (`-O3 -DNDEBUG`)
- Google Benchmark, 3 repetitions, mean reported; coefficient of variation below 2% unless noted
- Temporary files on tmpfs (`/tmp`) unless noted; ext4 for the disk-backed save figures
- The registry-scale figures were measured inside the Ubuntu 26.04 development container

Absolute timings are specific to this machine. Allocation counts and sizes carry over to other machines using the same standard library.

Reproduce with:

```sh
cmake --workflow --preset=release-gcc
./build/release/gcc/benchmark/cppfig_benchmark --benchmark_repetitions=3 --benchmark_report_aggregates_only=true
```

## Read benchmarks under GCC 15

Google Benchmark's `DoNotOptimize` uses `asm volatile("" : "+m,r"(value) : : "memory")` on GCC. GCC 15.2 removes that statement entirely at `-O2`/`-O3`, so loops that only read a value compile to empty loops and report about 0.1 ns whatever they do. The read benchmarks therefore use `cppfig::bench::KeepValue` (`benchmark/barrier.h`), whose single-alternative constraints are kept, and the generated loops were checked in the disassembly to contain one load per read.

## Small registries

Registries of 1, 5 and 14 settings in a flat `.conf` file (`benchmark_main.cpp`).

| Operation | Time | Allocations |
| --- | ---: | ---: |
| `Get` integer (copy) | 0.21 ns | 0 |
| `Get` double (copy) | 0.20 ns | 0 |
| `Get` Boolean (copy) | 0.20 ns | 0 |
| `Get` validated integer (copy) | 0.20 ns | 0 |
| `Get` string (copy, 15 chars) | 1.07 ns | 0 |
| `Get` string (by reference) | 0.20 ns | 0 |
| Load, 1 setting | 1.86 µs | |
| Load, 5 settings | 2.33 µs | |
| Load, 14 settings | 3.41 µs | |
| Save, 1 setting (tmpfs) | 8.88 µs | |
| Save, 5 settings (tmpfs) | 8.89 µs | |
| Save, 14 settings (tmpfs) | 8.82 µs | |

A read is a load from a fixed offset in the configuration object: no lookup, no parsing, no allocation. Copying a string costs only the string copy.

### Saving on a real disk

A save writes a temporary file, `fsync`s it, renames it over the destination and `fsync`s the directory, so a crash leaves either the old or the new file. The directory `fsync` is best effort: if it fails, the file has already been replaced, so the save still reports success. On systems without POSIX I/O, the save is atomic but not flushed. On tmpfs `fsync` is nearly free; on ext4 the flushes dominate:

| Save on ext4 | Wall time | CPU time |
| --- | ---: | ---: |
| 1 setting | 1.36 ms (cv 19%) | 77 µs |
| 5 settings | 1.20 ms | 84 µs |
| 14 settings | 1.16 ms | 90 µs |
| 1024 settings | 1.20 ms | 95 µs |

Saving is a cold operation, and with `ConfDocument::NeedsSave()` callers only save when the file is missing or settings were appended, so this cost is normally paid once per schema change rather than on every start.

## Registry scale

Generated registries with equal numbers of validated `int` settings (with environment overrides), `std::string`, `bool` and validated `double` settings (`benchmark_scale.cpp`).

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

Full load, save, builder and read path for a generated registry, GCC 15.2:

| Settings | `-O2` | Peak compiler memory |
| --- | ---: | ---: |
| 16 | 1.6 s | 238 MB |
| 128 | 2.7 s | 320 MB |
| 1024 | 23.6 s | 757 MB |
| 1024, declared as a derived struct | 20.9 s | 753 MB |

The linked 1024-setting load path (`-O2`, PIE) is 929 KB, with 3,852 relocations. `test/compile_scale` enforces a 120-second limit for 1024 settings in CTest.

## Where the time and memory go

- The file is read into one allocation of its size, and migrations are appended into one new string reserved at the size it needs (the standard library may round a reservation up), so the document keeps no growth slack.
- Loading parses values straight into a single set of candidate slots, records lines in 4 bytes, and builds a default only when the file lacks the setting.
- The environment is read in one pass, and each variable is looked up among the sorted override names.
- Every per-setting operation goes through one function and one table, which keeps load-time relocations, unwind entries and the number of functions to compile low.
- A save allocates for the `std::filesystem::path` it builds from the caller's path and for the temporary file's name.
