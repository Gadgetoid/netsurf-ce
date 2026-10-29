#include <windows.h>

#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "internal.h"

#define EXIT_HANDLER_LIMIT 32
#define ENVIRONMENT_LIMIT 32

struct environment_entry {
    char *name;
    char *value;
};

static void (*exit_handlers[EXIT_HANDLER_LIMIT])(void);
static int exit_handler_count;
static struct environment_entry environment[ENVIRONMENT_LIMIT];

int atexit(void (*function)(void)) {
    if (exit_handler_count == EXIT_HANDLER_LIMIT) {
        return -1;
    }
    exit_handlers[exit_handler_count++] = function;
    return 0;
}

void _Exit(int status) {
    TerminateProcess(GetCurrentProcess(), (UINT)status);
    for (;;) {
    }
}

void exit(int status) {
    while (exit_handler_count) {
        exit_handlers[--exit_handler_count]();
    }
    _Exit(status);
}

void abort(void) {
    static const char message[] = "abort\n";
    ce_log_write(message, sizeof message - 1);
    _Exit(3);
}

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

static struct environment_entry *find_environment(const char *name) {
    for (int index = 0; index < ENVIRONMENT_LIMIT; index++) {
        if (environment[index].name && strcmp(environment[index].name, name) == 0) {
            return &environment[index];
        }
    }
    return NULL;
}

char *getenv(const char *name) {
    struct environment_entry *entry = find_environment(name);
    return entry ? entry->value : NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
    struct environment_entry *entry = find_environment(name);
    if (entry && !overwrite) {
        return 0;
    }
    char *copy = strdup(value);
    if (!copy) {
        errno = ENOMEM;
        return -1;
    }
    if (!entry) {
        for (int index = 0; index < ENVIRONMENT_LIMIT && !entry; index++) {
            if (!environment[index].name) {
                entry = &environment[index];
                entry->name = strdup(name);
                if (!entry->name) {
                    free(copy);
                    errno = ENOMEM;
                    return -1;
                }
            }
        }
        if (!entry) {
            free(copy);
            errno = ENOMEM;
            return -1;
        }
    }
    free(entry->value);
    entry->value = copy;
    return 0;
}

int unsetenv(const char *name) {
    struct environment_entry *entry = find_environment(name);
    if (entry) {
        free(entry->name);
        free(entry->value);
        entry->name = NULL;
        entry->value = NULL;
    }
    return 0;
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

char *setlocale(int category, const char *locale) {
    return "C";
}

void setbuf(FILE *stream, char *buffer) {
}
