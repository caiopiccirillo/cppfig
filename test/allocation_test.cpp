#include <cppfig/cppfig.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

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
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_LT(allocations, 8U);
    }

    TEST(AllocationTest, MigrationDoesNotAllocatePerSetting)
    {
        const TempDirectory directory;
        const auto path = directory.file("app.conf").string();
        allocation_count = 0;
        auto loaded = Configuration<ScaleRegistry>::Load(path);
        const auto allocations = allocation_count.load();
        ASSERT_TRUE(loaded.ok()) << loaded.status().message();
        EXPECT_NE(loaded->document.GetSourceText().find("scale.s127 = 0.5\n"), std::string_view::npos);
        EXPECT_LT(allocations, 40U);
    }

    TEST(AllocationTest, TypedReadsNeverAllocate)
    {
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.ok());
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
