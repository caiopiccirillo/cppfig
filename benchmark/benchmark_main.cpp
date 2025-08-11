#include <benchmark/benchmark.h>

#include <filesystem>
#include <memory>
#include <random>
#include <string>

#include "cppfig.h"

// Define benchmark configuration enum
namespace benchmark_test {

enum class BenchmarkConfig : uint8_t {
    DatabaseUrl,
    MaxConnections,
    EnableLogging,
    LogLevel,
    ApiTimeout,
    ServerPort,
    DebugMode,
    CacheSize,
    RetryCount,
    CompressionRatio,
    // Additional settings for larger benchmarks
    Setting1,
    Setting2,
    Setting3,
    Setting4,
    Setting5,
    Setting6,
    Setting7,
    Setting8,
    Setting9,
    Setting10,
    Setting11,
    Setting12,
    Setting13,
    Setting14,
    Setting15,
    Setting16,
    Setting17,
    Setting18,
    Setting19,
    Setting20
};

// String conversion functions for JSON serialization
inline std::string ToString(BenchmarkConfig key)
{
    switch (key) {
    case BenchmarkConfig::DatabaseUrl:
        return "database_url";
    case BenchmarkConfig::MaxConnections:
        return "max_connections";
    case BenchmarkConfig::EnableLogging:
        return "enable_logging";
    case BenchmarkConfig::LogLevel:
        return "log_level";
    case BenchmarkConfig::ApiTimeout:
        return "api_timeout";
    case BenchmarkConfig::ServerPort:
        return "server_port";
    case BenchmarkConfig::DebugMode:
        return "debug_mode";
    case BenchmarkConfig::CacheSize:
        return "cache_size";
    case BenchmarkConfig::RetryCount:
        return "retry_count";
    case BenchmarkConfig::CompressionRatio:
        return "compression_ratio";
    case BenchmarkConfig::Setting1:
        return "setting1";
    case BenchmarkConfig::Setting2:
        return "setting2";
    case BenchmarkConfig::Setting3:
        return "setting3";
    case BenchmarkConfig::Setting4:
        return "setting4";
    case BenchmarkConfig::Setting5:
        return "setting5";
    case BenchmarkConfig::Setting6:
        return "setting6";
    case BenchmarkConfig::Setting7:
        return "setting7";
    case BenchmarkConfig::Setting8:
        return "setting8";
    case BenchmarkConfig::Setting9:
        return "setting9";
    case BenchmarkConfig::Setting10:
        return "setting10";
    case BenchmarkConfig::Setting11:
        return "setting11";
    case BenchmarkConfig::Setting12:
        return "setting12";
    case BenchmarkConfig::Setting13:
        return "setting13";
    case BenchmarkConfig::Setting14:
        return "setting14";
    case BenchmarkConfig::Setting15:
        return "setting15";
    case BenchmarkConfig::Setting16:
        return "setting16";
    case BenchmarkConfig::Setting17:
        return "setting17";
    case BenchmarkConfig::Setting18:
        return "setting18";
    case BenchmarkConfig::Setting19:
        return "setting19";
    case BenchmarkConfig::Setting20:
        return "setting20";
    default:
        return "unknown";
    }
}

inline BenchmarkConfig FromString(const std::string& str)
{
    if (str == "database_url")
        return BenchmarkConfig::DatabaseUrl;
    if (str == "max_connections")
        return BenchmarkConfig::MaxConnections;
    if (str == "enable_logging")
        return BenchmarkConfig::EnableLogging;
    if (str == "log_level")
        return BenchmarkConfig::LogLevel;
    if (str == "api_timeout")
        return BenchmarkConfig::ApiTimeout;
    if (str == "server_port")
        return BenchmarkConfig::ServerPort;
    if (str == "debug_mode")
        return BenchmarkConfig::DebugMode;
    if (str == "cache_size")
        return BenchmarkConfig::CacheSize;
    if (str == "retry_count")
        return BenchmarkConfig::RetryCount;
    if (str == "compression_ratio")
        return BenchmarkConfig::CompressionRatio;
    if (str == "setting1")
        return BenchmarkConfig::Setting1;
    if (str == "setting2")
        return BenchmarkConfig::Setting2;
    if (str == "setting3")
        return BenchmarkConfig::Setting3;
    if (str == "setting4")
        return BenchmarkConfig::Setting4;
    if (str == "setting5")
        return BenchmarkConfig::Setting5;
    if (str == "setting6")
        return BenchmarkConfig::Setting6;
    if (str == "setting7")
        return BenchmarkConfig::Setting7;
    if (str == "setting8")
        return BenchmarkConfig::Setting8;
    if (str == "setting9")
        return BenchmarkConfig::Setting9;
    if (str == "setting10")
        return BenchmarkConfig::Setting10;
    if (str == "setting11")
        return BenchmarkConfig::Setting11;
    if (str == "setting12")
        return BenchmarkConfig::Setting12;
    if (str == "setting13")
        return BenchmarkConfig::Setting13;
    if (str == "setting14")
        return BenchmarkConfig::Setting14;
    if (str == "setting15")
        return BenchmarkConfig::Setting15;
    if (str == "setting16")
        return BenchmarkConfig::Setting16;
    if (str == "setting17")
        return BenchmarkConfig::Setting17;
    if (str == "setting18")
        return BenchmarkConfig::Setting18;
    if (str == "setting19")
        return BenchmarkConfig::Setting19;
    if (str == "setting20")
        return BenchmarkConfig::Setting20;
    throw std::runtime_error("Invalid configuration key: " + str);
}

} // namespace benchmark_test

