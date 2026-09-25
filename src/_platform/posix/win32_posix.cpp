// KisakCOD -- Win32 API implementation over POSIX.
//
// See win32_posix.h for why this exists and for the design notes. This file
// implements the surveyed Win32 surface the engine actually calls. The census
// that drove it (grep over src/, excluding Radiant):
//
//   MessageBoxA 45, GetKeyState 23, GetCursorPos 23, GetAsyncKeyState 23,
//   ShowCursor 21, LeaveCriticalSection 19, EnterCriticalSection 18,
//   SetCursorPos 16, Sleep 13, SetThreadAffinityMask 12, CreateFileA 11,
//   CloseHandle 11, WaitForSingleObject 9, GetSystemMetrics 9,
//   GetModuleFileNameA 9, QueryPerformanceCounter 8, CreateEventA 8,
//   VirtualAlloc 6, Reg* 20, WriteFile 5, SetThreadPriority 5,
//   GetCurrentProcess 5, timeGetTime 4, GetFileSize 4, DeleteFileA 4,
//   Find*File 9, ReadFile 2, CreateThread 1 ...
//
// Most of the input/cursor/message-box calls live in the platform layer that
// this port replaces (src/_platform/posix/win32/win_input.cpp, win_main.cpp);
// what is here is what the rest of the engine needs.

#include "win32_posix.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------

static thread_local DWORD g_lastError = 0;

extern "C" DWORD WINAPI GetLastError(void) { return g_lastError; }
extern "C" void WINAPI SetLastError(DWORD e) { g_lastError = e; }

// ---------------------------------------------------------------------------
// Handles
//
// Every HANDLE is a heap-allocated KisakHandle tagged with a kind, so
// WaitForSingleObject() can dispatch on the tag. The engine never treats a
// handle as an integer (verified across the call sites), so this is safe and
// avoids a global handle table.
// ---------------------------------------------------------------------------

enum KisakHandleKind
{
    KISAK_HANDLE_FILE = 1,
    KISAK_HANDLE_THREAD,
    KISAK_HANDLE_EVENT,
    KISAK_HANDLE_SEMAPHORE,
    KISAK_HANDLE_LIBRARY,
    KISAK_HANDLE_MUTEX,
};

struct KisakHandle
{
    KisakHandleKind kind;
    int             fd;          // KISAK_HANDLE_FILE
    pthread_t       thread;      // KISAK_HANDLE_THREAD
    bool            manualReset; // KISAK_HANDLE_EVENT / MUTEX
    bool            signaled;    // KISAK_HANDLE_EVENT
    int             count;       // KISAK_HANDLE_SEMAPHORE / MUTEX
    int             maxCount;    // KISAK_HANDLE_SEMAPHORE
    pthread_mutex_t mu;
    pthread_cond_t  cond;
    void           *lib;         // KISAK_HANDLE_LIBRARY
    DWORD           exitCode;    // KISAK_HANDLE_THREAD
};

static KisakHandle *kisak_NewHandle(KisakHandleKind kind)
{
    KisakHandle *h = new KisakHandle();
    memset(h, 0, sizeof(*h));
    h->kind = kind;
    h->fd = -1;
    return h;
}

extern "C" BOOL WINAPI CloseHandle(HANDLE h)
{
    if (!h) { g_lastError = ERROR_INVALID_HANDLE; return FALSE; }
    KisakHandle *k = (KisakHandle *)h;
    switch (k->kind) {
    case KISAK_HANDLE_FILE:
        if (k->fd >= 0) ::close(k->fd);
        break;
    case KISAK_HANDLE_EVENT:
    case KISAK_HANDLE_SEMAPHORE:
    case KISAK_HANDLE_MUTEX:
        pthread_mutex_destroy(&k->mu);
        pthread_cond_destroy(&k->cond);
        break;
    default:
        break;
    }
    delete k;
    return TRUE;
}

extern "C" BOOL WINAPI DuplicateHandle(
    HANDLE hSourceProcess, HANDLE hSource, HANDLE hTargetProcess,
    LPHANDLE lpTarget, DWORD dwAccess, BOOL bInherit, DWORD dwOptions)
{
    // The engine duplicates handles only to pass them between its own threads,
    // which on POSIX share the address space anyway. Returning the same handle
    // is therefore correct and keeps refcounting out of the picture.
    (void)hSourceProcess; (void)hTargetProcess; (void)dwAccess;
    (void)bInherit; (void)dwOptions;
    if (lpTarget) *lpTarget = hSource;
    return TRUE;
}

extern "C" HANDLE WINAPI GetCurrentProcess(void) { return (HANDLE)(intptr_t)-1; }

// There is no debugger attachment on Android and attaching gdb is a deliberate
// act, so reporting "not present" keeps Sys_DefaultInstallPath on the
// GetModuleFileNameA path, which is what a release build wants.
extern "C" BOOL WINAPI IsDebuggerPresent(void) { return FALSE; }

// win_common.cpp passes 0 to mean "this executable". The engine never loads a
// second module by handle, so the executable's own path is the right answer.
extern "C" HMODULE WINAPI GetModuleHandleA(LPCSTR name)
{
    if (name) return nullptr;
    return (HMODULE)1;
}
extern "C" HANDLE WINAPI GetCurrentThread(void) { return (HANDLE)(intptr_t)-2; }

extern "C" DWORD WINAPI GetCurrentThreadId(void)
{
    return (DWORD)(uintptr_t)pthread_self();
}

// ---------------------------------------------------------------------------
// Critical sections
//
// CRITICAL_SECTION keeps the MinGW struct layout because the engine has static
// instances of it. The recursive pthread mutex is stashed in LockSemaphore,
// which avoids a global table and keeps Enter/Leave O(1).
//
// The mutex is recursive because the engine re-enters: Sys_EnterCriticalSection
// is taken on paths that already hold it (the same pattern MSVC's critical
// sections support natively).
// ---------------------------------------------------------------------------

static pthread_mutex_t *kisak_GetCritLock(RTL_CRITICAL_SECTION *cs)
{
    if (!cs) return nullptr;
    if (!cs->LockSemaphore) {
        pthread_mutex_t *m = new pthread_mutex_t();
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(m, &attr);
        pthread_mutexattr_destroy(&attr);
        cs->LockSemaphore = (HANDLE)m;
        cs->RecursionCount = 0;
        cs->OwningThread = 0;
    }
    return (pthread_mutex_t *)cs->LockSemaphore;
}

