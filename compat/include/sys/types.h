#ifndef CE_POSIX_SYS_TYPES_H
#define CE_POSIX_SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

typedef long ssize_t;
typedef long off_t;
typedef unsigned int mode_t;
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned short nlink_t;
typedef long suseconds_t;
typedef long blksize_t;
typedef long blkcnt_t;

#ifndef _TIME_T_DEFINED
typedef unsigned long time_t;
#define _TIME_T_DEFINED
#endif

#endif
