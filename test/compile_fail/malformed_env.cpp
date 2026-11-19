#include <cppfig/cppfig.h>

struct MalformedEnvSetting {
    static constexpr std::string_view path = "malformed.env";
    static constexpr std::string_view env_override = "NOT VALID=";
    using value_type = int;
    static constexpr auto default_value() -> int { return 0; }
};

using InvalidRegistry = cppfig::Registry<MalformedEnvSetting>;
static_assert(sizeof(InvalidRegistry) != 0);
