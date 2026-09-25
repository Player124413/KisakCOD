// KisakCOD -- MSVC compatibility types and intrinsics.
//
// The decompiled sources are MSVC x86 code and carry MSVC's fixed-width types
// and compiler intrinsics directly. On MSVC those arrive via <windows.h> and
// <intrin.h>; on a POSIX libc they have to be defined explicitly.
//
// This file is pulled in by src/universal/q_shared.h (which every module
// includes first) when KISAK_POSIX is defined, and by intrin.h for the three
// files that include that header directly. win32_posix.h also includes it so it
// stays usable on its own.
//
// Everything here is spelled exactly as MSVC spells it so no call site has to
// change.
//
// NOTE ON __int8..__int64: they cannot be emulated here. C++ forbids combining
// `unsigned` with a typedef-name, so `unsigned __int8` does not parse as a
// typedef. The 2586 spellings across src/ were rewritten in place to their
// exact-width POSIX equivalents (unsigned char / unsigned short / unsigned int /
// unsigned long long) instead -- width-identical on LLP64 and LP64, so struct
// layouts and casts are unaffected.

#pragma once

#ifdef __cplusplus
#include <cstddef>
#else
#include <stddef.h>
#endif

// ---------------------------------------------------------------------------
// Calling conventions
//
// MinGW's headers assume the compiler (or MSVC's own headers) already defines
// __cdecl/__stdcall/__fastcall. GCC and Clang do NOT: they are MSVC keywords,
// not attributes, so every Win32 declaration in the vendored headers fails to
// parse without these. Empty is correct on every target this port runs on:
// x86-64 has a single calling convention and ARM64 never had the Win32 ones.
// ---------------------------------------------------------------------------
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __thiscall
#define __thiscall
#endif
#ifndef __pascal
#define __pascal
#endif
#ifndef WINAPI
#define WINAPI __stdcall
#endif
#ifndef CALLBACK
#define CALLBACK __stdcall
#endif
#ifndef APIENTRY
#define APIENTRY WINAPI
#endif

// Width-pinned 64-bit aliases, used below.
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
typedef long long            kisak_int64;
typedef unsigned long long   kisak_uint64;
#else
typedef long                 kisak_int64;
typedef unsigned long        kisak_uint64;
#endif

// MSVC also spells 64-bit values with a leading underscore on some paths.
typedef kisak_int64          _int64;

// __forceinline: MSVC's stronger inline hint. com_math.h uses it 20 times for
// the COERCE_* type-punning helpers, which must stay inlined for those to work.
#ifndef __forceinline
#define __forceinline inline
#endif

// ---------------------------------------------------------------------------
// Interlocked* family
//
// The engine uses these as its lock-free primitives (InterlockedExchangeAdd 61x,
// InterlockedDecrement 61x, InterlockedIncrement 48x, InterlockedCompareExchange
// 27x, InterlockedExchange 7x, InterlockedExchangePointer 2x, plus And/Or/Xor).
//
// MinGW's winnt.h/winbase.h map the public names onto the _Interlocked*
// intrinsics, but that mapping is gated on _AMD64_, so on a 32-bit target (which
// is what this engine is -- see the 32-bit-ABI note in docs/ANDROID_PORT.md) the
// public names are simply not declared. The signatures below are pinned to
// MSVC's: `long` for the LONG forms and `void *` for the pointer form, which is
// what the engine's arguments are typed as.
//
// They are defined for every target; win32_posix.h #undefs MinGW's mapping after
// including <windows.h> so these are the ones the engine links against, keeping
// the signatures consistent on all of them.
//
// __sync_* builtins are used rather than C11 atomics because they are available
// on every GCC/Clang version the NDK ships and need no <stdatomic.h>.
// ---------------------------------------------------------------------------

static inline long InterlockedIncrement(volatile long *Addend)
{
    return __sync_add_and_fetch(Addend, 1);
}

static inline long InterlockedDecrement(volatile long *Addend)
{
    return __sync_sub_and_fetch(Addend, 1);
}

static inline long InterlockedExchange(volatile long *Target, long Value)
{
    return __sync_lock_test_and_set(Target, Value);
}

static inline long InterlockedCompareExchange(volatile long *Destination, long Exchange, long Comperand)
{
    return __sync_val_compare_and_swap(Destination, Comperand, Exchange);
}

static inline long InterlockedExchangeAdd(volatile long *Addend, long Value)
{
    return __sync_fetch_and_add(Addend, Value);
}

static inline long InterlockedAdd(volatile long *Addend, long Value)
{
    return __sync_add_and_fetch(Addend, Value);
}

static inline long InterlockedAnd(volatile long *Destination, long Value)
{
    return __sync_and_and_fetch(Destination, Value);
}

static inline long InterlockedOr(volatile long *Destination, long Value)
{
    return __sync_or_and_fetch(Destination, Value);
}

