#include <benchmark/benchmark.h>
#include <cppfig/cppfig.h>
#include <cppfig/json.h>

#include <cstdlib>
#include <filesystem>

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
        static auto default_value() -> double { return 3.14159; }
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
        static auto validator() -> Validator<int> { return Range(0, 100); }
    };

    struct EnvOverrideSetting {
        static constexpr std::string_view path = "benchmark.env";
        static constexpr std::string_view env_override = "BENCHMARK_ENV_SETTING";
        using value_type = std::string;
        static auto default_value() -> std::string { return "default"; }
    };

    // Hierarchical settings
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

using SmallSchema = ConfigSchema<settings::StringSetting>;

using MediumSchema = ConfigSchema<settings::StringSetting, settings::IntSetting, settings::DoubleSetting,
                                  settings::BoolSetting, settings::ValidatedSetting>;

using LargeSchema = ConfigSchema<settings::StringSetting, settings::IntSetting, settings::DoubleSetting,
                                 settings::BoolSetting, settings::ValidatedSetting, settings::EnvOverrideSetting,
                                 settings::DatabaseHost, settings::DatabasePort, settings::DatabaseUser,
                                 settings::DatabasePass, settings::CacheEnabled, settings::CacheSize,
                                 settings::LogLevel, settings::LogPath>;

class BenchmarkFixture : public ::benchmark::Fixture {
protected:
    std::string CreateTempFile()
    {
        std::filesystem::path temp_path = std::filesystem::temp_directory_path() / "cppfig_benchmark";
        temp_path += std::to_string(reinterpret_cast<uintptr_t>(this));
        temp_path += ".conf";
        return temp_path.string();
    }

    void RemoveFile(const std::string& path)
    {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
    }
};

BENCHMARK_DEFINE_F(BenchmarkFixture, GetString)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<SmallSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto value = config.Get<settings::StringSetting>();
        benchmark::DoNotOptimize(value);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, GetString);

BENCHMARK_DEFINE_F(BenchmarkFixture, GetInt)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto value = config.Get<settings::IntSetting>();
        benchmark::DoNotOptimize(value);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, GetInt);

BENCHMARK_DEFINE_F(BenchmarkFixture, GetDouble)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto value = config.Get<settings::DoubleSetting>();
        benchmark::DoNotOptimize(value);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, GetDouble);

BENCHMARK_DEFINE_F(BenchmarkFixture, GetBool)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto value = config.Get<settings::BoolSetting>();
        benchmark::DoNotOptimize(value);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, GetBool);

BENCHMARK_DEFINE_F(BenchmarkFixture, GetValidated)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto value = config.Get<settings::ValidatedSetting>();
        benchmark::DoNotOptimize(value);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, GetValidated);

BENCHMARK_DEFINE_F(BenchmarkFixture, SetString)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<SmallSchema> config(path);
    (void)config.Load();

    int counter = 0;
    for (auto _ : state) {
        std::string value = "value_" + std::to_string(counter++);
        auto status = config.Set<settings::StringSetting>(value);
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, SetString);

BENCHMARK_DEFINE_F(BenchmarkFixture, SetInt)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    int counter = 0;
    for (auto _ : state) {
        auto status = config.Set<settings::IntSetting>(counter++);
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, SetInt);

BENCHMARK_DEFINE_F(BenchmarkFixture, SetValidated)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto status = config.Set<settings::ValidatedSetting>(50);
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, SetValidated);

BENCHMARK_DEFINE_F(BenchmarkFixture, LoadSmallSchema)(benchmark::State& state)
{
    std::string path = CreateTempFile();

    // Create initial file
    {
        Configuration<SmallSchema> config(path);
        (void)config.Load();
        (void)config.Save();
    }

    for (auto _ : state) {
        Configuration<SmallSchema> config(path);
        auto status = config.Load();
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, LoadSmallSchema);

BENCHMARK_DEFINE_F(BenchmarkFixture, LoadMediumSchema)(benchmark::State& state)
{
    std::string path = CreateTempFile();

    // Create initial file
    {
        Configuration<MediumSchema> config(path);
        (void)config.Load();
        (void)config.Save();
    }

    for (auto _ : state) {
        Configuration<MediumSchema> config(path);
        auto status = config.Load();
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, LoadMediumSchema);

BENCHMARK_DEFINE_F(BenchmarkFixture, LoadLargeSchema)(benchmark::State& state)
{
    std::string path = CreateTempFile();

    // Create initial file
    {
        Configuration<LargeSchema> config(path);
        (void)config.Load();
        (void)config.Save();
    }

    for (auto _ : state) {
        Configuration<LargeSchema> config(path);
        auto status = config.Load();
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, LoadLargeSchema);

BENCHMARK_DEFINE_F(BenchmarkFixture, SaveSmallSchema)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<SmallSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto status = config.Save();
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, SaveSmallSchema);

BENCHMARK_DEFINE_F(BenchmarkFixture, SaveMediumSchema)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto status = config.Save();
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, SaveMediumSchema);

BENCHMARK_DEFINE_F(BenchmarkFixture, SaveLargeSchema)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<LargeSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto status = config.Save();
        benchmark::DoNotOptimize(status);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, SaveLargeSchema);

