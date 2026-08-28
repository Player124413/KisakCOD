// Android port — comprehensive Win32 API compatibility shim for the engine.
// Provides the Windows API functions that the KisakCOD engine's source files
// call directly, mapping them to POSIX equivalents on Android.
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
#include <climits>
#include <cstdarg>
#include <alloca.h>
#include <link.h>

// ---- Windows types that the engine uses ----
typedef int32_t LONG;
typedef int64_t LONGLONG;
typedef uint64_t ULONGLONG;
typedef union { LONGLONG QuadPart; } LARGE_INTEGER;
typedef union { LONGLONG QuadPart; } _LARGE_INTEGER;
typedef union { ULONGLONG QuadPart; } ULARGE_INTEGER;
typedef uint64_t ULONGLONG;
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
typedef ULONG_PTR UINT_PTR;
typedef size_t SIZE_T;
typedef wchar_t WCHAR;
typedef uint16_t OLECHAR;
typedef char TCHAR;
typedef int SOCKET;

#define WINAPI
#define CALLBACK
#define __cdecl
#define __stdcall
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

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

// Wait / sync
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_ABANDONED 0x80
#define WAIT_FAILED 0xFFFFFFFF

// Event
#define CREATE_SUSPENDED 0x00000004
#define WAIT_FOR_OBJECT 0

// File
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFF
#define _A_SUBDIR 0x10
#define _A_NORMAL 0x00
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define CREATE_ALWAYS 2
#define CREATE_NEW 1

// Error codes
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_ACCESS_DENIED 5
#define ERROR_SUCCESS 0
#define INVALID_FILE_SIZE 0xFFFFFFFF
#define ERROR_IO_PENDING 997
#define NO_ERROR 0

// Console
#define STD_OUTPUT_HANDLE ((DWORD)-11)
#define STD_ERROR_HANDLE ((DWORD)-12)
#define FOREGROUND_BLUE 1
#define FOREGROUND_GREEN 2
#define FOREGROUND_RED 4
#define FOREGROUND_INTENSITY 8

// TLS
#define TLS_OUT_OF_INDEXES 0xFFFFFFFF

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

inline DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles,
                                     BOOL bWaitAll, DWORD dwMilliseconds) {
    (void)bWaitAll;
    for (DWORD i = 0; i < nCount; i++) {
        DWORD r = WaitForSingleObject(lpHandles[i], dwMilliseconds);
        if (r != WAIT_OBJECT_0) return r;
    }
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

// ---- TLS (Thread Local Storage) ----
// Simple array-based TLS with 64 slots (engine uses very few)
#define ANDROID_TLS_MAX 64
extern __thread void *android_tls_table[ANDROID_TLS_MAX];

inline DWORD TlsAlloc() {
    // Find a free slot
    for (int i = 0; i < ANDROID_TLS_MAX; i++) {
        if (!android_tls_table[i]) {
            // Mark as used (non-null but weird pattern to distinguish from real data)
            android_tls_table[i] = (void*)(uintptr_t)0x1;
            return (DWORD)i;
        }
    }
    return TLS_OUT_OF_INDEXES;
}

inline BOOL TlsFree(DWORD dwTlsIndex) {
    if (dwTlsIndex >= ANDROID_TLS_MAX) return FALSE;
    android_tls_table[dwTlsIndex] = NULL;
    return TRUE;
}

inline LPVOID TlsGetValue(DWORD dwTlsIndex) {
    if (dwTlsIndex >= ANDROID_TLS_MAX) return NULL;
    void *v = android_tls_table[dwTlsIndex];
    return (v == (void*)(uintptr_t)0x1) ? NULL : v;
}

inline BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue) {
    if (dwTlsIndex >= ANDROID_TLS_MAX) return FALSE;
    android_tls_table[dwTlsIndex] = lpTlsValue;
    return TRUE;
}