// Specialize ConfigurationTraits for BenchmarkConfig
namespace config {

template <>
struct ConfigurationTraits<benchmark_test::BenchmarkConfig> {
    static nlohmann::json ToJson(const benchmark_test::BenchmarkConfig& value)
    {
        return benchmark_test::ToString(value);
    }

    static benchmark_test::BenchmarkConfig FromJson(const nlohmann::json& json)
    {
        return benchmark_test::FromString(json.get<std::string>());
    }

    static std::string ToString(const benchmark_test::BenchmarkConfig& value)
    {
        return benchmark_test::ToString(value);
    }

    static bool IsValid(const benchmark_test::BenchmarkConfig& value)
    {
        return true; // All enum values are valid
    }
};

// Template specializations for JsonSerializer
template <>
std::string JsonSerializer<benchmark_test::BenchmarkConfig, BasicSettingVariant<benchmark_test::BenchmarkConfig>>::ToString(benchmark_test::BenchmarkConfig enumValue)
{
    return benchmark_test::ToString(enumValue);
}

template <>
benchmark_test::BenchmarkConfig JsonSerializer<benchmark_test::BenchmarkConfig, BasicSettingVariant<benchmark_test::BenchmarkConfig>>::FromString(const std::string& str)
{
    return benchmark_test::FromString(str);
}

} // namespace config

// Declare compile-time type mappings
namespace config {
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::DatabaseUrl, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::MaxConnections, int);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::EnableLogging, bool);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::LogLevel, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::ApiTimeout, double);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::ServerPort, int);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::DebugMode, bool);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::CacheSize, int);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::RetryCount, int);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::CompressionRatio, double);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting1, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting2, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting3, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting4, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting5, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting6, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting7, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting8, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting9, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting10, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting11, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting12, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting13, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting14, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting15, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting16, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting17, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting18, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting19, std::string);
DECLARE_CONFIG_TYPE(benchmark_test::BenchmarkConfig, benchmark_test::BenchmarkConfig::Setting20, std::string);
} // namespace config

