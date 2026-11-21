// Fuzzes Configuration::FromText end to end with a registry of every built-in kind
// of value.
//
// Properties, for any file content:
// - loading never crashes, under either unknown-key policy;
// - rejecting unknown keys never accepts a file that preserving them rejects;
// - a loaded document, once its migrations are appended, loads again to the
//   same values and the same text, and no longer needs saving.

#include <cppfig/chrono.h>
#include <cppfig/cppfig.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

#include "fuzz_check.h"

namespace fuzz {

enum class Mode : std::uint8_t { kFast,
                                 kSafe,
                                 kDebug };

inline constexpr std::array<cppfig::EnumName<Mode>, 3> kModeNames { {
    { Mode::kFast, "fast" },
    { Mode::kSafe, "safe" },
    { Mode::kDebug, "debug" },
} };

}  // namespace fuzz

template <>
struct cppfig::ConfigTraits<fuzz::Mode> : cppfig::EnumTraits<fuzz::kModeNames> { };

namespace fuzz {

struct Name {
    static constexpr std::string_view path = "app.name";
    using value_type = std::string;
    static auto default_value() -> std::string { return "fuzz app"; }
    static auto validator() { return cppfig::NotEmpty(); }
};

struct Greeting {
    static constexpr std::string_view path = "app.greeting";
    using value_type = std::string;
    static auto default_value() -> std::string { return "line one\n\"quoted\" # not a comment\t"; }
};

struct Port {
    static constexpr std::string_view path = "server.port";
    using value_type = std::uint16_t;
    static constexpr auto default_value() -> std::uint16_t { return 8080; }
    static constexpr auto validator() { return cppfig::Min<std::uint16_t>(1); }
};

struct Workers {
    static constexpr std::string_view path = "server.workers";
    using value_type = int;
    static constexpr auto default_value() -> int { return 4; }
    static constexpr auto validator() { return cppfig::Range(1, 64); }
};

struct Offset {
    static constexpr std::string_view path = "server.offset";
    using value_type = long long;
    static constexpr auto default_value() -> long long { return -9'000'000'000LL; }
};

struct Ratio {
    static constexpr std::string_view path = "cache.ratio";
    using value_type = double;
    static constexpr auto default_value() -> double { return 0.1; }
};

struct Scale {
    static constexpr std::string_view path = "cache.scale";
    using value_type = float;
    static constexpr auto default_value() -> float { return 1.0e-30F; }
};

struct Enabled {
    static constexpr std::string_view path = "cache.enabled";
    using value_type = bool;
    static constexpr auto default_value() -> bool { return true; }
};

struct LogMode {
    static constexpr std::string_view path = "log.mode";
    using value_type = Mode;
    static constexpr auto default_value() -> Mode { return Mode::kSafe; }
};

struct Timeout {
    static constexpr std::string_view path = "http.timeout";
    using value_type = std::chrono::milliseconds;
    static constexpr auto default_value() -> std::chrono::milliseconds { return std::chrono::seconds { 30 }; }
};

struct Directory {
    static constexpr std::string_view path = "storage.directory";
    using value_type = std::filesystem::path;
    static auto default_value() -> std::filesystem::path { return "/var/lib/fuzz app"; }
};

struct FuzzRegistry
    : cppfig::Registry<Name, Greeting, Port, Workers, Offset, Ratio, Scale, Enabled, LogMode, Timeout, Directory> { };

using FuzzConfiguration = cppfig::Configuration<FuzzRegistry>;

}  // namespace fuzz

namespace {

template <typename T>
auto Same(const T& left, const T& right) -> bool
{
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(left) || std::isnan(right)) {
            return std::isnan(left) && std::isnan(right) && std::signbit(left) == std::signbit(right);
        }
        return left <= right && left >= right && std::signbit(left) == std::signbit(right);
    }
    else {
        return left == right;
    }
}

template <typename... Settings>
auto SameValues(const fuzz::FuzzConfiguration& left, const fuzz::FuzzConfiguration& right, cppfig::Registry<Settings...> /*registry*/)
    -> bool
{
    return (Same(left.Get<Settings>(), right.Get<Settings>()) && ...);
}

constexpr std::string_view kSource = "fuzz.conf";

auto Load(std::string_view text, cppfig::UnknownKeyPolicy policy) -> cppfig::StatusOr<cppfig::LoadResult<fuzz::FuzzRegistry>>
{
    return fuzz::FuzzConfiguration::FromText(std::string(text), kSource, { .unknown_keys = policy });
}

}  // namespace

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) -> int
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): libFuzzer's interface takes bytes.
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const auto strict = Load(text, cppfig::UnknownKeyPolicy::kReject);
    const auto lenient = Load(text, cppfig::UnknownKeyPolicy::kPreserve);
    if (strict.has_value()) {
        CPPFIG_FUZZ_CHECK(lenient.has_value());
    }
    if (!lenient.has_value()) {
        return 0;
    }

    const auto& first = lenient.value();
    CPPFIG_FUZZ_CHECK(first.document.GetText().starts_with(text));
    if (strict.has_value()) {
        CPPFIG_FUZZ_CHECK(first.document.GetUnknownKeys().empty());
        CPPFIG_FUZZ_CHECK(SameValues(strict.value().config, first.config, fuzz::FuzzRegistry {}));
    }

    const auto second = Load(first.document.GetText(), cppfig::UnknownKeyPolicy::kPreserve);
    CPPFIG_FUZZ_CHECK(second.has_value());
    CPPFIG_FUZZ_CHECK(SameValues(first.config, second.value().config, fuzz::FuzzRegistry {}));
    CPPFIG_FUZZ_CHECK(second.value().document.GetText() == first.document.GetText());
    CPPFIG_FUZZ_CHECK(!second.value().document.NeedsSave());
    CPPFIG_FUZZ_CHECK(second.value().document.GetUnknownKeys().size() == first.document.GetUnknownKeys().size());
    return 0;
}
