// Compiled on its own, with nothing included before the core headers, to
// check what they pull in. <filesystem> belongs to <cppfig/path.h> and
// <cppfig/file.h>, so code that only reads a configuration does not parse it.
#include <cppfig/conf.h>
#include <cppfig/configuration.h>
#include <cppfig/environment.h>
#include <cppfig/traits.h>
#include <cppfig/validator.h>

#if defined(_GLIBCXX_FILESYSTEM) || defined(_LIBCPP_FILESYSTEM)
#error "A core header includes <filesystem>; keep it in <cppfig/path.h> and <cppfig/file.h>"
#endif