static inline long InterlockedXor(volatile long *Destination, long Value)
{
    return __sync_xor_and_fetch(Destination, Value);
}

static inline void *InterlockedExchangePointer(void *volatile *Target, void *Value)
{
    return __sync_lock_test_and_set(Target, Value);
}

static inline void *InterlockedCompareExchangePointer(void *volatile *Destination, void *Exchange, void *Comperand)
{
    return __sync_val_compare_and_swap(Destination, Comperand, Exchange);
}

// ---------------------------------------------------------------------------
// Interlocked* overloads for the unsigned types
//
// The engine calls these with `unsigned`/`uint32_t` arguments, which is what
// MSVC's own C++ overload set in <winbase.h> accepts. MinGW ships the same
// overloads but they cast between `volatile long *` and `volatile __LONG32 *` in
// ways GCC rejects outside MSVC, which is why they are disabled and reproduced
// here.
//
// On a 32-bit target `unsigned` and `unsigned long` are the same width but
// remain distinct C++ types, so overload resolution still picks the exact match
// and no ambiguity arises.
// ---------------------------------------------------------------------------

static inline unsigned InterlockedIncrement(volatile unsigned *Addend) { return (unsigned)__sync_add_and_fetch(Addend, 1); }
static inline unsigned InterlockedDecrement(volatile unsigned *Addend) { return (unsigned)__sync_sub_and_fetch(Addend, 1); }
static inline unsigned InterlockedExchange(volatile unsigned *Target, unsigned Value) { return (unsigned)__sync_lock_test_and_set(Target, Value); }
static inline unsigned InterlockedCompareExchange(volatile unsigned *Destination, unsigned Exchange, unsigned Comperand) { return (unsigned)__sync_val_compare_and_swap(Destination, Comperand, Exchange); }
static inline unsigned InterlockedExchangeAdd(volatile unsigned *Addend, unsigned Value) { return (unsigned)__sync_fetch_and_add(Addend, Value); }
static inline unsigned InterlockedAdd(volatile unsigned *Addend, unsigned Value) { return (unsigned)__sync_add_and_fetch(Addend, Value); }
static inline unsigned InterlockedAnd(volatile unsigned *Destination, unsigned Value) { return (unsigned)__sync_and_and_fetch(Destination, Value); }
static inline unsigned InterlockedOr(volatile unsigned *Destination, unsigned Value) { return (unsigned)__sync_or_and_fetch(Destination, Value); }
static inline unsigned InterlockedXor(volatile unsigned *Destination, unsigned Value) { return (unsigned)__sync_xor_and_fetch(Destination, Value); }

static inline unsigned long InterlockedIncrement(volatile unsigned long *Addend) { return (unsigned long)__sync_add_and_fetch(Addend, 1); }
static inline unsigned long InterlockedDecrement(volatile unsigned long *Addend) { return (unsigned long)__sync_sub_and_fetch(Addend, 1); }
static inline unsigned long InterlockedExchange(volatile unsigned long *Target, unsigned long Value) { return (unsigned long)__sync_lock_test_and_set(Target, Value); }
static inline unsigned long InterlockedCompareExchange(volatile unsigned long *Destination, unsigned long Exchange, unsigned long Comperand) { return (unsigned long)__sync_val_compare_and_swap(Destination, Comperand, Exchange); }
static inline unsigned long InterlockedExchangeAdd(volatile unsigned long *Addend, unsigned long Value) { return (unsigned long)__sync_fetch_and_add(Addend, Value); }
static inline unsigned long InterlockedAdd(volatile unsigned long *Addend, unsigned long Value) { return (unsigned long)__sync_add_and_fetch(Addend, Value); }
static inline unsigned long InterlockedAnd(volatile unsigned long *Destination, unsigned long Value) { return (unsigned long)__sync_and_and_fetch(Destination, Value); }
static inline unsigned long InterlockedOr(volatile unsigned long *Destination, unsigned long Value) { return (unsigned long)__sync_or_and_fetch(Destination, Value); }
static inline unsigned long InterlockedXor(volatile unsigned long *Destination, unsigned long Value) { return (unsigned long)__sync_xor_and_fetch(Destination, Value); }

// ---------------------------------------------------------------------------
// MSVC C runtime names
//
// The decompilation calls the "_s" secure-CRT variants and the underscore-
// prefixed POSIX-with-MSVC-spelling helpers that MSVC's <stdio.h>/<string.h>
// export. A POSIX libc has none of them, so they are mapped onto the standard
// equivalents here. The semantics used by the engine are the plain ones -- it
// never relies on the _s functions' extra validation, only on their argument
// order.
//
//   _iobuf      -> FILE          (23 uses)
//   fopen_s     -> fopen         (4)
//   sprintf_s   -> snprintf      (2)
//   strcpy_s    -> strncpy-ish   (2)
//   sscanf_s    -> sscanf        (5)
//   _stricmp    -> strcasecmp    (186)
//   _strnicmp   -> strncasecmp   (14)
//   _getcwd     -> getcwd        (2)
//   _mkdir      -> mkdir         (1)
// ---------------------------------------------------------------------------

