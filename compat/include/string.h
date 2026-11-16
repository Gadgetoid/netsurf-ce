#ifndef CE_POSIX_STRING_H
#define CE_POSIX_STRING_H

#include_next <string.h>
#include <strings.h>

void *memmem(const void *haystack, size_t haystack_length, const void *needle, size_t needle_length);
char *strtok_r(char *text, const char *delimiters, char **state);
size_t strlcpy(char *destination, const char *source, size_t size);

#endif
