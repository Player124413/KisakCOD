// Android port — comprehensive Win32 API compatibility shim for the engine.
// Provides the Windows API functions that the engine's source files call
// directly, mapping them to POSIX equivalents on Android.
//
// This header is included automatically by the Android win_local.h or
// included explicitly in files that need it.
#pragma once

#if defined(__ANDROID__) && !defined(_WIN32)

#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

// ---- Windows types that the engine uses ----
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef int BOOL;
typedef void *HANDLE;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef char *LPSTR;
typedef const char *LPCSTR;
typedef DWORD *LPDWORD;
typedef uint32_t UINT;
typedef unsigned long ULONG;
typedef void *HMODULE;
typedef void *HINSTANCE__;
typedef HINSTANCE__ *HINSTANCE;
typedef void *HWND;
typedef void *HDC;
typedef void *HGLRC;
typedef void *HCURSOR;
typedef void *HICON;
typedef void *HBRUSH;
typedef void *HMENU;
typedef long LPARAM;
typedef long LRESULT;
typedef unsigned long WPARAM;
typedef long LONG_PTR;
typedef unsigned long ULONG_PTR;

#define WINAPI
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)-1)

// Memory allocation
#define MEM_COMMIT    0x1000
#define MEM_RESERVE   0x2000
#define MEM_RELEASE  0x8000
#define MEM_DECOMMIT 0x4000
#define PAGE_READWRITE 0x04
#define PAGE_READONLY  0x02
#define PAGE_EXECUTE_READWRITE 0x40

// Thread priorities
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_LOWEST -2
#define THREAD_PRIORITY_BELOW_NORMAL -1
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_TIME_CRITICAL 15
#define THREAD_PRIORITY_IDLE -15

// Wait
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_ABANDONED 0x80
#define WAIT_FAILED 0xFFFFFFFF

// Event
#define CREATE_SUSPENDED 0x00000004

// File
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF
#define MAX_PATH 260
#define _A_SUBDIR 0x10
#define _A_NORMAL 0x00

// Error codes
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_ACCESS_DENIED 5
#define ERROR_SUCCESS 0

