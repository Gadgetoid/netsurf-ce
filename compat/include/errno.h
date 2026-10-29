#ifndef CE_POSIX_ERRNO_H
#define CE_POSIX_ERRNO_H

#include_next <errno.h>

#ifndef ENOTSUP
#define ENOTSUP 129
#endif
#ifndef EOVERFLOW
#define EOVERFLOW 132
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT 138
#endif
#ifndef EILSEQ
#define EILSEQ 42
#endif
#ifndef ERANGE
#define ERANGE 34
#endif
#ifndef ENAMETOOLONG
#define ENAMETOOLONG 38
#endif
#ifndef ENOTEMPTY
#define ENOTEMPTY 41
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT 102
#endif

#endif
