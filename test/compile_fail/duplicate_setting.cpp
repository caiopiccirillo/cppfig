#include <cppfig/cppfig.h>

struct DuplicateSetting {
    static constexpr std::string_view path = "duplicate.path";
    using value_type = int;
    static constexpr auto default_value() -> int { return 0; }
};

using InvalidRegistry = cppfig::Registry<DuplicateSetting, DuplicateSetting>;
static_assert(sizeof(InvalidRegistry) != 0);
