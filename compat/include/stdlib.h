#ifndef CE_POSIX_STDLIB_H
#define CE_POSIX_STDLIB_H

#include_next <stdlib.h>
#include <limits.h>

int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int mkstemp(char *pattern);

#endif
