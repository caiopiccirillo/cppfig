// Registry-scale benchmarks: generated registries of 16, 128 and 1024 mixed
// settings (validated ints with environment overrides, strings, bools and
// validated doubles). Counters report heap allocations and bytes per
// operation, the frozen configuration's size and the document's size.
#include <benchmark/benchmark.h>
#include <cppfig/cppfig.h>
#include <malloc.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <string>
#include <type_traits>

#include "barrier.h"
#include "generated_registry.h"

namespace {

std::atomic<std::size_t> allocation_count = 0;
std::atomic<std::size_t> allocated_bytes = 0;
// Heap held by live allocations, as the allocator sizes them (glibc's
// malloc_usable_size), so capacity slack in retained buffers is counted.
std::atomic<std::ptrdiff_t> live_bytes = 0;

void Release(void* pointer) noexcept
{
    if (pointer != nullptr) {
        live_bytes.fetch_sub(static_cast<std::ptrdiff_t>(::malloc_usable_size(pointer)), std::memory_order_relaxed);
    }
    std::free(pointer);
}

auto CountedAllocate(std::size_t size, std::size_t alignment) noexcept -> void*
{
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    allocated_bytes.fetch_add(size, std::memory_order_relaxed);
    if (size == 0) {
        size = 1;
    }
    void* pointer = alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__
        ? std::malloc(size)
        : std::aligned_alloc(alignment, (size + alignment - 1) / alignment * alignment);
    if (pointer != nullptr) {
        live_bytes.fetch_add(static_cast<std::ptrdiff_t>(::malloc_usable_size(pointer)), std::memory_order_relaxed);
    }
    return pointer;
}

auto CountedAllocateOrAbort(std::size_t size, std::size_t alignment) -> void*
{
    if (void* pointer = CountedAllocate(size, alignment)) {
        return pointer;
    }
    std::abort();
}

}  // namespace

