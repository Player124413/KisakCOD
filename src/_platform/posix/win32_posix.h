// KisakCOD -- Win32 API compatibility layer for POSIX targets.
//
// WHY THIS EXISTS
//
// The engine is a decompilation of a 32-bit Windows game and calls the Win32
// API from ~40 files spread across the tree, not just from src/win32/. Rather
// than patch every call site, this file implements the Win32 functions the
// engine actually uses on top of POSIX (pthreads, fd-based I/O, clock_gettime).
// The declarations come from the vendored MinGW-w64 headers in
// deps/mingw-headers, which also supply the real D3D9/D3DX9/DDraw/DSound COM
// interfaces -- those are large, layout-sensitive, and must not be re-derived by
// hand.
//
// WHAT IS *NOT* HERE
//   - Window management, message pumps, cursor and keyboard state. Those belong
//     to the host platform (see src/_platform/android and src/_platform/linux)
//     because on Android there is no HWND at all: the surface is an
//     ANativeWindow and DXVK builds its Vulkan swapchain from it.
//   - Direct3D itself. The engine keeps calling D3D9 unchanged; DXVK translates
//     it to Vulkan.
//
// DESIGN NOTES
//   HANDLE is void*. Every handle is a heap-allocated KisakHandle tagged with a
//   type, so WaitForSingleObject() can dispatch correctly. Handles are never
//   reused as integers anywhere in the engine.
//
//   CRITICAL_SECTION keeps the MinGW struct layout (the engine has static
//   instances of it) and stashes the pthread mutex in the LockSemaphore field.
//   That avoids a global table and keeps Enter/Leave O(1).
//
// THE 32-BIT ABI
//
// This layer exists because the engine is 32-bit, and that is not negotiable:
// the fastfile loader streams hardcoded 32-bit struct sizes with 4-byte pointer
// slots (Load_GfxImage streams 36 bytes, then DB_PushStreamPos(4) skips the
// 4-byte name pointer), and 315 static_asserts pin 32-bit layouts. A 64-bit
// build produces 3429 static_assert failures in 60 translation units, all of
// them pointer-width drift. See docs/ANDROID_PORT.md.

#pragma once

// MSVC's fixed-width types and intrinsics are used directly by the decompiled
// sources. They live in msvc_compat.h so every translation unit can get them
// before anything else is included -- src/universal/q_shared.h does that, and
// including it here keeps this header usable on its own.
#include "msvc_compat.h"

// ---------------------------------------------------------------------------
// _WIN64
//
// THIS IS THE SINGLE MOST IMPORTANT DEFINE IN THE PORT.
//
// GCC and Clang do not define _WIN64 on any target. Without it, MinGW's headers
// take their 32-bit branch for every pointer-sized type: UINT_PTR, WPARAM,
// LPARAM, LONG_PTR, ULONG_PTR, HANDLE, SIZE_T and every struct holding one of
// those. The result is not a compile error -- it is silent ABI corruption that
// only shows up as crashes far away from the cause. Measured before this block
// existed: WPARAM came out 4 bytes on a 64-bit target.
//
// It is derived from the real pointer width rather than assumed, and it must
// appear before <windows.h>.
// ---------------------------------------------------------------------------
#if !defined(_WIN64)
#if defined(__x86_64__) || defined(__aarch64__) || defined(__LP64__) || defined(_M_ARM64) || defined(_M_X64)
#define _WIN64 1
#endif
#endif

// ---------------------------------------------------------------------------
// Host headers first
//
// MinGW's crt/ tree must never be on the include path: it redefines time_t,
// ssize_t, struct timeval, fd_set, select, mbstate_t and uintptr_t with Windows
// widths against a POSIX libc, which produces hundreds of errors. The host
// headers are pulled in here, before anything from deps/mingw-headers, so that
// every later include sees the POSIX definitions already in place.
//
// corecrt.h (this directory) shadows MinGW's and forwards to these.
// ---------------------------------------------------------------------------
#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#else
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <dirent.h>
#include <limits.h>

// The host headers above have already defined these; mark them so MinGW's
// crtdefs.h does not try again.
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
#endif
#ifndef _UINTPTR_T_DEFINED
#define _UINTPTR_T_DEFINED
#endif
#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
#endif

// ---------------------------------------------------------------------------
// The Win32 declarations themselves
// ---------------------------------------------------------------------------
#include <windows.h>
#include <winsock2.h>   // POSIX-backed shim, see winsock2.h in this directory
#include <mmsystem.h>   // MMRESULT / TIMECAPS / timeGetTime

// ---------------------------------------------------------------------------
// MinGW maps the public Interlocked* names onto its _Interlocked* intrinsics,
// but only for _AMD64_, and with __LONG32 (= int) parameter types. The engine
// passes `long` and `void *` (MSVC's LONG and PVOID), so the mapping is undone
// here in favour of the definitions in msvc_compat.h, which use the MSVC
// signatures on every target. Nothing in windows.h itself needs the mapping.
// ---------------------------------------------------------------------------
#ifdef InterlockedIncrement
#undef InterlockedIncrement
#endif
#ifdef InterlockedDecrement
#undef InterlockedDecrement
#endif
#ifdef InterlockedExchange
#undef InterlockedExchange
#endif
#ifdef InterlockedCompareExchange
#undef InterlockedCompareExchange
#endif
#ifdef InterlockedExchangeAdd
#undef InterlockedExchangeAdd
#endif
#ifdef InterlockedAdd
#undef InterlockedAdd
#endif
#ifdef InterlockedAnd
#undef InterlockedAnd
#endif
#ifdef InterlockedOr
#undef InterlockedOr
#endif
#ifdef InterlockedXor
#undef InterlockedXor
#endif
#ifdef InterlockedExchangePointer
#undef InterlockedExchangePointer
#endif
#ifdef InterlockedCompareExchangePointer
#undef InterlockedCompareExchangePointer
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Extensions the engine expects that MinGW declares but POSIX needs help with
// ---------------------------------------------------------------------------

// RaiseException is only used by the MSVC debugger thread-naming trick in
// threads.cpp. It is harmless to no-op: the name is set via the platform's own
// mechanism (pthread_setname_np) instead.
void WINAPI kisak_RaiseException(DWORD, DWORD, DWORD, const ULONG_PTR *);

// Set by the host before the renderer starts. GetSystemMetrics() reads them
// (r_init.cpp uses SM_CXSCREEN/SM_CYSCREEN as its display-size fallback).
void kisak_SetDisplayMetrics(int width, int height);
void kisak_GetDisplayMetrics(int *width, int *height);

// Fallback for GetModuleFileNameA() when /proc/self/exe is unavailable (some
// Android sandboxes restrict it).
extern const char *kisak_argv0_path;

#ifdef __cplusplus
}  // extern "C"
#endif