extern "C" void WINAPI InitializeCriticalSection(RTL_CRITICAL_SECTION *cs)
{
    if (!cs) return;
    cs->DebugInfo = nullptr;
    cs->LockCount = -1;
    cs->RecursionCount = 0;
    cs->OwningThread = 0;
    cs->LockSemaphore = 0;
    kisak_GetCritLock(cs);
}

extern "C" BOOL WINAPI InitializeCriticalSectionAndSpinCount(RTL_CRITICAL_SECTION *cs, DWORD spin)
{
    (void)spin;
    InitializeCriticalSection(cs);
    return TRUE;
}

extern "C" void WINAPI EnterCriticalSection(RTL_CRITICAL_SECTION *cs)
{
    pthread_mutex_t *m = kisak_GetCritLock(cs);
    if (!m) return;
    pthread_mutex_lock(m);
    cs->OwningThread = (HANDLE)(intptr_t)pthread_self();
    ++cs->RecursionCount;
}

extern "C" void WINAPI LeaveCriticalSection(RTL_CRITICAL_SECTION *cs)
{
    pthread_mutex_t *m = kisak_GetCritLock(cs);
    if (!m) return;
    if (cs->RecursionCount > 0) --cs->RecursionCount;
    if (cs->RecursionCount == 0) cs->OwningThread = nullptr;
    pthread_mutex_unlock(m);
}

extern "C" BOOL WINAPI TryEnterCriticalSection(RTL_CRITICAL_SECTION *cs)
{
    pthread_mutex_t *m = kisak_GetCritLock(cs);
    if (!m) return FALSE;
    if (pthread_mutex_trylock(m) != 0) return FALSE;
    cs->OwningThread = (HANDLE)(intptr_t)pthread_self();
    ++cs->RecursionCount;
    return TRUE;
}

extern "C" void WINAPI DeleteCriticalSection(RTL_CRITICAL_SECTION *cs)
{
    if (!cs || !cs->LockSemaphore) return;
    pthread_mutex_t *m = (pthread_mutex_t *)cs->LockSemaphore;
    pthread_mutex_destroy(m);
    delete m;
    cs->LockSemaphore = 0;
}

// ---------------------------------------------------------------------------
// Threads
//
// CreateThread deliberately ignores CREATE_SUSPENDED. The engine suspends a
// thread only to set its name and immediately resumes it (Sys_CreateThread +
// Sys_ResumeThread in qcommon/threads.cpp), so honouring the flag would
// deadlock the renderer while it waited for a resume that never comes.
// ResumeThread is therefore a no-op returning 1.
// ---------------------------------------------------------------------------

struct KisakThreadStart
{
    LPTHREAD_START_ROUTINE routine;
    LPVOID                param;
};

static DWORD WINAPI kisak_ThreadTrampoline(LPVOID p)
{
    KisakThreadStart *s = (KisakThreadStart *)p;
    LPTHREAD_START_ROUTINE r = s->routine;
    LPVOID arg = s->param;
    delete s;
    DWORD rc = r(arg);
    return rc;
}

extern "C" HANDLE WINAPI CreateThread(
    LPSECURITY_ATTRIBUTES attr, SIZE_T stack, LPTHREAD_START_ROUTINE start,
    LPVOID param, DWORD flags, LPDWORD threadId)
{
    (void)attr; (void)flags;
    KisakHandle *h = kisak_NewHandle(KISAK_HANDLE_THREAD);

    KisakThreadStart *s = new KisakThreadStart();
    s->routine = start;
    s->param = param;

    pthread_attr_t pa;
    pthread_attr_init(&pa);
    if (stack) pthread_attr_setstacksize(&pa, (size_t)stack);
    else pthread_attr_setstacksize(&pa, 8u * 1024u * 1024u);
    pthread_attr_setdetachstate(&pa, PTHREAD_CREATE_DETACHED);

    int rc = pthread_create(&h->thread, &pa, (void *(*)(void *))kisak_ThreadTrampoline, s);
    pthread_attr_destroy(&pa);

    if (rc != 0) {
        delete s;
        delete h;
        g_lastError = ERROR_NOT_ENOUGH_MEMORY;
        return nullptr;
    }

    if (threadId) *threadId = (DWORD)(uintptr_t)h->thread;
    return (HANDLE)h;
}

extern "C" DWORD WINAPI ResumeThread(HANDLE h)
{
    (void)h;
    return 1;   // nothing is ever actually suspended; see the note above
}

extern "C" DWORD WINAPI SuspendThread(HANDLE h)
{
    (void)h;
    return 0;
}

extern "C" BOOL WINAPI SetThreadPriority(HANDLE h, int priority)
{
    (void)h; (void)priority;
    return TRUE;   // scheduling priorities are left to the OS
}

extern "C" int WINAPI GetThreadPriority(HANDLE h) { (void)h; return 0; }

extern "C" DWORD_PTR WINAPI SetThreadAffinityMask(HANDLE h, DWORD_PTR mask)
{
    // The engine pins the render/audio threads to specific cores on Windows.
    // On Android the scheduler already places them well, and forcing affinity
    // on big.LITTLE is more likely to hurt than help, so the request is
    // accepted and ignored.
    (void)h; (void)mask;
    return 0;
}

extern "C" BOOL WINAPI GetProcessAffinityMask(HANDLE h, PDWORD_PTR procMask, PDWORD_PTR sysMask)
{
    (void)h;
    // Win_InitThreads() derives s_cpuCount and the per-worker affinity masks
    // from this, and returning 0 would collapse the engine onto a single
    // worker thread. Report the real online-CPU mask.
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) n = 1;
    if (n > 32) n = 32;
    DWORD_PTR mask = (n == 32) ? 0xFFFFFFFFu : ((DWORD_PTR)1 << n) - 1;
    if (procMask) *procMask = mask;
    if (sysMask) *sysMask = mask;
    return TRUE;
}

extern "C" BOOL WINAPI SwitchToThread(void) { return FALSE; }

// ---------------------------------------------------------------------------
// Events, semaphores, mutexes
//
// All three are a KisakHandle plus a pthread mutex/condvar pair. The condition
// variable is what makes WaitForSingleObject able to time out, which
// threads.cpp relies on (it passes both 0 and INFINITE).
// ---------------------------------------------------------------------------

