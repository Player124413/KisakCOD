// KisakCOD -- shim for MSVC's <intrin.h>
//
// Only three files include this header directly:
//     src/qcommon/qcommon.h:1594
//     src/radiant/engine_stubs.cpp:9
//     src/universal/assertive.cpp:535
//
// The MSVC intrinsics the engine actually uses are __rdtsc(), __debugbreak(),
// __cpuidex() and the __intN types, all of which live in msvc_compat.h. The SSE
// intrinsics (_mm_cvtss_si32 and friends) come from the host's <xmmintrin.h>,
// which is a real, complete implementation on both x86 and (via sse2neon) ARM.
//
// The host headers are pulled in through the include order established by
// scripts/platform/posix_common.cmake, which places src/_platform/posix ahead of
// everything else.

#pragma once

#include "msvc_compat.h"

#if defined(__i386__) || defined(__x86_64__)
#include <xmmintrin.h>
#include <emmintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
// sse2neon translates the SSE intrinsic set to NEON. It is fetched by
// scripts/extern/sse2neon.cmake and its include directory is added to the
// include path by scripts/platform/android/platform.cmake.
#if defined(KISAK_HAVE_SSE2NEON)
#include "sse2neon.h"
#else
#include <xmmintrin.h>
#endif
#endif
