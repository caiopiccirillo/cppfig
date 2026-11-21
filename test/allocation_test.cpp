#include <cppfig/cppfig.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>

#include "generated_registry.h"
#include "test_settings.h"

namespace {
std::atomic<std::size_t> allocation_count = 0;

// Every replaceable allocation form is routed through these two functions so
// that no allocation goes uncounted and every pointer is released by the same
// allocator family that produced it (otherwise ASan reports alloc-dealloc mismatches).
auto CountedAllocate(std::size_t size, std::size_t alignment) noexcept -> void*
{
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (size == 0) {
        size = 1;
    }
    if (alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
        return std::malloc(size);
    }
    const std::size_t rounded = (size + alignment - 1) / alignment * alignment;
    return std::aligned_alloc(alignment, rounded);
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

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete[](void* pointer, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::align_val_t, const std::nothrow_t&) noexcept { std::free(pointer); }
// NOLINTEND(misc-new-delete-overloads)

namespace cppfig::test {
namespace {

    template <typename T>
    void DoNotOptimize(const T& value)
    {
        asm volatile("" : : "g"(&value) : "memory");
    }

    // Guards against a vacuous zero-allocation result: the counter must see
    // ordinary, over-aligned and nothrow allocations alike.
    TEST(AllocationTest, CounterObservesEveryAllocationForm)
    {
        struct alignas(64) OverAligned {
            char bytes[64];
        };

        // DoNotOptimize keeps the compiler from eliding the new/delete pairs.
        allocation_count = 0;
        auto* plain = new int(1);
        DoNotOptimize(plain);
        delete plain;
        EXPECT_EQ(allocation_count.load(), 1U);

        allocation_count = 0;
        auto* aligned = new OverAligned {};
        DoNotOptimize(aligned);
        delete aligned;
        EXPECT_EQ(allocation_count.load(), 1U);

        allocation_count = 0;
        auto* nothrow = new (std::nothrow) int(1);
        DoNotOptimize(nothrow);
        delete nothrow;
        EXPECT_EQ(allocation_count.load(), 1U);
    }

    // 128 settings, a quarter of them strings short enough for the small-string
    // buffer: initialization should allocate a bounded number of times, not once
    // per setting.
    using ScaleRegistry = scale::GeneratedRegistry<128>;

    TEST(AllocationTest, FromDefaultsDoesNotAllocatePerSetting)
    {
        allocation_count = 0;
        auto loaded = Configuration<ScaleRegistry>::FromDefaults();
        const auto allocations = allocation_count.load();
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_LT(allocations, 8U);
    }

    TEST(AllocationTest, MigrationDoesNotAllocatePerSetting)
    {
        const TempDirectory directory;
        const auto path = directory.file("app.conf").string();
        allocation_count = 0;
        auto loaded = LoadFile<ScaleRegistry>(path);
        const auto allocations = allocation_count.load();
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message();
        EXPECT_NE(loaded->document.GetText().find("scale.s127 = 0.5\n"), std::string_view::npos);
        EXPECT_LT(allocations, 40U);
    }

    template <typename RegistryType>
    auto CountAllocationsOfLoadingDefaults() -> std::size_t
    {
        auto created = Configuration<RegistryType>::FromText("", "app.conf", { .environment = Environment::None() });
        EXPECT_TRUE(created);
        const std::string text(created->document.GetText());

        allocation_count = 0;
        auto loaded = Configuration<RegistryType>::FromText(text, "app.conf", { .environment = Environment::None() });
        const auto allocations = allocation_count.load();
        EXPECT_TRUE(loaded) << loaded.error().message();
        return allocations;
    }

    // Values are parsed into one set of candidate slots, so loading a complete
    // file costs the same number of allocations whatever the registry's size.
    TEST(AllocationTest, LoadingACompleteFileAllocatesAFixedNumberOfTimes)
    {
        const auto small = CountAllocationsOfLoadingDefaults<scale::GeneratedRegistry<16>>();
        const auto large = CountAllocationsOfLoadingDefaults<ScaleRegistry>();
        EXPECT_EQ(small, large);
        EXPECT_LE(large, 8U);
    }

    TEST(AllocationTest, BuiltInValidatorsDoNotAllocateForValidValues)
    {
        // Longer than the small-string buffer, so a copy into std::string would allocate.
        const std::string value = "an-allowed-value-longer-than-sixteen";
        allocation_count = 0;
        for (int i = 0; i < 100; ++i) {
            const auto one_of = OneOf("another-allowed-value-of-some-length", "an-allowed-value-longer-than-sixteen");
            const auto predicate = Predicate<std::string>([](const std::string& text) { return !text.empty(); },
                                                          "a failure message longer than the small-string buffer");
            DoNotOptimize(one_of);
            DoNotOptimize(predicate);
            EXPECT_TRUE(one_of(value));
            EXPECT_TRUE(predicate(value));
            EXPECT_TRUE(MinLength(1).And(MaxLength(64))(value));
        }
        EXPECT_EQ(allocation_count.load(), 0U);
    }

    TEST(AllocationTest, TypedReadsNeverAllocate)
    {
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.has_value());
        const auto& config = loaded.value();
        constexpr int iterations = 1000;

        DoNotOptimize(config.Get<settings::ServerPort>());
        DoNotOptimize(config.Get<settings::AppName>());
        DoNotOptimize(config.Get<settings::Origin>());

        allocation_count = 0;
        for (int i = 0; i < iterations; ++i) {
            DoNotOptimize(config.Get<settings::ServerPort>());
        }
        EXPECT_EQ(allocation_count.load(), 0U);

        allocation_count = 0;
        for (int i = 0; i < iterations; ++i) {
            DoNotOptimize(config.Get<settings::AppName>());
        }
        EXPECT_EQ(allocation_count.load(), 0U);

        allocation_count = 0;
        for (int i = 0; i < iterations; ++i) {
            DoNotOptimize(config.Get<settings::Origin>());
        }
        EXPECT_EQ(allocation_count.load(), 0U);
    }

}  // namespace
}  // namespace cppfig::test