namespace benchmark_test {

using BenchmarkTestConfig = ::config::BasicJsonConfiguration<BenchmarkConfig>;

// Create default configuration
BenchmarkTestConfig::DefaultConfigMap CreateBenchmarkDefaults()
{
    return {
        { BenchmarkConfig::DatabaseUrl,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::DatabaseUrl>(
              "postgresql://localhost:5432/benchmark", "Database connection URL") },
        { BenchmarkConfig::MaxConnections,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateIntSetting<BenchmarkConfig::MaxConnections>(
              100, 1, 1000, "Maximum database connections") },
        { BenchmarkConfig::EnableLogging,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateBoolSetting<BenchmarkConfig::EnableLogging>(
              true, "Enable application logging") },
        { BenchmarkConfig::LogLevel,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::LogLevel>(
              "info", "Logging level") },
        { BenchmarkConfig::ApiTimeout,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateDoubleSetting<BenchmarkConfig::ApiTimeout>(
              30.0, 0.1, 300.0, "API request timeout") },
        { BenchmarkConfig::ServerPort,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateIntSetting<BenchmarkConfig::ServerPort>(
              8080, 1024, 65535, "Server port number") },
        { BenchmarkConfig::DebugMode,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateBoolSetting<BenchmarkConfig::DebugMode>(
              false, "Enable debug mode") },
        { BenchmarkConfig::CacheSize,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateIntSetting<BenchmarkConfig::CacheSize>(
              256, 1, 10000, "Cache size") },
        { BenchmarkConfig::RetryCount,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateIntSetting<BenchmarkConfig::RetryCount>(
              3, 0, 10, "Number of retry attempts") },
        { BenchmarkConfig::CompressionRatio,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateDoubleSetting<BenchmarkConfig::CompressionRatio>(
              0.8, 0.0, 1.0, "Compression ratio") },
        // Additional settings for larger scale benchmarks
        { BenchmarkConfig::Setting1,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting1>("value1") },
        { BenchmarkConfig::Setting2,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting2>("value2") },
        { BenchmarkConfig::Setting3,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting3>("value3") },
        { BenchmarkConfig::Setting4,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting4>("value4") },
        { BenchmarkConfig::Setting5,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting5>("value5") },
        { BenchmarkConfig::Setting6,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting6>("value6") },
        { BenchmarkConfig::Setting7,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting7>("value7") },
        { BenchmarkConfig::Setting8,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting8>("value8") },
        { BenchmarkConfig::Setting9,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting9>("value9") },
        { BenchmarkConfig::Setting10,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting10>("value10") },
        { BenchmarkConfig::Setting11,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting11>("value11") },
        { BenchmarkConfig::Setting12,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting12>("value12") },
        { BenchmarkConfig::Setting13,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting13>("value13") },
        { BenchmarkConfig::Setting14,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting14>("value14") },
        { BenchmarkConfig::Setting15,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting15>("value15") },
        { BenchmarkConfig::Setting16,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting16>("value16") },
        { BenchmarkConfig::Setting17,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting17>("value17") },
        { BenchmarkConfig::Setting18,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting18>("value18") },
        { BenchmarkConfig::Setting19,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting19>("value19") },
        { BenchmarkConfig::Setting20,
          ::config::ConfigHelpers<BenchmarkConfig>::CreateStringSetting<BenchmarkConfig::Setting20>("value20") }
    };
}

} // namespace benchmark_test

// Global configuration for benchmarks
static std::unique_ptr<benchmark_test::BenchmarkTestConfig> g_config;

// Setup function called once before benchmarks
static void SetupBenchmark()
{
    if (!g_config) {
        std::filesystem::path benchmark_path = std::filesystem::temp_directory_path() / "benchmark_config.json";
        auto defaults = benchmark_test::CreateBenchmarkDefaults();
        g_config = std::make_unique<benchmark_test::BenchmarkTestConfig>(benchmark_path, defaults);
    }
}

// ============================================================================
// CORE SETTING ACCESS BENCHMARKS
// ============================================================================

static void BM_GetSetting_Int(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::ServerPort>();
        benchmark::DoNotOptimize(setting);
    }
}
BENCHMARK(BM_GetSetting_Int);

static void BM_GetSetting_String(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::DatabaseUrl>();
        benchmark::DoNotOptimize(setting);
    }
}
BENCHMARK(BM_GetSetting_String);

static void BM_GetSetting_Bool(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::EnableLogging>();
        benchmark::DoNotOptimize(setting);
    }
}
BENCHMARK(BM_GetSetting_Bool);

static void BM_GetSetting_Double(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::ApiTimeout>();
        benchmark::DoNotOptimize(setting);
    }
}
BENCHMARK(BM_GetSetting_Double);

static void BM_GetSetting_Float(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::CompressionRatio>();
        benchmark::DoNotOptimize(setting);
    }
}
BENCHMARK(BM_GetSetting_Float);

// ============================================================================
// VALUE ACCESS BENCHMARKS
// ============================================================================

