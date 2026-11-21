// Workloads equivalent to the pre-redesign suite recorded in
// docs/BASELINE_RESULTS.md: the same settings, registry sizes (1, 5 and 14
// settings) and flat .conf files, so results can be compared directly.
#include <benchmark/benchmark.h>
#include <cppfig/cppfig.h>

#include <cstdint>
#include <filesystem>
#include <numbers>
#include <string>

#include "barrier.h"

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

    struct EnvOverrideSetting {
        static constexpr std::string_view path = "benchmark.env";
        static constexpr std::string_view env_override = "BENCHMARK_ENV_SETTING";
        using value_type = std::string;
        static auto default_value() -> std::string { return "default"; }
    };

    struct DatabaseHost {
        static constexpr std::string_view path = "database.connection.host";
        using value_type = std::string;
        static auto default_value() -> std::string { return "localhost"; }
    };

    struct DatabasePort {
        static constexpr std::string_view path = "database.connection.port";
        using value_type = int;
        static auto default_value() -> int { return 5432; }
    };

    struct DatabaseUser {
        static constexpr std::string_view path = "database.connection.user";
        using value_type = std::string;
        static auto default_value() -> std::string { return "admin"; }
    };

    struct DatabasePass {
        static constexpr std::string_view path = "database.connection.password";
        using value_type = std::string;
        static auto default_value() -> std::string { return "secret"; }
    };

    struct CacheEnabled {
        static constexpr std::string_view path = "cache.enabled";
        using value_type = bool;
        static auto default_value() -> bool { return true; }
    };

    struct CacheSize {
        static constexpr std::string_view path = "cache.size_mb";
        using value_type = int;
        static auto default_value() -> int { return 128; }
    };

    struct LogLevel {
        static constexpr std::string_view path = "logging.level";
        using value_type = std::string;
        static auto default_value() -> std::string { return "info"; }
    };

    struct LogPath {
        static constexpr std::string_view path = "logging.path";
        using value_type = std::string;
        static auto default_value() -> std::string { return "/var/log/app.log"; }
    };

}  // namespace settings

struct SmallRegistry : Registry<settings::StringSetting> { };

struct MediumRegistry : Registry<settings::StringSetting, settings::IntSetting, settings::DoubleSetting, settings::BoolSetting,
                                 settings::ValidatedSetting> { };

struct LargeRegistry : Registry<settings::StringSetting, settings::IntSetting, settings::DoubleSetting, settings::BoolSetting,
                                settings::ValidatedSetting, settings::EnvOverrideSetting, settings::DatabaseHost,
                                settings::DatabasePort, settings::DatabaseUser, settings::DatabasePass, settings::CacheEnabled,
                                settings::CacheSize, settings::LogLevel, settings::LogPath> { };

namespace {

    /// A file path unique to one benchmark, removed when the benchmark ends.
    class TemporaryFile {
    public:
        explicit TemporaryFile(std::string_view name)
            : path_((std::filesystem::temp_directory_path() / ("cppfig_benchmark_" + std::string(name) + ".conf")).string())
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

    /// Reads a setting the way callers of the pre-redesign API did: as a copy.
    /// The configuration's address escapes before the loop and KeepValue
    /// clobbers memory, so every iteration loads the value again.
    template <typename S>
    void ReadCopy(benchmark::State& state)
    {
        auto loaded = Configuration<MediumRegistry>::FromDefaults();
        const auto& config = loaded.value();
        const auto* escaped = &config;
        KeepValue(escaped);
        for (auto _ : state) {
            auto value = config.Get<S>();
            KeepValue(value);
        }
    }

}  // namespace

static void GetString(benchmark::State& state) { ReadCopy<settings::StringSetting>(state); }
BENCHMARK(GetString);

static void GetInt(benchmark::State& state) { ReadCopy<settings::IntSetting>(state); }
BENCHMARK(GetInt);

static void GetDouble(benchmark::State& state) { ReadCopy<settings::DoubleSetting>(state); }
BENCHMARK(GetDouble);

static void GetBool(benchmark::State& state) { ReadCopy<settings::BoolSetting>(state); }
BENCHMARK(GetBool);

static void GetValidated(benchmark::State& state) { ReadCopy<settings::ValidatedSetting>(state); }
BENCHMARK(GetValidated);

/// Reading through the returned reference, without copying the string.
static void GetStringReference(benchmark::State& state)
{
    auto loaded = Configuration<MediumRegistry>::FromDefaults();
    const auto& config = loaded.value();
    const auto* escaped = &config;
    KeepValue(escaped);
    for (auto _ : state) {
        const auto& value = config.Get<settings::StringSetting>();
        const auto* data = value.data();
        auto size = value.size();
        KeepValue(data);
        KeepValue(size);
    }
}
BENCHMARK(GetStringReference);

/// Loads a complete, existing file, as the pre-redesign Load benchmarks did.
template <typename RegistryType>
static void LoadExisting(benchmark::State& state)
{
    const TemporaryFile file("load");
    {
        auto created = cppfig::LoadFile<RegistryType>(file.path());
        if (!created.has_value() || !cppfig::SaveFile(file.path(), created->document).ok()) {
            state.SkipWithError("could not create the configuration file");
            return;
        }
    }
    for (auto _ : state) {
        auto loaded = cppfig::LoadFile<RegistryType>(file.path());
        benchmark::DoNotOptimize(loaded);
    }
}
BENCHMARK(LoadExisting<SmallRegistry>)->Name("LoadSmallRegistry");
BENCHMARK(LoadExisting<MediumRegistry>)->Name("LoadMediumRegistry");
BENCHMARK(LoadExisting<LargeRegistry>)->Name("LoadLargeRegistry");

/// Saves a loaded document. Each save writes a temporary file, fsyncs it,
/// renames it over the destination and fsyncs the directory; the pre-redesign
/// save did not flush to disk, so most of the difference is durability.
template <typename RegistryType>
static void SaveDocument(benchmark::State& state)
{
    const TemporaryFile file("save");
    auto loaded = cppfig::LoadFile<RegistryType>(file.path());
    if (!loaded.has_value()) {
        state.SkipWithError("could not load the configuration");
        return;
    }
    for (auto _ : state) {
        auto status = cppfig::SaveFile(file.path(), loaded->document);
        benchmark::DoNotOptimize(status);
    }
}
BENCHMARK(SaveDocument<SmallRegistry>)->Name("SaveSmallRegistry")->UseRealTime();
BENCHMARK(SaveDocument<MediumRegistry>)->Name("SaveMediumRegistry")->UseRealTime();
BENCHMARK(SaveDocument<LargeRegistry>)->Name("SaveLargeRegistry")->UseRealTime();

}  // namespace cppfig::bench
