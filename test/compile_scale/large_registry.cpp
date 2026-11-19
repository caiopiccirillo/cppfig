// Compiles the complete initialization path (Load, FromDefaults, Save, the
// test fake and Get) for a generated registry of CPPFIG_SCALE_SETTINGS
// settings with mixed value types, environment overrides and validators.
// CTest runs it at -O2 with a timeout to catch compile-time regressions.
#include <cppfig/cppfig.h>
#include <cppfig/testing/mock.h>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#ifndef CPPFIG_SCALE_SETTINGS
#define CPPFIG_SCALE_SETTINGS 1024
#endif

namespace scale {

template <std::size_t I>
struct Name {
    static constexpr auto storage = [] {
        std::array<char, 32> buffer {};
        constexpr std::string_view prefix = "scale.s";
        std::size_t position = 0;
        for (const char c : prefix) {
            buffer[position++] = c;
        }
        std::array<char, 20> digits {};
        std::size_t count = 0;
        std::size_t value = I;
        do {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0);
        while (count != 0) {
            buffer[position++] = digits[--count];
        }
        return std::pair { buffer, position };
    }();
    static constexpr std::string_view path { storage.first.data(), storage.second };
};

template <std::size_t I>
struct EnvName {
    static constexpr auto storage = [] {
        auto buffer = Name<I>::storage.first;
        for (char& c : buffer) {
            c = c == '.' ? '_' : (c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c);
        }
        return buffer;
    }();
    static constexpr std::string_view value { storage.data(), Name<I>::storage.second };
};

template <std::size_t I, std::size_t Kind = I % 4>
struct Setting;

template <std::size_t I>
struct Setting<I, 0> {
    static constexpr std::string_view path = Name<I>::path;
    static constexpr std::string_view env_override = EnvName<I>::value;
    using value_type = int;
    static constexpr auto default_value() -> int { return static_cast<int>(I); }
    static constexpr auto validator() { return cppfig::NonNegative<int>(); }
};

template <std::size_t I>
struct Setting<I, 1> {
    static constexpr std::string_view path = Name<I>::path;
    using value_type = std::string;
    static auto default_value() -> std::string { return "value"; }
};

template <std::size_t I>
struct Setting<I, 2> {
    static constexpr std::string_view path = Name<I>::path;
    using value_type = bool;
    static constexpr auto default_value() -> bool { return true; }
};

template <std::size_t I>
struct Setting<I, 3> {
    static constexpr std::string_view path = Name<I>::path;
    using value_type = double;
    static constexpr auto default_value() -> double { return 0.5; }
    static constexpr auto validator() { return cppfig::Range(0.0, 1.0); }
};

template <typename Sequence>
struct MakeRegistry;

template <std::size_t... I>
struct MakeRegistry<std::index_sequence<I...>> {
    using type = cppfig::Registry<Setting<I>...>;
};

using LargeRegistry = typename MakeRegistry<std::make_index_sequence<CPPFIG_SCALE_SETTINGS>>::type;
static_assert(LargeRegistry::size == CPPFIG_SCALE_SETTINGS);

}  // namespace scale

auto LoadLarge(const std::string& path) -> int
{
    auto loaded = cppfig::Configuration<scale::LargeRegistry>::Load(path);
    if (!loaded.ok() || !loaded->Save().ok()) {
        return -1;
    }
    auto defaults = cppfig::Configuration<scale::LargeRegistry>::FromDefaults();
    auto fake = cppfig::testing::MockConfigurationBuilder<scale::LargeRegistry> {}.Build();
    if (!defaults.ok() || !fake.ok()) {
        return -1;
    }
    constexpr std::size_t last = CPPFIG_SCALE_SETTINGS - 4;
    return loaded->Get<scale::Setting<last>>() + defaults->Get<scale::Setting<0>>() + fake->Get<scale::Setting<4>>();
}