static void BM_ValueAccess_Int(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::ServerPort>();
    for (auto _ : state) {
        auto value = setting.Value();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_ValueAccess_Int);

static void BM_ValueAccess_String(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::DatabaseUrl>();
    for (auto _ : state) {
        auto value = setting.Value();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_ValueAccess_String);

static void BM_ValueAccess_Bool(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::EnableLogging>();
    for (auto _ : state) {
        auto value = setting.Value();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_ValueAccess_Bool);

// ============================================================================
// COMBINED ACCESS BENCHMARKS (REALISTIC USAGE)
// ============================================================================

static void BM_GetSettingAndValue_Int(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::ServerPort>();
        auto value = setting.Value();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_GetSettingAndValue_Int);

static void BM_GetSettingAndValue_String(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::DatabaseUrl>();
        auto value = setting.Value();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_GetSettingAndValue_String);

// ============================================================================
// METADATA ACCESS BENCHMARKS
// ============================================================================

static void BM_MetadataAccess(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->template GetSetting<benchmark_test::BenchmarkConfig::ServerPort>();
    for (auto _ : state) {
        auto desc = setting.Description();
        auto unit = setting.Unit();
        auto min_val = setting.MinValue();
        auto max_val = setting.MaxValue();
        benchmark::DoNotOptimize(desc);
        benchmark::DoNotOptimize(unit);
        benchmark::DoNotOptimize(min_val);
        benchmark::DoNotOptimize(max_val);
    }
}
BENCHMARK(BM_MetadataAccess);

// ============================================================================
// SETTING MODIFICATION BENCHMARKS
// ============================================================================

static void BM_SetValue_Int(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->GetSetting<benchmark_test::BenchmarkConfig::ServerPort>();
    int value = 8080;
    for (auto _ : state) {
        setting.SetValue(value);
        value = (value == 8080) ? 9090 : 8080; // Alternate values
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_SetValue_Int);

static void BM_SetValue_String(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->GetSetting<benchmark_test::BenchmarkConfig::DatabaseUrl>();
    bool toggle = false;
    for (auto _ : state) {
        if (toggle) {
            setting.SetValue(std::string("postgresql://localhost:5432/benchmark"));
        }
        else {
            setting.SetValue(std::string("mysql://localhost:3306/benchmark"));
        }
        toggle = !toggle;
        benchmark::DoNotOptimize(toggle);
    }
}
BENCHMARK(BM_SetValue_String);

// ============================================================================
// VALIDATION BENCHMARKS
// ============================================================================

static void BM_ValidationCheck(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->GetSetting<benchmark_test::BenchmarkConfig::ServerPort>();
    for (auto _ : state) {
        auto is_valid = setting.IsValid();
        benchmark::DoNotOptimize(is_valid);
    }
}
BENCHMARK(BM_ValidationCheck);

static void BM_ValidateAll(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto is_valid = g_config->ValidateAll();
        benchmark::DoNotOptimize(is_valid);
    }
}
BENCHMARK(BM_ValidateAll);

// ============================================================================
// MULTIPLE SETTING ACCESS BENCHMARKS (REALISTIC WORKLOADS)
// ============================================================================

static void BM_MultipleSettingAccess_5Settings(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto port = g_config->GetSetting<benchmark_test::BenchmarkConfig::ServerPort>().Value();
        auto db_url = g_config->GetSetting<benchmark_test::BenchmarkConfig::DatabaseUrl>().Value();
        auto logging = g_config->GetSetting<benchmark_test::BenchmarkConfig::EnableLogging>().Value();
        auto timeout = g_config->GetSetting<benchmark_test::BenchmarkConfig::ApiTimeout>().Value();
        auto debug = g_config->GetSetting<benchmark_test::BenchmarkConfig::DebugMode>().Value();

        benchmark::DoNotOptimize(port);
        benchmark::DoNotOptimize(db_url);
        benchmark::DoNotOptimize(logging);
        benchmark::DoNotOptimize(timeout);
        benchmark::DoNotOptimize(debug);
    }
}
BENCHMARK(BM_MultipleSettingAccess_5Settings);

static void BM_MultipleSettingAccess_10Settings(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto port = g_config->GetSetting<benchmark_test::BenchmarkConfig::ServerPort>().Value();
        auto db_url = g_config->GetSetting<benchmark_test::BenchmarkConfig::DatabaseUrl>().Value();
        auto logging = g_config->GetSetting<benchmark_test::BenchmarkConfig::EnableLogging>().Value();
        auto timeout = g_config->GetSetting<benchmark_test::BenchmarkConfig::ApiTimeout>().Value();
        auto debug = g_config->GetSetting<benchmark_test::BenchmarkConfig::DebugMode>().Value();
        auto cache = g_config->GetSetting<benchmark_test::BenchmarkConfig::CacheSize>().Value();
        auto retry = g_config->GetSetting<benchmark_test::BenchmarkConfig::RetryCount>().Value();
        auto compression = g_config->GetSetting<benchmark_test::BenchmarkConfig::CompressionRatio>().Value();
        auto log_level = g_config->GetSetting<benchmark_test::BenchmarkConfig::LogLevel>().Value();
        auto max_conn = g_config->GetSetting<benchmark_test::BenchmarkConfig::MaxConnections>().Value();

        benchmark::DoNotOptimize(port);
        benchmark::DoNotOptimize(db_url);
        benchmark::DoNotOptimize(logging);
        benchmark::DoNotOptimize(timeout);
        benchmark::DoNotOptimize(debug);
        benchmark::DoNotOptimize(cache);
        benchmark::DoNotOptimize(retry);
        benchmark::DoNotOptimize(compression);
        benchmark::DoNotOptimize(log_level);
        benchmark::DoNotOptimize(max_conn);
    }
}
BENCHMARK(BM_MultipleSettingAccess_10Settings);

// ============================================================================
// LEGACY API BENCHMARKS (FOR COMPARISON)
// ============================================================================

static void BM_LegacyAPI_GetValue_Int(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto value = g_config->GetValue<benchmark_test::BenchmarkConfig::ServerPort, int>();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_LegacyAPI_GetValue_Int);

static void BM_LegacyAPI_GetValue_String(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto value = g_config->GetValue<benchmark_test::BenchmarkConfig::DatabaseUrl, std::string>();
        benchmark::DoNotOptimize(value);
    }
}
BENCHMARK(BM_LegacyAPI_GetValue_String);

