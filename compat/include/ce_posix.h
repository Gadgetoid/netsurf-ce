#ifndef CE_POSIX_H
#define CE_POSIX_H

#include <stddef.h>

void ce_log_open(const char *path);
void ce_log_write(const char *text, size_t length);
int ce_utf8_to_wide(const char *text, wchar_t *output, size_t capacity);
int ce_wide_to_utf8(const wchar_t *text, char *output, size_t capacity);
int ce_path_to_wide(const char *path, wchar_t *output, size_t capacity);

#endif
