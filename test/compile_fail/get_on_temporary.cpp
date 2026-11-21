#include <cppfig/cppfig.h>

struct PortSetting {
    static constexpr std::string_view path = "server.port";
    using value_type = int;
    static constexpr auto default_value() -> int { return 8080; }
};

using AppRegistry = cppfig::Registry<PortSetting>;

// The configuration is destroyed at the end of the full expression, so the
// returned reference would dangle.
auto ReadFromTemporary() -> const int&
{
    return cppfig::Configuration<AppRegistry>::FromDefaults().value().Get<PortSetting>();
}
