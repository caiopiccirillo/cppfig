#include <cppfig/cppfig.h>

struct FirstSetting {
    static constexpr std::string_view path = "first.value";
    static constexpr std::string_view env_override = "SHARED_NAME";
    using value_type = int;
    static constexpr auto default_value() -> int { return 0; }
};

struct SecondSetting {
    static constexpr std::string_view path = "second.value";
    static constexpr std::string_view env_override = "SHARED_NAME";
    using value_type = int;
    static constexpr auto default_value() -> int { return 0; }
};

using InvalidRegistry = cppfig::Registry<FirstSetting, SecondSetting>;
static_assert(sizeof(InvalidRegistry) != 0);
