#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#ifdef _WIN32
#include <stdlib.h>  // _environ
#else
// POSIX defines environ but no header is required to declare it.
extern "C" char** environ;  // NOLINT(readability-redundant-declaration)
#endif

namespace cppfig {

/// @brief One environment variable.
struct EnvironmentVariable {
    std::string_view name;
    std::string_view value;
};

/// @brief The environment variables a configuration reads its overrides from.
///
/// Loading reads the variables once, in a single pass, and looks each one up
/// among the registry's override names; it never calls getenv. Like getenv,
/// reading the process environment must not race with setenv or putenv.
class Environment {
public:
    /// The environment of the running process. The default for loading.
    [[nodiscard]] static constexpr auto Process() noexcept -> Environment { return { Kind::kProcess, {} }; }

    /// No variables: environment overrides are not applied.
    [[nodiscard]] static constexpr auto None() noexcept -> Environment { return { Kind::kList, {} }; }

    /// A fixed set of variables, for tests and for applications that gather
    /// their environment elsewhere. The variables are not copied and must stay
    /// valid while loading. When a name repeats, the first one wins.
    [[nodiscard]] static constexpr auto From(std::span<const EnvironmentVariable> variables) noexcept -> Environment
    {
        return { Kind::kList, variables };
    }

    /// Calls fn(name, value) for every variable, in order.
    template <typename Fn>
    void ForEach(Fn&& fn) const
    {
        if (kind_ == Kind::kList) {
            for (const auto& variable : variables_) {
                fn(variable.name, variable.value);
            }
            return;
        }
        for (char* const* entry = ProcessEntries(); entry != nullptr && *entry != nullptr; ++entry) {
            const std::string_view text(*entry);
            const auto equals = text.find('=');
            if (equals != std::string_view::npos) {
                fn(text.substr(0, equals), text.substr(equals + 1));
            }
        }
    }

private:
    enum class Kind : std::uint8_t {
        kProcess,
        kList,
    };

    constexpr Environment(Kind kind, std::span<const EnvironmentVariable> variables) noexcept
        : variables_(variables)
        , kind_(kind)
    {
    }

    [[nodiscard]] static auto ProcessEntries() noexcept -> char**
    {
#ifdef _WIN32
        return _environ;
#else
        return environ;
#endif
    }

    std::span<const EnvironmentVariable> variables_;
    Kind kind_;
};

}  // namespace cppfig
