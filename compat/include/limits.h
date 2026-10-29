#ifndef CE_POSIX_LIMITS_H
#define CE_POSIX_LIMITS_H

#include_next <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 260
#endif
#ifndef SSIZE_MAX
#define SSIZE_MAX LONG_MAX
#endif

#endif
