#ifndef CE_POSIX_STDLIB_H
#define CE_POSIX_STDLIB_H

#include_next <stdlib.h>
#include <limits.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

void exit(int status) __attribute__((noreturn));
void _Exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atexit(void (*function)(void));
void *bsearch(const void *key, const void *base, size_t count, size_t size, int (*compare)(const void *, const void *));
char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int mkstemp(char *pattern);

#endif