BENCHMARK_DEFINE_F(BenchmarkFixture, ValidatorRange)(benchmark::State& state)
{
    auto range_validator = cppfig::Range(0, 100);

    for (auto _ : state) {
        auto result = range_validator(50);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, ValidatorRange);

BENCHMARK_DEFINE_F(BenchmarkFixture, ValidatorNotEmpty)(benchmark::State& state)
{
    auto not_empty_validator = cppfig::NotEmpty();
    std::string test_value = "test";

    for (auto _ : state) {
        auto result = not_empty_validator(test_value);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, ValidatorNotEmpty);

BENCHMARK_DEFINE_F(BenchmarkFixture, ValidatorOneOf)(benchmark::State& state)
{
    auto one_of_validator = cppfig::OneOf<std::string>({ "debug", "info", "warn", "error" });

    for (auto _ : state) {
        auto result = one_of_validator("info");
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, ValidatorOneOf);

BENCHMARK_DEFINE_F(BenchmarkFixture, ValidatorCombined)(benchmark::State& state)
{
    auto combined_validator = cppfig::Min(0).And(cppfig::Max(100)).And(cppfig::Predicate<int>([](int v) { return v % 2 == 0; }, "must be even"));

    for (auto _ : state) {
        auto result = combined_validator(50);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, ValidatorCombined);

BENCHMARK_DEFINE_F(BenchmarkFixture, DiffNoChanges)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    for (auto _ : state) {
        auto diff = config.Diff();
        benchmark::DoNotOptimize(diff);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, DiffNoChanges);

BENCHMARK_DEFINE_F(BenchmarkFixture, DiffWithChanges)(benchmark::State& state)
{
    std::string path = CreateTempFile();
    Configuration<MediumSchema> config(path);
    (void)config.Load();

    // Make some changes
    (void)config.Set<settings::IntSetting>(999);
    (void)config.Set<settings::StringSetting>("modified");

    for (auto _ : state) {
        auto diff = config.Diff();
        benchmark::DoNotOptimize(diff);
    }

    RemoveFile(path);
}
BENCHMARK_REGISTER_F(BenchmarkFixture, DiffWithChanges);

BENCHMARK_DEFINE_F(BenchmarkFixture, JsonSerializerParse)(benchmark::State& state)
{
    std::string json_str = R"({
        "benchmark": {
            "string": "test",
            "int": 42,
            "double": 3.14159,
            "bool": true,
            "validated": 50
        }
    })";

    for (auto _ : state) {
        auto result = JsonSerializer::ParseString(json_str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, JsonSerializerParse);

BENCHMARK_DEFINE_F(BenchmarkFixture, JsonSerializerStringify)(benchmark::State& state)
{
    auto value = Value::Object();
    auto benchmark_obj = Value::Object();
    benchmark_obj["string"] = Value("test");
    benchmark_obj["int"] = Value(42);
    benchmark_obj["double"] = Value(3.14159);
    benchmark_obj["bool"] = Value(true);
    benchmark_obj["validated"] = Value(50);
    value["benchmark"] = benchmark_obj;

    for (auto _ : state) {
        auto result = JsonSerializer::Stringify(value);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, JsonSerializerStringify);

BENCHMARK_DEFINE_F(BenchmarkFixture, ValueGetAtPath)(benchmark::State& state)
{
    auto value = Value::Object();
    auto database = Value::Object();
    auto connection = Value::Object();
    connection["host"] = Value("localhost");
    connection["port"] = Value(5432);
    database["connection"] = connection;
    value["database"] = database;
    std::string path = "database.connection.host";

    for (auto _ : state) {
        auto result = value.GetAtPath(path);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, ValueGetAtPath);

BENCHMARK_DEFINE_F(BenchmarkFixture, ValueSetAtPath)(benchmark::State& state)
{
    auto value = Value::Object();
    auto database = Value::Object();
    auto connection = Value::Object();
    connection["host"] = Value("localhost");
    connection["port"] = Value(5432);
    database["connection"] = connection;
    value["database"] = database;
    std::string path = "database.connection.host";

    for (auto _ : state) {
        Value value_copy = value;
        value_copy.SetAtPath(path, Value("example.com"));
        benchmark::DoNotOptimize(value_copy);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, ValueSetAtPath);

BENCHMARK_DEFINE_F(BenchmarkFixture, TraitsSerializeInt)(benchmark::State& state)
{
    for (auto _ : state) {
        auto val = ConfigTraits<int>::Serialize(42);
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, TraitsSerializeInt);

BENCHMARK_DEFINE_F(BenchmarkFixture, TraitsDeserializeInt)(benchmark::State& state)
{
    Value val(42);

    for (auto _ : state) {
        auto result = ConfigTraits<int>::Deserialize(val);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, TraitsDeserializeInt);

BENCHMARK_DEFINE_F(BenchmarkFixture, TraitsSerializeString)(benchmark::State& state)
{
    std::string value = "benchmark_test_string";

    for (auto _ : state) {
        auto val = ConfigTraits<std::string>::Serialize(value);
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, TraitsSerializeString);

BENCHMARK_DEFINE_F(BenchmarkFixture, TraitsDeserializeString)(benchmark::State& state)
{
    Value val("benchmark_test_string");

    for (auto _ : state) {
        auto result = ConfigTraits<std::string>::Deserialize(val);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK_REGISTER_F(BenchmarkFixture, TraitsDeserializeString);

}  // namespace cppfig::bench

BENCHMARK_MAIN();
