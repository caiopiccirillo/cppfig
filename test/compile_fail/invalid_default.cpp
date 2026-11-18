#include <cppfig/cppfig.h>

struct InvalidDefault {
    static constexpr std::string_view path = "invalid.default";
    using value_type = int;
    static constexpr auto default_value() -> int { return 0; }
    static constexpr auto validator() { return cppfig::Min(1); }
};

using InvalidRegistry = cppfig::Registry<InvalidDefault>;
static_assert(sizeof(InvalidRegistry) != 0);
