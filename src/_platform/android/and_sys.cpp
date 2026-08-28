// ============================================================================
// KisakCOD Android port — low-level platform services (see and_sys.h)
// ============================================================================

#include "and_sys.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__ANDROID__)
#include <android/log.h>
#include <sys/mman.h>
#else
#include <sys/mman.h>
#endif

#if defined(_WIN32)
#error "and_sys.cpp must not be compiled on Windows"
#endif

// ---- time -------------------------------------------------------------------

static uint32_t s_timeBase = 0;
static bool s_timeBaseInit = false;

static uint64_t nowNs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uint32_t AndroidSys_Milliseconds(void) {
    uint64_t ns = nowNs();
    if (!s_timeBaseInit) {
        s_timeBase = (uint32_t)(ns / 1000000ull);
        s_timeBaseInit = true;
    }
    return (uint32_t)(ns / 1000000ull) - s_timeBase;
}

uint32_t AndroidSys_MillisecondsRaw(void) {
    return (uint32_t)(nowNs() / 1000000ull);
}

int64_t AndroidSys_PerfCounter(void) {
    return (int64_t)nowNs();
}

int64_t AndroidSys_PerfFrequency(void) {
    return 1000000000ll;
}

void AndroidSys_Sleep(unsigned ms) {
    usleep((useconds_t)ms * 1000u);
}

// ---- data dir ----------------------------------------------------------------

static char s_dataDir[1024] = { 0 };

void AndroidSys_SetDataDir(const char *dir) {
    if (!dir) return;
    strncpy(s_dataDir, dir, sizeof(s_dataDir) - 1);
    s_dataDir[sizeof(s_dataDir) - 1] = 0;
    // strip trailing slash
    size_t n = strlen(s_dataDir);
    while (n > 1 && s_dataDir[n - 1] == '/') {
        s_dataDir[--n] = 0;
    }
}

const char *AndroidSys_DataDir(void) {
    if (s_dataDir[0] == 0) return "/";
    return s_dataDir;
}

void AndroidSys_GetCwd(char *buf, size_t len) {
    if (!buf || len == 0) return;
    // On Android the process cwd is unusable; game data lives in the app dir.
    strncpy(buf, AndroidSys_DataDir(), len - 1);
    buf[len - 1] = 0;
}

// ---- logging -----------------------------------------------------------------

void AndroidSys_Log(const char *tag, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, tag ? tag : "kisakcod", "%s", buf);
#else
    printf("[%s] %s\n", tag ? tag : "kisakcod", buf);
    fflush(stdout);
#endif
}

// ---- WinAPI-compat shims -----------------------------------------------------

// PAGE_READWRITE etc.
enum {
    KISAK_PAGE_READWRITE = 0x04,
    KISAK_MEM_COMMIT     = 0x1000,
    KISAK_MEM_RESERVE    = 0x2000,
    KISAK_MEM_RELEASE    = 0x8000,
};

void *AndroidSys_VirtualAlloc(void *addr, size_t size,
                              unsigned allocType, unsigned protect) {
    (void)addr; (void)allocType;
    if (size == 0) return NULL;
    if (protect != KISAK_PAGE_READWRITE) {
        // engine only uses PAGE_READWRITE for its hunk/zone buffers
        protect = KISAK_PAGE_READWRITE;
    }
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

bool AndroidSys_VirtualFree(void *addr, size_t size, unsigned freeType) {
    if (!addr) return false;
    if (freeType == KISAK_MEM_RELEASE) {
        return munmap(addr, size) == 0;
    }
    // MEM_DECOMMIT — no-op on this map model
    return true;
}

bool AndroidSys_VirtualProtect(void *addr, size_t size, unsigned protect,
                               unsigned *oldProtect) {
    (void)addr; (void)size; (void)protect;
    if (oldProtect) *oldProtect = KISAK_PAGE_READWRITE;
    return true;
}

// ---- threads -----------------------------------------------------------------

struct AndroidThreadImpl {
    pthread_t handle;
};

typedef void *(*pthread_fn)(void *);

AndroidThread *AndroidSys_CreateThread(void *(*fn)(void *), void *arg) {
    AndroidThreadImpl *t = (AndroidThreadImpl *)calloc(1, sizeof(AndroidThreadImpl));
    if (!t) return NULL;
    if (pthread_create(&t->handle, NULL, (pthread_fn)fn, arg) != 0) {
        free(t);
        return NULL;
    }
    return (AndroidThread *)t;
}

bool AndroidSys_WaitThread(AndroidThread *t) {
    if (!t) return false;
    pthread_join(((AndroidThreadImpl *)t)->handle, NULL);
    return true;
}

void AndroidSys_CloseThread(AndroidThread *t) {
    if (t) free(t);
}

uintptr_t AndroidSys_CurrentThreadId(void) {
    // pthread_t on Linux/Android is an unsigned long — enough for thread
    // identity compared against the main thread id captured at startup.
    return (uintptr_t)pthread_self();
}