#ifdef __cplusplus
#include <cstdio>
#include <cstring>
#include <cstdlib>
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif

#include <strings.h>   // strcasecmp / strncasecmp
#include <unistd.h>    // getcwd
#include <sys/stat.h>  // mkdir
#include <errno.h>

typedef FILE _iobuf;

#define fopen_s(ppFile, name, mode) \
    ((*(ppFile) = fopen((name), (mode))) ? 0 : errno)

#define sprintf_s(buf, ...) snprintf((buf), sizeof(buf), __VA_ARGS__)

#define strcpy_s(dst, dstsz, src) \
    ((strlen(src) < (dstsz)) ? (strcpy((dst), (src)), 0) : (errno = ERANGE, ERANGE))

#define sscanf_s(str, fmt, ...) sscanf((str), (fmt), __VA_ARGS__)

#define _stricmp(a, b)  strcasecmp((a), (b))
#define _strnicmp(a, b, n) strncasecmp((a), (b), (n))

#define _getcwd(buf, size) getcwd((buf), (size))
#define _chdir(path)       chdir((path))
#define _unlink(path)      unlink((path))

// _mkdir takes only the path on MSVC (the mode is implicitly 0777 & ~umask).
static inline int _mkdir(const char *path) { return mkdir(path, 0777); }

#define _strdup(str) strdup((str))

