#ifndef CE_POSIX_STDIO_H
#define CE_POSIX_STDIO_H

#include_next <stdio.h>
#include <sys/types.h>

#define BUFSIZ 1024
#define FILENAME_MAX 260
#define L_tmpnam 260

int ce_printf(const char *format, ...);
int ce_vprintf(const char *format, va_list arguments);
int ce_fprintf(FILE *stream, const char *format, ...);
int ce_vfprintf(FILE *stream, const char *format, va_list arguments);
int ce_sprintf(char *buffer, const char *format, ...);
int ce_vsprintf(char *buffer, const char *format, va_list arguments);
int ce_snprintf(char *buffer, size_t count, const char *format, ...);
int ce_vsnprintf(char *buffer, size_t count, const char *format, va_list arguments);
int ce_vasprintf(char **result, const char *format, va_list arguments);
int ce_asprintf(char **result, const char *format, ...);
FILE *ce_fopen(const char *path, const char *mode);
int ce_remove(const char *path);
int ce_rename(const char *existing, const char *replacement);
int ce_fileno(FILE *stream);
FILE *ce_fdopen(int descriptor, const char *mode);
void ce_perror(const char *message);
int ce_fputc(int character, FILE *stream);
int ce_fputs(const char *text, FILE *stream);
void setbuf(FILE *stream, char *buffer);

#define printf(...) ce_printf(__VA_ARGS__)
#define vprintf(...) ce_vprintf(__VA_ARGS__)
#define fprintf(...) ce_fprintf(__VA_ARGS__)
#define vfprintf(...) ce_vfprintf(__VA_ARGS__)
#define sprintf(...) ce_sprintf(__VA_ARGS__)
#define vsprintf(...) ce_vsprintf(__VA_ARGS__)
#define snprintf(...) ce_snprintf(__VA_ARGS__)
#define vsnprintf(...) ce_vsnprintf(__VA_ARGS__)
#define vasprintf(...) ce_vasprintf(__VA_ARGS__)
#define asprintf(...) ce_asprintf(__VA_ARGS__)
#define fopen(...) ce_fopen(__VA_ARGS__)
#define remove(...) ce_remove(__VA_ARGS__)
#define rename(...) ce_rename(__VA_ARGS__)
#undef fileno
#define fileno(...) ce_fileno(__VA_ARGS__)
#define fdopen(...) ce_fdopen(__VA_ARGS__)
#define perror(...) ce_perror(__VA_ARGS__)
#undef putc
#define fputc(...) ce_fputc(__VA_ARGS__)
#define putc(...) ce_fputc(__VA_ARGS__)
#define fputs(...) ce_fputs(__VA_ARGS__)

#endif
