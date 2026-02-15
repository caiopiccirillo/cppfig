#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <latch>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace cppfig::test {

namespace settings {

struct Counter {
    static constexpr std::string_view path = "app.counter";
    using value_type = int;
    static auto default_value() -> int { return 0; }
};

struct Name {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "default"; }
};

struct Ratio {
    static constexpr std::string_view path = "app.ratio";
    using value_type = double;
    static auto default_value() -> double { return 1.0; }
};

struct Enabled {
    static constexpr std::string_view path = "app.enabled";
    using value_type = bool;
    static auto default_value() -> bool { return true; }
};

struct ValidatedPort {
    static constexpr std::string_view path = "server.port";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
    static auto validator() -> cppfig::Validator<int> { return cppfig::Range(1, 65535); }
};

struct HostWithEnv {
    static constexpr std::string_view path = "server.host";
    static constexpr std::string_view env_override = "CPPFIG_TEST_HOST";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};

}  // namespace settings

using TestSchema = ConfigSchema<settings::Counter, settings::Name, settings::Ratio,
                                settings::Enabled, settings::ValidatedPort, settings::HostWithEnv>;

using ThreadSafeConfig = Configuration<TestSchema, JsonSerializer, MultiThreadedPolicy>;

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        file_path_ = testing::ConfigurationTestFixture::CreateTempFilePath("thread_safety_test");
    }

    void TearDown() override { testing::ConfigurationTestFixture::RemoveFile(file_path_); }

    std::string file_path_;
};

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

    (void)config.Set<settings::Counter>(99);

    auto diff = config.Diff();
    EXPECT_TRUE(diff.HasDifferences());

    auto validate_status = config.ValidateAll();
    EXPECT_TRUE(validate_status.ok());
}

TEST_F(ThreadSafetyTest, ConcurrentReads)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    (void)config.Set<settings::Counter>(42);
    (void)config.Set<settings::Name>("concurrent");

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

TEST_F(ThreadSafetyTest, ConcurrentReadsAndWrites)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    (void)config.Set<settings::Counter>(0);

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
            (void)config.Set<settings::Counter>(i);
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            (void)config.Set<settings::Name>("value_" + std::to_string(i));
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            (void)config.Set<settings::Ratio>(static_cast<double>(i) / 100.0);
        }
    });

    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            (void)config.Set<settings::Enabled>(i % 2 == 0);
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
            (void)config.Set<settings::Counter>(i);
            (void)config.Set<settings::ValidatedPort>(1024 + (i % 60000));
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

TEST_F(ThreadSafetyTest, ConcurrentLoadAndSave)
{
    // Create a valid initial file
    {
        ThreadSafeConfig config(file_path_);
        ASSERT_TRUE(config.Load().ok());
        (void)config.Set<settings::Counter>(100);
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
            (void)config.Set<settings::Counter>(i);
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

TEST_F(ThreadSafetyTest, ConcurrentReload)
{
    {
        ThreadSafeConfig config(file_path_);
        ASSERT_TRUE(config.Load().ok());
        (void)config.Set<settings::Counter>(55);
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

TEST_F(ThreadSafetyTest, ConcurrentValidationRejection)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    (void)config.Set<settings::ValidatedPort>(8080);

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

TEST_F(ThreadSafetyTest, ConcurrentVirtualInterface)
{
    ThreadSafeConfig config(file_path_);
    ASSERT_TRUE(config.Load().ok());
    (void)config.Set<settings::Counter>(7);

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
                (void)config.Set<settings::Counter>(t * kIters + i);
            }
        });
    }

    // Thread 2: Set Name
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            (void)config.Set<settings::Name>("stress_" + std::to_string(i));
        }
    });

    // Thread 3: Set ValidatedPort (valid values only)
    threads.emplace_back([&] {
        start_latch.arrive_and_wait();
        for (int i = 0; i < kIters; ++i) {
            (void)config.Set<settings::ValidatedPort>(1024 + (i % 60000));
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

TEST_F(ThreadSafetyTest, SingleThreadedPolicyCompiles)
{
    Configuration<TestSchema, JsonSerializer, SingleThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    (void)config.Set<settings::Counter>(123);
    EXPECT_EQ(config.Get<settings::Counter>(), 123);

    auto diff = config.Diff();
    EXPECT_TRUE(diff.HasDifferences());

    EXPECT_TRUE(config.ValidateAll().ok());
    EXPECT_TRUE(config.Save().ok());
}

TEST_F(ThreadSafetyTest, DefaultPolicyIsSingleThreaded)
{
    // Configuration<TestSchema> should compile and work (uses SingleThreadedPolicy)
    Configuration<TestSchema> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    (void)config.Set<settings::Counter>(999);
    EXPECT_EQ(config.Get<settings::Counter>(), 999);
    EXPECT_TRUE(config.Save().ok());
}

namespace edge_settings {

struct PortWithEnv {
    static constexpr std::string_view path = "server.port";
    static constexpr std::string_view env_override = "CPPFIG_EDGE_PORT";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
};

struct HostWithEnv {
    static constexpr std::string_view path = "server.host";
    static constexpr std::string_view env_override = "CPPFIG_EDGE_HOST";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};

struct AppName {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "EdgeApp"; }
};

struct AppPort {
    static constexpr std::string_view path = "app.port";
    using value_type = int;
    static auto default_value() -> int { return 3000; }
};

struct AppVersion {
    static constexpr std::string_view path = "app.version";
    using value_type = std::string;
    static auto default_value() -> std::string { return "1.0.0"; }
};

struct ValidatedPort {
    static constexpr std::string_view path = "validated.port";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
    static auto validator() -> cppfig::Validator<int> { return cppfig::Range(1, 65535); }
};

}  // namespace edge_settings

TEST_F(ThreadSafetyTest, EdgeEnvVarSuccessfulParse)
{
    setenv("CPPFIG_EDGE_HOST", "env-host.example.com", 1);

    using Schema = ConfigSchema<edge_settings::HostWithEnv>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    std::string host = config.Get<edge_settings::HostWithEnv>();
    EXPECT_EQ(host, "env-host.example.com");

    unsetenv("CPPFIG_EDGE_HOST");
}

TEST_F(ThreadSafetyTest, EdgeEnvVarParseFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"server": {"port": 9090}})";
    }

    setenv("CPPFIG_EDGE_PORT", "not_a_number", 1);

    using Schema = ConfigSchema<edge_settings::PortWithEnv>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    int port = config.Get<edge_settings::PortWithEnv>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("CPPFIG_EDGE_PORT"), std::string::npos);
    EXPECT_EQ(port, 9090);

    unsetenv("CPPFIG_EDGE_PORT");
}

