#ifndef CE_POSIX_UNISTD_H
#define CE_POSIX_UNISTD_H

#include <sys/types.h>

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

ssize_t read(int descriptor, void *buffer, size_t count);
ssize_t write(int descriptor, const void *buffer, size_t count);
off_t lseek(int descriptor, off_t offset, int origin);
ssize_t pread(int descriptor, void *buffer, size_t count, off_t offset);
ssize_t pwrite(int descriptor, const void *buffer, size_t count, off_t offset);
int close(int descriptor);
int access(const char *path, int mode);
int unlink(const char *path);
int rmdir(const char *path);
int isatty(int descriptor);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int microseconds);
char *getcwd(char *buffer, size_t size);
pid_t getpid(void);
uid_t getuid(void);

extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char *const argv[], const char *options);

#endif
