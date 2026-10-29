#include <windows.h>

#include <ce_posix.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <printf/printf.h>

#include "internal.h"

#undef fopen
#undef fputc
#undef fputs

#define OUTPUT_BUFFER_SIZE 256

struct stream_output {
    FILE *stream;
    int log;
    char buffer[OUTPUT_BUFFER_SIZE];
    size_t used;
};

static CRITICAL_SECTION log_lock;
static HANDLE log_file = INVALID_HANDLE_VALUE;
static int log_ready;

void putchar_(char character) {
    ce_log_write(&character, 1);
}

void ce_log_open(const char *path) {
    wchar_t wide_path[CE_PATH_LENGTH];
    if (!log_ready) {
        InitializeCriticalSection(&log_lock);
        log_ready = 1;
    }
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0) {
        return;
    }
    EnterCriticalSection(&log_lock);
    if (log_file != INVALID_HANDLE_VALUE) {
        CloseHandle(log_file);
    }
    log_file = CreateFileW(wide_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    LeaveCriticalSection(&log_lock);
}

void ce_log_write(const char *text, size_t length) {
    DWORD written;
    if (!log_ready) {
        return;
    }
    EnterCriticalSection(&log_lock);
    if (log_file != INVALID_HANDLE_VALUE) {
        WriteFile(log_file, text, (DWORD)length, &written, NULL);
    }
    LeaveCriticalSection(&log_lock);
}

static int is_standard_stream(FILE *stream) {
    return stream == stdout || stream == stderr;
}

static void flush_output(struct stream_output *output) {
    if (output->log) {
        ce_log_write(output->buffer, output->used);
    } else if (output->stream) {
        fwrite(output->buffer, 1, output->used, output->stream);
    }
    output->used = 0;
}

static void put_output(char character, void *context) {
    struct stream_output *output = context;
    output->buffer[output->used++] = character;
    if (output->used == OUTPUT_BUFFER_SIZE) {
        flush_output(output);
    }
}

int ce_vfprintf(FILE *stream, const char *format, va_list arguments) {
    struct stream_output output = { .stream = stream, .log = is_standard_stream(stream) };
    int written = vfctprintf(put_output, &output, format, arguments);
    flush_output(&output);
    return written;
}

int ce_fprintf(FILE *stream, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int written = ce_vfprintf(stream, format, arguments);
    va_end(arguments);
    return written;
}

int ce_vprintf(const char *format, va_list arguments) {
    return ce_vfprintf(stdout, format, arguments);
}

int ce_printf(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int written = ce_vfprintf(stdout, format, arguments);
    va_end(arguments);
    return written;
}

int ce_vsnprintf(char *buffer, size_t count, const char *format, va_list arguments) {
    return vsnprintf_(buffer, count, format, arguments);
}

int ce_snprintf(char *buffer, size_t count, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf_(buffer, count, format, arguments);
    va_end(arguments);
    return written;
}

int ce_vsprintf(char *buffer, const char *format, va_list arguments) {
    return vsprintf_(buffer, format, arguments);
}

int ce_sprintf(char *buffer, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int written = vsprintf_(buffer, format, arguments);
    va_end(arguments);
    return written;
}

int ce_vasprintf(char **result, const char *format, va_list arguments) {
    va_list measure;
    va_copy(measure, arguments);
    int length = vsnprintf_(NULL, 0, format, measure);
    va_end(measure);
    *result = NULL;
    if (length < 0) {
        return -1;
    }
    *result = malloc((size_t)length + 1);
    if (!*result) {
        errno = ENOMEM;
        return -1;
    }
    return vsnprintf_(*result, (size_t)length + 1, format, arguments);
}

int ce_asprintf(char **result, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int written = ce_vasprintf(result, format, arguments);
    va_end(arguments);
    return written;
}

int ce_fputc(int character, FILE *stream) {
    if (is_standard_stream(stream)) {
        char text = (char)character;
        ce_log_write(&text, 1);
        return character & 0xFF;
    }
    return fputc(character, stream);
}

int ce_fputs(const char *text, FILE *stream) {
    if (is_standard_stream(stream)) {
        ce_log_write(text, strlen(text));
        return 0;
    }
    return fputs(text, stream);
}

void ce_perror(const char *message) {
    ce_fprintf(stderr, "%s: %s\n", message ? message : "", strerror(errno));
}
