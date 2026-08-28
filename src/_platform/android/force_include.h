// ============================================================================
// Android port — forced include header (added via -include compiler flag)
//
// This header is included BEFORE everything else in every translation unit on
// Android. It defines MSVC-specific keywords and macros that the KisakCOD
// engine code uses throughout, ensuring they compile on GCC/Clang.
// ============================================================================
#pragma once

// ---- MSVC keywords that GCC/Clang don't have ---- 
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#ifndef __unaligned
#define __unaligned
#endif
#ifndef __declspec
#define __declspec(x) __attribute__((x))
#endif
#ifndef __noop
#define __noop ((void)0)
#endif
#ifndef __forceinline
#define __forceinline inline
#endif
#ifndef __inline
#define __inline inline
#endif

// ---- MSVC-specific type keywords ----
#ifndef __int64
#define __int64 long long
#endif
#ifndef __int32
#define __int32 int
#endif
#ifndef __int16
#define __int16 short
#endif
#ifndef __int8
#define __int8 signed char
#endif
#ifndef __w64
#define __w64
#endif

// ---- __assume ----
#ifndef __assume
#define __assume(cond) do { if (!(cond)) __builtin_unreachable(); } while(0)
#endif

// ---- MSVC SAL annotations (no-ops) ----
#ifndef _In_
#define _In_
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Inout_
#define _Inout_
#endif
#ifndef _In_z_
#define _In_z_
#endif
#ifndef _Out_z_
#define _Out_z_
#endif
#ifndef _Printf_format_string_
#define _Printf_format_string_
#endif
#ifndef _Ret_maybenull_
#define _Ret_maybenull_
#endif
#ifndef _Check_return_
#define _Check_return_
#endif
#ifndef _Pre_defensive_
#define _Pre_defensive_
#endif

// ---- __unaligned ----
#ifndef UNALIGNED
#define UNALIGNED
#endif

// ---- restrict ----
#ifndef __restrict
#define __restrict __restrict__
#endif

// ---- Aligned allocation ----
// __declspec(align(N)) becomes __attribute__((aligned(N)))
// This is handled via the __declspec macro above.

// ---- NOINLINE ----
#ifndef NOINLINE
#define NOINLINE
#endif

// ---- DECLSPEC_NORETURN ----
#ifndef DECLSPEC_NORETURN
#define DECLSPEC_NORETURN
#endif

// ---- ASSERT / VERIFY ----
#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif
#ifndef VERIFY
#define VERIFY(x) ((void)(x))
#endif

// ---- UNREFERENCED_PARAMETER ----
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#endif

// ---- COMPILE_TIME_ASSERT ----
#ifndef COMPILE_TIME_ASSERT
#define COMPILE_TIME_ASSERT(x) static_assert(x, "")
#endif

// ---- LOBYTE, HIBYTE, etc ----
#ifndef LOBYTE
#define LOBYTE(w) ((unsigned char)((uintptr_t)(w) & 0xff))
#endif
#ifndef HIBYTE
#define HIBYTE(w) ((unsigned char)(((uintptr_t)(w) >> 8) & 0xff))
#endif
#ifndef LOWORD
#define LOWORD(d) ((unsigned short)((uintptr_t)(d) & 0xffff))
#endif
#ifndef HIWORD
#define HIWORD(d) ((unsigned short)(((uintptr_t)(d) >> 16) & 0xffff))
#endif