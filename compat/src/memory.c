#include <windows.h>

#include <errno.h>

#define WIN32 1
#define USE_LOCKS 2
#define MLOCK_T CRITICAL_SECTION
#define INITIAL_LOCK(lock) (InitializeCriticalSection(lock), 0)
#define DESTROY_LOCK(lock) (DeleteCriticalSection(lock), 0)
#define ACQUIRE_LOCK(lock) (EnterCriticalSection(lock), 0)
#define RELEASE_LOCK(lock) LeaveCriticalSection(lock)
#define TRY_LOCK(lock) (EnterCriticalSection(lock), 1)
#define ACQUIRE_MALLOC_GLOBAL_LOCK()
#define RELEASE_MALLOC_GLOBAL_LOCK()
#define HAVE_MORECORE 0
#define NO_MALLINFO 1
#define NO_MALLOC_STATS 1

#include <dlmalloc/malloc.c>
