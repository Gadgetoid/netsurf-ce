#include <windows.h>

#include <ce_posix.h>
#include <stdint.h>

#include "display.h"
#include "profile.h"

#define SAMPLE_INTERVAL_MS 2
#define SAMPLES_PER_WRITE 512
#define STACK_WORDS 1024
#define CALLERS 8
#define IMAGE_START 0x11000
#define IMAGE_END 0x400000

struct sample {
    DWORD pc;
    DWORD lr;
    DWORD waiting;
    DWORD cpu;
    DWORD callers[CALLERS];
};

static HANDLE target_thread;
static HANDLE output;
static DWORD stack_top;

static DWORD WINAPI sample_target(LPVOID parameter) {
    struct sample samples[SAMPLES_PER_WRITE];
    unsigned count = 0;
    CONTEXT context;
    DWORD written;
    uint64_t previous_cpu = 0;

    for (;;) {
        Sleep(SAMPLE_INTERVAL_MS);
        if (SuspendThread(target_thread) == 0xFFFFFFFF) {
            break;
        }
        context.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(target_thread, &context)) {
            struct sample *sample = &samples[count++];
            const DWORD *stack = (const DWORD *)context.Sp;
            unsigned found = 0;
            sample->pc = context.Pc;
            sample->lr = context.Lr;
            sample->waiting = (DWORD)ce_display_waiting;
            FILETIME created, exited, kernel, user;
            sample->cpu = 0;
            if (GetThreadTimes(target_thread, &created, &exited, &kernel, &user)) {
                uint64_t cpu = ((uint64_t)kernel.dwHighDateTime << 32 | kernel.dwLowDateTime) +
                               ((uint64_t)user.dwHighDateTime << 32 | user.dwLowDateTime);
                sample->cpu = (DWORD)(cpu - previous_cpu);
                previous_cpu = cpu;
            }
            for (unsigned index = 0; index < STACK_WORDS && found < CALLERS; index++) {
                if ((DWORD)&stack[index] >= stack_top) {
                    break;
                }
                DWORD word = stack[index];
                if (word >= IMAGE_START && word < IMAGE_END && !(word & 3)) {
                    sample->callers[found++] = word;
                }
            }
            while (found < CALLERS) {
                sample->callers[found++] = 0;
            }
        }
        ResumeThread(target_thread);
        if (count == SAMPLES_PER_WRITE) {
            WriteFile(output, samples, sizeof samples, &written, NULL);
            FlushFileBuffers(output);
            count = 0;
        }
    }
    return 0;
}

void ce_profile_start(const char *path) {
    wchar_t wide_path[MAX_PATH];
    stack_top = (DWORD)wide_path;
    HANDLE process = GetCurrentProcess();
    if (ce_path_to_wide(path, wide_path, MAX_PATH) < 0) {
        return;
    }
    output = CreateFileW(wide_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (output == INVALID_HANDLE_VALUE) {
        return;
    }
    if (!DuplicateHandle(process, GetCurrentThread(), process, &target_thread, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        return;
    }
    HANDLE sampler = CreateThread(NULL, 0, sample_target, NULL, 0, NULL);
    if (sampler) {
        SetThreadPriority(sampler, THREAD_PRIORITY_HIGHEST);
        CloseHandle(sampler);
    }
}
