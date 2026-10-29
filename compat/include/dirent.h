#ifndef CE_POSIX_DIRENT_H
#define CE_POSIX_DIRENT_H

#include <sys/types.h>

struct dirent {
    ino_t d_ino;
    char d_name[260 * 3 + 1];
};

typedef struct ce_directory DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *directory);
int closedir(DIR *directory);

#endif
