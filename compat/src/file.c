#include <windows.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "internal.h"

#undef fopen
#undef remove
#undef rename

#define DESCRIPTOR_LIMIT 32
#define FIRST_DESCRIPTOR 3

struct ce_directory {
    HANDLE search;
    WIN32_FIND_DATAW found;
    BOOL pending;
    struct dirent entry;
};

static HANDLE descriptors[DESCRIPTOR_LIMIT];

int ce_path_to_wide(const char *path, wchar_t *output, size_t capacity) {
    int length = ce_utf8_to_wide(path, output, capacity);
    if (length < 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (int index = 0; index < length; index++) {
        if (output[index] == L'/') {
            output[index] = L'\\';
        }
    }
    while (length > 1 && output[length - 1] == L'\\') {
        output[--length] = 0;
    }
    return length;
}

void ce_set_errno_from_last_error(void) {
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        errno = ENOENT;
        break;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
        errno = EACCES;
        break;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        errno = EEXIST;
        break;
    case ERROR_DIR_NOT_EMPTY:
        errno = ENOTEMPTY;
        break;
    case ERROR_DISK_FULL:
        errno = ENOSPC;
        break;
    default:
        errno = EIO;
        break;
    }
}

int64_t ce_unix_time_from_filetime(uint32_t low, uint32_t high) {
    int64_t intervals = ((int64_t)high << 32) | low;
    return intervals / 10000000 - 11644473600LL;
}

void ce_filetime_from_unix_time(int64_t seconds, uint32_t *low, uint32_t *high) {
    int64_t intervals = (seconds + 11644473600LL) * 10000000;
    *low = (uint32_t)intervals;
    *high = (uint32_t)(intervals >> 32);
}

FILE *ce_fopen(const char *path, const char *mode) {
    wchar_t wide_path[CE_PATH_LENGTH];
    wchar_t wide_mode[8];
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0 || ce_utf8_to_wide(mode, wide_mode, 8) < 0) {
        return NULL;
    }
    FILE *stream = _wfopen(wide_path, wide_mode);
    if (!stream) {
        errno = ENOENT;
    }
    return stream;
}

int ce_remove(const char *path) {
    return unlink(path);
}

int ce_rename(const char *existing, const char *replacement) {
    wchar_t from[CE_PATH_LENGTH];
    wchar_t to[CE_PATH_LENGTH];
    if (ce_path_to_wide(existing, from, CE_PATH_LENGTH) < 0 || ce_path_to_wide(replacement, to, CE_PATH_LENGTH) < 0) {
        return -1;
    }
    DeleteFileW(to);
    if (!MoveFileW(from, to)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    return 0;
}

int ce_fileno(FILE *stream) {
    return -1;
}

FILE *ce_fdopen(int descriptor, const char *mode) {
    errno = ENOTSUP;
    return NULL;
}

static int allocate_descriptor(HANDLE handle) {
    for (int index = FIRST_DESCRIPTOR; index < DESCRIPTOR_LIMIT; index++) {
        if (!descriptors[index]) {
            descriptors[index] = handle;
            return index;
        }
    }
    CloseHandle(handle);
    errno = EMFILE;
    return -1;
}

static HANDLE descriptor_handle(int descriptor) {
    if (descriptor < FIRST_DESCRIPTOR || descriptor >= DESCRIPTOR_LIMIT || !descriptors[descriptor]) {
        errno = EBADF;
        return NULL;
    }
    return descriptors[descriptor];
}

int open(const char *path, int flags, ...) {
    wchar_t wide_path[CE_PATH_LENGTH];
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0) {
        return -1;
    }
    DWORD access = 0;
    switch (flags & O_ACCMODE) {
    case O_RDONLY:
        access = GENERIC_READ;
        break;
    case O_WRONLY:
        access = GENERIC_WRITE;
        break;
    default:
        access = GENERIC_READ | GENERIC_WRITE;
        break;
    }
    DWORD disposition = OPEN_EXISTING;
    if ((flags & O_CREAT) && (flags & O_EXCL)) {
        disposition = CREATE_NEW;
    } else if ((flags & O_CREAT) && (flags & O_TRUNC)) {
        disposition = CREATE_ALWAYS;
    } else if (flags & O_CREAT) {
        disposition = OPEN_ALWAYS;
    } else if (flags & O_TRUNC) {
        disposition = TRUNCATE_EXISTING;
    }
    HANDLE handle = CreateFileW(wide_path, access, FILE_SHARE_READ, NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        ce_set_errno_from_last_error();
        return -1;
    }
    if (flags & O_APPEND) {
        SetFilePointer(handle, 0, NULL, FILE_END);
    }
    return allocate_descriptor(handle);
}

