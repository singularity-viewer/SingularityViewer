// The following is the libcwd related mandatory part.
// It must be included before any system header file is included.
#ifdef CWDEBUG
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <libcwd/sys.h>
#endif

