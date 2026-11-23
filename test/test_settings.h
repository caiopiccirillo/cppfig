#pragma once

#include <cppfig/cppfig.h>
#include <stdlib.h>  // NOLINT(modernize-deprecated-headers): setenv and unsetenv are POSIX, not in <cstdlib>
#include <unistd.h>

#include <atomic>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace cppfig::test {

struct Point {
    int x = 0;
    int y = 0;

    auto operator==(const Point&) const -> bool = default;
};

struct AlignedValue {
    alignas(16) int value = 7;

    auto operator==(const AlignedValue&) const -> bool = default;
};

}  // namespace cppfig::test

template <>
struct cppfig::ConfigTraits<cppfig::test::Point> {
    static auto FromString(std::string_view text) -> std::optional<cppfig::test::Point>
    {
        const std::size_t comma = text.find(',');
        if (comma == std::string_view::npos) {
            return std::nullopt;
        }
        cppfig::test::Point point;
        const auto first = std::from_chars(text.data(), text.data() + comma, point.x);
        const auto* second_begin = text.data() + comma + 1;
        const auto second = std::from_chars(second_begin, text.data() + text.size(), point.y);
        if (first.ec != std::errc {} || first.ptr != text.data() + comma || second.ec != std::errc {}
            || second.ptr != text.data() + text.size()) {
            return std::nullopt;
        }
        return point;
    }

    static auto ToString(const cppfig::test::Point& point) -> std::string
    {
        return std::to_string(point.x) + "," + std::to_string(point.y);
    }
};

template <>
struct cppfig::ConfigTraits<cppfig::test::AlignedValue> {
    static auto FromString(std::string_view text) -> std::optional<cppfig::test::AlignedValue>
    {
        auto parsed = ConfigTraits<int>::FromString(text);
        if (!parsed) {
            return std::nullopt;
        }
        return cppfig::test::AlignedValue { *parsed };
    }

    static auto ToString(const cppfig::test::AlignedValue& value) -> std::string { return ConfigTraits<int>::ToString(value.value); }
};

namespace cppfig::test::settings {

struct AppName {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "MyApplication"; }
};

struct ServerHost {
    static constexpr std::string_view path = "server.host";
    static constexpr std::string_view env_override = "CPPFIG_TEST_SERVER_HOST";
    using value_type = std::string;
    static auto default_value() -> std::string { return "localhost"; }
};

struct ServerPort {
    static constexpr std::string_view path = "server.port";
    static constexpr std::string_view env_override = "CPPFIG_TEST_SERVER_PORT";
    using value_type = int;
    static auto default_value() -> int { return 8080; }
    static constexpr auto validator() { return Range(1, 65535); }
};

struct LoggingEnabled {
    static constexpr std::string_view path = "logging.enabled";
    using value_type = bool;
    static auto default_value() -> bool { return true; }
};

struct Ratio {
    static constexpr std::string_view path = "numeric.ratio";
    using value_type = double;
    static auto default_value() -> double { return 1.5; }
};

struct Origin {
    static constexpr std::string_view path = "custom.point";
    using value_type = Point;
    static auto default_value() -> Point { return { .x = 1, .y = 2 }; }
};

struct Aligned {
    static constexpr std::string_view path = "aligned.value";
    using value_type = AlignedValue;
    static auto default_value() -> AlignedValue { return { 16 }; }
};

}  // namespace cppfig::test::settings

namespace cppfig::test {

using TestRegistry = Registry<settings::AppName, settings::ServerHost, settings::ServerPort, settings::LoggingEnabled,
                              settings::Ratio, settings::Origin, settings::Aligned>;
using TestConfiguration = Configuration<TestRegistry>;

class TempDirectory {
public:
    TempDirectory()
    {
        static std::atomic<unsigned long> counter = 0;
        path_ = std::filesystem::temp_directory_path() / ("cppfig_test_" + std::to_string(::getpid()) + "_" + std::to_string(counter.fetch_add(1)));
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TempDirectory() { std::filesystem::remove_all(Path()); }

    TempDirectory(const TempDirectory&) = delete;
    auto operator=(const TempDirectory&) -> TempDirectory& = delete;

    [[nodiscard]] auto Path() const -> const std::filesystem::path& { return path_; }

    [[nodiscard]] auto File(std::string_view name) const -> std::filesystem::path { return path_ / name; }

    void Write(std::string_view name, std::string_view contents) const
    {
        std::ofstream output(File(name), std::ios::binary | std::ios::trunc);
        output << contents;
    }

    [[nodiscard]] auto Read(std::string_view name) const -> std::string
    {
        std::ifstream input(File(name), std::ios::binary);
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }

private:
    std::filesystem::path path_;
};

class ScopedEnvironment {
public:
    ScopedEnvironment(std::string name, const std::string& value)
        : name_(std::move(name))
    {
        if (const char* previous = std::getenv(name_.c_str())) {
            previous_ = previous;
        }
        setenv(name_.c_str(), value.c_str(), 1);
    }

    ~ScopedEnvironment()
    {
        if (previous_) {
            setenv(name_.c_str(), previous_->c_str(), 1);
        }
        else {
            unsetenv(name_.c_str());
        }
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    auto operator=(const ScopedEnvironment&) -> ScopedEnvironment& = delete;

private:
    std::string name_;
    std::optional<std::string> previous_;
};

}  // namespace cppfig::test
