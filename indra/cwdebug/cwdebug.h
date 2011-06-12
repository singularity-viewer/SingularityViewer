// We support compiling C++ source files that are not
// thread-safe, but in that case we assume that they
// will not be linked with libcwd_r.
#if !defined(_REENTRANT) || !defined(__linux__)
#undef CWDEBUG
#endif

#include "sys.h"
#include "debug.h"
