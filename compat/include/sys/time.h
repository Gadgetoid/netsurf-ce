#ifndef CE_POSIX_SYS_TIME_H
#define CE_POSIX_SYS_TIME_H

#include <sys/types.h>
#include <time.h>

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif

int gettimeofday(struct timeval *result, void *zone);

#define timerisset(value) ((value)->tv_sec || (value)->tv_usec)
#define timercmp(a, b, CMP) (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP (b)->tv_usec) : ((a)->tv_sec CMP (b)->tv_sec))
#define timeradd(a, b, result) do { \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
    if ((result)->tv_usec >= 1000000) { (result)->tv_sec++; (result)->tv_usec -= 1000000; } \
} while (0)
#define timersub(a, b, result) do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((result)->tv_usec < 0) { (result)->tv_sec--; (result)->tv_usec += 1000000; } \
} while (0)

#endif
