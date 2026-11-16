#include <windows.h>

#include <sys/time.h>
#include <time.h>

#include "internal.h"

static int64_t days_from_civil(int64_t year, unsigned month, unsigned day) {
    year -= month <= 2;
    int64_t era = (year >= 0 ? year : year - 399) / 400;
    unsigned year_of_era = (unsigned)(year - era * 400);
    unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return era * 146097 + (int64_t)day_of_era - 719468;
}

static struct tm *civil_from_seconds(int64_t seconds, struct tm *result) {
    int64_t days = seconds / 86400;
    int64_t remainder = seconds % 86400;
    if (remainder < 0) {
        remainder += 86400;
        days--;
    }
    result->tm_hour = (int)(remainder / 3600);
    result->tm_min = (int)(remainder / 60 % 60);
    result->tm_sec = (int)(remainder % 60);
    result->tm_wday = (int)((days % 7 + 11) % 7);
    int64_t shifted = days + 719468;
    int64_t era = (shifted >= 0 ? shifted : shifted - 146096) / 146097;
    unsigned day_of_era = (unsigned)(shifted - era * 146097);
    unsigned year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int64_t year = (int64_t)year_of_era + era * 400;
    unsigned day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    unsigned month_index = (5 * day_of_year + 2) / 153;
    unsigned day = day_of_year - (153 * month_index + 2) / 5 + 1;
    unsigned month = month_index < 10 ? month_index + 3 : month_index - 9;
    year += month <= 2;
    result->tm_year = (int)(year - 1900);
    result->tm_mon = (int)month - 1;
    result->tm_mday = (int)day;
    result->tm_yday = (int)(days - days_from_civil(year, 1, 1));
    result->tm_isdst = 0;
    return result;
}

int gettimeofday(struct timeval *result, void *zone) {
    struct timespec now;
    timespec_get(&now, TIME_UTC);
    result->tv_sec = (long)now.tv_sec;
    result->tv_usec = now.tv_nsec / 1000;
    return 0;
}

int clock_gettime(clockid_t clock, struct timespec *result) {
    return timespec_get(result, clock == CLOCK_MONOTONIC ? TIME_MONOTONIC : TIME_UTC) ? 0 : -1;
}

time_t timegm(struct tm *value) {
    int64_t year = (int64_t)value->tm_year + 1900 + value->tm_mon / 12;
    int month = value->tm_mon % 12;
    if (month < 0) {
        month += 12;
        year--;
    }
    int64_t seconds = days_from_civil(year, (unsigned)month + 1, 1) * 86400 + (int64_t)(value->tm_mday - 1) * 86400 +
                      (int64_t)value->tm_hour * 3600 + (int64_t)value->tm_min * 60 + value->tm_sec;
    civil_from_seconds(seconds, value);
    return (time_t)seconds;
}
