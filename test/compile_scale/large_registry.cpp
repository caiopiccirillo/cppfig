// Compiles the complete initialization path (LoadFile, FromDefaults, SaveFile,
// ConfigurationBuilder and Get) for a generated registry of CPPFIG_SCALE_SETTINGS
// settings with mixed value types, environment overrides and validators.
// CTest runs it at -O2 with a timeout to catch compile-time regressions.
#include <cppfig/cppfig.h>

#include <cstddef>
#include <string>

#include "generated_registry.h"

#ifndef CPPFIG_SCALE_SETTINGS
#define CPPFIG_SCALE_SETTINGS 1024
#endif

namespace scale {

using LargeRegistry = GeneratedRegistry<CPPFIG_SCALE_SETTINGS>;
static_assert(LargeRegistry::size == CPPFIG_SCALE_SETTINGS);

}  // namespace scale

// External linkage keeps the compiler from discarding this function, so the
// test measures code generation as well as parsing and instantiation.
auto LoadLarge(const std::string& path) -> int  // NOLINT(misc-use-internal-linkage)
{
    auto loaded = cppfig::LoadFile<scale::LargeRegistry>(path);
    if (!loaded.has_value() || !cppfig::SaveFile(path, loaded->document).ok()) {
        return -1;
    }
    auto defaults = cppfig::Configuration<scale::LargeRegistry>::FromDefaults();
    auto fake = cppfig::ConfigurationBuilder<scale::LargeRegistry> {}.Build();
    if (!defaults.has_value() || !fake.has_value()) {
        return -1;
    }
    constexpr std::size_t last = CPPFIG_SCALE_SETTINGS - 4;
    return loaded->config.Get<scale::Setting<last>>() + defaults->Get<scale::Setting<0>>() + fake->Get<scale::Setting<4>>();
}