// ============================================================================
// SERIALIZATION BENCHMARKS
// ============================================================================

static void BM_ConfigurationSave(benchmark::State& state)
{
    SetupBenchmark();
    for (auto _ : state) {
        auto result = g_config->Save();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ConfigurationSave);

static void BM_ConfigurationLoad(benchmark::State& state)
{
    SetupBenchmark();
    g_config->Save(); // Ensure file exists
    for (auto _ : state) {
        auto result = g_config->Load();
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_ConfigurationLoad);

// ============================================================================
// CONFIGURATION CREATION BENCHMARKS
// ============================================================================

static void BM_ConfigurationCreation(benchmark::State& state)
{
    auto defaults = benchmark_test::CreateBenchmarkDefaults();
    for (auto _ : state) {
        std::filesystem::path temp_path = std::filesystem::temp_directory_path() / ("bench_config_" + std::to_string(state.iterations()) + ".json");
        benchmark_test::BenchmarkTestConfig config(temp_path, defaults);
        benchmark::DoNotOptimize(config);
    }
}
BENCHMARK(BM_ConfigurationCreation);

// ============================================================================
// STRING REPRESENTATION BENCHMARKS
// ============================================================================

static void BM_ToString(benchmark::State& state)
{
    SetupBenchmark();
    auto setting = g_config->GetSetting<benchmark_test::BenchmarkConfig::ServerPort>();
    for (auto _ : state) {
        auto str = setting.ToString();
        benchmark::DoNotOptimize(str);
    }
}
BENCHMARK(BM_ToString);

// ============================================================================
// RANDOM ACCESS PATTERN BENCHMARKS
// ============================================================================

static void BM_RandomAccess(benchmark::State& state)
{
    SetupBenchmark();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9); // 10 different settings

    for (auto _ : state) {
        int choice = dis(gen);
        switch (choice) {
        case 0: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::ServerPort>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 1: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::DatabaseUrl>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 2: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::EnableLogging>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 3: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::ApiTimeout>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 4: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::DebugMode>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 5: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::CacheSize>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 6: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::RetryCount>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 7: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::CompressionRatio>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 8: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::LogLevel>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        case 9: {
            auto val = g_config->GetSetting<benchmark_test::BenchmarkConfig::MaxConnections>().Value();
            benchmark::DoNotOptimize(val);
            break;
        }
        }
    }
}
BENCHMARK(BM_RandomAccess);

// Custom main to clean up after benchmarks
int main(int argc, char** argv)
{
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return 1;

    auto result = benchmark::RunSpecifiedBenchmarks();

    // Cleanup
    if (g_config) {
        std::filesystem::path benchmark_path = std::filesystem::temp_directory_path() / "benchmark_config.json";
        if (std::filesystem::exists(benchmark_path)) {
            std::filesystem::remove(benchmark_path);
        }
        g_config.reset();
    }

    return result;
}
