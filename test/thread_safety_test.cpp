#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <latch>
#include <string>
#include <thread>
#include <vector>

namespace cppfig::test {

// ---------------------------------------------------------------------------
// Test settings
// ---------------------------------------------------------------------------

namespace settings {

struct Counter {
    static constexpr std::string_view kPath = "app.counter";
    using ValueType = int;
    static auto DefaultValue() -> int { return 0; }
};

struct Name {
    static constexpr std::string_view kPath = "app.name";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "default"; }
};

struct Ratio {
    static constexpr std::string_view kPath = "app.ratio";
    using ValueType = double;
    static auto DefaultValue() -> double { return 1.0; }
};

struct Enabled {
    static constexpr std::string_view kPath = "app.enabled";
    using ValueType = bool;
    static auto DefaultValue() -> bool { return true; }
};

struct ValidatedPort {
    static constexpr std::string_view kPath = "server.port";
    using ValueType = int;
    static auto DefaultValue() -> int { return 8080; }
    static auto GetValidator() -> cppfig::Validator<int> { return cppfig::Range(1, 65535); }
};

struct HostWithEnv {
    static constexpr std::string_view kPath = "server.host";
    static constexpr std::string_view kEnvOverride = "CPPFIG_TEST_HOST";
    using ValueType = std::string;
    static auto DefaultValue() -> std::string { return "localhost"; }
};

}  // namespace settings

using TestSchema = ConfigSchema<settings::Counter, settings::Name, settings::Ratio,
                                settings::Enabled, settings::ValidatedPort, settings::HostWithEnv>;

using ThreadSafeConfig = Configuration<TestSchema, JsonSerializer, MultiThreadedPolicy>;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        file_path_ = testing::ConfigurationTestFixture::CreateTempFilePath("thread_safety_test");
    }

    void TearDown() override { testing::ConfigurationTestFixture::RemoveFile(file_path_); }

    std::string file_path_;
};

// ---------------------------------------------------------------------------
// Basic sanity: MultiThreadedPolicy behaves identically to default in a
// single-threaded scenario.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, SingleThreadedBasicOperations)
{
    ThreadSafeConfig config(file_path_);

    auto status = config.Load();
    ASSERT_TRUE(status.ok()) << status.message();

    EXPECT_EQ(config.Get<settings::Counter>(), 0);
    EXPECT_EQ(config.Get<settings::Name>(), "default");
    EXPECT_DOUBLE_EQ(config.Get<settings::Ratio>(), 1.0);
    EXPECT_TRUE(config.Get<settings::Enabled>());
    EXPECT_EQ(config.Get<settings::ValidatedPort>(), 8080);
    EXPECT_EQ(config.Get<settings::HostWithEnv>(), "localhost");

    status = config.Set<settings::Counter>(42);
    ASSERT_TRUE(status.ok());
    EXPECT_EQ(config.Get<settings::Counter>(), 42);

    status = config.Save();
    ASSERT_TRUE(status.ok());
}

TEST_F(ThreadSafetyTest, SingleThreadedValidation)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    auto status = config.Set<settings::ValidatedPort>(0);
    EXPECT_FALSE(status.ok());

    status = config.Set<settings::ValidatedPort>(70000);
    EXPECT_FALSE(status.ok());

    status = config.Set<settings::ValidatedPort>(443);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(config.Get<settings::ValidatedPort>(), 443);
}

TEST_F(ThreadSafetyTest, SingleThreadedDiffAndValidateAll)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    config.Set<settings::Counter>(99);

    auto diff = config.Diff();
    EXPECT_TRUE(diff.HasDifferences());

    auto validate_status = config.ValidateAll();
    EXPECT_TRUE(validate_status.ok());
}

