// ============================================================================
// KisakCOD Android port — low-level platform services
//
// Pure C/POSIX with Android logcat glue (#ifdef __ANDROID__). Compiled by the
// shell build AND the engine build (M2). Provides the WinAPI-compat shims the
// engine's "universal" files expect on non-Windows builds, plus the Android
// data-dir override for Sys_Cwd.
// ============================================================================
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- milliseconds (replaces timeGetTime / QueryPerformanceCounter) ----------
uint32_t AndroidSys_Milliseconds(void);
uint32_t AndroidSys_MillisecondsRaw(void);
int64_t  AndroidSys_PerfCounter(void);
int64_t  AndroidSys_PerfFrequency(void);

// ---- game data root (fs_basepath) --------------------------------------------
void        AndroidSys_SetDataDir(const char *dir);
const char *AndroidSys_DataDir(void);
// Sys_Cwd() the engine calls must return this on Android (override in and_main).
void AndroidSys_GetCwd(char *buf, size_t len);

// ---- logging -----------------------------------------------------------------
void AndroidSys_Log(const char *tag, const char *fmt, ...);

// ---- sleep -------------------------------------------------------------------
void AndroidSys_Sleep(unsigned ms);

// ---- WinAPI-compat shims ------------------------------------------------------
// (engine universal files call these on non-_WIN32 builds)

void *AndroidSys_VirtualAlloc(void *addr, size_t size,
                              unsigned allocType, unsigned protect);
bool  AndroidSys_VirtualFree(void *addr, size_t size, unsigned freeType);
bool  AndroidSys_VirtualProtect(void *addr, size_t size, unsigned protect,
                                unsigned *oldProtect);

// minimal thread shim
typedef void AndroidThread;
AndroidThread *AndroidSys_CreateThread(void *(*fn)(void *), void *arg);
bool AndroidSys_WaitThread(AndroidThread *t);
void AndroidSys_CloseThread(AndroidThread *t);
uintptr_t AndroidSys_CurrentThreadId(void);

#ifdef __cplusplus
}
#endif