// ---- Events ----
inline HANDLE CreateEventA(LPVOID lpEventAttributes, BOOL bManualReset,
                           BOOL bInitialState, LPCSTR lpName) {
    (void)lpEventAttributes; (void)bManualReset; (void)lpName;
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

inline LONG InterlockedExchangeAdd(LONG volatile *Addend, LONG Value) {
    return __sync_fetch_and_add(Addend, Value);
}

inline LONGLONG InterlockedExchangeAdd64(LONGLONG volatile *Addend, LONGLONG Value) {
    return __sync_fetch_and_add(Addend, Value);
}

// Bit scan
inline unsigned char _BitScanForward(unsigned long *Index, unsigned long Mask) {
    if (!Mask) return 0;
    *Index = (unsigned long)__builtin_ctz(Mask);
    return 1;
}

inline unsigned char _BitScanReverse(unsigned long *Index, unsigned long Mask) {
    if (!Mask) return 0;
    *Index = (unsigned long)(31 - __builtin_clz(Mask));
    return 1;
}

// Rotate
inline unsigned int _rotl(unsigned int value, int shift) {
    shift &= 31;
    return (value << shift) | (value >> (32 - shift));
}

inline unsigned int _rotr(unsigned int value, int shift) {
    shift &= 31;
    return (value >> shift) | (value << (32 - shift));
}

// Compiler hint
#define __assume(cond) do { if (!(cond)) __builtin_unreachable(); } while(0)

// ---- File I/O (POSIX) ----
typedef struct {
    char name[260];
    int attrib;
    size_t size;
    time_t time_write;
} _finddata_t;

inline long _findfirst(const char *pattern, _finddata_t *finddata) {
    char dir[260];
    strncpy(dir, pattern, sizeof(dir));
    char *star = strchr(dir, '*');
    if (star) *star = '\0';
    char *qmark = strchr(dir, '?');
    if (qmark) *qmark = '\0';
    if (dir[0] == '\0') strcpy(dir, ".");
    
    DIR *d = opendir(dir);
    if (!d) return -1;
    
    struct dirent *entry = readdir(d);
    if (entry) {
        strncpy(finddata->name, entry->d_name, sizeof(finddata->name) - 1);
        finddata->name[sizeof(finddata->name) - 1] = '\0';
        finddata->attrib = (entry->d_type == DT_DIR) ? _A_SUBDIR : _A_NORMAL;
        return (long)(intptr_t)d;
    }
    closedir(d);
    return -1;
}

inline int _findnext(long handle, _finddata_t *finddata) {
    DIR *d = (DIR *)(intptr_t)handle;
    if (!d) return -1;
    struct dirent *entry = readdir(d);
    if (entry) {
        strncpy(finddata->name, entry->d_name, sizeof(finddata->name) - 1);
        finddata->name[sizeof(finddata->name) - 1] = '\0';
        finddata->attrib = (entry->d_type == DT_DIR) ? _A_SUBDIR : _A_NORMAL;
        return 0;
    }
    return -1;
}

inline int _findclose(long handle) {
    DIR *d = (DIR *)(intptr_t)handle;
    if (d) closedir(d);
    return 0;
}

// ---- File handle-based I/O (CreateFile/ReadFile/WriteFile) ----
// The engine uses two modes: _findfirst-style (MSVC) and CreateFile style.
// For CreateFile we simply wrap POSIX file descriptors in a HANDLE.

inline HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess,
                          DWORD dwShareMode, LPVOID lpSecurityAttributes,
                          DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                          HANDLE hTemplateFile) {
    (void)dwShareMode; (void)lpSecurityAttributes;
    (void)dwFlagsAndAttributes; (void)hTemplateFile;
    int flags = 0;
    int mode = 0;
    
    if (dwDesiredAccess & GENERIC_READ) flags |= O_RDONLY;
    if (dwDesiredAccess & GENERIC_WRITE) flags |= O_WRONLY;
    if ((dwDesiredAccess & (GENERIC_READ|GENERIC_WRITE)) == (GENERIC_READ|GENERIC_WRITE))
        flags = O_RDWR;
    
    switch (dwCreationDisposition) {
        case CREATE_NEW:    flags |= O_CREAT | O_EXCL; break;
        case CREATE_ALWAYS: flags |= O_CREAT | O_TRUNC; break;
        case OPEN_EXISTING: break;
        case OPEN_ALWAYS:   flags |= O_CREAT; break;
        default: break;
    }
    
    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fd = ::open(lpFileName, flags, mode);
    if (fd < 0) return INVALID_HANDLE_VALUE;
    return (HANDLE)(intptr_t)(fd + 64); // offset to avoid confusion with 0/1/2
}