// _itoa(value, buffer, radix). MSVC writes a NUL-terminated string; radix 10 is
// the only one the engine uses (db_registry.cpp formats zone file sizes).
static inline char *_itoa(int value, char *buffer, int radix)
{
    if (radix == 10) { snprintf(buffer, 16, "%d", value); return buffer; }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Compiler intrinsics
// ---------------------------------------------------------------------------

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#endif

// __rdtsc() reads the processor's time-stamp counter. The decompiled profiling
// paths in rb_backend.cpp / rb_pixelcost.cpp / com_profilemapload.cpp /
// r_init.cpp use it to measure GPU stall time and frame cost.
//
// On x86-64 the compiler's own ia32intrin.h already defines it, so it is left
// alone here. On x86-32 the builtin is used directly. ARM64 has no equivalent
// userspace instruction, so the virtual counter is used instead:
// architecturally 64-bit, monotonic, readable from EL0 at a fixed frequency.
// (A clock_gettime() call per __rdtsc would be far too slow for the pixel-cost
// paths that call it in a tight loop.) Anything else gets 0, which makes the
// profile counters inert rather than wrong.
//
// timing.cpp's SecondsPerTick() measures QPC against __rdtsc() and derives
// msecPerRawTimerTick from the ratio, so any *monotonic* high-frequency source
// self-calibrates correctly.
#if !defined(__x86_64__)
#if defined(__i386__)
static inline unsigned long long __rdtsc(void) { return __builtin_ia32_rdtsc(); }
#elif defined(__aarch64__)
static inline unsigned long long __rdtsc(void)
{
    unsigned long long v;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}
#else
static inline unsigned long long __rdtsc(void) { return 0ULL; }
#endif
#endif

// __debugbreak() raises a breakpoint exception. The engine uses it as a
// "we should never get here" marker. MinGW's intrin-impl.h already declares and
// defines it (with a working implementation per architecture), so this is only
// a fallback for toolchains without it.
#if !defined(__MINGW32__) && !defined(_MSC_VER)
#if defined(__i386__) || defined(__x86_64__)
static inline void __debugbreak(void) { __asm__ __volatile__("int $3"); }
#elif defined(__aarch64__) || defined(__arm__)
static inline void __debugbreak(void) { __builtin_trap(); }
#else
static inline void __debugbreak(void) { __builtin_trap(); }
#endif
#endif

// __cpuidex(level, sublevel, *a, *b, *c, *d). Only used to report the CPU name
// and feature bits, so it is fine for the leaf set to be empty.
#if defined(__i386__) || defined(__x86_64__)
static inline void __cpuidex(int cpuInfo[4], int function_id, int subfunction_id)
{
    __asm__ __volatile__("cpuid"
                         : "=a"(cpuInfo[0]), "=b"(cpuInfo[1]),
                           "=c"(cpuInfo[2]), "=d"(cpuInfo[3])
                         : "a"(function_id), "c"(subfunction_id));
}
#else
static inline void __cpuidex(int cpuInfo[4], int, int)
{
    cpuInfo[0] = cpuInfo[1] = cpuInfo[2] = cpuInfo[3] = 0;
}
#endif

static inline void __cpuid(int cpuInfo[4], int function_id)
{
    __cpuidex(cpuInfo, function_id, 0);
}

// _BitScanReverse/_BitScanForward find the index of the highest (resp. lowest)
// set bit. The decompiled callers treat the return as "bit found" and read the
// index, which is exactly __builtin_clz's job.
//
// MSVC's _BitScanReverse returns FALSE for 0 and does not write the index;
// matching that matters because several callers do `if (!_BitScanReverse(...))`
// and then use the value unconditionally.
//
// MinGW's intrin-impl.h provides these, so only supply them when its are absent.
#if defined(__GNUC__) && !defined(__MINGW32__)
static inline unsigned char _BitScanReverse(unsigned long *index, unsigned long mask)
{
    if (!mask) return 0;
    *index = (unsigned long)((sizeof(unsigned long) * 8 - 1) - __builtin_clzl(mask));
    return 1;
}

static inline unsigned char _BitScanReverse64(unsigned long *index, unsigned long long mask)
{
    if (!mask) return 0;
    *index = (unsigned long)((sizeof(unsigned long long) * 8 - 1) - __builtin_clzll(mask));
    return 1;
}

static inline unsigned char _BitScanForward(unsigned long *index, unsigned long mask)
{
    if (!mask) return 0;
    *index = (unsigned long)__builtin_ctzl(mask);
    return 1;
}

static inline unsigned char _BitScanForward64(unsigned long *index, unsigned long long mask)
{
    if (!mask) return 0;
    *index = (unsigned long)__builtin_ctzll(mask);
    return 1;
}
#endif

// __popcnt / __popcnt64 -- MSVC names for the population count.
static inline int __popcnt(unsigned int v) { return __builtin_popcount(v); }
#if defined(__GNUC__)
static inline long long __popcnt64(unsigned long long v) { return __builtin_popcountll(v); }
#endif

// The 64-bit _Interlocked* forms come from MinGW's intrin-impl.h where
// available; the plain integer forms come from the Win32 shim (windows.h).
#if !defined(__MINGW32__) && !defined(_MSC_VER)
static inline long long _InterlockedIncrement64(volatile long long *v) { return __sync_add_and_fetch(v, 1); }
static inline long long _InterlockedDecrement64(volatile long long *v) { return __sync_sub_and_fetch(v, 1); }
#endif

// ---------------------------------------------------------------------------
// CONTEXT
//
// MinGW's winnt.h defines CONTEXT for x64 (guarded on `__x86_64`) and for ARM
// (guarded on `__arm__`), but has no 32-bit x86 definition, and every Win32
// header that mentions exception handling then fails to parse. The engine never
// dereferences a CONTEXT -- it is only reachable through the SEH paths this port
// does not use -- so a placeholder is sufficient and honest.
// ---------------------------------------------------------------------------
#if !defined(__x86_64) && !defined(__arm__) && !defined(__aarch64__)
typedef struct _KISAK_CONTEXT { unsigned long dummy; } CONTEXT, *PCONTEXT;
#endif

// ---------------------------------------------------------------------------
// MinGW's C++ Interlocked* overload block
//
// winbase.h's `extern "C++"` overload set casts between `volatile long *` and
// `volatile __LONG32 *` in ways GCC rejects outside MSVC (the two are distinct
// types even at equal width). The engine calls none of the Interlocked* family
// through MinGW's versions -- the definitions above are used instead -- so the
// block is disabled with the macro MinGW provides for exactly this purpose, and
// the plain extern "C" declarations from winbase.h/winnt.h are what remains.
// ---------------------------------------------------------------------------
#ifndef NOWINBASEINTERLOCK
#define NOWINBASEINTERLOCK 1
#endif

// MSVC's __declspec(x). GCC and Clang have no such keyword; the specifiers the
// engine's headers actually use (dllimport, dllexport, align, noinline, naked,
// thread) all map onto __attribute__.
//
// NOTE: `align` is the exception. GCC spells it `aligned`, so
// `__declspec(align(N))` is rewritten to `__attribute__((aligned(N)))` in the
// source instead of being mapped here. Getting that wrong silently drops 8-byte
// alignment, which is exactly what animation_s was losing (96 vs 104 bytes).
#ifndef __declspec
#define __declspec(x) __attribute__((x))
#endif

// __assume / __noop: MSVC's optimisation hints. GCC ignores them, which is
// safe -- they never change observable behaviour, only codegen.
#define __assume(cond) ((void)0)
#define __noop(...)   ((void)0)

// Exception handling helpers MSVC's CRT headers reference.
#define __try      try
#define __except(x) catch(...)

// MSVC-only code paths in the decompilation test on this.
#define KISAK_MSVC_COMPAT 1