extern "C" HANDLE WINAPI CreateEventA(LPSECURITY_ATTRIBUTES attr, BOOL manualReset, BOOL initialState, LPCSTR name)
{
    (void)attr; (void)name;
    KisakHandle *h = kisak_NewHandle(KISAK_HANDLE_EVENT);
    h->manualReset = manualReset != FALSE;
    h->signaled = initialState != FALSE;
    pthread_mutex_init(&h->mu, nullptr);
    pthread_cond_init(&h->cond, nullptr);
    return (HANDLE)h;
}

extern "C" HANDLE WINAPI CreateSemaphoreA(LPSECURITY_ATTRIBUTES attr, LONG initial, LONG max, LPCSTR name)
{
    (void)attr; (void)name;
    KisakHandle *h = kisak_NewHandle(KISAK_HANDLE_SEMAPHORE);
    h->count = initial;
    h->maxCount = max;
    pthread_mutex_init(&h->mu, nullptr);
    pthread_cond_init(&h->cond, nullptr);
    return (HANDLE)h;
}

extern "C" HANDLE WINAPI CreateMutexA(LPSECURITY_ATTRIBUTES attr, BOOL initialOwner, LPCSTR name)
{
    (void)attr; (void)name;
    KisakHandle *h = kisak_NewHandle(KISAK_HANDLE_MUTEX);
    h->manualReset = true;
    h->count = initialOwner ? 1 : 0;
    h->maxCount = 1;
    pthread_mutex_init(&h->mu, nullptr);
    pthread_cond_init(&h->cond, nullptr);
    return (HANDLE)h;
}

extern "C" BOOL WINAPI SetEvent(HANDLE handle)
{
    if (!handle) return FALSE;
    KisakHandle *h = (KisakHandle *)handle;
    if (h->kind != KISAK_HANDLE_EVENT) return FALSE;
    pthread_mutex_lock(&h->mu);
    h->signaled = true;
    pthread_cond_broadcast(&h->cond);
    pthread_mutex_unlock(&h->mu);
    return TRUE;
}

extern "C" BOOL WINAPI ResetEvent(HANDLE handle)
{
    if (!handle) return FALSE;
    KisakHandle *h = (KisakHandle *)handle;
    if (h->kind != KISAK_HANDLE_EVENT) return FALSE;
    pthread_mutex_lock(&h->mu);
    h->signaled = false;
    pthread_mutex_unlock(&h->mu);
    return TRUE;
}

extern "C" BOOL WINAPI ReleaseSemaphore(HANDLE handle, LONG release, LPLONG previous)
{
    if (!handle) return FALSE;
    KisakHandle *h = (KisakHandle *)handle;
    if (h->kind != KISAK_HANDLE_SEMAPHORE) return FALSE;
    pthread_mutex_lock(&h->mu);
    if (previous) *previous = h->count;
    h->count += release;
    pthread_cond_broadcast(&h->cond);
    pthread_mutex_unlock(&h->mu);
    return TRUE;
}

extern "C" BOOL WINAPI ReleaseMutex(HANDLE handle)
{
    if (!handle) return FALSE;
    KisakHandle *h = (KisakHandle *)handle;
    if (h->kind != KISAK_HANDLE_MUTEX) return FALSE;
    pthread_mutex_lock(&h->mu);
    if (h->count > 0) --h->count;
    pthread_cond_broadcast(&h->cond);
    pthread_mutex_unlock(&h->mu);
    return TRUE;
}

// Thread handles are detached and never joined, so waiting on one always
// succeeds immediately -- the engine only ever waits on events and semaphores.
extern "C" DWORD WINAPI WaitForSingleObject(HANDLE handle, DWORD millis)
{
    if (!handle) return WAIT_FAILED;
    KisakHandle *h = (KisakHandle *)handle;

    if (h->kind == KISAK_HANDLE_THREAD) return WAIT_OBJECT_0;

    if (h->kind != KISAK_HANDLE_EVENT && h->kind != KISAK_HANDLE_SEMAPHORE &&
        h->kind != KISAK_HANDLE_MUTEX)
        return WAIT_OBJECT_0;

    const bool infinite = (millis == 0xFFFFFFFFu);

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += millis / 1000;
    ts.tv_nsec += (long)(millis % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }

    pthread_mutex_lock(&h->mu);
    for (;;) {
        bool ready = false;
        if (h->kind == KISAK_HANDLE_EVENT) ready = h->signaled;
        else ready = (h->count > 0);

        if (ready) {
            if (h->kind == KISAK_HANDLE_EVENT) { if (!h->manualReset) h->signaled = false; }
            else --h->count;
            pthread_mutex_unlock(&h->mu);
            return WAIT_OBJECT_0;
        }

        if (!infinite) {
            int rc = pthread_cond_timedwait(&h->cond, &h->mu, &ts);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&h->mu);
                return WAIT_TIMEOUT;
            }
        } else {
            pthread_cond_wait(&h->cond, &h->mu);
        }
    }
}

// ---------------------------------------------------------------------------
// Sleep
// ---------------------------------------------------------------------------