inline BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                     LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped) {
    (void)lpOverlapped;
    int fd = (int)(intptr_t)hFile - 64;
    ssize_t r = ::read(fd, lpBuffer, nNumberOfBytesToRead);
    if (r < 0) return FALSE;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (DWORD)r;
    return TRUE;
}

inline BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
                      LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped) {
    (void)lpOverlapped;
    int fd = (int)(intptr_t)hFile - 64;
    ssize_t r = ::write(fd, lpBuffer, nNumberOfBytesToWrite);
    if (r < 0) return FALSE;
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)r;
    return TRUE;
}

inline DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
                            LONG *lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    int fd = (int)(intptr_t)hFile - 64;
    int whence = SEEK_SET;
    if (dwMoveMethod == FILE_CURRENT) whence = SEEK_CUR;
    else if (dwMoveMethod == FILE_END) whence = SEEK_END;
    off_t result = lseek(fd, lDistanceToMove, whence);
    if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = (LONG)(result >> 32);
    return (DWORD)(result & 0xFFFFFFFF);
}

inline DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    int fd = (int)(intptr_t)hFile - 64;
    struct stat st;
    if (fstat(fd, &st) < 0) return INVALID_FILE_SIZE;
    if (lpFileSizeHigh) *lpFileSizeHigh = (DWORD)(st.st_size >> 32);
    return (DWORD)(st.st_size & 0xFFFFFFFF);
}

// ---- String functions ----
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _snprintf snprintf
#define _vsnprintf vsnprintf
#define _alloca alloca
#define lstrcpyA strcpy
#define lstrcatA strcat
#define lstrlenA strlen
#define lstrcpy strcpy
#define lstrcat strcat
#define lstrlen strlen

// ---- Time / Performance ----
// The engine passes LARGE_INTEGER* to QueryPerformanceCounter
// LARGE_INTEGER has a QuadPart member (int64).
inline BOOL QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    lpPerformanceCount->QuadPart = (LONGLONG)ts.tv_sec * 1000000000LL + ts.tv_nsec;
    return TRUE;
}

inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency) {
    lpFrequency->QuadPart = 1000000000LL; // 1 GHz (nanosecond ticks)
    return TRUE;
}

inline DWORD GetTickCount() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ---- Module / System info ----
inline HMODULE GetModuleHandleA(LPCSTR lpModuleName) {
    if (!lpModuleName) {
        // Return pseudo-handle for current module
        Dl_info info;
        if (dladdr((void*)GetModuleHandleA, &info) && info.dli_fbase)
            return (HMODULE)info.dli_fbase;
        return (HMODULE)(intptr_t)1;
    }
    // Find loaded module by name
    struct link_map *lm = NULL;
    dlopen(lpModuleName, RTLD_NOLOAD | RTLD_LAZY);
    // Try to find it
    Dl_info info;
    if (dladdr((void*)GetModuleHandleA, &info) && info.dli_fname) {
        // Simplistic: return current module if name matches
        if (strstr(info.dli_fname, lpModuleName))
            return (HMODULE)info.dli_fbase;
    }
    return NULL;
}