TEST_F(ThreadSafetyTest, EdgeFileValueParseFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"port": "not_an_int"}})";
    }

    using Schema = ConfigSchema<edge_settings::AppPort>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    ::testing::internal::CaptureStderr();
    int port = config.Get<edge_settings::AppPort>();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_EQ(port, 3000);  // falls back to default
}

TEST_F(ThreadSafetyTest, EdgeDefaultValueFallback)
{
    {
        std::ofstream file(file_path_);
        file << R"({"unrelated": {"key": 1}})";
    }

    // Schema migration will add app.name, but before that it didn't exist.
    // After migration the value equals the default.
    using Schema = ConfigSchema<edge_settings::AppName>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    EXPECT_EQ(config.Get<edge_settings::AppName>(), "EdgeApp");
}

TEST_F(ThreadSafetyTest, EdgeLoadInvalidJsonFile)
{
    {
        std::ofstream file(file_path_);
        file << "this is not valid json {{{";
    }

    using Schema = ConfigSchema<edge_settings::AppName>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);

    auto status = config.Load();
    EXPECT_FALSE(status.ok());
}

TEST_F(ThreadSafetyTest, EdgeSchemaMigration)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "Old"}})";
    }

    using Schema = ConfigSchema<edge_settings::AppName, edge_settings::AppPort, edge_settings::AppVersion>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);

    ::testing::internal::CaptureStderr();
    auto status = config.Load();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    ASSERT_TRUE(status.ok()) << status.message();
    EXPECT_NE(stderr_output.find("WARN"), std::string::npos);
    EXPECT_NE(stderr_output.find("app.port"), std::string::npos);
    EXPECT_NE(stderr_output.find("app.version"), std::string::npos);

    EXPECT_EQ(config.Get<edge_settings::AppName>(), "Old");
    EXPECT_EQ(config.Get<edge_settings::AppPort>(), 3000);
    EXPECT_EQ(config.Get<edge_settings::AppVersion>(), "1.0.0");
}

TEST_F(ThreadSafetyTest, EdgeSchemaMigrationSaveFailure)
{
    {
        std::ofstream file(file_path_);
        file << R"({"app": {"name": "Old"}})";
    }

    // Make the file read-only so Save fails during migration
    std::filesystem::permissions(file_path_,
                                 std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);

    using Schema = ConfigSchema<edge_settings::AppName, edge_settings::AppPort>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);

    ::testing::internal::CaptureStderr();
    auto status = config.Load();
    auto stderr_output = ::testing::internal::GetCapturedStderr();

    EXPECT_FALSE(status.ok());
    EXPECT_NE(stderr_output.find("Failed to save migrated configuration"), std::string::npos);

    // Restore permissions for cleanup
    std::filesystem::permissions(file_path_, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
}

TEST_F(ThreadSafetyTest, EdgeSaveDirectoryCreationFailure)
{
    std::string bad_path = "/proc/fakedir/subdir/config.json";

    using Schema = ConfigSchema<edge_settings::AppName>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(bad_path);

    auto status = config.Save();
    EXPECT_FALSE(status.ok());
}

TEST_F(ThreadSafetyTest, EdgeSaveNoParentPath)
{
    std::string bare = "bare_ts_config_temp.json";

    using Schema = ConfigSchema<edge_settings::AppName>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(bare);
    ASSERT_TRUE(config.Load().ok());
    ASSERT_TRUE(config.Save().ok());

    EXPECT_TRUE(std::filesystem::exists(bare));
    std::filesystem::remove(bare);
}

TEST_F(ThreadSafetyTest, EdgeValidateAllInvalidValue)
{
    {
        std::ofstream file(file_path_);
        file << R"({"validated": {"port": 99999}})";
    }

    using Schema = ConfigSchema<edge_settings::ValidatedPort>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    auto status = config.ValidateAll();
    EXPECT_FALSE(status.ok());
    EXPECT_NE(std::string(status.message()).find("validated.port"), std::string::npos);
}

TEST_F(ThreadSafetyTest, EdgeValidateAllEarlyStop)
{
    {
        std::ofstream file(file_path_);
        // Both values are invalid
        file << R"({"validated": {"port": -1}, "app": {"port": 999}})";
    }

    using Schema = ConfigSchema<edge_settings::ValidatedPort, edge_settings::AppPort>;
    Configuration<Schema, JsonSerializer, MultiThreadedPolicy> config(file_path_);
    ASSERT_TRUE(config.Load().ok());

    // ValidatedPort has a validator and its value (-1) is invalid.
    // AppPort has no validator so it doesn't cause an error.
    auto status = config.ValidateAll();
    EXPECT_FALSE(status.ok());
    EXPECT_NE(std::string(status.message()).find("validated.port"), std::string::npos);
}

}  // namespace cppfig::test
