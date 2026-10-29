#ifndef CE_POSIX_STRINGS_H
#define CE_POSIX_STRINGS_H

#include <stddef.h>

int strcasecmp(const char *first, const char *second);
int strncasecmp(const char *first, const char *second, size_t count);

#endif