inline DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
    Dl_info info;
    // If hModule is NULL or a pseudo-handle, get the executable name
    if (!hModule || hModule == (HMODULE)1) {
        // Read /proc/self/exe
        ssize_t len = readlink("/proc/self/exe", lpFilename, nSize - 1);
        if (len > 0) {
            lpFilename[len] = '\0';
            return (DWORD)len;
        }
        strncpy(lpFilename, "/data/data/com.kisakcod.android/files/kisakcod", nSize - 1);
        return (DWORD)strlen(lpFilename);
    }
    
    if (dladdr((void*)hModule, &info) && info.dli_fname) {
        strncpy(lpFilename, info.dli_fname, nSize - 1);
        lpFilename[nSize - 1] = '\0';
        return (DWORD)strlen(lpFilename);
    }
    return 0;
}

typedef struct _SYSTEM_INFO {
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD dwNumberOfProcessors;
} SYSTEM_INFO;

inline void GetSystemInfo(SYSTEM_INFO *lpSystemInfo) {
    if (!lpSystemInfo) return;
    lpSystemInfo->dwPageSize = (DWORD)sysconf(_SC_PAGESIZE);
    lpSystemInfo->lpMinimumApplicationAddress = (LPVOID)(intptr_t)0x10000;
    lpSystemInfo->lpMaximumApplicationAddress = (LPVOID)(intptr_t)0x7FFFFFFF;
    long nprocs = sysconf(_SC_NPROCESSORS_CONF);
    lpSystemInfo->dwNumberOfProcessors = (DWORD)(nprocs > 0 ? nprocs : 1);
}

// ---- Memory size (MSVC _msize) ----
// Not easily supported on all Linux malloc implementations; for the few
// uses in the engine (radiant editor), approximate with a generous size.
inline size_t _msize(void *memblock) {
    (void)memblock;
    return 0; // Callers should not rely on this
}

// ---- Debug ----
inline void OutputDebugStringA(LPCSTR lpOutputString) {
    if (lpOutputString) {
        write(STDERR_FILENO, lpOutputString, strlen(lpOutputString));
    }
}

// ---- Console ----
inline HANDLE GetStdHandle(DWORD nStdHandle) {
    (void)nStdHandle;
    return (HANDLE)(intptr_t)1; // pseudo-handle
}

inline BOOL SetConsoleTextAttribute(HANDLE hConsoleOutput, WORD wAttributes) {
    (void)hConsoleOutput; (void)wAttributes;
    return TRUE;
}

inline BOOL SetConsoleTitleA(LPCSTR lpConsoleTitle) {
    (void)lpConsoleTitle;
    return TRUE;
}

// ---- Structured exception (SEH) stubs ----
// The engine uses __try/__except in a few places. On Android, these are no-ops.
// The engine defines these via macros already, but provide fallback.
#ifndef __try
#define __try if (true)
#endif
#ifndef __except
#define __except(x) else
#endif
#ifndef __leave
#define __leave do { goto __try_leave; } while(0)
#endif
#define EXCEPTION_EXECUTE_HANDLER 1

// ---- Misc ----
inline BOOL IsBadReadPtr(const void *lp, UINT_PTR ucb) {
    (void)lp; (void)ucb;
    return FALSE; // Assume valid; signal handler will catch invalid access
}

inline void ZeroMemory(void *ptr, SIZE_T size) {
    memset(ptr, 0, size);
}

inline void CopyMemory(void *dest, const void *src, SIZE_T size) {
    memcpy(dest, src, size);
}

inline void MoveMemory(void *dest, const void *src, SIZE_T size) {
    memmove(dest, src, size);
}

inline void FillMemory(void *dest, SIZE_T size, BYTE fill) {
    memset(dest, fill, size);
}

// ---- Min/Max ----
// min/max macros NOT defined — they break C++ std::min/std::max
// Engine code uses std::min/std::max or explicit comparisons

// ---- timeGetTime ----
inline DWORD timeGetTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// INVALID_FILE_SIZE is used in GetFileSize


#endif // __ANDROID__ && !_WIN32