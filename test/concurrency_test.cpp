#include <cppfig/cppfig.h>
#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <thread>

#include "test_settings.h"

namespace cppfig::test {
namespace {

    TEST(ConcurrencyTest, FrozenConfigurationSupportsConcurrentReads)
    {
        auto loaded = TestConfiguration::FromDefaults();
        ASSERT_TRUE(loaded.has_value());
        const auto& config = loaded.value();
        constexpr std::size_t thread_count = 8;
        constexpr int iterations = 10000;

        std::atomic<bool> valid = true;
        std::array<std::thread, thread_count> threads;
        for (auto& thread : threads) {
            thread = std::thread([&] {
                const auto* name = std::addressof(config.Get<settings::AppName>());
                for (int i = 0; i < iterations; ++i) {
                    if (config.Get<settings::ServerPort>() != 8080 || config.Get<settings::AppName>() != "MyApplication"
                        || std::addressof(config.Get<settings::AppName>()) != name) {
                        valid = false;
                    }
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        EXPECT_TRUE(valid);
    }

    TEST(ConcurrencyTest, SavingDoesNotChangeTypedValuesOrReferences)
    {
        const TempDirectory directory;
        auto loaded = TestConfiguration::Load(directory.file("app.conf").string());
        ASSERT_TRUE(loaded.has_value());
        const auto& config = loaded->config;
        const auto& document = loaded->document;
        const auto* host = std::addressof(config.Get<settings::ServerHost>());

        std::atomic<bool> valid = true;
        std::atomic<bool> done = false;
        std::thread reader([&] {
            while (!done) {
                if (config.Get<settings::ServerHost>() != "localhost" || std::addressof(config.Get<settings::ServerHost>()) != host) {
                    valid = false;
                }
            }
        });
        for (int i = 0; i < 20; ++i) {
            if (!document.Save().ok()) {
                valid = false;
            }
        }
        done = true;
        reader.join();
        EXPECT_TRUE(valid);
    }

}  // namespace
}  // namespace cppfig::test
