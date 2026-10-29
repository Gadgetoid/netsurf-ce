#ifndef CE_POSIX_TIME_H
#define CE_POSIX_TIME_H

#include_next <time.h>
#include <stddef.h>

typedef long clock_t;

#define CLOCKS_PER_SEC 1000

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
typedef int clockid_t;

time_t time(time_t *result);
clock_t clock(void);
struct tm *gmtime(const time_t *value);
struct tm *gmtime_r(const time_t *value, struct tm *result);
struct tm *localtime(const time_t *value);
struct tm *localtime_r(const time_t *value, struct tm *result);
time_t mktime(struct tm *value);
time_t timegm(struct tm *value);
size_t strftime(char *buffer, size_t size, const char *format, const struct tm *value);
int clock_gettime(clockid_t clock, struct timespec *result);

#endif