// NOLINTBEGIN(misc-new-delete-overloads)
void* operator new(std::size_t size) { return CountedAllocateOrAbort(size, 0); }
void* operator new[](std::size_t size) { return CountedAllocateOrAbort(size, 0); }
void* operator new(std::size_t size, std::align_val_t alignment) { return CountedAllocateOrAbort(size, static_cast<std::size_t>(alignment)); }
void* operator new[](std::size_t size, std::align_val_t alignment) { return CountedAllocateOrAbort(size, static_cast<std::size_t>(alignment)); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept { return CountedAllocate(size, 0); }
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept { return CountedAllocate(size, 0); }
void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return CountedAllocate(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    return CountedAllocate(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* pointer) noexcept { Release(pointer); }
void operator delete[](void* pointer) noexcept { Release(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { Release(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { Release(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { Release(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { Release(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { Release(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { Release(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { Release(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { Release(pointer); }
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { Release(pointer); }
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { Release(pointer); }
// NOLINTEND(misc-new-delete-overloads)

namespace cppfig::bench {
namespace {

    template <std::size_t N>
    struct Scale : scale::GeneratedRegistry<N> { };

    template <std::size_t N>
    using ScaleConfiguration = Configuration<Scale<N>>;

    class TemporaryFile {
    public:
        explicit TemporaryFile(std::string_view name)
            : path_((std::filesystem::temp_directory_path() / ("cppfig_scale_" + std::string(name) + ".conf")).string())
        {
            std::filesystem::remove(path_);
        }

        ~TemporaryFile()
        {
            std::error_code error;
            std::filesystem::remove(path_, error);
        }

        TemporaryFile(const TemporaryFile&) = delete;
        auto operator=(const TemporaryFile&) -> TemporaryFile& = delete;

        [[nodiscard]] auto path() const -> const std::string& { return path_; }

    private:
        std::string path_;
    };

    /// Measures allocations made by the timed loop and reports them per iteration.
    class AllocationCounter {
    public:
        AllocationCounter()
            : count_(allocation_count.load())
            , bytes_(allocated_bytes.load())
        {
        }

        void Report(benchmark::State& state) const
        {
            const auto iterations = static_cast<double>(state.iterations());
            state.counters["allocs"] = static_cast<double>(allocation_count.load() - count_) / iterations;
            state.counters["alloc_bytes"] = static_cast<double>(allocated_bytes.load() - bytes_) / iterations;
        }

    private:
        std::size_t count_;
        std::size_t bytes_;
    };

    template <std::size_t N>
    void ReportLayout(benchmark::State& state)
    {
        state.counters["config_bytes"] = sizeof(ScaleConfiguration<N>);
        state.counters["bytes_per_setting"] = static_cast<double>(sizeof(ScaleConfiguration<N>)) / static_cast<double>(N);
    }

    /// Writes a complete file holding every default.
    template <std::size_t N>
    auto CreateCompleteFile(const std::string& path) -> bool
    {
        auto created = cppfig::LoadFile<Scale<N>>(path);
        return created.has_value() && cppfig::SaveFile(path, created->document).ok();
    }

    template <std::size_t N>
    void LoadCompleteFile(benchmark::State& state)
    {
        const TemporaryFile file("complete_" + std::to_string(N));
        if (!CreateCompleteFile<N>(file.path())) {
            state.SkipWithError("could not create the configuration file");
            return;
        }
        std::size_t document_bytes = 0;
        const AllocationCounter counter;
        for (auto _ : state) {
            auto loaded = cppfig::LoadFile<Scale<N>>(file.path());
            document_bytes = loaded->document.GetText().size();
            benchmark::DoNotOptimize(loaded);
        }
        counter.Report(state);
        ReportLayout<N>(state);
        state.counters["document_bytes"] = static_cast<double>(document_bytes);

        // Heap still held by one load's result: the document's text (with any
        // capacity slack) and whatever the values themselves allocate.
        const auto live_before = live_bytes.load();
        auto kept = cppfig::LoadFile<Scale<N>>(file.path());
        state.counters["retained_bytes"] = static_cast<double>(live_bytes.load() - live_before);
        benchmark::DoNotOptimize(kept);
    }

    /// Loads with no file: every default is serialized into a new document.
    template <std::size_t N>
    void LoadMissingFile(benchmark::State& state)
    {
        const TemporaryFile file("missing_" + std::to_string(N));
        const AllocationCounter counter;
        for (auto _ : state) {
            auto loaded = cppfig::LoadFile<Scale<N>>(file.path());
            benchmark::DoNotOptimize(loaded);
        }
        counter.Report(state);
    }

    /// Loads a complete file while every declared environment override is set.
    template <std::size_t N>
    void LoadWithEnvironment(benchmark::State& state)
    {
        const TemporaryFile file("environment_" + std::to_string(N));
        if (!CreateCompleteFile<N>(file.path())) {
            state.SkipWithError("could not create the configuration file");
            return;
        }
        Scale<N>::ForEachSetting([]<typename S>() {
            if constexpr (HasEnvOverride<S>) {
                setenv(std::string(GetEnvOverride<S>()).c_str(), "7", 1);
            }
        });
        const AllocationCounter counter;
        for (auto _ : state) {
            auto loaded = cppfig::LoadFile<Scale<N>>(file.path());
            benchmark::DoNotOptimize(loaded);
        }
        counter.Report(state);
        Scale<N>::ForEachSetting([]<typename S>() {
            if constexpr (HasEnvOverride<S>) {
                unsetenv(std::string(GetEnvOverride<S>()).c_str());
            }
        });
    }

    template <std::size_t N>
    void FromDefaults(benchmark::State& state)
    {
        const AllocationCounter counter;
        for (auto _ : state) {
            auto loaded = ScaleConfiguration<N>::FromDefaults();
            benchmark::DoNotOptimize(loaded);
        }
        counter.Report(state);
    }

    template <std::size_t N>
    void SaveCompleteDocument(benchmark::State& state)
    {
        const TemporaryFile file("save_" + std::to_string(N));
        auto loaded = cppfig::LoadFile<Scale<N>>(file.path());
        if (!loaded.has_value()) {
            state.SkipWithError("could not load the configuration");
            return;
        }
        const AllocationCounter counter;
        for (auto _ : state) {
            auto status = cppfig::SaveFile(file.path(), loaded->document);
            benchmark::DoNotOptimize(status);
        }
        counter.Report(state);
    }

    /// Reads every setting once per iteration through its reference.
    template <std::size_t N>
    void ReadEverySetting(benchmark::State& state)
    {
        auto loaded = ScaleConfiguration<N>::FromDefaults();
        const auto& config = loaded.value();
        const auto* escaped = &config;
        KeepValue(escaped);
        const AllocationCounter counter;
        for (auto _ : state) {
            Scale<N>::ForEachSetting([&]<typename S>() {
                const auto& value = config.template Get<S>();
                if constexpr (std::is_trivially_copyable_v<typename S::value_type>) {
                    auto copy = value;
                    KeepValue(copy);
                }
                else {
                    auto size = value.size();
                    KeepValue(size);
                }
            });
        }
        counter.Report(state);
        state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(N));
    }

}  // namespace

BENCHMARK(LoadCompleteFile<16>)->Name("LoadCompleteFile/16");
BENCHMARK(LoadCompleteFile<128>)->Name("LoadCompleteFile/128");
BENCHMARK(LoadCompleteFile<1024>)->Name("LoadCompleteFile/1024");
BENCHMARK(LoadMissingFile<16>)->Name("LoadMissingFile/16");
BENCHMARK(LoadMissingFile<128>)->Name("LoadMissingFile/128");
BENCHMARK(LoadMissingFile<1024>)->Name("LoadMissingFile/1024");
BENCHMARK(LoadWithEnvironment<16>)->Name("LoadWithEnvironment/16");
BENCHMARK(LoadWithEnvironment<128>)->Name("LoadWithEnvironment/128");
BENCHMARK(LoadWithEnvironment<1024>)->Name("LoadWithEnvironment/1024");
BENCHMARK(FromDefaults<16>)->Name("FromDefaults/16");
BENCHMARK(FromDefaults<128>)->Name("FromDefaults/128");
BENCHMARK(FromDefaults<1024>)->Name("FromDefaults/1024");
BENCHMARK(SaveCompleteDocument<16>)->Name("SaveCompleteDocument/16")->UseRealTime();
BENCHMARK(SaveCompleteDocument<128>)->Name("SaveCompleteDocument/128")->UseRealTime();
BENCHMARK(SaveCompleteDocument<1024>)->Name("SaveCompleteDocument/1024")->UseRealTime();
BENCHMARK(ReadEverySetting<16>)->Name("ReadEverySetting/16");
BENCHMARK(ReadEverySetting<128>)->Name("ReadEverySetting/128");

}  // namespace cppfig::bench
