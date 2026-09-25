// KisakCOD -- shim for MinGW's corecrt.h
//
// MinGW's corecrt.h is the root of its CRT type hierarchy: it pulls in the
// whole crt/ tree, which redefines time_t, ssize_t, struct timeval, fd_set,
// select, mbstate_t and uintptr_t with Windows widths. On a POSIX libc those
// are already correct, and redefining them produces hundreds of errors.
//
// This file shadows MinGW's (src/_platform/posix comes first on the include
// path) and forwards to the host's own C headers instead, then defines the
// handful of macros MinGW's crtdefs.h expects to find here.
//
// crt/ is deliberately NOT on the include path -- see docs/ANDROID_PORT.md.

#pragma once

#ifdef __cplusplus
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <ctime>
#include <cstdlib>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#endif

#include <sys/types.h>
#include <wchar.h>

// crtdefs.h uses these to decide how to decorate declarations. Empty on POSIX:
// there is no import library and no MSVC deprecation machinery.
#ifndef _CRTIMP
#define _CRTIMP
#endif
#ifndef _CRT_ALIGN
#define _CRT_ALIGN(x)
#endif
#ifndef _CRT_DEPRECATE_TEXT
#define _CRT_DEPRECATE_TEXT(t)
#endif

// MinGW's crt headers guard these; the host headers have already defined the
// types, so mark them done to stop any later redefinition.
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
#endif
#ifndef _UINTPTR_T_DEFINED
#define _UINTPTR_T_DEFINED
#endif
#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
#endif
