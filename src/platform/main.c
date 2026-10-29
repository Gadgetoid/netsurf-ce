#include <windows.h>

#include <ce_posix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "display.h"
#include "http.h"
#include "profile.h"

#define PATH_LENGTH 780
#define ARGUMENT_LIMIT 16

int netsurf_framebuffer_main(int argc, char **argv);

static bool find_install_directory(char *directory, size_t capacity) {
    wchar_t module[MAX_PATH];
    if (!GetModuleFileNameW(NULL, module, MAX_PATH)) {
        return false;
    }
    wchar_t *separator = wcsrchr(module, L'\\');
    if (separator) {
        *separator = 0;
    }
    if (ce_wide_to_utf8(module, directory, capacity) < 0) {
        return false;
    }
    for (char *character = directory; *character; character++) {
        if (*character == '\\') {
            *character = '/';
        }
    }
    return true;
}

static char *join_path(const char *directory, const char *name) {
    size_t length = strlen(directory) + strlen(name) + 2;
    char *path = malloc(length);
    if (path) {
        snprintf(path, length, "%s/%s", directory, name);
    }
    return path;
}

static char *option_argument(const char *name, const char *value) {
    size_t length = strlen(name) + strlen(value) + 4;
    char *argument = malloc(length);
    if (argument) {
        snprintf(argument, length, "--%s=%s", name, value);
    }
    return argument;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPWSTR command_line, int show) {
    char directory[PATH_LENGTH];
    char url[PATH_LENGTH];
    char *arguments[ARGUMENT_LIMIT];
    int count = 0;

    HWND existing = FindWindowW(L"NetSurf", NULL);
    if (existing) {
        SetForegroundWindow(existing);
        return 0;
    }
    if (!find_install_directory(directory, sizeof directory)) {
        return 1;
    }
    char *log_path = join_path(directory, "NetSurf.log");
    char *cookies = join_path(directory, "Cookies");
    if (!log_path || !cookies) {
        return 1;
    }
    ce_log_open(log_path);
#ifdef NETSURF_CE_PROFILE
    ce_profile_start(join_path(directory, "profile.bin"));
#endif
    setenv("NETSURFRES", directory, 1);
    if (!ce_http_initialise(ce_display_wake)) {
        MessageBoxW(NULL, L"The network could not be started.", L"NetSurf", MB_OK | MB_ICONERROR);
        return 1;
    }

    char *word = NULL;
    if (command_line && command_line[0] && ce_wide_to_utf8(command_line, url, sizeof url) > 0) {
        word = url;
        while (*word == ' ') {
            word++;
        }
    }
    arguments[count++] = "NetSurf";
    if (word && strncmp(word, "-v", 2) == 0 && (word[2] == ' ' || word[2] == 0)) {
        arguments[count++] = "-v";
        word += 2;
        while (*word == ' ') {
            word++;
        }
    }
    arguments[count++] = option_argument("cookie_file", cookies);
    arguments[count++] = option_argument("cookie_jar", cookies);
    arguments[count++] = "-f";
    arguments[count++] = "ce";
    arguments[count++] = "-b";
    arguments[count++] = "16";
    if (word && *word) {
        arguments[count++] = word;
    }
    arguments[count] = NULL;

    int result = netsurf_framebuffer_main(count, arguments);
    ce_http_finalise();
    return result;
}
