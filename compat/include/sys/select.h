#ifndef CE_POSIX_SYS_SELECT_H
#define CE_POSIX_SYS_SELECT_H

#include <sys/time.h>

#ifndef FD_SETSIZE
#define FD_SETSIZE 64
typedef struct fd_set {
    unsigned int fd_count;
    unsigned int fd_array[FD_SETSIZE];
} fd_set;
#define FD_ZERO(set) (((fd_set *)(set))->fd_count = 0)
#endif

#endif