ssize_t read(int descriptor, void *buffer, size_t count) {
    HANDLE handle = descriptor_handle(descriptor);
    DWORD done = 0;
    if (!handle) {
        return -1;
    }
    if (!ReadFile(handle, buffer, (DWORD)count, &done, NULL)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    return (ssize_t)done;
}

ssize_t write(int descriptor, const void *buffer, size_t count) {
    if (descriptor == STDOUT_FILENO || descriptor == STDERR_FILENO) {
        ce_log_write(buffer, count);
        return (ssize_t)count;
    }
    HANDLE handle = descriptor_handle(descriptor);
    DWORD done = 0;
    if (!handle) {
        return -1;
    }
    if (!WriteFile(handle, buffer, (DWORD)count, &done, NULL)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    return (ssize_t)done;
}

off_t lseek(int descriptor, off_t offset, int origin) {
    HANDLE handle = descriptor_handle(descriptor);
    if (!handle) {
        return -1;
    }
    DWORD method = origin == SEEK_SET ? FILE_BEGIN : origin == SEEK_CUR ? FILE_CURRENT : FILE_END;
    DWORD position = SetFilePointer(handle, offset, NULL, method);
    if (position == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
        ce_set_errno_from_last_error();
        return -1;
    }
    return (off_t)position;
}

ssize_t pread(int descriptor, void *buffer, size_t count, off_t offset) {
    if (lseek(descriptor, offset, SEEK_SET) < 0) {
        return -1;
    }
    return read(descriptor, buffer, count);
}

ssize_t pwrite(int descriptor, const void *buffer, size_t count, off_t offset) {
    if (lseek(descriptor, offset, SEEK_SET) < 0) {
        return -1;
    }
    return write(descriptor, buffer, count);
}

int close(int descriptor) {
    HANDLE handle = descriptor_handle(descriptor);
    if (!handle) {
        return -1;
    }
    CloseHandle(handle);
    descriptors[descriptor] = NULL;
    return 0;
}

static void fill_stat(struct stat *result, DWORD attributes, DWORD size, const FILETIME *modified) {
    memset(result, 0, sizeof *result);
    result->st_mode = (attributes & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR | 0755 : S_IFREG | 0644;
    if (attributes & FILE_ATTRIBUTE_READONLY) {
        result->st_mode &= ~0222;
    }
    result->st_nlink = 1;
    result->st_size = (off_t)size;
    result->st_mtime = (time_t)ce_unix_time_from_filetime(modified->dwLowDateTime, modified->dwHighDateTime);
    result->st_atime = result->st_mtime;
    result->st_ctime = result->st_mtime;
}

int stat(const char *path, struct stat *result) {
    wchar_t wide_path[CE_PATH_LENGTH];
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0) {
        return -1;
    }
    if (!GetFileAttributesExW(wide_path, GetFileExInfoStandard, &data)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    fill_stat(result, data.dwFileAttributes, data.nFileSizeLow, &data.ftLastWriteTime);
    return 0;
}

int fstat(int descriptor, struct stat *result) {
    HANDLE handle = descriptor_handle(descriptor);
    BY_HANDLE_FILE_INFORMATION information;
    if (!handle) {
        return -1;
    }
    if (!GetFileInformationByHandle(handle, &information)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    fill_stat(result, information.dwFileAttributes, information.nFileSizeLow, &information.ftLastWriteTime);
    return 0;
}

int mkdir(const char *path, mode_t mode) {
    wchar_t wide_path[CE_PATH_LENGTH];
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0) {
        return -1;
    }
    if (!CreateDirectoryW(wide_path, NULL)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    return 0;
}

int rmdir(const char *path) {
    wchar_t wide_path[CE_PATH_LENGTH];
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0) {
        return -1;
    }
    if (!RemoveDirectoryW(wide_path)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    return 0;
}

int unlink(const char *path) {
    wchar_t wide_path[CE_PATH_LENGTH];
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0) {
        return -1;
    }
    if (!DeleteFileW(wide_path)) {
        ce_set_errno_from_last_error();
        return -1;
    }
    return 0;
}

int access(const char *path, int mode) {
    wchar_t wide_path[CE_PATH_LENGTH];
    if (ce_path_to_wide(path, wide_path, CE_PATH_LENGTH) < 0) {
        return -1;
    }
    DWORD attributes = GetFileAttributesW(wide_path);
    if (attributes == 0xFFFFFFFF) {
        ce_set_errno_from_last_error();
        return -1;
    }
    if ((mode & W_OK) && (attributes & FILE_ATTRIBUTE_READONLY)) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

int isatty(int descriptor) {
    return 0;
}

char *getcwd(char *buffer, size_t size) {
    if (size < 2) {
        errno = ERANGE;
        return NULL;
    }
    strcpy(buffer, "/");
    return buffer;
}

int mkstemp(char *pattern) {
    size_t length = strlen(pattern);
    if (length < 6 || strcmp(pattern + length - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    for (unsigned attempt = 0; attempt < 100; attempt++) {
        unsigned value = (unsigned)GetTickCount() + attempt * 7919;
        for (int index = 0; index < 6; index++) {
            pattern[length - 6 + index] = "abcdefghijklmnopqrstuvwxyz0123456789"[value % 36];
            value /= 36;
        }
        int descriptor = open(pattern, O_RDWR | O_CREAT | O_EXCL);
        if (descriptor >= 0 || errno != EEXIST) {
            return descriptor;
        }
    }
    return -1;
}

DIR *opendir(const char *path) {
    wchar_t pattern[CE_PATH_LENGTH];
    int length = ce_path_to_wide(path, pattern, CE_PATH_LENGTH - 2);
    if (length < 0) {
        return NULL;
    }
    wcscpy(pattern + length, length && pattern[length - 1] == L'\\' ? L"*" : L"\\*");
    DIR *directory = calloc(1, sizeof *directory);
    if (!directory) {
        errno = ENOMEM;
        return NULL;
    }
    directory->search = FindFirstFileW(pattern, &directory->found);
    if (directory->search == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error != ERROR_NO_MORE_FILES && error != ERROR_FILE_NOT_FOUND) {
            ce_set_errno_from_last_error();
            free(directory);
            return NULL;
        }
    } else {
        directory->pending = TRUE;
    }
    return directory;
}

struct dirent *readdir(DIR *directory) {
    if (!directory->pending) {
        if (directory->search == INVALID_HANDLE_VALUE || !FindNextFileW(directory->search, &directory->found)) {
            return NULL;
        }
    }
    directory->pending = FALSE;
    if (ce_wide_to_utf8(directory->found.cFileName, directory->entry.d_name, sizeof directory->entry.d_name) < 0) {
        directory->entry.d_name[0] = 0;
    }
    return &directory->entry;
}

int closedir(DIR *directory) {
    if (directory->search != INVALID_HANDLE_VALUE) {
        FindClose(directory->search);
    }
    free(directory);
    return 0;
}