// ---------------------------------------------------------------------------
// Concurrent reads — must never crash or return corrupt data.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentReads)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    config.Set<settings::Counter>(42);
    config.Set<settings::Name>("concurrent");

    constexpr int kNumThreads = 8;
    constexpr int kReadsPerThread = 10'000;

    std::latch start_latch(kNumThreads);
    std::vector<std::thread> threads;
    std::atomic<int> error_count{0};

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&] {
            start_latch.arrive_and_wait();
            for (int i = 0; i < kReadsPerThread; ++i) {
                int counter = config.Get<settings::Counter>();
                if (counter != 42) {
                    error_count.fetch_add(1, std::memory_order_relaxed);
                }
                std::string name = config.Get<settings::Name>();
                if (name != "concurrent") {
                    error_count.fetch_add(1, std::memory_order_relaxed);
                }
                double ratio = config.Get<settings::Ratio>();
                if (ratio != 1.0) {
                    error_count.fetch_add(1, std::memory_order_relaxed);
                }
                bool enabled = config.Get<settings::Enabled>();
                if (!enabled) {
                    error_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(error_count.load(), 0) << "Concurrent reads returned unexpected values";
}

// ---------------------------------------------------------------------------
// Concurrent reads + writes — must never crash. Reads should always return a
// value that was either the old or the new one (no torn reads).
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentReadsAndWrites)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    config.Set<settings::Counter>(0);

    constexpr int kNumReaders = 6;
    constexpr int kNumWriters = 2;
    constexpr int kOpsPerThread = 5'000;

    std::latch start_latch(kNumReaders + kNumWriters);
    std::vector<std::thread> threads;
    std::atomic<int> torn_read_count{0};

    // Writers increment the counter
    for (int w = 0; w < kNumWriters; ++w) {
        threads.emplace_back([&, w] {
            start_latch.arrive_and_wait();
            for (int i = 0; i < kOpsPerThread; ++i) {
                int value = w * kOpsPerThread + i;
                auto status = config.Set<settings::Counter>(value);
                // All values we write are non-negative ints — must always pass validation
                if (!status.ok()) {
                    // We don't use ASSERT in threads, just count.
                    torn_read_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // Readers check that we always get a valid int (not garbage)
    for (int r = 0; r < kNumReaders; ++r) {
        threads.emplace_back([&] {
            start_latch.arrive_and_wait();
            for (int i = 0; i < kOpsPerThread; ++i) {
                int counter = config.Get<settings::Counter>();
                // The counter must be a non-negative integer set by one of the writers
                if (counter < 0) {
                    torn_read_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(torn_read_count.load(), 0) << "Detected torn reads or failed writes";
}

// ---------------------------------------------------------------------------
// Concurrent writes to different settings — must not crash.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentWritesToDifferentSettings)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    constexpr int kIters = 5'000;

    std::latch start_latch(4);
    std::vector<std::thread> threads;

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::Counter>(i);
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::Name>("value_" + std::to_string(i));
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::Ratio>(static_cast<double>(i) / 100.0);
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::Enabled>(i % 2 == 0);
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    // After all writes complete, each setting should hold the last value written.
    EXPECT_EQ(config.Get<settings::Counter>(), kIters - 1);
    EXPECT_EQ(config.Get<settings::Name>(), "value_" + std::to_string(kIters - 1));
    EXPECT_DOUBLE_EQ(config.Get<settings::Ratio>(), static_cast<double>(kIters - 1) / 100.0);
    EXPECT_EQ(config.Get<settings::Enabled>(), (kIters - 1) % 2 == 0);
}

// ---------------------------------------------------------------------------
// Concurrent Diff / ValidateAll with writes — must not crash.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentDiffAndValidateAllWithWrites)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    constexpr int kIters = 2'000;

    std::latch start_latch(3);
    std::vector<std::thread> threads;

    // Writer
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::Counter>(i);
            config.Set<settings::ValidatedPort>(1024 + (i % 60000));
        }
    });

    // Diff reader
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto diff = config.Diff();
            // Just exercise the method; no specific value guarantee while writing
            (void)diff.HasDifferences();
            (void)diff.ToString();
        }
    });

    // ValidateAll reader
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto status = config.ValidateAll();
            // All values written are valid, so validation should always pass
            (void)status.ok();
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    // Smoke check — final state should be valid
    EXPECT_TRUE(config.ValidateAll().ok());
}

// ---------------------------------------------------------------------------
// Concurrent Load and Save — must not crash or corrupt state.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentLoadAndSave)
{
    // Create a valid initial file
    {
        ThreadSafeConfig config(file_path_);
        ASSERT_TRUE(config.Load().ok());
        config.Set<settings::Counter>(100);
        ASSERT_TRUE(config.Save().ok());
    }

    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    constexpr int kIters = 500;

    std::latch start_latch(3);
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    // Saver
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto status = config.Save();
            if (!status.ok()) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Writer + saver
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::Counter>(i);
            auto status = config.Save();
            if (!status.ok()) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Reader
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            int val = config.Get<settings::Counter>();
            if (val < 0) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// ---------------------------------------------------------------------------
// Concurrent Load (reload from disk) — must not crash.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentReload)
{
    {
        ThreadSafeConfig config(file_path_);
        ASSERT_TRUE(config.Load().ok());
        config.Set<settings::Counter>(55);
        ASSERT_TRUE(config.Save().ok());
    }

    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    constexpr int kIters = 500;

    std::latch start_latch(3);
    std::vector<std::thread> threads;

    // Multiple loaders
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&] {
            start_latch.arrive_and_wait();
            for (int i = 0; i < kIters; ++i) {
                auto status = config.Load();
                (void)status;
            }
        });
    }

    // Reader while loads are happening
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            int val = config.Get<settings::Counter>();
            (void)val;
            std::string name = config.Get<settings::Name>();
            (void)name;
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    // Should still be in a consistent state
    EXPECT_EQ(config.Get<settings::Counter>(), 55);
}

