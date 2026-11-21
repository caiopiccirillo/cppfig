#pragma once

#include "cppfig/conf.h"           // IWYU pragma: export
#include "cppfig/configuration.h"  // IWYU pragma: export
#include "cppfig/document.h"       // IWYU pragma: export
#include "cppfig/environment.h"    // IWYU pragma: export
#include "cppfig/file.h"           // IWYU pragma: export
#include "cppfig/schema.h"         // IWYU pragma: export
#include "cppfig/setting.h"        // IWYU pragma: export
#include "cppfig/status.h"         // IWYU pragma: export
#include "cppfig/traits.h"         // IWYU pragma: export
#include "cppfig/validator.h"      // IWYU pragma: export

/// @namespace cppfig
/// @brief Header-only C++20 typed, frozen configuration registry.
///
/// Settings are declared as types and grouped in Registry. LoadFile (or
/// Configuration::FromText) validates defaults, flat .conf values and
/// environment overrides once and returns the frozen Configuration together
/// with its ConfDocument. After
/// initialization, Get returns a const reference directly from typed storage.
