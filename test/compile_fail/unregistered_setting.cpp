#include <cppfig/cppfig.h>

struct KnownSetting {
    static constexpr std::string_view path = "known.path";
    using value_type = int;
    static constexpr auto default_value() -> int { return 0; }
};

struct UnknownSetting {
    using value_type = int;
};

void ReadUnknown()
{
    auto loaded = cppfig::Configuration<cppfig::Registry<KnownSetting>>::FromDefaults();
    (void)loaded->Get<UnknownSetting>();
}
