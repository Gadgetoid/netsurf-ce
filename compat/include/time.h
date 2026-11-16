#ifndef CE_POSIX_TIME_H
#define CE_POSIX_TIME_H

#include_next <time.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
typedef int clockid_t;

struct tm *gmtime_r(const time_t *value, struct tm *result);
struct tm *localtime_r(const time_t *value, struct tm *result);
time_t timegm(struct tm *value);
int clock_gettime(clockid_t clock, struct timespec *result);

#endif