// ---- Memory ----
inline LPVOID VirtualAlloc(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect) {
    (void)lpAddress; (void)flProtect;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (flAllocationType & MEM_RESERVE) flags |= MAP_NORESERVE;
    void *p = mmap(NULL, dwSize, PROT_READ | PROT_WRITE, flags, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

inline BOOL VirtualFree(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType) {
    if (!lpAddress) return FALSE;
    if (dwFreeType == MEM_RELEASE) {
        return munmap(lpAddress, dwSize) == 0;
    }
    return TRUE;
}

inline BOOL VirtualProtect(LPVOID lpAddress, SIZE_T dwSize, DWORD flNewProtect, DWORD *lpflOldProtect) {
    (void)lpAddress; (void)dwSize; (void)flNewProtect;
    if (lpflOldProtect) *lpflOldProtect = PAGE_READWRITE;
    return TRUE;
}

// ---- Threads ----
typedef void *(*LPTHREAD_START_ROUTINE)(LPVOID);

inline HANDLE CreateThread(LPVOID lpThreadAttributes, SIZE_T dwStackSize,
                           LPTHREAD_START_ROUTINE lpStartAddress,
                           LPVOID lpParameter, DWORD dwCreationFlags,
                           DWORD *lpThreadId) {
    (void)lpThreadAttributes;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (dwStackSize > 0) pthread_attr_setstacksize(&attr, dwStackSize);
    pthread_t *thread = (pthread_t *)malloc(sizeof(pthread_t));
    if (pthread_create(thread, &attr, lpStartAddress, lpParameter) != 0) {
        free(thread);
        return NULL;
    }
    if (dwCreationFlags & CREATE_SUSPENDED) {
        // Not easily supported; ignore for now
    }
    if (lpThreadId) *lpThreadId = (DWORD)(uintptr_t)(*thread);
    pthread_attr_destroy(&attr);
    return (HANDLE)thread;
}

inline DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    pthread_t *thread = (pthread_t *)hHandle;
    if (!thread) return WAIT_FAILED;
    if (dwMilliseconds == INFINITE) {
        pthread_join(*thread, NULL);
        return WAIT_OBJECT_0;
    }
    // timed wait not easily supported; use join
    pthread_join(*thread, NULL);
    return WAIT_OBJECT_0;
}

inline BOOL CloseHandle(HANDLE hObject) {
    free(hObject);
    return TRUE;
}

inline BOOL SetThreadPriority(HANDLE hThread, int nPriority) {
    (void)hThread; (void)nPriority;
    return TRUE;
}

inline BOOL TerminateThread(HANDLE hThread, DWORD dwExitCode) {
    (void)dwExitCode;
    pthread_t *thread = (pthread_t *)hThread;
    if (thread) pthread_cancel(*thread);
    return TRUE;
}

inline DWORD GetCurrentThreadId() {
    return (DWORD)(uintptr_t)pthread_self();
}

inline DWORD ResumeThread(HANDLE hThread) {
    (void)hThread;
    return 1;
}

// ---- Events ----
inline HANDLE CreateEventA(LPVOID lpEventAttributes, BOOL bManualReset,
                           BOOL bInitialState, LPCSTR lpName) {
    (void)lpEventAttributes; (void)bManualReset; (void)lpName;
    // Simple semaphore-based event
    sem_t *sem = (sem_t *)malloc(sizeof(sem_t));
    sem_init(sem, 0, bInitialState ? 1 : 0);
    return (HANDLE)sem;
}

inline BOOL SetEvent(HANDLE hEvent) {
    sem_t *sem = (sem_t *)hEvent;
    if (!sem) return FALSE;
    sem_post(sem);
    return TRUE;
}

inline BOOL ResetEvent(HANDLE hEvent) {
    sem_t *sem = (sem_t *)hEvent;
    if (!sem) return FALSE;
    int val;
    sem_getvalue(sem, &val);
    while (val > 0) { sem_trywait(sem); sem_getvalue(sem, &val); }
    return TRUE;
}

inline BOOL PulseEvent(HANDLE hEvent) {
    SetEvent(hEvent);
    ResetEvent(hEvent);
    return TRUE;
}

// ---- Critical Sections ----
typedef struct { pthread_mutex_t mutex; } CRITICAL_SECTION;
typedef CRITICAL_SECTION _RTL_CRITICAL_SECTION;

inline void InitializeCriticalSection(CRITICAL_SECTION *cs) {
    pthread_mutex_init(&cs->mutex, NULL);
}

inline void EnterCriticalSection(CRITICAL_SECTION *cs) {
    pthread_mutex_lock(&cs->mutex);
}

inline void LeaveCriticalSection(CRITICAL_SECTION *cs) {
    pthread_mutex_unlock(&cs->mutex);
}

inline void DeleteCriticalSection(CRITICAL_SECTION *cs) {
    pthread_mutex_destroy(&cs->mutex);
}

// ---- Interlocked operations ----
inline LONG InterlockedIncrement(LONG volatile *Addend) {
    return __sync_add_and_fetch(Addend, 1);
}

inline LONG InterlockedDecrement(LONG volatile *Addend) {
    return __sync_sub_and_fetch(Addend, 1);
}

inline LONG InterlockedExchange(LONG volatile *Target, LONG Value) {
    return __sync_lock_test_and_set(Target, Value);
}

inline LONG InterlockedCompareExchange(LONG volatile *Destination, LONG Exchange, LONG Comperand) {
    return __sync_val_compare_and_swap(Destination, Comperand, Exchange);
}

// ---- File I/O (POSIX) ----
typedef struct {
    char name[260];
    int attrib;
    size_t size;
    time_t time_write;
} _finddata_t;

typedef DIR *intptr_t;

inline intptr_t _findfirst(const char *pattern, _finddata_t *finddata) {
    // Extract directory from pattern
    char dir[260];
    strncpy(dir, pattern, sizeof(dir));
    char *star = strchr(dir, '*');
    if (star) *star = '\0';
    char *qmark = strchr(dir, '?');
    if (qmark) *qmark = '\0';
    if (dir[0] == '\0') strcpy(dir, ".");
    
    DIR *d = opendir(dir);
    if (!d) return -1;
    
    // Read first entry
    struct dirent *entry = readdir(d);
    if (entry) {
        strncpy(finddata->name, entry->name, sizeof(finddata->name) - 1);
        finddata->name[sizeof(finddata->name) - 1] = '\0';
        finddata->attrib = (entry->d_type == DT_DIR) ? _A_SUBDIR : _A_NORMAL;
        // Store the DIR pointer and the directory path for continuation
        return (intptr_t)d;
    }
    closedir(d);
    return -1;
}

inline int _findnext(intptr_t handle, _finddata_t *finddata) {
    DIR *d = (DIR *)handle;
    if (!d) return -1;
    struct dirent *entry = readdir(d);
    if (entry) {
        strncpy(finddata->name, entry->name, sizeof(finddata->name) - 1);
        finddata->name[sizeof(finddata->name) - 1] = '\0';
        finddata->attrib = (entry->d_type == DT_DIR) ? _A_SUBDIR : _A_NORMAL;
        return 0;
    }
    return -1;
}

inline int _findclose(intptr_t handle) {
    DIR *d = (DIR *)handle;
    if (d) closedir(d);
    return 0;
}

inline int _stat(const char *path, struct stat *buffer) {
    return stat(path, buffer);
}

inline int _mkdir(const char *path) {
    return mkdir(path, 0777);
}

inline int _chdir(const char *path) {
    return chdir(path);
}

inline char *_getcwd(char *buf, int maxlen) {
    return getcwd(buf, maxlen);
}

inline int _access(const char *path, int mode) {
    return access(path, mode);
}

inline int _unlink(const char *path) {
    return unlink(path);
}

inline int _rmdir(const char *path) {
    return rmdir(path);
}

inline int _splitpath(const char *path, char *drive, char *dir, char *name, char *ext) {
    if (drive) drive[0] = '\0';
    const char *last_slash = strrchr(path, '/');
    const char *last_dot = strrchr(path, '.');
    if (dir && last_slash) {
        size_t len = last_slash - path;
        strncpy(dir, path, len);
        dir[len] = '\0';
    } else if (dir) {
        dir[0] = '\0';
    }
    if (name) {
        const char *start = last_slash ? last_slash + 1 : path;
        if (last_dot && last_dot > start) {
            size_t len = last_dot - start;
            strncpy(name, start, len);
            name[len] = '\0';
        } else {
            strcpy(name, start);
        }
    }
    if (ext && last_dot) {
        strcpy(ext, last_dot);
    } else if (ext) {
        ext[0] = '\0';
    }
    return 0;
}

inline int _fullpath(char *absPath, const char *relPath, size_t maxLength) {
    char *res = realpath(relPath, NULL);
    if (res) {
        strncpy(absPath, res, maxLength);
        free(res);
        return 0;
    }
    return -1;
}

// ---- Time ----
inline DWORD timeGetTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ---- Misc ----
inline HMODULE LoadLibraryA(LPCSTR lpLibFileName) {
    return dlopen(lpLibFileName, RTLD_NOW);
}

inline BOOL FreeLibrary(HMODULE hLibModule) {
    dlclose(hLibModule);
    return TRUE;
}

inline LPVOID GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    return dlsym(hModule, lpProcName);
}

inline DWORD GetLastError() {
    return errno;
}

inline void SetLastError(DWORD dwErrCode) {
    errno = dwErrCode;
}

inline void Sleep(DWORD dwMilliseconds) {
    usleep(dwMilliseconds * 1000);
}

inline int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
                                int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar) {
    (void)CodePage; (void)dwFlags;
    if (cbMultiByte < 0) cbMultiByte = (int)strlen(lpMultiByteStr) + 1;
    if (cchWideChar < cbMultiByte) return 0;
    for (int i = 0; i < cbMultiByte; i++) {
        lpWideCharStr[i] = (wchar_t)(unsigned char)lpMultiByteStr[i];
    }
    return cbMultiByte;
}

// ---- Min/Max ----
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#endif // __ANDROID__ && !_WIN32