#include <cppfig/cppfig.h>

#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdlib>

#include "test_settings.h"

namespace {
std::atomic<std::size_t> allocation_count = 0;
}

void* operator new(std::size_t size)
{
    allocation_count.fetch_add(1, std::memory_order_relaxed);
    if (void* pointer = std::malloc(size)) {
        return pointer;
    }
    std::abort();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace cppfig::test {
namespace {

    template <typename T>
    void DoNotOptimize(const T& value)
    {
        asm volatile("" : : "g"(&value) : "memory");
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