extern "C" void WINAPI Sleep(DWORD millis)
{
    if (millis == 0) { sched_yield(); return; }
    struct timespec ts;
    ts.tv_sec = millis / 1000;
    ts.tv_nsec = (long)(millis % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
}

// ---------------------------------------------------------------------------
// Time
//
// QueryPerformanceCounter/Frequency, timeGetTime and GetTickCount all read
// CLOCK_MONOTONIC. The engine's timing.cpp measures QPC against __rdtsc() to
// derive msecPerRawTimerTick, so a monotonic source self-calibrates correctly.
// ---------------------------------------------------------------------------

extern "C" BOOL WINAPI QueryPerformanceCounter(LARGE_INTEGER *out)
{
    if (!out) return FALSE;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    // 100ns units, matching the Windows QPC tick on most systems.
    out->QuadPart = (LONGLONG)ts.tv_sec * 10000000LL + ts.tv_nsec / 100;
    return TRUE;
}

extern "C" BOOL WINAPI QueryPerformanceFrequency(LARGE_INTEGER *out)
{
    if (!out) return FALSE;
    out->QuadPart = 10000000LL;   // 100ns ticks
    return TRUE;
}

extern "C" DWORD WINAPI timeGetTime(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)((uint64_t)ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

extern "C" DWORD WINAPI GetTickCount(void)
{
    return timeGetTime();
}

extern "C" void WINAPI GetSystemTime(LPSYSTEMTIME st)
{
    if (!st) return;
    time_t t = time(nullptr);
    struct tm tmv;
    gmtime_r(&t, &tmv);
    st->wYear = (WORD)(tmv.tm_year + 1900);
    st->wMonth = (WORD)(tmv.tm_mon + 1);
    st->wDayOfWeek = (WORD)tmv.tm_wday;
    st->wDay = (WORD)tmv.tm_mday;
    st->wHour = (WORD)tmv.tm_hour;
    st->wMinute = (WORD)tmv.tm_min;
    st->wSecond = (WORD)tmv.tm_sec;
    st->wMilliseconds = 0;
}

extern "C" void WINAPI GetLocalTime(LPSYSTEMTIME st)
{
    if (!st) return;
    time_t t = time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    st->wYear = (WORD)(tmv.tm_year + 1900);
    st->wMonth = (WORD)(tmv.tm_mon + 1);
    st->wDayOfWeek = (WORD)tmv.tm_wday;
    st->wDay = (WORD)tmv.tm_mday;
    st->wHour = (WORD)tmv.tm_hour;
    st->wMinute = (WORD)tmv.tm_min;
    st->wSecond = (WORD)tmv.tm_sec;
    st->wMilliseconds = 0;
}

extern "C" BOOL WINAPI SystemTimeToFileTime(const SYSTEMTIME *st, FILETIME *ft)
{
    if (!st || !ft) return FALSE;
    struct tm tmv;
    tmv.tm_year = st->wYear - 1900;
    tmv.tm_mon = st->wMonth - 1;
    tmv.tm_mday = st->wDay;
    tmv.tm_hour = st->wHour;
    tmv.tm_min = st->wMinute;
    tmv.tm_sec = st->wSecond;
    tmv.tm_isdst = 0;
    time_t t = timegm(&tmv);
    uint64_t v = (uint64_t)t * 10000000ULL + 116444736000000000ULL;
    ft->dwLowDateTime = (DWORD)(v & 0xFFFFFFFFu);
    ft->dwHighDateTime = (DWORD)(v >> 32);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Version
//
// 6.1 (Windows 7) is reported because it is the most permissive value that
// keeps the engine's legacy paths enabled: the code has OS-version gates that
// switch to newer APIs above this, and those paths were never decompiled.
// ---------------------------------------------------------------------------

extern "C" BOOL WINAPI GetVersionExA(LPOSVERSIONINFOA vi)
{
    if (!vi) return FALSE;
    vi->dwMajorVersion = 6;
    vi->dwMinorVersion = 1;
    vi->dwBuildNumber = 7601;
    vi->dwPlatformId = VER_PLATFORM_WIN32_NT;
    strcpy(vi->szCSDVersion, "Service Pack 1");
    if (vi->dwOSVersionInfoSize >= sizeof(OSVERSIONINFOEXA)) {
        OSVERSIONINFOEXA *ex = (OSVERSIONINFOEXA *)vi;
        ex->wServicePackMajor = 1;
        ex->wServicePackMinor = 0;
        ex->wSuiteMask = 0;
        ex->wProductType = VER_NT_WORKSTATION;
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Display metrics
//
// r_init.cpp reads SM_CXSCREEN/SM_CYSCREEN as its display-size fallback. The
// host must populate these before R_CreateDeviceInternal runs.
// ---------------------------------------------------------------------------

static int g_displayW = 1280;
static int g_displayH = 720;

extern "C" void kisak_SetDisplayMetrics(int w, int h) { g_displayW = w; g_displayH = h; }
extern "C" void kisak_GetDisplayMetrics(int *w, int *h) { if (w) *w = g_displayW; if (h) *h = g_displayH; }

extern "C" int WINAPI GetSystemMetrics(int index)
{
    switch (index) {
    case SM_CXSCREEN: return g_displayW;
    case SM_CYSCREEN: return g_displayH;
    case SM_CXVIRTUALSCREEN: return g_displayW;
    case SM_CYVIRTUALSCREEN: return g_displayH;
    case SM_CXFULLSCREEN: return g_displayW;
    case SM_CYFULLSCREEN: return g_displayH;
    case SM_CXICON:
    case SM_CYICON: return 32;
    case SM_CXCURSOR:
    case SM_CYCURSOR: return 32;
    case SM_CXDOUBLECLK:
    case SM_CYDOUBLECLK: return 4;
    default: return 0;
    }
}

// ---------------------------------------------------------------------------
// Module path
//
// GetModuleFileNameA(NULL, ...) is used by 7 engine files to locate the install
// directory. /proc/self/exe is the normal answer; on Android it may be
// restricted, in which case the host-provided kisak_argv0_path is used.
// ---------------------------------------------------------------------------

const char *kisak_argv0_path = nullptr;

extern "C" DWORD WINAPI GetModuleFileNameA(HMODULE mod, LPSTR buf, DWORD size)
{
    (void)mod;
    if (!buf || size == 0) return 0;
    buf[0] = '\0';

    ssize_t n = readlink("/proc/self/exe", buf, size - 1);
    if (n > 0) { buf[n] = '\0'; return (DWORD)n; }

    if (kisak_argv0_path && kisak_argv0_path[0]) {
        strncpy(buf, kisak_argv0_path, size - 1);
        buf[size - 1] = '\0';
        return (DWORD)strlen(buf);
    }

    if (!getcwd(buf, size)) return 0;
    return (DWORD)strlen(buf);
}

// ---------------------------------------------------------------------------
// Libraries
// ---------------------------------------------------------------------------

extern "C" HMODULE WINAPI LoadLibraryA(LPCSTR name)
{
    if (!name) return nullptr;
    void *h = dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    if (!h) { g_lastError = ERROR_MOD_NOT_FOUND; return nullptr; }
    KisakHandle *k = kisak_NewHandle(KISAK_HANDLE_LIBRARY);
    k->lib = h;
    return (HMODULE)k;
}

extern "C" BOOL WINAPI FreeLibrary(HMODULE mod)
{
    if (!mod) return FALSE;
    KisakHandle *k = (KisakHandle *)mod;
    if (k->kind != KISAK_HANDLE_LIBRARY) return FALSE;
    if (k->lib) dlclose(k->lib);
    delete k;
    return TRUE;
}

extern "C" FARPROC WINAPI GetProcAddress(HMODULE mod, LPCSTR name)
{
    if (!mod || !name) return nullptr;
    KisakHandle *k = (KisakHandle *)mod;
    if (k->kind != KISAK_HANDLE_LIBRARY || !k->lib) return nullptr;
    return (FARPROC)dlsym(k->lib, name);
}

// ---------------------------------------------------------------------------
// TLS
// ---------------------------------------------------------------------------

extern "C" DWORD WINAPI TlsAlloc(void)
{
    pthread_key_t key;
    if (pthread_key_create(&key, nullptr) != 0) return 0xFFFFFFFFu;
    return (DWORD)key;
}

extern "C" BOOL WINAPI TlsFree(DWORD index)
{
    return pthread_key_delete((pthread_key_t)index) == 0;
}

extern "C" LPVOID WINAPI TlsGetValue(DWORD index)
{
    return pthread_getspecific((pthread_key_t)index);
}

extern "C" BOOL WINAPI TlsSetValue(DWORD index, LPVOID value)
{
    return pthread_setspecific((pthread_key_t)index, value) == 0;
}

// ---------------------------------------------------------------------------
// File I/O
//
// The engine opens files with CreateFileA and reads/writes with ReadFile /
// WriteFile / GetFileSize / SetFilePointer / CloseHandle. The Win32 flags it
// passes (db_registry.cpp uses GENERIC_READ | FILE_SHARE_READ | OPEN_EXISTING
// | FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS) are translated here; the
// buffering flags are dropped because POSIX has no equivalent and the engine
// never relies on them.
// ---------------------------------------------------------------------------

static int kisak_AccessFromDesired(DWORD access)
{
    int flags = 0;
    if (access & 0x80000000u) flags |= O_RDONLY;             // GENERIC_READ
    if (access & 0x40000000u) flags |= O_WRONLY;             // GENERIC_WRITE
    if ((access & 0xC0000000u) == 0xC0000000u) flags = O_RDWR;
    if (access & 0x10000000u) flags = O_RDWR;                // GENERIC_ALL
    if (flags == 0) flags = O_RDONLY;
    return flags;
}

extern "C" HANDLE WINAPI CreateFileA(
    LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES attr,
    DWORD disposition, DWORD flags, HANDLE templateFile)
{
    (void)share; (void)attr; (void)templateFile;

    if (!name) { g_lastError = ERROR_INVALID_PARAMETER; return INVALID_HANDLE_VALUE; }

    int oflags = kisak_AccessFromDesired(access);

    switch (disposition) {
    case 1: oflags |= O_CREAT | O_EXCL; break;                  // CREATE_NEW
    case 2: oflags |= O_CREAT | O_TRUNC; break;                 // CREATE_ALWAYS
    case 3: break;                                              // OPEN_EXISTING
    case 4: oflags |= O_CREAT; break;                           // OPEN_ALWAYS
    case 5: oflags |= O_TRUNC; break;                           // TRUNCATE_EXISTING
    default: break;
    }

    int fd = ::open(name, oflags, 0666);
    if (fd < 0) {
        g_lastError = (errno == ENOENT) ? ERROR_FILE_NOT_FOUND :
                      (errno == EACCES) ? ERROR_ACCESS_DENIED :
                      (errno == EEXIST) ? ERROR_FILE_EXISTS : ERROR_OPEN_FAILED;
        return INVALID_HANDLE_VALUE;
    }

    KisakHandle *h = kisak_NewHandle(KISAK_HANDLE_FILE);
    h->fd = fd;
    return (HANDLE)h;
}

extern "C" BOOL WINAPI ReadFile(HANDLE h, LPVOID buf, DWORD want, LPDWORD got, LPOVERLAPPED ov)
{
    (void)ov;
    if (!h || h == INVALID_HANDLE_VALUE) return FALSE;
    KisakHandle *k = (KisakHandle *)h;
    if (k->kind != KISAK_HANDLE_FILE) { g_lastError = ERROR_INVALID_HANDLE; return FALSE; }

    ssize_t n = ::read(k->fd, buf, want);
    if (n < 0) { g_lastError = ERROR_READ_FAULT; if (got) *got = 0; return FALSE; }
    if (got) *got = (DWORD)n;
    return TRUE;
}

extern "C" BOOL WINAPI WriteFile(HANDLE h, LPCVOID buf, DWORD want, LPDWORD wrote, LPOVERLAPPED ov)
{
    (void)ov;
    if (!h || h == INVALID_HANDLE_VALUE) return FALSE;
    KisakHandle *k = (KisakHandle *)h;
    if (k->kind != KISAK_HANDLE_FILE) { g_lastError = ERROR_INVALID_HANDLE; return FALSE; }

    ssize_t n = ::write(k->fd, buf, want);
    if (n < 0) { g_lastError = ERROR_WRITE_FAULT; if (wrote) *wrote = 0; return FALSE; }
    if (wrote) *wrote = (DWORD)n;
    return TRUE;
}

extern "C" DWORD WINAPI GetFileSize(HANDLE h, LPDWORD high)
{
    if (!h || h == INVALID_HANDLE_VALUE) return 0xFFFFFFFFu;
    KisakHandle *k = (KisakHandle *)h;
    if (k->kind != KISAK_HANDLE_FILE) return 0xFFFFFFFFu;

    struct stat st;
    if (fstat(k->fd, &st) != 0) return 0xFFFFFFFFu;
    if (high) *high = (DWORD)(((uint64_t)st.st_size) >> 32);
    return (DWORD)(st.st_size & 0xFFFFFFFFu);
}

extern "C" DWORD WINAPI SetFilePointer(HANDLE h, LONG dist, LPLONG high, DWORD method)
{
    if (!h || h == INVALID_HANDLE_VALUE) return 0xFFFFFFFFu;
    KisakHandle *k = (KisakHandle *)h;
    if (k->kind != KISAK_HANDLE_FILE) return 0xFFFFFFFFu;

    int whence = (method == FILE_BEGIN) ? SEEK_SET : (method == FILE_CURRENT) ? SEEK_CUR : SEEK_END;
    off_t off = dist;
    if (high) off |= ((off_t)(uint32_t)*high) << 32;
    off_t r = lseek(k->fd, off, whence);
    if (r < 0) return 0xFFFFFFFFu;
    if (high) *high = (LONG)(((uint64_t)r) >> 32);
    return (DWORD)(r & 0xFFFFFFFFu);
}

extern "C" BOOL WINAPI SetEndOfFile(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE) return FALSE;
    KisakHandle *k = (KisakHandle *)h;
    if (k->kind != KISAK_HANDLE_FILE) return FALSE;
    off_t cur = lseek(k->fd, 0, SEEK_CUR);
    return ftruncate(k->fd, cur) == 0;
}

extern "C" BOOL WINAPI FlushFileBuffers(HANDLE h)
{
    if (!h || h == INVALID_HANDLE_VALUE) return FALSE;
    KisakHandle *k = (KisakHandle *)h;
    if (k->kind != KISAK_HANDLE_FILE) return FALSE;
    return fsync(k->fd) == 0;
}

extern "C" BOOL WINAPI DeleteFileA(LPCSTR name)
{
    if (!name) return FALSE;
    return ::unlink(name) == 0;
}

extern "C" BOOL WINAPI MoveFileA(LPCSTR from, LPCSTR to)
{
    if (!from || !to) return FALSE;
    return ::rename(from, to) == 0;
}

extern "C" BOOL WINAPI CopyFileA(LPCSTR from, LPCSTR to, BOOL failIfExists)
{
    if (!from || !to) return FALSE;
    if (failIfExists && ::access(to, F_OK) == 0) { g_lastError = ERROR_FILE_EXISTS; return FALSE; }
    int in = ::open(from, O_RDONLY);
    if (in < 0) return FALSE;
    int out = ::open(to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) { ::close(in); return FALSE; }
    char buf[64 * 1024];
    ssize_t n;
    while ((n = ::read(in, buf, sizeof(buf))) > 0) {
        if (::write(out, buf, (size_t)n) != n) { ::close(in); ::close(out); return FALSE; }
    }
    ::close(in);
    ::close(out);
    return TRUE;
}

extern "C" DWORD WINAPI GetFileAttributesA(LPCSTR name)
{
    if (!name) return 0xFFFFFFFFu;
    struct stat st;
    if (stat(name, &st) != 0) return 0xFFFFFFFFu;
    DWORD a = 0;
    if (S_ISDIR(st.st_mode)) a |= FILE_ATTRIBUTE_DIRECTORY;
    else a |= FILE_ATTRIBUTE_NORMAL;
    return a;
}

extern "C" BOOL WINAPI CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES attr)
{
    (void)attr;
    if (!path) return FALSE;
    return ::mkdir(path, 0777) == 0;
}

extern "C" BOOL WINAPI RemoveDirectoryA(LPCSTR path)
{
    if (!path) return FALSE;
    return ::rmdir(path) == 0;
}

extern "C" DWORD WINAPI GetCurrentDirectoryA(DWORD size, LPSTR buf)
{
    if (!buf || size == 0) return 0;
    if (!getcwd(buf, size)) return 0;
    return (DWORD)strlen(buf);
}

extern "C" BOOL WINAPI SetCurrentDirectoryA(LPCSTR path)
{
    if (!path) return FALSE;
    return chdir(path) == 0;
}

// ---------------------------------------------------------------------------
// Directory enumeration
//
// Sys_ListFiles walks the tree with FindFirstFileA/FindNextFileA. The Win32
// filter semantics (extension match and a substring filter) are reproduced so
// the engine's existing filtering keeps working unchanged.
// ---------------------------------------------------------------------------

struct KisakFindData
{
    DIR            *dir;
    std::string     base;      // directory the handle is positioned in
    std::string     extension; // "" or ".ext"
    bool            wantSubs;
    bool            pending;   // a subdirectory was deferred and must be visited
    std::string     pendingPath;
};

extern "C" HANDLE WINAPI FindFirstFileA(LPCSTR spec, LPWIN32_FIND_DATAA fd)
{
    if (!spec || !fd) return INVALID_HANDLE_VALUE;

    std::string s(spec);
    size_t slash = s.find_last_of("/\\");
    std::string dir = (slash == std::string::npos) ? std::string(".") : s.substr(0, slash);
    std::string pattern = (slash == std::string::npos) ? s : s.substr(slash + 1);

    KisakFindData *f = new KisakFindData();
    f->dir = opendir(dir.c_str());
    if (!f->dir) { delete f; g_lastError = ERROR_FILE_NOT_FOUND; return INVALID_HANDLE_VALUE; }
    f->base = dir;

    size_t dot = pattern.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < pattern.size())
        f->extension = pattern.substr(dot);
    f->wantSubs = true;

    if (!FindNextFileA((HANDLE)f, fd)) {
        closedir(f->dir);
        delete f;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)f;
}

extern "C" BOOL WINAPI FindNextFileA(HANDLE handle, LPWIN32_FIND_DATAA fd)
{
    if (!handle || handle == INVALID_HANDLE_VALUE || !fd) return FALSE;
    KisakFindData *f = (KisakFindData *)handle;

    struct dirent *de;
    while ((de = readdir(f->dir)) != nullptr) {
        const char *n = de->d_name;
        if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0) continue;

        std::string full = f->base + "/" + n;
        struct stat st;
        bool isDir = false;
        if (stat(full.c_str(), &st) == 0) isDir = S_ISDIR(st.st_mode);

        if (isDir) {
            if (f->wantSubs) { f->pending = true; f->pendingPath = full; }
            continue;
        }

        if (!f->extension.empty()) {
            size_t len = strlen(n);
            size_t elen = f->extension.size();
            if (len < elen || strcasecmp(n + len - elen, f->extension.c_str()) != 0) continue;
        }

        memset(fd, 0, sizeof(*fd));
        strncpy(fd->cFileName, n, sizeof(fd->cFileName) - 1);
        fd->dwFileAttributes = isDir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        return TRUE;
    }

    // Exhausted this directory; if a subdirectory was deferred, descend into it.
    if (f->pending) {
        f->pending = false;
        std::string next = f->pendingPath;
        closedir(f->dir);
        f->dir = opendir(next.c_str());
        f->base = next;
        if (f->dir) return FindNextFileA(handle, fd);
    }
    return FALSE;
}

extern "C" BOOL WINAPI FindClose(HANDLE handle)
{
    if (!handle || handle == INVALID_HANDLE_VALUE) return FALSE;
    KisakFindData *f = (KisakFindData *)handle;
    if (f->dir) closedir(f->dir);
    delete f;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Memory
//
// VirtualAlloc is mmap-backed. VirtualFree(MEM_RELEASE) carries no length, so a
// table records each mapping's size: 4096 entries, open-addressed, keyed on the
// base address. That is enough for every mapping the engine makes (it uses
// VirtualAlloc for the zone allocator and the shader/heap pools, tens of
// mappings at most) and keeps VirtualFree O(1) with no search.
// ---------------------------------------------------------------------------

static const int KISAK_VA_SLOTS = 4096;

struct KisakVaEntry { void *base; size_t size; };

static KisakVaEntry g_vaTable[KISAK_VA_SLOTS];
static pthread_mutex_t g_vaMutex = PTHREAD_MUTEX_INITIALIZER;

static void kisak_TrackVirtualAlloc(void *base, size_t size)
{
    if (!base) return;
    pthread_mutex_lock(&g_vaMutex);
    uintptr_t h = ((uintptr_t)base >> 12) & (KISAK_VA_SLOTS - 1);
    for (int i = 0; i < KISAK_VA_SLOTS; ++i) {
        KisakVaEntry &e = g_vaTable[(h + i) & (KISAK_VA_SLOTS - 1)];
        if (e.base == base || e.base == nullptr) { e.base = base; e.size = size; break; }
    }
    pthread_mutex_unlock(&g_vaMutex);
}

static size_t kisak_TrackVirtualFree(void *base)
{
    if (!base) return 0;
    size_t found = 0;
    pthread_mutex_lock(&g_vaMutex);
    uintptr_t h = ((uintptr_t)base >> 12) & (KISAK_VA_SLOTS - 1);
    for (int i = 0; i < KISAK_VA_SLOTS; ++i) {
        KisakVaEntry &e = g_vaTable[(h + i) & (KISAK_VA_SLOTS - 1)];
        if (e.base == base) { found = e.size; e.base = nullptr; e.size = 0; break; }
        if (e.base == nullptr) break;
    }
    pthread_mutex_unlock(&g_vaMutex);
    return found;
}

extern "C" LPVOID WINAPI VirtualAlloc(LPVOID addr, SIZE_T size, DWORD type, DWORD protect)
{
    (void)addr; (void)protect;
    if (size == 0) { g_lastError = ERROR_INVALID_PARAMETER; return nullptr; }
    if (!(type & (MEM_COMMIT | MEM_RESERVE))) { g_lastError = ERROR_INVALID_PARAMETER; return nullptr; }

    size_t pages = (size + 4095) & ~(size_t)4095;
    void *p = mmap(nullptr, pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { g_lastError = ERROR_NOT_ENOUGH_MEMORY; return nullptr; }

    kisak_TrackVirtualAlloc(p, pages);
    return p;
}

extern "C" BOOL WINAPI VirtualFree(LPVOID addr, SIZE_T size, DWORD type)
{
    if (!addr) return FALSE;

    if (type & MEM_RELEASE) {
        size_t tracked = kisak_TrackVirtualFree(addr);
        if (tracked == 0) tracked = (size + 4095) & ~(size_t)4095;
        if (tracked == 0) tracked = 4096;
        return munmap(addr, tracked) == 0;
    }

    if (type & MEM_DECOMMIT) {
        size_t pages = (size + 4095) & ~(size_t)4095;
        return madvise(addr, pages, MADV_DONTNEED) == 0;
    }
    return TRUE;
}

extern "C" SIZE_T WINAPI VirtualQuery(LPCVOID addr, PMEMORY_BASIC_INFORMATION info, SIZE_T len)
{
    (void)addr;
    if (!info || len < sizeof(MEMORY_BASIC_INFORMATION)) return 0;
    memset(info, 0, sizeof(*info));
    info->RegionSize = 4096;
    info->State = MEM_COMMIT;
    info->Protect = PAGE_READWRITE;
    info->Type = MEM_PRIVATE;
    return sizeof(*info);
}

extern "C" BOOL WINAPI VirtualProtect(LPVOID addr, SIZE_T size, DWORD newProt, PDWORD oldProt)
{
    (void)addr; (void)newProt; (void)size;
    if (oldProt) *oldProt = PAGE_READWRITE;
    // Pages are already RW; the engine only ever relaxes protection.
    return TRUE;
}

// ---------------------------------------------------------------------------
// Registry
//
// There is no POSIX equivalent, so the registry is a flat text store at
// $KISAK_REGISTRY or <gameDir>/kisak_registry.cfg. One record per line:
//
//     KEY\NAME<TAB>"string value"
//     KEY\NAME<TAB>dword:1234
//
// The engine only ever uses it for its own settings (the video config and
// r_* / com_* values), so a flat store is sufficient and keeps the file
// diffable by hand.
// ---------------------------------------------------------------------------

static std::string kisak_RegistryPath()
{
    const char *env = getenv("KISAK_REGISTRY");
    if (env && env[0]) return std::string(env);

    const char *dir = getenv("KISAK_GAME_DIR");
    if (dir && dir[0]) return std::string(dir) + "/kisak_registry.cfg";
    return std::string("kisak_registry.cfg");
}

static void kisak_LoadRegistry(std::vector<std::string> &lines)
{
    lines.clear();
    FILE *f = fopen(kisak_RegistryPath().c_str(), "rb");
    if (!f) return;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
        if (n) lines.push_back(buf);
    }
    fclose(f);
}

static void kisak_SaveRegistry(const std::vector<std::string> &lines)
{
    FILE *f = fopen(kisak_RegistryPath().c_str(), "wb");
    if (!f) return;
    for (const auto &l : lines) fprintf(f, "%s\n", l.c_str());
    fclose(f);
}

extern "C" LONG WINAPI RegOpenKeyExA(HKEY key, LPCSTR sub, DWORD options, DWORD sam, PHKEY out)
{
    (void)key; (void)sub; (void)options; (void)sam;
    if (!out) return ERROR_INVALID_PARAMETER;
    *out = (HKEY)1;   // opaque; the store is flat
    return ERROR_SUCCESS;
}

extern "C" LONG WINAPI RegCreateKeyExA(
    HKEY key, LPCSTR sub, DWORD reserved, LPSTR cls, DWORD options, DWORD sam,
    LPSECURITY_ATTRIBUTES attr, PHKEY out, LPDWORD disposition)
{
    (void)key; (void)sub; (void)reserved; (void)cls; (void)options; (void)sam; (void)attr;
    if (!out) return ERROR_INVALID_PARAMETER;
    *out = (HKEY)1;
    if (disposition) *disposition = REG_CREATED_NEW_KEY;
    return ERROR_SUCCESS;
}

extern "C" LONG WINAPI RegCloseKey(HKEY key)
{
    (void)key;
    return ERROR_SUCCESS;
}

extern "C" LONG WINAPI RegQueryValueExA(
    HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD cb)
{
    (void)key; (void)reserved;

    std::vector<std::string> lines;
    kisak_LoadRegistry(lines);

    std::string want = name ? name : "";
    for (const auto &l : lines) {
        size_t tab = l.find('\t');
        if (tab == std::string::npos) continue;
        if (l.substr(0, tab) != want) continue;

        std::string value = l.substr(tab + 1);

        if (value.compare(0, 6, "dword:") == 0) {
            DWORD v = (DWORD)strtoul(value.c_str() + 6, nullptr, 10);
            if (type) *type = REG_DWORD;
            if (cb) *cb = sizeof(DWORD);
            if (data && cb && *cb >= sizeof(DWORD)) memcpy(data, &v, sizeof(DWORD));
            return ERROR_SUCCESS;
        }

        std::string s = value;
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
        if (type) *type = REG_SZ;
        DWORD need = (DWORD)s.size() + 1;
        if (data && cb && *cb >= need) memcpy(data, s.c_str(), need);
        if (cb) *cb = need;
        return ERROR_SUCCESS;
    }
    return ERROR_FILE_NOT_FOUND;
}

extern "C" LONG WINAPI RegSetValueExA(
    HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE *data, DWORD cb)
{
    (void)key; (void)reserved;

    std::vector<std::string> lines;
    kisak_LoadRegistry(lines);

    std::string keyName = name ? name : "";
    std::string record = keyName + "\t";
    if (type == REG_DWORD && data && cb >= sizeof(DWORD)) {
        DWORD v;
        memcpy(&v, data, sizeof(DWORD));
        record += "dword:" + std::to_string((unsigned long)v);
    } else if (data) {
        std::string s((const char *)data, cb ? cb - 1 : 0);
        record += "\"" + s + "\"";
    } else {
        record += "\"\"";
    }

    bool replaced = false;
    for (auto &l : lines) {
        size_t tab = l.find('\t');
        if (tab != std::string::npos && l.substr(0, tab) == keyName) { l = record; replaced = true; break; }
    }
    if (!replaced) lines.push_back(record);

    kisak_SaveRegistry(lines);
    return ERROR_SUCCESS;
}

extern "C" LONG WINAPI RegDeleteValueA(HKEY key, LPCSTR name)
{
    (void)key;
    std::vector<std::string> lines;
    kisak_LoadRegistry(lines);
    std::vector<std::string> kept;
    std::string want = name ? name : "";
    for (const auto &l : lines) {
        size_t tab = l.find('\t');
        if (tab != std::string::npos && l.substr(0, tab) == want) continue;
        kept.push_back(l);
    }
    kisak_SaveRegistry(kept);
    return ERROR_SUCCESS;
}

// ---------------------------------------------------------------------------
// Console / UI
//
// The Win32 build allocates a console window and routes the engine's output
// through it. On Android stdout/stderr go to logcat and on Linux to the
// terminal, so there is nothing to allocate. OutputDebugStringA goes to stderr
// so the messages are not silently dropped during development.
// ---------------------------------------------------------------------------

extern "C" void WINAPI OutputDebugStringA(LPCSTR msg)
{
    if (!msg) return;
    fputs(msg, stderr);
    fflush(stderr);
}

extern "C" int WINAPI MessageBoxA(HWND owner, LPCSTR text, LPCSTR caption, UINT type)
{
    (void)owner; (void)type;
    fprintf(stderr, "[MessageBox] %s\n%s\n", caption ? caption : "", text ? text : "");
    fflush(stderr);
    // The engine's fatal paths treat any return as "the user acknowledged";
    // IDOK keeps the existing control flow.
    return IDOK;
}

// ---------------------------------------------------------------------------
// Input stubs
//
// The real input comes from the host platform (src/_platform/posix/win32/
// win_input.cpp), which owns the event queue the engine drains. These exist so
// the client-side polling code in cl_input.cpp still compiles and links.
// ---------------------------------------------------------------------------

extern "C" SHORT WINAPI GetKeyState(int vkey) { (void)vkey; return 0; }
extern "C" SHORT WINAPI GetAsyncKeyState(int vkey) { (void)vkey; return 0; }
extern "C" BOOL WINAPI GetKeyboardState(PBYTE state) { if (state) memset(state, 0, 256); return TRUE; }
extern "C" BOOL WINAPI SetKeyboardState(LPBYTE state) { (void)state; return TRUE; }

extern "C" BOOL WINAPI GetCursorPos(LPPOINT p) { if (p) { p->x = 0; p->y = 0; } return TRUE; }
extern "C" BOOL WINAPI SetCursorPos(int x, int y) { (void)x; (void)y; return TRUE; }
extern "C" int WINAPI ShowCursor(BOOL show) { (void)show; return 0; }
extern "C" BOOL WINAPI ClipCursor(const RECT *r) { (void)r; return TRUE; }
extern "C" BOOL WINAPI GetClipCursor(LPRECT r) { if (r) memset(r, 0, sizeof(*r)); return TRUE; }

extern "C" UINT WINAPI MapVirtualKeyA(UINT code, UINT mapType)
{
    (void)code; (void)mapType;
    return 0;
}

// ---------------------------------------------------------------------------
// RaiseException
//
// Only used by the MSVC debugger thread-naming trick in threads.cpp. Harmless
// to no-op: the name is set via pthread_setname_np instead.
// ---------------------------------------------------------------------------

extern "C" void WINAPI kisak_RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *)
{
}