// ---------------------------------------------------------------------------
// Validation rejection under concurrency — rejected Sets must not mutate state.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentValidationRejection)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    config.Set<settings::ValidatedPort>(8080);

    constexpr int kIters = 5'000;

    std::latch start_latch(3);
    std::vector<std::thread> threads;
    std::atomic<int> unexpected_values{0};

    // Writer that always gets rejected (port 0 is invalid)
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto status = config.Set<settings::ValidatedPort>(0);
            if (status.ok()) {
                unexpected_values.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Writer that always gets rejected (port 70000 is invalid)
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto status = config.Set<settings::ValidatedPort>(70000);
            if (status.ok()) {
                unexpected_values.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    // Reader — port must always remain 8080 since all writes are rejected
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            int port = config.Get<settings::ValidatedPort>();
            if (port != 8080) {
                unexpected_values.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(unexpected_values.load(), 0);
    EXPECT_EQ(config.Get<settings::ValidatedPort>(), 8080);
}

// ---------------------------------------------------------------------------
// GetFilePath is safe without locking (immutable after construction).
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentGetFilePath)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    constexpr int kNumThreads = 8;
    constexpr int kIters = 10'000;

    std::latch start_latch(kNumThreads);
    std::vector<std::thread> threads;
    std::atomic<int> mismatches{0};

    for (int t = 0; t < kNumThreads; ++t) {
        threads.emplace_back([&] {
            start_latch.arrive_and_wait();
            for (int i = 0; i < kIters; ++i) {
                auto path = config.GetFilePath();
                if (path != file_path_) {
                    mismatches.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(mismatches.load(), 0);
}

// ---------------------------------------------------------------------------
// Virtual interface concurrent access — exercise the virtual overrides.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, ConcurrentVirtualInterface)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    config.Set<settings::Counter>(7);

    IConfigurationProviderVirtual& vconfig = config;

    constexpr int kIters = 1'000;

    std::latch start_latch(3);
    std::vector<std::thread> threads;

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto status = vconfig.ValidateAll();
            (void)status;
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto diff_str = vconfig.GetDiffString();
            (void)diff_str;
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto path = vconfig.GetFilePath();
            (void)path;
        }
    });

    for (auto& th : threads) {
        th.join();
    }
}

// ---------------------------------------------------------------------------
// Stress test: all operations mixed concurrently.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, StressAllOperationsMixed)
{
    {
        ThreadSafeConfig init_config(file_path_);
        ASSERT_TRUE(init_config.Load().ok());
        ASSERT_TRUE(init_config.Save().ok());
    }

    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    constexpr int kIters = 1'000;
    constexpr int kNumThreads = 8;

    std::latch start_latch(kNumThreads);
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    // Thread 0-1: Set Counter
    for (int t = 0; t < 2; ++t) {
        threads.emplace_back([&, t] {
            start_latch.arrive_and_wait();
            for (int i = 0; i < kIters; ++i) {
                config.Set<settings::Counter>(t * kIters + i);
            }
        });
    }

    // Thread 2: Set Name
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::Name>("stress_" + std::to_string(i));
        }
    });

    // Thread 3: Set ValidatedPort (valid values only)
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            config.Set<settings::ValidatedPort>(1024 + (i % 60000));
        }
    });

    // Thread 4: Readers
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            (void)config.Get<settings::Counter>();
            (void)config.Get<settings::Name>();
            (void)config.Get<settings::ValidatedPort>();
            (void)config.Get<settings::Ratio>();
            (void)config.Get<settings::Enabled>();
        }
    });

    // Thread 5: Diff
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto diff = config.Diff();
            (void)diff.ToString();
        }
    });

    // Thread 6: ValidateAll
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto status = config.ValidateAll();
            (void)status;
        }
    });

    // Thread 7: Save
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            auto status = config.Save();
            if (!status.ok()) {
                errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(errors.load(), 0);

    // Final state must be readable and valid
    EXPECT_TRUE(config.ValidateAll().ok());
    EXPECT_TRUE(config.Save().ok());
}

// ---------------------------------------------------------------------------
// Verify SingleThreadedPolicy compiles and works (no overhead sanity check).
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, SingleThreadedPolicyCompiles)
{
    Configuration<TestSchema, JsonSerializer, SingleThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    config.Set<settings::Counter>(123);
    EXPECT_EQ(config.Get<settings::Counter>(), 123);

    auto diff = config.Diff();
    EXPECT_TRUE(diff.HasDifferences());

    EXPECT_TRUE(config.ValidateAll().ok());
    EXPECT_TRUE(config.Save().ok());
}

// ---------------------------------------------------------------------------
// Default template argument is SingleThreadedPolicy.
// ---------------------------------------------------------------------------

TEST_F(ThreadSafetyTest, DefaultPolicyIsSingleThreaded)
{
    // Configuration<TestSchema> should compile and work (uses SingleThreadedPolicy)
    Configuration<TestSchema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    config.Set<settings::Counter>(999);
    EXPECT_EQ(config.Get<settings::Counter>(), 999);
    EXPECT_TRUE(config.Save().ok());
}

}  // namespace cppfig::test