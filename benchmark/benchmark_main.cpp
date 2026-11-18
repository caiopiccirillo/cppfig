#include <benchmark/benchmark.h>
#include <cppfig/cppfig.h>

#include <filesystem>
#include <numbers>
#include <string>

namespace cppfig::bench {
namespace settings {

    struct StringSetting {
        static constexpr std::string_view path = "benchmark.string";
        using value_type = std::string;
        static auto default_value() -> std::string { return "benchmark_value"; }
    };

    struct IntSetting {
        static constexpr std::string_view path = "benchmark.int";
        using value_type = int;
        static auto default_value() -> int { return 42; }
    };

    struct DoubleSetting {
        static constexpr std::string_view path = "benchmark.double";
        using value_type = double;
        static auto default_value() -> double { return std::numbers::pi; }
    };

    struct BoolSetting {
        static constexpr std::string_view path = "benchmark.bool";
        using value_type = bool;
        static auto default_value() -> bool { return true; }
    };

    struct ValidatedSetting {
        static constexpr std::string_view path = "benchmark.validated";
        using value_type = int;
        static auto default_value() -> int { return 50; }
        static constexpr auto validator() { return Range(0, 100); }
    };

}  // namespace settings

using BenchmarkRegistry = Registry<settings::StringSetting, settings::IntSetting, settings::DoubleSetting,
                                   settings::BoolSetting, settings::ValidatedSetting>;
using BenchmarkConfiguration = Configuration<BenchmarkRegistry>;

static void GetString(benchmark::State& state)
{
    auto loaded = BenchmarkConfiguration::FromDefaults();
    const auto& config = loaded.value();
    for (auto _ : state) {
        const auto& value = config.Get<settings::StringSetting>();
        benchmark::DoNotOptimize(&value);
    }
}
BENCHMARK(GetString);

static void GetInt(benchmark::State& state)
{
    auto loaded = BenchmarkConfiguration::FromDefaults();
    const auto& config = loaded.value();
    for (auto _ : state) {
        const auto& value = config.Get<settings::IntSetting>();
        benchmark::DoNotOptimize(&value);
    }
}
BENCHMARK(GetInt);

static void GetDouble(benchmark::State& state)
{
    auto loaded = BenchmarkConfiguration::FromDefaults();
    const auto& config = loaded.value();
    for (auto _ : state) {
        const auto& value = config.Get<settings::DoubleSetting>();
        benchmark::DoNotOptimize(&value);
    }
}
BENCHMARK(GetDouble);

static void GetBool(benchmark::State& state)
{
    auto loaded = BenchmarkConfiguration::FromDefaults();
    const auto& config = loaded.value();
    for (auto _ : state) {
        const auto& value = config.Get<settings::BoolSetting>();
        benchmark::DoNotOptimize(&value);
    }
}
BENCHMARK(GetBool);

static void GetValidated(benchmark::State& state)
{
    auto loaded = BenchmarkConfiguration::FromDefaults();
    const auto& config = loaded.value();
    for (auto _ : state) {
        const auto& value = config.Get<settings::ValidatedSetting>();
        benchmark::DoNotOptimize(&value);
    }
}
BENCHMARK(GetValidated);

static void LoadAndSave(benchmark::State& state)
{
    const auto path = std::filesystem::temp_directory_path() / "cppfig_typed_benchmark.conf";
    for (auto _ : state) {
        auto loaded = BenchmarkConfiguration::Load(path.string());
        benchmark::DoNotOptimize(loaded);
        if (loaded.ok()) {
            benchmark::DoNotOptimize(loaded->Save());
        }
    }
    std::error_code error;
    std::filesystem::remove(path, error);
}
BENCHMARK(LoadAndSave);

}  // namespace cppfig::bench
