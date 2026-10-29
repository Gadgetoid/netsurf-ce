#ifndef CE_POSIX_ICONV_H
#define CE_POSIX_ICONV_H

#include <stddef.h>

typedef struct ce_iconv *iconv_t;

iconv_t iconv_open(const char *to, const char *from);
size_t iconv(iconv_t converter, char **input, size_t *input_left, char **output, size_t *output_left);
int iconv_close(iconv_t converter);

#endif
