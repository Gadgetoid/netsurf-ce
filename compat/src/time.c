#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "internal.h"

#define CLOCK_CHECK_INTERVAL_MS 1000

static const char *const weekday_names[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *const month_names[] = { "January", "February", "March", "April", "May", "June",
                                           "July", "August", "September", "October", "November", "December" };

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

static int64_t local_offset_seconds(void) {
    TIME_ZONE_INFORMATION zone;
    DWORD state = GetTimeZoneInformation(&zone);
    LONG bias = zone.Bias;
    if (state == TIME_ZONE_ID_DAYLIGHT) {
        bias += zone.DaylightBias;
    } else if (state == TIME_ZONE_ID_STANDARD) {
        bias += zone.StandardBias;
    }
    return -(int64_t)bias * 60;
}

static int64_t current_unix_time(void) {
    SYSTEMTIME now;
    FILETIME file_time;
    GetSystemTime(&now);
    SystemTimeToFileTime(&now, &file_time);
    return ce_unix_time_from_filetime(file_time.dwLowDateTime, file_time.dwHighDateTime);
}

time_t time(time_t *result) {
    time_t now = (time_t)current_unix_time();
    if (result) {
        *result = now;
    }
    return now;
}

clock_t clock(void) {
    return (clock_t)GetTickCount();
}

static int64_t precise_unix_milliseconds(void) {
    static int64_t base_milliseconds;
    static DWORD base_ticks;
    static int based;
    static DWORD checked_ticks;
    DWORD ticks = GetTickCount();
    int64_t estimate = base_milliseconds + (int64_t)(DWORD)(ticks - base_ticks);
    if (based && (DWORD)(ticks - checked_ticks) < CLOCK_CHECK_INTERVAL_MS) {
        return estimate;
    }
    checked_ticks = ticks;
    int64_t system = current_unix_time() * 1000;
    if (!based || estimate < system - 2000 || estimate > system + 3000) {
        base_milliseconds = system;
        base_ticks = ticks;
        based = 1;
        return system;
    }
    return estimate;
}

int gettimeofday(struct timeval *result, void *zone) {
    int64_t now = precise_unix_milliseconds();
    result->tv_sec = (long)(now / 1000);
    result->tv_usec = (long)(now % 1000) * 1000;
    return 0;
}

int clock_gettime(clockid_t clock, struct timespec *result) {
    if (clock == CLOCK_MONOTONIC) {
        DWORD ticks = GetTickCount();
        result->tv_sec = ticks / 1000;
        result->tv_nsec = (long)(ticks % 1000) * 1000000;
    } else {
        int64_t now = precise_unix_milliseconds();
        result->tv_sec = (time_t)(now / 1000);
        result->tv_nsec = (long)(now % 1000) * 1000000;
    }
    return 0;
}

struct tm *gmtime_r(const time_t *value, struct tm *result) {
    return civil_from_seconds((int64_t)*value, result);
}

struct tm *gmtime(const time_t *value) {
    static struct tm result;
    return gmtime_r(value, &result);
}

struct tm *localtime_r(const time_t *value, struct tm *result) {
    return civil_from_seconds((int64_t)*value + local_offset_seconds(), result);
}

struct tm *localtime(const time_t *value) {
    static struct tm result;
    return localtime_r(value, &result);
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

time_t mktime(struct tm *value) {
    struct tm copy = *value;
    int64_t seconds = (int64_t)timegm(&copy) - local_offset_seconds();
    localtime_r(&(time_t){ (time_t)seconds }, value);
    return (time_t)seconds;
}

static size_t append(char *buffer, size_t size, size_t used, const char *text) {
    size_t length = strlen(text);
    if (used + length >= size) {
        return (size_t)-1;
    }
    memcpy(buffer + used, text, length);
    return used + length;
}

size_t strftime(char *buffer, size_t size, const char *format, const struct tm *value) {
    size_t used = 0;
    char field[64];
    if (!size) {
        return 0;
    }
    for (; *format && used != (size_t)-1; format++) {
        if (*format != '%' || !format[1]) {
            field[0] = *format;
            field[1] = 0;
            used = append(buffer, size, used, field);
            continue;
        }
        switch (*++format) {
        case 'a':
            snprintf(field, sizeof field, "%.3s", weekday_names[value->tm_wday % 7]);
            break;
        case 'A':
            snprintf(field, sizeof field, "%s", weekday_names[value->tm_wday % 7]);
            break;
        case 'b':
        case 'h':
            snprintf(field, sizeof field, "%.3s", month_names[value->tm_mon % 12]);
            break;
        case 'B':
            snprintf(field, sizeof field, "%s", month_names[value->tm_mon % 12]);
            break;
        case 'c':
            snprintf(field, sizeof field, "%.3s %.3s %2d %02d:%02d:%02d %d", weekday_names[value->tm_wday % 7],
                     month_names[value->tm_mon % 12], value->tm_mday, value->tm_hour, value->tm_min, value->tm_sec,
                     value->tm_year + 1900);
            break;
        case 'd':
            snprintf(field, sizeof field, "%02d", value->tm_mday);
            break;
        case 'e':
            snprintf(field, sizeof field, "%2d", value->tm_mday);
            break;
        case 'F':
            snprintf(field, sizeof field, "%d-%02d-%02d", value->tm_year + 1900, value->tm_mon + 1, value->tm_mday);
            break;
        case 'H':
            snprintf(field, sizeof field, "%02d", value->tm_hour);
            break;
        case 'I':
            snprintf(field, sizeof field, "%02d", value->tm_hour % 12 ? value->tm_hour % 12 : 12);
            break;
        case 'j':
            snprintf(field, sizeof field, "%03d", value->tm_yday + 1);
            break;
        case 'm':
            snprintf(field, sizeof field, "%02d", value->tm_mon + 1);
            break;
        case 'M':
            snprintf(field, sizeof field, "%02d", value->tm_min);
            break;
        case 'p':
            snprintf(field, sizeof field, "%s", value->tm_hour < 12 ? "AM" : "PM");
            break;
        case 'S':
            snprintf(field, sizeof field, "%02d", value->tm_sec);
            break;
        case 'T':
            snprintf(field, sizeof field, "%02d:%02d:%02d", value->tm_hour, value->tm_min, value->tm_sec);
            break;
        case 'y':
            snprintf(field, sizeof field, "%02d", (value->tm_year + 1900) % 100);
            break;
        case 'Y':
            snprintf(field, sizeof field, "%d", value->tm_year + 1900);
            break;
        case 'z':
            snprintf(field, sizeof field, "+0000");
            break;
        case 'Z':
            snprintf(field, sizeof field, "GMT");
            break;
        case '%':
            snprintf(field, sizeof field, "%%");
            break;
        default:
            snprintf(field, sizeof field, "%%%c", *format);
            break;
        }
        used = append(buffer, size, used, field);
    }
    if (used == (size_t)-1) {
        buffer[0] = 0;
        return 0;
    }
    buffer[used] = 0;
    return used;
}
