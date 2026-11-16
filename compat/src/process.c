#include <windows.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "internal.h"

void ce_assert_failed(const char *expression, const char *file, int line) {
    char text[512];
    wchar_t wide[512];
    int length = snprintf(text, sizeof text, "Assertion failed: %s (%s:%d)\n", expression, file, line);
    ce_log_write(text, (size_t)length);
    if (ce_utf8_to_wide(text, wide, sizeof wide / sizeof wide[0]) >= 0) {
        MessageBoxW(NULL, wide, L"NetSurf", MB_OK | MB_ICONERROR);
    }
    abort();
}

pid_t getpid(void) {
    return (pid_t)GetCurrentProcessId();
}

uid_t getuid(void) {
    return 0;
}

unsigned int sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
    return 0;
}

int usleep(unsigned int microseconds) {
    Sleep((microseconds + 999) / 1000);
    return 0;
}
